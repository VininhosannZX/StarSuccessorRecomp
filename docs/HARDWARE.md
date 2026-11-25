# Hardware and validation scope

## Owner-reported working configuration

| Component | Value |
|---|---|
| CPU | AMD Ryzen 5 5600 |
| GPU | AMD Radeon RX 6750 XT |
| Memory | 16 GB DDR4 |
| OS | Windows 11 |
| GPU driver | Not supplied |
| Resolution / measured FPS | Not supplied |
| Build | v20 milestone |
| Gameplay scope | First stage only |

This is a compatibility report, not a minimum-specification list, a benchmark, or a guarantee for all AMD GPUs. No claim is made that the stage was completed or that the entire game works. Audio is not 100% correct.

## Unsupported integrated Intel configuration

Intel integrated graphics are currently unsupported by this milestone. The observed failing PC used an Intel Core i5-13420H, Intel UHD Graphics, driver 32.0.101.7088 and Windows build 26200. The game ran but its captured frames were black. Simple OpenGL tests passed; more complete GX input handling diverged. v20 made synthetic geometry tests pass without restoring visible gameplay on that machine.

This does not establish that every Intel GPU is incapable of running a future corrected port. It describes the current support boundary. There is no confirmed Intel workaround in this milestone.

## Development environment

Earlier Linux x86_64 tests used Mesa llvmpipe software rendering without a physical GPU. They verified first-stage images and runtime behavior, not dedicated-GPU performance, Windows driver behavior or stable 60 FPS.

Future reports should record the exact EXE hash, driver, output resolution, internal scale, VSync state, gameplay interval and measured FPS. Avoid interpreting menu FPS or a black-screen counter as successful gameplay performance.
