# Scorecard for Kindle Voyage

A standalone, touch-driven golf scorecard for a jailbroken Kindle Voyage. It ports the
round model and scoring rules from [Lucidedream/scorecard](https://github.com/Lucidedream/scorecard)
and replaces the CrossPoint display and input layers with Kindle-specific code.

The current milestone is an input and rendering demo for the touch UI toolchain.

## Build

Install [Zig](https://ziglang.org/download/) 0.16.0 or later, then run:

```sh
./build.sh
```

If Zig is not on `PATH`, point the script at it:

```sh
ZIG_BIN=/path/to/zig ./build.sh
```

The build produces `build/scorecard`, a static 32-bit ARM EABI executable for the
Voyage's pre-5.16.3 firmware. It uses the `fbink` executable supplied by the jailbreak to
display a native 1072 x 1448 grayscale screen.

For a native build, gesture/region tests, and a rendered demo image, run:

```sh
./build-host.sh
```

This produces `build-host/scorecard`, runs `build-host/m0_tests`, and writes
`build-host/demo.pgm`.

## UI toolchain

`PgmCanvas` renders proportional mixed-case UTF-8 text from bundled font8x8 Latin glyphs.
The `Small`, `Body`, and `Display` sizes have nominal cap heights of 22, 32, and 62 pixels.
Latin-1 is supported; unsupported code points render as `?`, while common smart quotes and
dashes fall back to their ASCII forms. The original font8x8 glyphs by Daniel Hepper are
public domain; provenance is recorded in `third_party/font8x8/LICENSE.txt`.

`TouchInput::waitForEvent()` blocks until a tap, horizontal swipe, or long press completes.
Screens can rebuild a `HitTester` each frame; touch targets should be at least 130 pixels
on each axis, though this convention is intentionally not enforced by the dispatcher.

## Install

The Kindle must have the current KindleModding jailbreak and shell integration. Copy:

```text
build/scorecard  -> /mnt/us/scorecard/scorecard
Scorecard.sh     -> /mnt/us/documents/Scorecard.sh
```

Eject the Kindle and open **Scorecard** from its library. The M0 demo logs taps, swipes,
long presses, and hit-region action IDs; tap **Exit** in the upper-right corner to return.

## Device layout

```text
/mnt/us/documents/Scorecard.sh
/mnt/us/scorecard/scorecard
/mnt/us/scorecard/state.bin
```

`launcher.log`, `runtime.log`, and `fbink.log` record launch, touchscreen events, and
display failures. The generated `screen.pgm` is retained to make framebuffer problems
diagnosable over USB.

## License

MIT. The reusable golf model and rules retain the license of the original CrossPoint
project. FBInk is a separate executable installed on the device and is not linked into
this program.
