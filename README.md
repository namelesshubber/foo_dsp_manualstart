# foo_dsp_manualstart

A DSP for foobar2000 2.26 that inserts 1000 ms of silence at the beginning of the first track started manually or after a user-initiated "Stop."

Sequential playback, skipping to the next track, or double-clicking a track during playback = no silence added.

## Install process and Usage

Install the Pregap DSP component (https://foobar.hyv.fi/?view=foo_dsp_pregap), set it to 500ms, and toggle on:

- Add silence after manual track change

- Add silence if audio format change

Install the foo_dsp_manualstart DLL:

- 64-bit foobar2000: Copy `foo_dsp_manualstart.dll` (x64) to
`<profile>\user-components-x64\foo_dsp_manualstart\`

- 32-bit foobar2000: Copy `foo_dsp_manualstart.dll` (Win32) to
`<profile>\user-components\foo_dsp_manualstart\`

The profile folder can be accessed via File > Open profile folder (or it is the
foobar2000 folder itself if using portable mode).

Restart foobar2000.

Set the default chain as follows:

`Pregap -> Manual start silence`
