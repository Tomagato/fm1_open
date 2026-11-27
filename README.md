# Beach 909 for M-VAVE FM-1

A Terms Of Service release on Mizu Records.

A fork of [FM1 Open V14](https://github.com/m-vave/fm1_open) that turns the FM-1 into a very simple 909-style drum machine: one page, eight drums, sixteen steps per bar, reverb and delay, under a pixel-art view of deep space. The red star is the Terms Of Service logo, and its pulse lights up in the colour of whichever drum is sounding.

The FM-1 sound engine is a closed binary, so the kit is the built-in sampler loaded with a 909-style drum kit. The kit is synthesized (oscillators and noise, `web/synth/demo_bank.h`); it contains no recordings. The stock firmware fills its Flash slot to within a few hundred bytes, so the multi-page FM1 Open panel (DX7/303/808 editors, mixer, Bluetooth and MIDI menus) is not in this build. The engines, MIDI input, USB audio and the workbench transfers are unchanged.

| Control | Beach 909 |
| --- | --- |
| Black keys F#3 to A#4 (first eight) | Play and select a drum: BD, SD, CH, OH, CP, RS, TM, CR |
| White keys (sixteen) | Tap to add or remove a step for the selected drum in the bar on screen |
| PLAY / REC | Start or stop / record the black keys into the running pattern |
| K1 to K4 | LEVEL, TUNE, DECAY of the selected drum, TEMPO |
| FX | Switch K1 to K4 to REVERB send, DELAY send, delay FEEDBACK, SWING |
| Other three encoders | Two select the drum, one sets the tempo |
| SCL | Pattern length, 1 to 4 bars |
| OCT - / OCT + / SEQ | Previous bar / next bar / follow the playing bar |
| EDIT + white key | Delete that step (every drum on it) |
| EDIT + black key | Delete that drum from the whole pattern |
| EDIT (hold 2 s) | Delete the whole pattern |
| HOME | Synth mode: all 27 keys play a DX7 on track 1 (MIDI channel 1). OCT shifts the octave; K1 to K4 are VOICE, VOLUME, REVERB and DELAY. Press HOME again for the drums. The synth is played live; it is not recorded into the pattern |
| ENV | Vape knobs for the e-cigarette: K1 SMOKE (0 to 100; at 100 the cloud fills the whole screen), K2 DRIFT speed, K3 MAX, K4 CLEAR. Smoke is also the effects macro: it pushes the reverb and delay sends of the drums and the synth, and the delay feedback, from your own settings up to full |
| LFO | Tide Blocks, a falling-block game that runs while the pattern plays (OCT moves, SEQ turns, REC drops) |
| Hold a white key + K1 to K4 | Edit that step only: VELOCITY, CHANCE of playing, parameter LOCK (level, tune or decay of the selected drum) and its VALUE. A step keeps one velocity, chance and lock for all the drums on it; a pattern holds 16 locks |
| ARP (tap) | Note repeat: four rates, then off. A held drum key retriggers in time |
| ARP (hold) | Internal tempo or external MIDI clock |
| GLO | Bluetooth MIDI: tap shows the link; hold closes it, or swaps between waiting for a phone or computer and searching for a Bluetooth MIDI controller |
| SAVE | Store the pattern (project slot 16; other slots are never written) |
| MASTER | Output volume |

The kit also answers MIDI notes on channel 10.

## Background picture

<https://tomagato.github.io/fm1_open/background.html> puts your own picture behind the screen in place of the space scene. The page shrinks a PNG or JPEG to 120 x 56 pixels in the browser and sends it to the FM-1 over USB MIDI (transfer commands 30 to 33 in `app/transfer.c`); nothing is uploaded. The picture is kept in the last four sectors of the sample area (`app/background.c`) and survives a restart. Remove it from the same page.

## Install

1. Back up projects and samples with the workbench you already use.
2. Open <https://tomagato.github.io/fm1_open/> and follow it to the install page in Chrome or Edge, connect the FM-1 over USB and update. The page is published by the build workflow; the same file is in the `fm1-workbench` build artifact and also works from `localhost`.
3. On the page's sample tab, load the built-in samples and write them to the device. This replaces the sample bank on the device with the 909 kit; until then the screen shows `NO SAMPLES` or plays the samples that are already there.

The same page restores the original M-VAVE V15 firmware (`sdk/firmware/FM-1_015.fwsc`). This build has not been validated on hardware by its author; use it at your own risk.

## Build

`.github/workflows/build.yml` builds everything on a `windows-latest` runner: it unpacks JieLi toolchain 2.5.3 with innoextract, then runs the preview, firmware and workbench builds and uploads `FM1_OFFICIAL_OPEN.fwsc`, the workbench page and the preview as artifacts. `FM1_SOURCE_URL` is the GitHub archive of the commit being built.

The rest of this file is the FM1 Open V14 README.

---

# FM1 Open V14

English · [简体中文](README.zh-CN.md)

8 tracks, 8 sample slots, 16 FX demos; binary SDK included.

[Workbench and development kit](https://open.m-vave.com/fm1-workbench) · [Official forum](https://forum.m-vave.com)

Use the workbench to update firmware, preview the instrument and manage projects, samples and DX7 voices. Select "Source + SDK" to download the development kit matching the online release. Back up projects, voices and samples before updating, and keep the device powered during writes.

## V14 features

- **DX7 library:** 128 voices in four groups of 32, with the M-VAVE FM1 original V15 factory voices and order. Saved user banks take priority. DX7 single-voice and 32-voice bank SysEx import/export is supported; extension parameters are not handled. Changing the library does not change patches already saved in projects.
- **Track roles:** on SEQ **8/8 TRACK ROLE**, K1 selects NORMAL, PLAY or OFF; K2 selects input MIDI channel 1–16. PLAY retains the track's timbre and held notes across project changes. MUTE/SOLO use the existing controls, with independent NORMAL and PLAY solo groups. PLAY timbre edits are not saved with projects: export a timbre file, or save DX7 voices to the independent library.
- **Step recording:** with the track set to NORMAL, press REC while stopped and play notes. Releasing all played keys advances to the next step, including DRUM 808 and SAMPLER. Overlapping notes share a step; during playback, REC records in time. PLAY tracks are for live performance and do not record notes.
- **Tempo and delay:** internal tempo is 30–300 BPM. Tap Tempo is available on the workbench, with SEQ on the FX DELAY page, or with HOME held while tapping SEQ elsewhere. Delay supports tempo synchronization and fixed times of about 30–2040 ms.

## MIDI

FM1 accepts DIN, USB and BLE MIDI input, with USB/BLE output. Default input channels 1–7 address tracks 1–7; channel 10 addresses track 8. Change each input channel on TRACK ROLE.

- **5/8 SYNC / LOOP:** select internal or external clock, tempo, loop length and clock input.
- **6/8 MIDI TRACK:** select SEQ/ARP LOCAL, BOTH or MIDI output, output channel, output ports and physical KEY OUT.
- **7/8 MIDI ROUTING:** configure LOCAL, CLK OUT and MIDI Thru. For DAW monitoring with Local Off, enable KEY OUT and select the output ports.

MIDI preferences are saved independently of projects. The browser simulator previews these settings without sending performance MIDI to external ports; configure the instrument on its panel. MIDI Clock, Start, Continue, Stop and Song Position Pointer input are supported. Clock loss stops synchronized playback.

## Updates and compatibility

V13 can update to V14. The workbench supports returning to original V15 and migrating from compatible third-party firmware. The standard FM1 update protocol remains available. Compatibility has been checked with selected firmware versions; it is not a guarantee for every third-party release.

Firmware switching does not make project and sample formats interchangeable. Keep exported backups for rollback. Older firmware may not support V14 track preferences, the independent voice library or synchronized/extended delay settings.

To back up the independent voice library, read and export groups A–D separately. Project files do not contain sample audio. Restore custom samples from their sample bank backup; writing the built-in sample bank replaces the current samples.

The eight sample slots share about 19.55 seconds of new sample storage. Existing larger banks remain readable; they may need to be backed up and reduced before writing the independent DX7 library.

## Build

Requires Windows x64, PowerShell, `curl.exe` and `tar.exe`. Firmware builds require **JieLi Windows toolchain 2.5.3**, which includes PI32v2 clang 4.0.1. [Download the 2.5.3 installer from JieLi](https://jl-update.oss-cn-shenzhen.aliyuncs.com/2.5.3.exe) and follow the [installation guide](https://doc.zh-jieli.com/Tools/zh-cn/dev_tools/dev_env/jl_toolchain.html). Set `FM1_PI32_TOOLCHAIN` to its `pi32` folder containing `bin/clang.exe` (normally `C:\JL\pi32`). FM1 continues to use the `r3` CPU target.

The build checks all 22 toolchain files against `sdk/manifest.json`. Older V14 kits were pinned to 2.5.2 and reject 2.5.3; update the complete source + SDK kit together, keeping the manifest checks enabled. For this revision, the expected SHA-256 of `pi32/bin/clang.exe` is `d4bc97cf421c1ccc373dd8924ce88720ff7457b3f1478d15df4aeda6ae923bce`.

For a local web preview:

~~~powershell
.\tools\Build-Web.ps1 -LocalPreview
~~~

For distribution builds, first set `FM1_SOURCE_URL` to the HTTPS URL of the matching source archive, including your changes, licenses and build/install scripts. Then run:

~~~powershell
.\build.bat --web
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
.\build.bat --package
~~~

For a local firmware build without a distribution source URL, run `./sdk/Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'`. A moving GitHub `tree/main` page is not a matching source archive for distribution.

The web version builds from the included C sources. Its first build downloads WASI SDK 27; set `FM1_WASI_SDK` to reuse an installation. Outputs are `build/preview/index.html`, `build/dx7/generated/FM1_OFFICIAL_OPEN.fwsc` and `build/official-web/FM1-Official-Open-Source.html`.

The [per-track tuning API](app/instrument.h) supports custom tuning; a dedicated tuning editor is not included.

## License

APP: [GPLv3 with the FM1 SDK linking exception](LICENSE.md). The device engine is supplied as a binary SDK with separate terms. Retain the applicable [SDK terms, third-party licenses and contributor notices](THIRD_PARTY.md) when distributing builds.
