from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path

import numpy as np
import torch
from scipy.signal import firwin

from nam.models._from_nam import _init_wavenet, init_from_nam
from nam.models.wavenet._conv import Conv1d


def nyquist_interpolate_kernel(
    weight: torch.Tensor, factor: int, scale_exponent: float, half_lobes: int = 12
) -> torch.Tensor:
    old_length = weight.shape[-1]
    new_length = (old_length - 1) * factor + 1
    taps = 2 * half_lobes * factor + 1
    prototype = factor * firwin(taps, 1.0 / factor, window=("kaiser", 8.0))
    center = taps // 2

    flat = weight.detach().cpu().numpy().reshape(-1, old_length)
    expanded = np.zeros((flat.shape[0], new_length), dtype=np.float64)
    expanded[:, ::factor] = flat
    filtered = np.empty_like(expanded)
    for row, values in enumerate(expanded):
        full = np.convolve(values, prototype, mode="full")
        filtered[row] = full[center : center + new_length]

    # A discrete convolution at F times the sample rate needs 1/F-scaled
    # coefficients to preserve the kernel's integral/DC response. Without this
    # normalization, every expanded convolution gains approximately F.
    scale = factor**(-scale_exponent)
    filtered *= scale
    filtered[:, ::factor] = flat * scale
    return torch.from_numpy(filtered.reshape(*weight.shape[:-1], new_length)).to(
        dtype=weight.dtype, device=weight.device
    )


def expanded_export_config(model_dict: dict, factor: int) -> dict:
    result = copy.deepcopy(model_dict["config"])
    for layer_array in result["layers"]:
        layer_array["kernel_sizes"] = [
            (int(size) - 1) * factor + 1 for size in layer_array["kernel_sizes"]
        ]
        head = layer_array.get("head")
        if head is not None:
            head["kernel_size"] = (int(head["kernel_size"]) - 1) * factor + 1
    return result


def expand_submodel(
    model_dict: dict, factor: int, scale_exponent: float
) -> tuple[dict, dict]:
    teacher = init_from_nam(model_dict)
    student_config = expanded_export_config(model_dict, factor)
    student = _init_wavenet(student_config, model_dict.get("sample_rate"))

    teacher_modules = dict(teacher.named_modules())
    copied = 0
    expanded = 0
    grid_error = 0.0
    with torch.no_grad():
        for name, student_module in student.named_modules():
            teacher_module = teacher_modules.get(name)
            if not isinstance(student_module, Conv1d) or not isinstance(teacher_module, Conv1d):
                continue
            if teacher_module.weight is not None:
                if teacher_module.weight.shape[-1] == 1:
                    student_module.weight.copy_(teacher_module.weight)
                    copied += 1
                else:
                    new_weight = nyquist_interpolate_kernel(
                        teacher_module.weight, factor, scale_exponent
                    )
                    if new_weight.shape != student_module.weight.shape:
                        raise RuntimeError(
                            f"{name}: expanded shape {tuple(new_weight.shape)} "
                            f"does not match student {tuple(student_module.weight.shape)}"
                        )
                    student_module.weight.copy_(new_weight)
                    grid_error = max(
                        grid_error,
                        float(
                            torch.max(
                                torch.abs(
                                    student_module.weight[..., ::factor]
                                    - teacher_module.weight * factor**(-scale_exponent)
                                )
                            )
                        ),
                    )
                    expanded += 1
            if teacher_module.bias is not None:
                student_module.bias.copy_(teacher_module.bias)

    exported = student._get_export_dict()
    exported["metadata"] = copy.deepcopy(model_dict.get("metadata", {}))
    exported["metadata"]["kernel_expansion"] = {
        "factor": factor,
        "scale_exponent": scale_exponent,
        "method": "zero-insertion + Kaiser-windowed sinc Nyquist FIR",
        "half_lobes": 12,
        "fine_tuned": False,
    }
    report = {
        "copied_1x1_convolutions": copied,
        "expanded_convolutions": expanded,
        "original_grid_max_abs_error": grid_error,
        "teacher_receptive_field": int(teacher.receptive_field),
        "student_receptive_field": int(student.receptive_field),
    }
    return exported, report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--factors", type=int, nargs="+", default=[2, 4])
    parser.add_argument("--scale-exponents", type=float, nargs="+", default=[1.0])
    args = parser.parse_args()

    source = json.loads(args.input.read_text(encoding="utf-8"))
    if source.get("architecture") != "SlimmableContainer":
        raise ValueError("This experiment currently expects a SlimmableContainer.")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for factor in args.factors:
        for scale_exponent in args.scale_exponents:
            result = copy.deepcopy(source)
            reports = []
            for submodel in result["config"]["submodels"]:
                submodel["model"], report = expand_submodel(
                    submodel["model"], factor, scale_exponent
                )
                reports.append(report)
            result.setdefault("metadata", {})["kernel_expansion"] = {
                "factor": factor,
                "scale_exponent": scale_exponent,
                "method": "zero-insertion + Kaiser-windowed sinc Nyquist FIR",
                "fine_tuned": False,
            }
            suffix = str(scale_exponent).replace(".", "p")
            output = (
                args.output_dir
                / f"{args.input.stem}_kernel_fir_x{factor}_g{suffix}.nam"
            )
            output.write_text(
                json.dumps(result, separators=(",", ":")), encoding="utf-8"
            )
            print(
                json.dumps(
                    {
                        "output": str(output),
                        "factor": factor,
                        "scale_exponent": scale_exponent,
                        "submodels": reports,
                    }
                )
            )


if __name__ == "__main__":
    main()
