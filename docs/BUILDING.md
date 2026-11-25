# Building the v20 source checkpoint

## Scope

The archived private v20 executable was built in a Linux x86_64 workspace using LLVM-MinGW, and was later reported working by the owner on the reference AMD/Windows PC. This repository provides that source milestone plus new preparation/build orchestration.

**The orchestration was checked through private input preparation; a clean end-to-end rebuild was not repeated for this publication package.** It is a developer workflow, not a one-click installer. Native Windows/Visual Studio support is not supplied. On Windows, use an x86_64 WSL2 Linux environment for this build path and run the resulting EXE on Windows.

## Dependencies

Install Python **3.12 or newer**, .NET SDK **8**, CMake, a make tool and Git. Allow at least roughly 20 GB of working disk space for downloaded toolchains, extracted private files and generated output. RAM/time requirements vary; start with two workers.

The dependency helper pins LLVM-MinGW **20261117** (Ubuntu 22.04 x86_64 host, UCRT Windows target) and SDL **3.2.10** source. Download hashes match the archives preserved from the development workspace and are checked before extraction. Zstandard **1.5.7** library sources and tiny-AES-c **1.0.0** source accompany this package with notices. OpenGL headers are included.

From the repository root:

```bash
python3 scripts/prepare_dependencies.py --jobs 2
python3 scripts/prepare_dependencies.py --check
```

The helper downloads from the official LLVM-MinGW and SDL GitHub releases and compiles Windows static SDL3 and Zstandard. A normal .NET restore also needs access to NuGet for the upstream YamlDotNet/Tomlyn dependencies. Toolchain archives, compiled libraries and NuGet/build caches are local outputs, not GitHub source content.

## Prepare your game input

Use your own clean **USA R2VE01** disc. Extract its DATA partition with your preferred compatible disc tool. Supply the directory that directly contains `sys/main.dol`, `files/gs2.sel`, and the stage `.rso` files. The public `config/game-inputs.json` stores identity digests only; input bytes are not included.

```bash
python3 scripts/rebuild.py --game-tree /absolute/path/to/DATA --prepare-only
```

This validates the recorded game inputs, creates a local `game/extracted/DATA` symlink, constructs the relocated analysis DOL, generates the local project manifest and RSO relocation metadata, and copies function budgets/coverage metadata. An existing different game tree is not overwritten. Choose a native Linux filesystem location under WSL2 for the source/build tree.

The helper does not extract an ISO itself. The runtime's importer is present in source, but the initial build explicitly starts from an extracted DATA tree.

## Translate and build

```bash
python3 scripts/rebuild.py --game-tree /absolute/path/to/DATA --threads 2
```

The same tree may be reused after preparation. The helper:

1. Validates inputs and prepares the fixed-address RSO analysis image.
2. Builds the patched Wiicompiled CLI using .NET SDK 8.
3. Runs recursive translation with saved function seeds, scoped budgets and runtime relocation sites.
4. Checks required module callback coverage and compiler quality.
5. Emits the generated build shards.
6. Generates embedded initialization data from the **original main.dol**, rather than the combined analysis overlay.
7. Derives the small DSP boot header locally from the generated data.
8. Cross-compiles the O1 guest/O2 runtime Windows executable.

The native v20 compiler helper retains its historical filename `tooling/build_v9.py`; that name is not the milestone version. The intended output is `dist/v20-local/StarSuccessor.exe`, with launch/diagnostic scripts and component notices. Neither byte-for-byte reproducibility nor a clean full rebuild of this newly assembled source tree is claimed.

The original archived private v20 EXE SHA-256 is recorded in `MILESTONE_V20.json`. A local rebuild can differ due to dependency versions, paths, timestamps and compiler output. Preserve the original milestone separately rather than labeling an untested rebuild as validated.

## Public versus private files

Keep `game/`, `combined-build/`, `dist/`, generated `dsp_boot_program.h`, translated functions, embedded section blobs, binaries and object caches private. The original milestone EXE embeds game-derived data/code, so it is deliberately not a public release asset. This repository supplies neither an ISO nor a self-contained game binary.

Before publishing, run:

```bash
python3 scripts/check_public_tree.py
```

See [PUBLISHING.md](PUBLISHING.md) and [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for publishing and common build errors.
