# foo_dsp_manualstart

A foobar2000 2.x DSP that inserts 1000 ms of silence at the start of a track
**only when playback is started manually**. It is useful for DACs and receivers
that need a moment to initialise after being powered on, and would otherwise cut
off the beginning of the first track. It does not depend on any other component.

## Usage

1. Add **Manual start silence** to your DSP chain (Preferences > Playback > DSP Manager).
2. "Manual" means the first playback after foobar2000 is launched, or the first
   playback after you pressed Stop. Sequential (gapless) track changes, Next, and
   double-clicking a track while playing do not add any silence.

The manual-start silence stays pending until audio actually passes through the
DSP, so it is not lost if the first file played bypasses the DSP chain.

In addition, the **first audio format change** (sample rate or channel count)
after foobar2000 is launched also receives the silence, once. This helps outputs
that need to be re-initialised the first time the format changes, for example
when switching from a DSD-converted stream to a PCM file.

## Installation

Install the Pregap DSP component (https://foobar.hyv.fi/?view=foo_dsp_pregap), set it to 500ms, and toggle on:

- Add silence after manual track change

- Add silence if audio format change

Install the foo_dsp_manualstart DLL:

Copy `foo_dsp_manualstart.dll` into your foobar2000 profile folder
(File > Open profile folder) and restart foobar2000:

- 64-bit foobar2000: `user-components-x64\foo_dsp_manualstart\`
- 32-bit foobar2000: `user-components\foo_dsp_manualstart\`
