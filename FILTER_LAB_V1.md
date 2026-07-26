# NAM-Oversampler Filter Lab v1

This branch is an experimental build with a separate plug-in identity:

- Plug-in name: `NAM-Oversampler Filter Lab`
- Plug-in ID: `NFL1`
- Version: inherited from the starting branch
- Convenience solution: `NeuralAmpModeler/NeuralAmpModeler-FilterLab.sln`

## Editable parameters

### Linear-phase upsampler
- Short taps
- Long taps
- Cutoff bias
- Kaiser beta

### Linear-phase downsampler
- Short taps
- Long taps
- Cutoff bias
- Kaiser beta

### Inner stages and guards
- Inner short taps
- Inner long taps
- Inner Kaiser beta
- Passband target
- Input guard enable
- Output guard enable
- Guard cutoff bias
- Guard order

### Minimum-phase downsampler
- True polyphase / clean fused
- Clean-fused Butterworth order

## Behavior

Values edited in the GUI remain pending until **APPLY** is pressed.

When APPLY is pressed during realtime playback, the existing NAM-Oversampler
fade transition is used. Coefficients and buffers are redesigned only after
the fade-out reaches zero.

Tap counts are sanitized to odd values. IIR orders are sanitized to even
values between 2 and 64.

## Build

Open:

`NeuralAmpModeler/NeuralAmpModeler-FilterLab.sln`

Build the VST3 project using `Release | x64`.

The original `NeuralAmpModeler.sln` also works because the new solution points
to the same `.vcxproj` files; the copy only gives the solution/projects clearer
display names.

If the starting AudioDSPTools checkout uses the older shared-FIR layout (including
the 4e4d47a-era implementation), the script rebases only
`filter-lab-ui-v1-audiodsptools` onto the compatible stage-specific resampler commit
`0685f054c09fec6eac438117b9dfe899311b0540`. The original submodule branch and
commit remain untouched.