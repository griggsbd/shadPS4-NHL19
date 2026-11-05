<!--
SPDX-FileCopyrightText: 2026 griggsbd
SPDX-License-Identifier: GPL-2.0-or-later
-->

# shadPS4 for NHL 19

A fork of the [shadPS4](https://github.com/shadps4-emu/shadPS4) PlayStation 4 emulator with fixes and tuning
for **NHL 19**, plus a ready-made game config that runs it in PS4 Pro mode at 4K and 90 fps.

With this build and config, NHL 19:

- renders gameplay, instead of a black screen
- draws the goal nets as nets, not solid sheets
- doesn't crash with "Out of order flip IRQ"
- has steady frame pacing, without judder or shaking jerseys
- runs at 90 fps in gameplay, menus, stoppages and replays

> [!IMPORTANT]
> You need your own copy of NHL 19 dumped from your PS4: **CUSA11126** (North America) with **update 1.60**.
> This project doesn't include or link to game files.

**[⬇ Download the latest build](https://github.com/griggsbd/shadPS4-NHL19/releases/latest)**

## Contents

- [Performance](#performance)
- [What you need](#what-you-need)
- [Setup](#setup)
- [The config, setting by setting](#the-config-setting-by-setting)
- [Lighter settings for other PCs and screens](#lighter-settings-for-other-pcs-and-screens)
- [Using Puck Peak Overtime](#using-puck-peak-overtime)
- [Troubleshooting](#troubleshooting)
- [What was changed and why](#what-was-changed-and-why)
- [Building from source](#building-from-source)
- [Credits and license](#credits-and-license)

## Performance

Tested on Windows 11 with a Ryzen 7 7700X, a Radeon RX 9070 XT, 32 GB of RAM and a 2560×1440 180 Hz monitor,
using the config in this repository.

| Scene | Frame rate |
|---|---|
| Gameplay | 90 fps most of the time, 86 fps on average over a full period. The busiest moments dip into the 70s and 80s. |
| Menus, stoppages and replays | 90 fps |
| Crowd close-ups and the pregame intro | 30 to 47 fps |

The crowd shots and the intro make about 3,800 draw calls per frame, compared with about 1,500 in gameplay. Those
scenes are limited by how fast the emulator processes each draw, which none of these changes touch.

## What you need

- **Windows 10 or 11, 64-bit.** The code also builds for Linux and macOS, but only Windows was tested.
- **A fast CPU and GPU.** At 90 fps the emulator leans hard on one CPU core, and 4K Pro mode needs a strong
  graphics card. On a slower PC, see [Lighter settings](#lighter-settings-for-other-pcs-and-screens).
- **A screen that shows 90 fps evenly:** 90 Hz, 180 Hz, or any screen with variable refresh (FreeSync or G-Sync).
  On other screens, use 60 fps.
- **Your NHL 19 dump**, CUSA11126, update 1.60. The update can be merged into the game folder or sit next to
  it as `CUSA11126-UPDATE`. The game folder is the one that contains `eboot.bin` and `sce_sys`.

No PS4 firmware modules or system fonts were needed in testing.

## Setup

### 1. Get the build

Download the [latest release](https://github.com/griggsbd/shadPS4-NHL19/releases/latest) and unzip
`shadPS4.exe` into a folder of its own, for example `C:\Games\shadPS4-NHL19\`.

This is the emulator itself, without a game list or settings window. You start the game with a command or a
desktop shortcut (step 5). If you use the shadPS4 QtLauncher, you can run this build from it instead: open its
**Version Manager**, add this `shadPS4.exe` as a local version and select it.

### 2. Put the game somewhere

For example `C:\Games\PS4\CUSA11126\`. Keep the folder named `CUSA11126` if you want to use
[Puck Peak Overtime](#using-puck-peak-overtime).

### 3. Run it once

Open PowerShell in the build's folder (in File Explorer, type `powershell` in that folder's address bar and press
Enter) and start the game, using your own game path:

```powershell
.\shadPS4.exe --game "C:\Games\PS4\CUSA11126\eboot.bin"
```

Close it once the game is running. The first run creates shadPS4's settings folder, `%APPDATA%\shadPS4`, with
`config.json` inside. To open it, paste `%APPDATA%\shadPS4` into File Explorer's address bar.

### 4. Add the NHL 19 config

Download [`nhl19/CUSA11126.json`](nhl19/CUSA11126.json) from this repository (open it and use **Download raw
file**) and put it in `%APPDATA%\shadPS4\custom_configs\`. Create the `custom_configs` folder if it isn't there.

Then set the resolution the game renders at. This setting only exists in the global `config.json`, so it applies
to every game you run from that settings folder. Open `%APPDATA%\shadPS4\config.json` in Notepad, find the
`"GPU"` section and set:

```json
"internal_screen_height": 2160,
"internal_screen_width": 3840,
```

The two lines may not sit next to each other in the file. Make sure the width gets 3840. Edit the file while
shadPS4 is closed.

`CUSA11126.json` holds the settings that make the game run well. Each one is explained
[below](#the-config-setting-by-setting).

### 5. Play

Start the game the same way as in step 3. For a desktop shortcut with the NHL 19 icon instead:

1. Download [`nhl19/make-nhl19-shortcut.ps1`](nhl19/make-nhl19-shortcut.ps1).
2. Open it in Notepad and set the two paths at the top: where `shadPS4.exe` is and where your game folder is.
3. Open PowerShell in the folder where you saved it and run:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\make-nhl19-shortcut.ps1
   ```

The first launch stutters for a while as shaders compile. Later launches load them from the cache.

Press **F10** in game to show shadPS4's frame-rate counter.

### 6. Check that the config is used

Open `%APPDATA%\shadPS4\log\shad_log.txt` after a launch. Near the top you should see:

```
Game-specific config used: true
GPU readbacksMode: 2
GPU vblankFrequency: 90
GPU frameOverlap: true
GPU ignoreFlipRate: true
GPU flipQueueDepth: 2
```

If `frameOverlap`, `ignoreFlipRate` and `flipQueueDepth` are missing, you're running a different shadPS4 build.

## The config, setting by setting

Settings marked **this build** only exist in this fork. Other shadPS4 builds ignore them.

| Setting | Value | What it does |
|---|---|---|
| `neo_mode` | `true` | PS4 Pro mode. The game uses its Pro renderer, which renders at the resolution set in `config.json` (4K above). |
| `readbacks_mode` | `2` | Precise GPU readbacks. NHL 19 reads data back from the GPU about a dozen times every frame. Without this, gameplay renders black. |
| `readback_linear_images_enabled` | `true` | Also reads back images the GPU wrote in a linear layout. On in all testing; the game wasn't tested without it. |
| `vblank_frequency` | `90` | The refresh rate of the emulated PS4 display, which sets the game's frame rate. NHL 19 keeps normal game speed at 90. |
| `flip_queue_depth` | `2` | **This build.** Lets two finished frames wait for the screen instead of one, so a late frame doesn't cost a whole refresh. Adds about one frame (11 ms) of input delay. [Details](#4-dips-from-90-and-45-fps-stoppages-flip-queue-frame-overlap-and-flip-rate). |
| `frame_overlap` | `true` | **This build.** Lets the game start its next frame while the emulator is still processing the current one. Works together with `flip_queue_depth`. |
| `ignore_flip_rate` | `true` | **This build.** NHL 19 asks for half-rate output (30 fps on a PS4, 45 here) in menus, stoppages and replays. This ignores the request so those run at 90 too. Replays still play at normal speed. |
| `submit_draw_interval` | `200` | Sends recorded work to the graphics card every 200 draws instead of all at once, so there's less to wait for when the game reads data back mid-frame. |
| `full_screen`, `full_screen_mode` | `true`, `"Fullscreen"` | Exclusive fullscreen. A matter of taste. |
| `pipeline_cache_enabled` | `true` | Saves compiled shaders to disk, so later launches don't stutter while compiling them. |

The test PC also had these global settings in `config.json`: `"present_mode": "Immediate"` and
`"fsr_enabled": true`. The default present mode wasn't tested with this setup.

> [!NOTE]
> Changing NHL 19's per-game settings in QtLauncher can rewrite `CUSA11126.json` without the three settings this
> build adds. If you change them there, check the file afterwards.

## Lighter settings for other PCs and screens

| If | Change |
|---|---|
| Your screen is 60 Hz, or 120 or 144 Hz without variable refresh | Set `"vblank_frequency": 60` in `CUSA11126.json`. 90 fps doesn't line up with those refresh rates. |
| Your CPU can't hold 90 fps | Set `"vblank_frequency": 60`. Frame rate depends mostly on single-core CPU speed. |
| Your GPU struggles at 4K | Lower the resolution in `config.json` to 2560 × 1440 or 1920 × 1080. |
| It's still too slow | Set `"neo_mode": false` to use the base PS4 renderer. |
| You notice judder or shaking jerseys | Set `"flip_queue_depth": 1`. This brings back the dips from 90, but keeps the pacing fix. |
| You'd rather have menus and replays at the game's own half rate | Set `"ignore_flip_rate": false`. |

## Using Puck Peak Overtime

[Puck Peak Overtime](https://github.com/MatusRiedl/puckpeak-nhl-19-updater) brings NHL 19 back online with current
rosters. It works with this build:

- In Overtime's **Setup**, pick this build's `shadPS4.exe` if it doesn't find it on its own.
- Leave **Boost performance** off. It switches NHL 19 to a different shadPS4 build (Kravickas's NHL 19 build).
- Overtime finds the game through the game folders listed in shadPS4's `config.json`, and only looks one level
  down. The game folder has to be named `CUSA11126` and sit directly inside a listed folder. To add a folder to the
  list:

  ```powershell
  .\shadPS4.exe --add-game-folder "C:\Games\PS4"
  ```

Overtime's game patch and your saves live in `%APPDATA%\shadPS4`, which every shadPS4 build shares, so they carry
over.

## Troubleshooting

| Problem | Fix |
|---|---|
| Gameplay is black, menus are fine | The NHL 19 config isn't being used. Check that the file is named exactly `CUSA11126.json` and is in `%APPDATA%\shadPS4\custom_configs\`, then check the log as in [step 6](#6-check-that-the-config-is-used). |
| Goal nets are solid sheets | You're on a shadPS4 build without the alpha to coverage fix. Use this build. |
| Everything renders small or blurry | The resolution in `config.json` is still at its default of 1280 × 720. See [step 4](#4-add-the-nhl-19-config). |
| Judder or shaking jerseys | Set `"flip_queue_depth": 1`. |
| The settings don't change anything | If the folder shadPS4 starts in (normally the one with `shadPS4.exe`) has a `user` folder, shadPS4 keeps its settings there instead of in `%APPDATA%\shadPS4`. Put the files there instead, or rename that folder to switch back. |
| Something else | Look at the end of `%APPDATA%\shadPS4\log\shad_log.txt` and open an issue with the log attached. |

## What was changed and why

Four sets of changes to the emulator are in the `nhl19` branch, and the rest comes from the config. Each fix
below links to its commit.

### 1. Solid goal nets: alpha to coverage

Commits [`be392d4`](https://github.com/griggsbd/shadPS4-NHL19/commit/be392d48) and
[`e7d6c27`](https://github.com/griggsbd/shadPS4-NHL19/commit/e7d6c276).

NHL 19 draws the netting with *alpha to coverage*: the GPU turns each pixel's transparency into a multisampling
coverage mask, so only the strands of the net are drawn and the gaps are left out. shadPS4 didn't implement the
PS4 register that turns this on (`DB_ALPHA_TO_MASK`), so every pixel of the net came out solid.

The fix reads that register and turns on Vulkan's alpha to coverage when the pixel shader writes an alpha value to
its first render target and that target keeps the alpha channel.

A follow-up fixed the nets turning solid again once the pipeline cache was on. Shaders loaded from the cache didn't
carry the information that decision uses, so it's now saved with them. The cache version was bumped, so old caches
are rebuilt once.

### 2. Crashes: display buffers released too late

Commit [`8a5d34a`](https://github.com/griggsbd/shadPS4-NHL19/commit/8a5d34ad).

When a new frame reached the screen, shadPS4 first told the game (flip status and flip event) and only then
released the display buffer the new frame had replaced. NHL 19 reacts to the flip event right away and claims that
buffer for its next frame, so the late release wiped out the game's claim. When that frame's flip came in, the
emulator stopped on an assertion ("Out of order flip IRQ").

The old buffer is now released before the game can see the flip. If a flip still arrives out of order, it's logged
as a warning and the frame is shown instead of stopping the emulator.

### 3. Judder and shaking jerseys: frame pacing

Commit [`9f3c408`](https://github.com/griggsbd/shadPS4-NHL19/commit/9f3c4085).

NHL 19 watches how many of its frames are still waiting to be shown, and when it sees two, it waits until both are
on screen before going on. shadPS4 let the game queue frames faster than the emulated display showed them, so the
game kept running ahead and then stalling. The uneven frame times showed up as judder, and the jersey cloth shook.

Now the game is held after each frame it submits until the frame before it is on screen, so it never builds up the
backlog that makes it stall. Each frame gets a sequence number, so a late or dropped frame can't throw the pacing
off.

### 4. Dips from 90, and 45 fps stoppages: flip queue, frame overlap and flip rate

Commit [`4ced645`](https://github.com/griggsbd/shadPS4-NHL19/commit/4ced6456). It adds three per-game settings,
all off by default, which the config turns on.

With the pacing fix and `vblank_frequency` at 90, busy moments still slid from 90 toward 60. A profiling build
(the `gpu-prof` branch) broke down where the emulator's GPU command thread spends each frame, and a later one
(`pacing-lab`) recorded every frame: when the game submitted it, what it waited on, and when it reached the screen.
That showed:

- The emulator spends about 5 µs per draw, about 7.5 ms for a typical 1,500-draw gameplay frame, plus up to 2 ms
  waiting on the graphics card when the game reads data back from it.
- In busy scenes, the game and the emulator each spent several milliseconds per frame waiting on each other, and in
  the end on the display. Only one finished frame could wait for the screen, so one late frame cost a whole refresh
  and left the emulator idle.
- NHL 19 itself asks for half-rate output (`sceVideoOutSetFlipRate`) in menus, stoppages and replays.

The three settings:

- **`flip_queue_depth`** sets how many finished frames can wait for the screen. At 2, a late frame is absorbed
  instead of stalling everything. So that NHL 19's own pacing (from fix 3) doesn't kick in early, the game is told
  about one fewer waiting frame than there really is.
- **`frame_overlap`**: by default, after each frame shadPS4 makes the game wait until the emulator has completely
  processed it before accepting any of the next frame. With this on, the emulator's end-of-frame work is queued in
  order behind that frame, and the game can start the next frame right away. The game is still kept no more than
  one frame ahead.
- **`ignore_flip_rate`** ignores the game's half-rate requests, so menus, stoppages and replays run at 90.

Two things were tried first and didn't help on their own. `submit_draw_interval` cut the emulator's waits on the
graphics card from about 2.0 to 1.3 ms per frame, and frame overlap removed the game's wait on the emulator, but in
both cases the time just moved into the pacing wait. Raising `flip_queue_depth` is what fixed it:

| Gameplay on the test PC | Before | After |
|---|---|---|
| Moments with 1,650–1,850 draws per frame | 70–78 fps | 87–88 fps |
| Moments with 1,900–2,200 draws per frame | 61–65 fps | 72–80 fps |
| Average over a period | 80–83 fps | 86 fps |
| Time at a full 90 fps | 42–57% | 67% |
| Time below 75 fps | 21–26% | 10% |

### Config-only fixes

- **Black gameplay:** `readbacks_mode` 2 (Precise). Every frame, NHL 19's CPU code reads results the GPU wrote,
  and without readbacks it never sees them.
- **4K:** `neo_mode` plus a 3840 × 2160 resolution in `config.json`. In Pro mode the game renders at the resolution
  shadPS4 reports for the display.
- **90 fps:** `vblank_frequency` 90. NHL 19 keeps normal game speed at 90 fps.

### Branches

`nhl19` is the build to play. `gpu-prof`, `frame-overlap` and `pacing-lab` are the diagnostic builds used for the
measurements above. They add profiling overhead and `pacing-lab` writes a large timeline file every session, so
don't use them to play. `nets-fix` and `flip-fix` are earlier steps that `nhl19` already includes. `pacing-diag`
logs frame timings, and `flip-pacing` is the pacing fix on top of that logging; `nhl19` has the same fix without
it.

## Building from source

Check out the `nhl19` branch and follow shadPS4's [Windows build guide](documents/building-windows.md). Every push
is also built by GitHub Actions. The Windows build is the run's artifact whose name starts with `shadps4-win64-sdl`.

## Credits and license

- [shadPS4](https://github.com/shadps4-emu/shadPS4), the emulator this fork is based on, by the shadPS4 Emulator
  Project. Its own README and documentation are in the upstream repository.
- [Puck Peak Overtime](https://github.com/MatusRiedl/puckpeak-nhl-19-updater) for online play and rosters.
- NHL 19 fixes and tuning by griggsbd, developed with Claude (Anthropic).

Like shadPS4, this fork is licensed under the GPL-2.0-or-later. NHL 19 is published by Electronic Arts. This
project isn't affiliated with EA, the NHL or Sony.
