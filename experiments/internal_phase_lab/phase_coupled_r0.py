from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F
from scipy.io import wavfile
from scipy.signal import resample_poly

from nam.models._from_nam import init_from_nam


def load_mono(path: Path) -> tuple[int, np.ndarray]:
    rate, audio = wavfile.read(path)
    if audio.ndim != 1:
        raise ValueError(f"{path} is not mono")
    if np.issubdtype(audio.dtype, np.integer):
        audio = audio.astype(np.float64) / max(
            abs(np.iinfo(audio.dtype).min), np.iinfo(audio.dtype).max
        )
    return rate, audio.astype(np.float32)


def prototype(factor: int, half_lobes: int, device: torch.device) -> torch.Tensor:
    n = np.arange(-half_lobes * factor, half_lobes * factor + 1)
    taps = np.sinc(n / factor) * np.kaiser(len(n), 8.0)
    taps /= taps[n.size // 2]
    return torch.tensor(taps, dtype=torch.float32, device=device)[None, None]


def interpolate_bandlimited(
    x: torch.Tensor, factor: int, taps: torch.Tensor
) -> torch.Tensor:
    batch, channels, length = x.shape
    expanded = torch.zeros(
        batch, channels, (length - 1) * factor + 1, device=x.device, dtype=x.dtype
    )
    expanded[..., ::factor] = x
    kernel = taps.expand(channels, 1, -1)
    y = F.conv1d(expanded, kernel, padding=taps.shape[-1] // 2, groups=channels)
    # Keep the native grid exact, including finite-window numerical residue.
    y[..., ::factor] = x
    return y


def recouple_from_phase_zero(
    high: torch.Tensor, factor: int, taps: torch.Tensor
) -> tuple[torch.Tensor, float]:
    phase_zero = high[..., ::factor].clone()
    coupled = interpolate_bandlimited(phase_zero, factor, taps)
    common = min(coupled.shape[-1], high.shape[-1])
    coupled = coupled[..., -common:]
    phase_zero = phase_zero[..., -((common - 1) // factor + 1):]
    coupled[..., ::factor] = phase_zero
    error = float(torch.max(torch.abs(coupled[..., ::factor] - phase_zero)))
    return coupled, error


@torch.inference_mode()
def phase_coupled_forward(
    public_model,
    x: torch.Tensor,
    factor: int,
    taps: torch.Tensor,
    recouple_each_layer: bool,
) -> tuple[torch.Tensor, float]:
    net = public_model._net
    if len(net._layer_arrays) != 1 or net._head is not None:
        raise NotImplementedError("Prototype expects one layer array and no top-level head.")
    layer_array = net._layer_arrays[0]
    if any(
        getattr(layer, name) is not None
        for layer in layer_array._layers
        for name in (
            "_conv_pre_film",
            "_conv_post_film",
            "_input_mixin_pre_film",
            "_input_mixin_post_film",
            "_activation_pre_film",
            "_activation_post_film",
            "_layer1x1_post_film",
            "_head1x1_post_film",
        )
    ):
        raise NotImplementedError("FiLM is not implemented in this diagnostic prototype.")

    c_native = x
    c_high = interpolate_bandlimited(c_native, factor, taps)
    native = layer_array._rechannel(x)
    high = interpolate_bandlimited(native, factor, taps)
    high_head = None
    native_head = None
    max_phase0_error = 0.0

    out_length_no_head = min(x.shape[-1], c_native.shape[-1]) - (
        layer_array._receptive_field_no_head_rechannel - 1
    )
    for layer in layer_array._layers:
        # Native branch: this is the exact 48 kHz teacher state used to pin
        # phase zero after every nonlinear residual block.
        zconv_native = layer.conv(native)
        mix_native = layer._input_mixer(c_native)[..., -zconv_native.shape[-1] :]
        z1_native = zconv_native + mix_native
        post_native = layer._activation(z1_native)
        layer_output_native = (
            post_native
            if layer._layer1x1 is None
            else layer._layer1x1(post_native)
        )
        native = native[..., -layer_output_native.shape[-1] :] + layer_output_native
        native_head_term = (
            post_native
            if layer.head1x1 is None
            else layer.head1x1(post_native)
        )[..., -out_length_no_head:]
        native_head = (
            native_head_term
            if native_head is None
            else native_head[..., -out_length_no_head:] + native_head_term
        )

        # High-rate branch: same coefficients and kernel lengths, but temporal
        # dilation is multiplied by F. Each modulo-F phase therefore sees the
        # original physical lookback while all phases traverse every nonlinear
        # layer.
        zconv_high = F.conv1d(
            high,
            layer.conv.weight,
            layer.conv.bias,
            dilation=layer.dilation * factor,
            groups=layer.conv.groups,
        )
        mix_high = layer._input_mixer(c_high)[..., -zconv_high.shape[-1] :]
        z1_high = zconv_high + mix_high
        post_high = layer._activation(z1_high)
        layer_output_high = (
            post_high if layer._layer1x1 is None else layer._layer1x1(post_high)
        )
        high = high[..., -layer_output_high.shape[-1] :] + layer_output_high

        head_high = (
            post_high if layer.head1x1 is None else layer.head1x1(post_high)
        )
        high_out_length = (out_length_no_head - 1) * factor + 1
        head_high = head_high[..., -high_out_length:]
        high_head = (
            head_high
            if high_head is None
            else high_head[..., -high_out_length:] + head_high
        )

        # r_p = 0: non-zero phases are left exactly as produced by the
        # band-limited initialization and scaled-dilation network. Phase zero
        # is replaced with the teacher state, guaranteeing exact native behavior.
        expected_high_length = (native.shape[-1] - 1) * factor + 1
        high = high[..., -expected_high_length:]
        high[..., ::factor] = native
        expected_head_length = (native_head.shape[-1] - 1) * factor + 1
        high_head = high_head[..., -expected_head_length:]
        high_head[..., ::factor] = native_head

        if recouple_each_layer:
            high, coupling_error = recouple_from_phase_zero(high, factor, taps)
            high_head, head_coupling_error = recouple_from_phase_zero(
                high_head, factor, taps
            )
            max_phase0_error = max(
                max_phase0_error, coupling_error, head_coupling_error
            )

        phase0 = high[..., ::factor]
        max_phase0_error = max(
            max_phase0_error,
            float(torch.max(torch.abs(phase0 - native))),
        )

    head_conv = layer_array._head_rechannel
    high_output = F.conv1d(
        high_head,
        head_conv.weight,
        head_conv.bias,
        dilation=factor,
        groups=head_conv.groups,
    )
    high_output = net._head_scale * high_output
    native_output = net._head_scale * layer_array._head_rechannel(native_head)
    phase0 = high_output[..., ::factor]
    expected_output_length = (native_output.shape[-1] - 1) * factor + 1
    high_output = high_output[..., -expected_output_length:]
    high_output[..., ::factor] = native_output
    phase0 = high_output[..., ::factor]
    max_phase0_error = max(
        max_phase0_error,
        float(torch.max(torch.abs(phase0 - native_output))),
    )
    return high_output, max_phase0_error


def esr_db(reference: np.ndarray, candidate: np.ndarray) -> float:
    n = min(len(reference), len(candidate))
    reference = reference[-n:]
    candidate = candidate[-n:]
    return 10.0 * np.log10(
        (np.sum((candidate - reference) ** 2) + 1e-30)
        / (np.sum(reference**2) + 1e-30)
    )


def error_and_reference_energy(
    reference: np.ndarray, candidate: np.ndarray
) -> tuple[float, float]:
    n = min(len(reference), len(candidate))
    reference = reference[-n:].astype(np.float64)
    candidate = candidate[-n:].astype(np.float64)
    return (
        float(np.sum((candidate - reference) ** 2)),
        float(np.sum(reference**2)),
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("model", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("--factor", type=int, default=2)
    parser.add_argument("--half-lobes", type=int, default=12)
    parser.add_argument("--recouple-each-layer", action="store_true")
    parser.add_argument("--windows", type=int, default=3)
    args = parser.parse_args()

    source = json.loads(args.model.read_text(encoding="utf-8"))
    submodel_dict = max(
        source["config"]["submodels"], key=lambda value: value["max_value"]
    )["model"]
    model = init_from_nam(submodel_dict).eval()
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model.to(device)

    input_rate, input_audio = load_mono(args.input)
    target_rate, target_audio = load_mono(args.target)
    if input_rate != target_rate:
        raise ValueError("Sample rates differ")
    fir = prototype(args.factor, args.half_lobes, device)

    window_seconds = 1.5
    preroll_seconds = 0.5
    evaluation = int(window_seconds * input_rate)
    preroll = int(preroll_seconds * input_rate)
    common = min(len(input_audio), len(target_audio))
    native_errors = []
    coupled_errors = []
    output_only_errors = []
    phase0_errors = []
    forward_seconds = []
    native_energy = [0.0, 0.0]
    coupled_energy = [0.0, 0.0]
    output_only_energy = [0.0, 0.0]

    fractions = np.linspace(0.08, 0.92, args.windows)
    for fraction in fractions:
        center = int(fraction * common)
        start = max(0, min(center - preroll, common - preroll - evaluation))
        stop = start + preroll + evaluation
        x_np = input_audio[start:stop]
        target = target_audio[start:stop]
        x = torch.tensor(x_np, device=device)[None, None]

        native = model(x[:, 0], pad_start=True).detach().cpu().numpy()[0]
        native_high = resample_poly(
            native, args.factor, 1, window=("kaiser", 8.0)
        )
        output_only = resample_poly(
            native_high, 1, args.factor, window=("kaiser", 8.0)
        )
        if device.type == "cuda":
            torch.cuda.synchronize()
        started = time.perf_counter()
        high, phase0_error = phase_coupled_forward(
            model,
            F.pad(x, (model.receptive_field - 1, 0)),
            args.factor,
            fir,
            args.recouple_each_layer,
        )
        if device.type == "cuda":
            torch.cuda.synchronize()
        forward_seconds.append(time.perf_counter() - started)
        high_np = high.detach().cpu().numpy()[0, 0]
        coupled = resample_poly(high_np, 1, args.factor, window=("kaiser", 8.0))

        native_errors.append(esr_db(target[preroll:], native[preroll:]))
        coupled_errors.append(esr_db(target[preroll:], coupled[preroll:]))
        output_only_errors.append(
            esr_db(target[preroll:], output_only[preroll:])
        )
        for accumulator, candidate in (
            (native_energy, native),
            (coupled_energy, coupled),
            (output_only_energy, output_only),
        ):
            error_energy, reference_energy = error_and_reference_energy(
                target[preroll:], candidate[preroll:]
            )
            accumulator[0] += error_energy
            accumulator[1] += reference_energy
        phase0_errors.append(phase0_error)

    aggregate_db = lambda energy: 10.0 * np.log10(
        (energy[0] + 1e-30) / (energy[1] + 1e-30)
    )
    print(
        json.dumps(
            {
                "factor": args.factor,
                "half_lobes": args.half_lobes,
                "recouple_each_layer": args.recouple_each_layer,
                "windows": args.windows,
                "native_window_esr_db": [float(value) for value in native_errors],
                "coupled_r0_window_esr_db": [
                    float(value) for value in coupled_errors
                ],
                "output_only_window_esr_db": [
                    float(value) for value in output_only_errors
                ],
                "native_mean_esr_db": float(np.mean(native_errors)),
                "coupled_r0_mean_esr_db": float(np.mean(coupled_errors)),
                "output_only_mean_esr_db": float(np.mean(output_only_errors)),
                "native_aggregate_esr_db": float(aggregate_db(native_energy)),
                "coupled_r0_aggregate_esr_db": float(
                    aggregate_db(coupled_energy)
                ),
                "output_only_aggregate_esr_db": float(
                    aggregate_db(output_only_energy)
                ),
                "max_phase0_abs_error": max(phase0_errors),
                "mean_phase_forward_seconds": float(np.mean(forward_seconds)),
                "device": str(device),
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
