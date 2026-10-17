# foo_dsp_manualstart

A DSP for foobar2000 2.26 that hosts "Affix silence" (1000 ms / 0 ms / [empty]),
applied only to the first track started manually or after a user-initiated "Stop."

Sequential playback, skipping to the next track, or double-clicking a track during playback = no silence added.

## Usage

Install the Pregap DSP component.

Install the Affix silence component.

Installing the DLL:

- 64-bit foobar2000: Copy `foo_dsp_manualstart.dll` (x64 artifact) to
`<profile>\user-components-x64\foo_dsp_manualstart\`

- 32-bit foobar2000: Copy the DLL (Win32 artifact) to
`<profile>\user-components\foo_dsp_manualstart\`

The profile folder can be accessed via File > Open profile folder (or it is the
foobar2000 folder itself if using portable mode).

Restart foobar2000.

Set the default chain as follows:

`Host Affix silence -> Pregap`
