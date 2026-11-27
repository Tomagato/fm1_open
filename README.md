# Bitch909 for M-VAVE FM-1

A Terms Of Service release.

A fork of [FM1 Open V14](https://github.com/m-vave/fm1_open) that turns the FM-1 into a simple instrument to play live: a DX7 synth with a quick preset list, a 909-style drum kit, a loop recorder, reverb and delay turned all the way up, a coin pusher that plays notes by chance, under a pixel-art view of deep space. The red star is the Terms Of Service logo, and its pulse lights up in the colour of whichever drum is sounding.

The FM-1 sound engine is a closed binary, so the kit is the built-in sampler loaded with a 909-style drum kit. The kit is synthesized (oscillators and noise, `web/synth/demo_bank.h`); it contains no recordings. The stock firmware fills its Flash slot to within a few hundred bytes, so the multi-page FM1 Open panel (DX7/303/808 editors, mixer, Bluetooth and MIDI menus) is not in this build. The engines, MIDI input, USB audio and the workbench transfers are unchanged.

| Control | Bitch909 |
| --- | --- |
| Keys | Play the DX7 synth (all 27 keys) |
| REC | Loop recorder. One press starts recording what you play into a four-bar loop (and starts the loop if it was stopped); press again to keep it and carry on playing over it. Hold REC for three seconds to delete everything. The loop records whichever of the synth or drums the keys are on |
| PLAY | Start or stop the loop |
| SEQ / SCL | Next / previous synth preset. The three outer encoders scroll the preset list too |
| HOME | Swap the keys between the synth and the drum kit. Drums: the first eight black keys or the first eight white keys are the pads (BD SD CH OH CP RS TM CR) |
| OCT - / OCT + | Octave of the synth |
| K1 to K4 | Synth: VOLUME, REVERB, DELAY, TEMPO. Drums: LEVEL, TUNE, DECAY of the last pad hit, TEMPO |
| FX (tap) | Switch K1 to K4 to REVERB send, DELAY send, delay FEEDBACK, FX LEVEL |
| FX (hold) | Master FX tiles, laid over whatever is on screen in any mode, the games included. White keys 1-12 hold an effect, black keys 1-11 latch it: crush, downsample, drive, fold, ring, chop, stutter, reverse, lowpass, highpass, freeze, warp. K1 DEPTH, K2 RATE. Hold FX again or tap HOME and the tiles go; latched effects stay on and the mode underneath carries on |
| ENV | Vape knobs for the e-cigarette: K1 SMOKE (0 to 100; at 100 the cloud fills the whole screen), K2 DRIFT speed, K3 MAX, K4 CLEAR. Smoke is also the effects macro: it pushes the reverb and delay sends of the drums and the synth, and the delay feedback, from your own settings up to full |
| OP1 + OP2 + OP3 together | Coin pusher: a coin machine fills the screen. Each coin that goes over the edge plays a synth note chosen by where it fell (fifteen slots, low to high), always in the chosen scale, and pushes the smoke, and so the effects, somewhere new. Any key drops a coin above it. K1 SMOKE, K2 SCALE, K3 number of COINS, K4 SPEED. The same three keys, or HOME, leave |
| OP4 + OP5 together | Knives: a spinning target darts round the wall and any key throws a knife from above it. A hit plays a note, a figure or, on a bullseye, a ten-note run with drums and a glitch, all in the scale; a miss gets a rim shot. K1 SMOKE, K2 SCALE, K3 target SIZE, K4 target SPEED |
| ARP (tap) | Drum repeat: four rates, then off. A held pad retriggers in time |
| ARP (hold) | Internal tempo or external MIDI clock |
| OP6 + PIT + GLO together | Tide Blocks, a falling-block game (OCT moves, SEQ turns, REC drops). The same three keys leave |
| GLO | Bluetooth MIDI: tap shows the link; hold closes it, or swaps between waiting for a phone or computer and searching for a Bluetooth MIDI controller |
| SAVE | Store the sound settings (project slot 16; other slots are never written) |
| MASTER | Output volume |

The synth answers MIDI notes on channel 1 and the kit on channel 10.

A one-page cheat sheet of every control: <https://tomagato.github.io/fm1_open/cheatsheet.html>.

## Reading the screen

Nothing on the screen is written: no words, no numbers, no bars. The scene fills the whole display. The loop is two orbits around the red star, drums inside and synth outside, a moon for every recorded step and a comet for the playhead. Eight moons sit on the ground while the keys are drums and fill as their pads are hit. Sixteen lights along the bottom edge show the preset. Smoke shows how far the vape is up.

## Scales

The two games share one scale, chosen with K2: minor pentatonic, major pentatonic, Hijaz, Hijaz Kar, Kurd, Nahawand, Bayati, Rast and Saba, all from A. Bayati, Rast and Saba have half-flat degrees; while a game is open the synth track is retuned with the engine's per-note tuning (`groove_tuning_set`) so those degrees are a quarter tone flat, and it goes back to equal temperament when you leave.

## Master effects

The first ten master effects run in the app's own output stage (`app/audio_output.c`, `fm1_master_fx`), on the finished mix just before the master volume: stutter and reverse from a 186 ms memory, then drive, fold, crush, ring, downsample, chop, lowpass and highpass. Freeze and warp use the engine's delay: freeze sets its feedback to full, warp steps its time about. Both put the delay back as you had it when released.

## Pictures

<https://tomagato.github.io/fm1_open/background.html> sends two pictures: one behind the top half of the main screen (120 x 56) and one that replaces the target on the knife board (40 x 40, transfer commands 34 to 37). It is the back layer: the logo star, the e-cigarette, the stars, the smoke and the glitch all play over it. The page shrinks a PNG or JPEG to 120 x 56 pixels in the browser and sends it to the FM-1 over USB MIDI (transfer commands 30 to 33 in `app/transfer.c`); nothing is uploaded. The pictures are kept in the last five sectors of the sample area (`app/background.c`) and survives a restart. Remove it from the same page.

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
