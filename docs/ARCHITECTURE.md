# Architecture at the v20 milestone

Wiicompiled translates the guest PowerPC functions into C++ and builds native Windows x64 code. The native runtime provides guest memory, scheduling/interrupt integration, IOS behavior, disc access, audio, input and a wiikit-derived GX command processor/OpenGL renderer.

This port does not inherit WiiCompiled's Aurora/Dawn rendering implementation. Its graphics changes must be assessed against this renderer's own buffer layouts, shader generation, framebuffer copies and driver behavior.

The guest uses dynamically loaded RSO stage modules. Offline preparation gives their analysis sections fixed addresses; the runtime separately maps relocated module identities/call targets. Required callback coverage is checked, but is not equivalent to playing every stage.

The original v20 build identity records 32,352 translated functions and checks 13,731 required RSO references. Generated translated C++ is private output and is not part of this public source package. The v20 renderer retains separate attribute bindings after Intel debugging; it still did not produce gameplay images on the tested Intel UHD machine.

The owner later reported v20 working on the dedicated Radeon RX 6750 XT PC. v21/v22 diagnostics and conversion experiments are intentionally outside this milestone. The source preparation helpers added for publication do not change the v20 renderer source bytes; see the recorded renderer hashes.
