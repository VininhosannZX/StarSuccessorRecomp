# Star Successor Recompiled

### v20 · Experimental Windows x64 milestone

An unofficial native PC port of **Sin and Punishment: Star Successor**, using the **Wiicompiled** static translator and a **wiikit-derived** runtime for graphics, audio, input and Wii services.

**The v20 build has been reported working on the dedicated-GPU PC below. Only the first stage has been tested. Audio is not 100% correct. Intel integrated graphics are currently unsupported.**

This is a development milestone, not a finished release or a claim that the entire game is playable. It is not affiliated with Nintendo, Treasure, WiiCompiled or the wiikit authors.

## Milestone status

| Area | Current status |
|---|---|
| Native execution | Statically translated PowerPC code compiled to Windows x64 |
| Dedicated-GPU rendering | v20 reported working on the owner's Radeon RX 6750 XT PC |
| Gameplay coverage | **First stage only**; no complete-game validation |
| Audio | **Not 100% correct**; incomplete/inaccurate playback may occur |
| Keyboard and mouse | Implemented; mappings below |
| Graphics settings | Resolution and internal scaling options, filtering, VSync and fullscreen |
| 60 FPS | Target; **stable 60 FPS has not been verified** |
| 4K | Resolution options up to 3840 × 2160; no 4K/60 performance guarantee |
| Intel integrated graphics | **Currently unsupported**; black-screen failures on the tested Intel UHD system |
| Other GPUs | Not validated by this milestone |

The project uses native code execution, not a Dolphin frontend. The graphics backend in this milestone is the wiikit-derived OpenGL renderer; it does **not** use WiiCompiled's Aurora/Dawn graphics integration.

## Reference test PC

These are the owner's reported test specifications, **not minimum requirements**:

| Component | Tested PC |
|---|---|
| CPU | AMD Ryzen 5 5600 |
| GPU | AMD Radeon RX 6750 XT |
| RAM | 16 GB DDR4 |
| Operating system | Windows 11 |
| GPU driver | Not recorded |
| Resolution / measured FPS | Not recorded |
| Test scope | First stage only |

Windows rendering on this machine is owner-reported. Local development tests also rendered the first stage on Linux/Mesa software rendering; those tests do not establish Windows GPU compatibility or performance. See [hardware notes](docs/HARDWARE.md).

## What this repository contains

This source package preserves **v20**, before the subsequent v21/v22 renderer experiments. The renderer files are identical to the archived v20 sources; their hashes and the original private executable identity are in [MILESTONE_V20.json](MILESTONE_V20.json).

| Path | Purpose |
|---|---|
| `port/` | Native runtime adaptations, disc importer, input, audio, GX renderer and RSO handling |
| `project/Wiicompiled/` | Upstream source base and translator/runtime modifications |
| `deliverables/StarSuccessorBoot9/` | Native launcher/boot source used by the milestone |
| `config/` | Address seeds, function budgets and input digests |
| `tooling/`, `scripts/` | Analysis, translation, dependency preparation and build helpers |
| `launcher/` | Windows launch and diagnostic scripts |
| `docs/` | Build instructions, troubleshooting, publication checklist and scope |
| `licenses/`, `LICENSE` | Component notices and licenses |

**No disc image, extracted executable sections, translated game-function sources, game textures/audio, build cache or game-containing EXE is included.** Generate private build inputs from your own supported USA disc image. A clean clone is a source checkpoint, not a ready-to-play download.

## Running your local v20 executable

If you already have the private v20 build, keep it outside the public repository:

1. Extract its complete package to a writable folder.
2. Start `StarSuccessor.exe` or `Jogar-OpenGL.bat`.
3. Select your own **USA R2VE01** ISO or supported Zstandard-compressed RVZ when prompted.
4. Wait for the initial import. Later launches reuse the imported data.
5. Use **Enter / right mouse** to confirm menu selections.
6. Press **Esc** for graphics settings; **F11** toggles fullscreen.

OpenGL 4.5 and a compatible graphics driver are required. The archived EXE statically links SDL3 and Zstandard; Python and .NET are build dependencies, not requirements for running that EXE.

The private EXE embeds game-derived sections and translated game code. **Do not upload it, generated C++ or its build cache to this public repository or a GitHub Release.** A future local installer/recompiler would be needed for convenient source-only distribution. See [publication checklist](docs/PUBLISHING.md).

## Controls

| Action | Default input |
|---|---|
| Aim | Mouse |
| Move | W / A / S / D |
| B — fire while held; melee on a short press | Left mouse / Backspace |
| A — confirm, lock-on; hold and release for a charged shot | Right mouse / Enter |
| C — jump | Space |
| Z — evade | Left Shift |
| + / skip a scene | Tab |
| Button 2 / help | 2 |
| Port settings | Esc |
| Fullscreen | F11 / Alt+Enter |

The port settings and diagnostic scripts currently include Portuguese labels. Bindings are stored in `keys.txt`. A short B press and holding B are different actions; holding A is not the normal continuous-fire input.

## Resolution and rendering

Open **Esc → Resolution** to choose 720p, 1080p, 1440p or 4K. Internal rendering scale changes with the selection; this is more than stretching the window. The rendering menu also exposes display filtering, VSync and GPU information.

These options do not establish a performance guarantee. Begin at 1080p and evaluate actual gameplay, not a menu FPS counter. Vulkan/Zink launchers from earlier experimental packages are not the supported baseline of this milestone.

## Saves, settings and logs

Normal standalone runs store user data under:

```text
%APPDATA%\StarSuccessor\NativePort
```

This contains imported data, saves, settings, shader cache, `keys.txt` and `star-successor.log`. Back up this folder before experimenting; do not commit it or delete it wholesale to troubleshoot graphics.

Development runs from a prepared source workspace can instead reuse `game/extracted/DATA` and `game/nand`. These paths are ignored by Git.

## Building from source

Read [BUILDING.md](docs/BUILDING.md) before starting. The preserved build path is **Linux x86_64 / WSL2 cross-compiling to Windows x64**, with Python 3.12+, .NET SDK 8, CMake, LLVM-MinGW, SDL3 and Zstandard. Native Windows/Visual Studio build automation is not provided.

The workflow regenerates analysis images, translated functions and embedded data locally from your own extracted game tree. The packaged preparation step was checked; a fresh full dependency download, 32,352-function translation and final link were **not** repeated for this publication package. Treat the new bootstrap orchestration as experimental.

## Known issues and troubleshooting

- **Intel integrated graphics are currently unsupported.** The tested Intel UHD configuration initialized OpenGL but produced black game frames. v20 improved synthetic vertex tests without resolving actual gameplay on that system.
- **Audio is not 100% correct.** Do not use this checkpoint as an audio-accuracy reference.
- **Only the first stage has been tested.** Later stages, completion, transitions, progression and long-term save behavior have not been validated.
- **60 FPS is a target, not a verified result.** Capture FPS during visible gameplay before reporting performance.
- A missing ISO/DOL, wrong game revision, stale generated path, stale CMake cache, unsupported RVZ compression, missing compiler symbol or unimplemented runtime operation can prevent a build or launch.

For specific errors and safe recovery steps, see [KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md). Include your CPU, GPU, driver, Windows version, resolution and the relevant log in a bug report. **Do not attach your disc, extracted files, generated game code or saves.**

## Development and credits

The immediate priorities are graphics compatibility, audio accuracy, consistent performance and testing beyond the first stage. Further changes should branch from this preserved milestone.

- [Wiicompiled](https://github.com/patchzyy/Wiicompiled), base commit `64453f161a1af09ea680373846c3fda8c00cabe4` — static translator and CPU runtime foundations.
- wiikit by Samuele Voltan — MIT-licensed runtime components adapted for GX, audio, input and Wii services; its original license is preserved in `port/ios/LICENSE`.
- [SDL](https://github.com/libsdl-org/SDL), [Zstandard](https://github.com/facebook/zstd), [tiny-AES-c](https://github.com/kokke/tiny-AES-c) and [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw).
- Nintendo and Treasure — original game creators; no affiliation or endorsement is implied.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This source collection retains the GPLv3 license of the Wiicompiled-derived project and the separate notices of its other components. Original game rights remain with their respective owners.
