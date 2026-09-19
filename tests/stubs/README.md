# Test stubs

Just enough of OpenRGB's `RGBController.h`, `DetectionManager.h`, `LogManager.h` and hidapi's
`hidapi.h` for the driver in `src/DareuEK75Controller/` to compile and run without OpenRGB, Qt
or hardware. Names, constant values and signatures follow OpenRGB.

They can drift from the real API. The `build` CI job compiles the driver against the real
OpenRGB tree, which is what catches that.
