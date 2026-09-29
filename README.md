# SA Android Unlimited Gym Training

> [!IMPORTANT]
> No more daily gym limits in **GTA: San Andreas Android 2.10**: train at any
> machine for as long as you want, every day.

Removes the daily gym limit in GTA: San Andreas for Android 2.10. There is no
forced step-off at 200 reps and no "come back tomorrow" block.

[![Version: 1.2](https://img.shields.io/badge/version-1.2-green.svg)](https://github.com/Jean7z/gta-sa-unlimited-gym/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)
[![Game: GTA SA 2.10 Android](https://img.shields.io/badge/game-GTA%20SA%202.10%20Android-blueviolet.svg)]()
[![Platform: Android](https://img.shields.io/badge/platform-Android-lightgrey.svg)]()
[![Loader: AML](https://img.shields.io/badge/loader-Android%20Mod%20Loader-orange.svg)](https://github.com/AndroidModLoader/AndroidModLoader)

An [Android Mod Loader (AML)](https://github.com/AndroidModLoader/AndroidModLoader) plugin.
Requires the official SA Android plugin SDK ([aml-psdk](https://github.com/AndroidModLoader/aml-psdk), included as a submodule).

---

## Table of contents

- [Features](#features)
- [How it works](#how-it-works)
- [Requirements](#requirements)
- [Installation](#installation)
- [Configuration](#configuration)
- [Building from source](#building-from-source)
- [Project layout](#project-layout)
- [Compatibility](#compatibility)
- [Credits](#credits)
- [License](#license)

---

## Features

- No daily limit. Keep training on any gym machine for as long as you want;
  you are never thrown off the bench or bike at 200 reps, and re-entering the
  gym is always allowed.
- Coexists with other AML mods. Nothing in the game is modified: a background
  thread pins four script variables every 50 ms.
- Uninstall is deleting the `.so`.

## How it works

The gym scripts (GYMBENC, GYMBIKE, GYMDUMB, GYMTREA) allow training only when

```text
gym_day > gym_final_day  OR  gym_month > gym_final_month
```

and during a session they raise `gym_day_fitness` until it hits `200.0`, then
they record today's date into `gym_final_day`/`gym_final_month`, print the
limit message, and force the player off the machine. Those variables are saved,
so the block survives a restart.

In the Android engine (2.10) the script variables live in one global buffer —
`CTheScripts::ScriptSpace`, an **array** (the symbol address is the buffer
base). The variables sit at fixed byte offsets:

| Offset   | Variable          | Pin value          |
|----------|-------------------|--------------------|
| `+0x6924` | `gym_day`         | (read only)        |
| `+0x6928` | `gym_month`       | (read only)        |
| `+0x692C` | `gym_final_day`   | `-1`               |
| `+0x6930` | `gym_final_month` | `-1`               |
| `+0x6934` | `gym_day_fitness` | `0.0`              |

Pinning `gym_final_*` to `-1` keeps the entry gate open; pinning
`gym_day_fitness` to `0.0` makes the 200-rep cap unreachable.

## Requirements

- **GTA: San Andreas** for Android **2.10** (play store version).
- **[Android Mod Loader (AML)](https://github.com/AndroidModLoader/AndroidModLoader)**
  installed and working (the game must load `libAML.so`).
- An **arm64-v8a** device/module — GTA:SA 2.10 ships arm64-only.

## Installation

1. Grab the latest **`.so`** from the [Releases](https://github.com/Jean7z/gta-sa-unlimited-gym/releases)
   page:
   - `libAML_PSDK_Gym64.so` → **arm64-v8a**
2. Push it into the game's mods folder:
   `/Android/data/com.rockstargames.gtasa/mods/`
3. Start the game. The mod is active automatically.

### Uninstall

Delete the `.so` from the mods folder. Nothing else is changed.

## Configuration

All keys live under the `[Unlimited Gym]` section of the AML config file. Set
the toggle to `0`/`false` to disable it without deleting the mod:

| Key | Default | Description |
|-----|---------|-------------|
| `MasterEnabled` | `true` | Master switch for the whole mod |

## Building from source

### Prerequisites

- [Android NDK](https://developer.android.com/ndk/downloads) (r21 or newer; r29 recommended)
- `git`

### Steps

```bash
# 1. Clone the repository including the aml-psdk submodule
git clone --recurse-submodules https://github.com/Jean7z/gta-sa-unlimited-gym.git
cd gta-sa-unlimited-gym

# 2. Build with the NDK's ndk-build
$ANDROID_NDK_HOME/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk NDK_APPLICATION_MK=./Application.mk
```

The resulting libraries land in `libs/`:

```text
libs/arm64-v8a/libAML_PSDK_Gym64.so
```

> Environment tip: make sure `ANDROID_NDK_HOME` points at your NDK directory
> (the one containing `ndk-build`), or call the full path to `ndk-build`
> directly.

### Reusing an existing aml-psdk checkout (no submodule needed)

If you already have the SDK checked out somewhere, you can use it instead of
the submodule:

```bash
mv psdk psdk.bak && ln -s /path/to/aml-psdk psdk
```

## Project layout

```text
.
├── Android.mk          # ndk-build makefile (selects module name per ABI)
├── Application.mk      # ABI targets and toolchain settings
├── main.cpp            # the entire mod
├── mod/                # AML mod interface helpers (logger/config), vendored
└── psdk/               # aml-psdk submodule (official SA Android SDK headers)
```

The `mod/` helpers and the `psdk/` SDK headers are the same pieces the official
AML mods use (`RusJJ/AndroidModLoader` + `AndroidModLoader/aml-psdk`, both MIT).

## Compatibility

- Game: GTA San Andreas **2.10** for Android.
- Verified on: Android 15 (SDK 35), AML 1.4, alongside the
  `net.psdk.samod.ahead` mod.
- The muscle **stats** still increase during training; the **visual** muscle
  re-blend happens on gym exit — this is vanilla behaviour.

## Credits

- [RusJJ](https://github.com/AndroidModLoader) — Android Mod Loader, the mod
  interface helpers (`mod/`) and the SA Android SDK (`aml-psdk`), all MIT.
- [GTA: San Andreas Reverse Engineering](https://github.com/gta-reversed/gta-reversed-android)
  — reference documentation of the game's engine and scripts.

## License

MIT — see [LICENSE](./LICENSE). This project is not affiliated with Rockstar
Games or Take-Two Interactive. GTA: San Andreas and its trademarks belong to
their respective owners. Use at your own risk.