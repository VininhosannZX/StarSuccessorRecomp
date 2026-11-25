# Known issues and troubleshooting

## Support boundaries

- Intel integrated graphics: unsupported; black-screen failures remain in v20.
- Audio: not 100% correct; playback accuracy is unfinished.
- Gameplay: first stage tested only; complete-game compatibility is unknown.
- Performance: stable 60 FPS and 4K/60 have not been established.
- Other disc regions/revisions and arbitrary RVZ compression formats are not supported by this build recipe.

## Black window

Confirm that a complete v20 package is being used and the driver offers OpenGL 4.5. Prefer the default OpenGL launcher. Preserve the log and report CPU, GPU and driver. Intel UHD failures are already known; switching to the old Vulkan/Zink launcher is not a confirmed fix. Do not delete the full user-data directory, which contains saves.

## No window, process remains running

Inspect `star-successor.log` for the last SDL/window/context startup checkpoint. Earlier Vulkan/Zink experiments could stop before context initialization. End only the stuck instance before retrying OpenGL; do not run several instances against the same save directory.

## Missing DOL / `%RECOMP_GENERIC_DOL%` path

A manifest or old command points at a missing local input. Use your own supported extracted `DATA/sys/main.dol`, regenerate preparation files in the current workspace, and verify the path. Do not copy an old generated manifest with another machine's absolute path.

## `.incbin` cannot find `_text.bin`, `_data.bin`, etc.

Generated assembler still references an old location, or the data blobs were not generated. Regenerate data initialization in the current root and keep the `.S` file and its blob directory together. Renaming the project folder without regeneration can trigger this.

## CMake cache points at an old directory

Dependency build directories preserve compiler/source paths. Use a fresh dependency build directory after moving the repository or changing toolchains. Keep game data and saves intact.

## `Address 0x00000060 is outside the loaded RAM image`

This was observed during translation work. It indicates an invalid discovered target or inconsistent translation inputs, not a request to increase the game's RAM allocation. Check the isolated Star Successor manifest, supported game digests and function seeds; do not reuse Mario Kart's project manifest or unrelated output metadata.

## Missing translated `func_...` symbol / incomplete module coverage

Regenerate translation and shards together. Keep the scoped function-budget configuration for the seven large functions and the RSO callback seeds. `check_module_coverage.py` must pass. Do not silently stub an unknown guest function to claim gameplay compatibility.

## MMIO exception or unimplemented runtime call

The generic translator does not supply every Wii hardware behavior. Earlier boot tests stopped at non-GPU MMIO writes. If this happens in a current build, preserve the address and backtrace; confirm that the correct native runtime and launcher were linked. Later gameplay may expose additional operations.

## Image import rejected

Use a clean USA R2VE01 source. The native importer supports ISO and the Zstandard RVZ layout exercised during development; it is not a universal disc-format importer. For source builds, an extracted DATA tree is the explicit input. Verify its hashes with `scripts/rebuild.py --prepare-only`.

## Audio gaps or inaccurate sound

Audio is unfinished. Report a reproducible stage location, whether sound is missing/distorted, and whether gameplay stalls at the same time. Do not claim that audio is fully fixed because one scene sounds correct.

## Reporting

Provide a short reproduction, expected/actual behavior, exact build identity, hardware/driver, resolution and the relevant log excerpt. Review logs before posting personal paths. Do not upload game files, extracted content, generated game-function sources, object files, executables containing game code or save data.
