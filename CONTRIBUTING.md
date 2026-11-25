# Contributing

Work from a branch based on the preserved v20 milestone. Describe the observable problem, hardware/driver, exact build and a short reproduction. Changes affecting GX attributes, module relocation, scheduling or audio should include a focused validation and a visible-gameplay regression where possible.

Do not contribute game images, extracted binaries/assets, generated guest-function C++, embedded game sections, build caches or saves. Keep existing component notices. Do not silently stub unimplemented guest calls, assume synthetic rendering tests prove gameplay, or mark stable 60 FPS without measurements during visible gameplay.

Intel integrated graphics remain unsupported until a corrected build is tested on affected hardware. Audio and later-stage coverage are open development areas.
