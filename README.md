# Scorecard for Kindle Voyage

A standalone, touch-driven golf scorecard for a jailbroken Kindle Voyage. It ports the
round model and scoring rules from [Lucidedream/scorecard](https://github.com/Lucidedream/scorecard)
and replaces the CrossPoint display and input layers with Kindle-specific code.

The current version supports one player, 18 holes, putts, strokes from inside 100 yards,
strokes from outside 100 yards, previous/next hole navigation, and automatic save/resume.

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

## Install

The Kindle must have the current KindleModding jailbreak and shell integration. Copy:

```text
build/scorecard  -> /mnt/us/scorecard/scorecard
Scorecard.sh     -> /mnt/us/documents/Scorecard.sh
```

Eject the Kindle, open **Scorecard** from its library, and use the large `+` and `-`
buttons. Tap **EXIT** in the upper-right corner to return to the Kindle interface.

## Device layout

```text
/mnt/us/documents/Scorecard.sh
/mnt/us/scorecard/scorecard
/mnt/us/scorecard/state.bin
```

`state.bin` is created after the first interaction. `launcher.log`, `runtime.log`, and
`fbink.log` record launch, touchscreen, and display failures. The generated `screen.pgm`
is retained to make framebuffer problems diagnosable over USB.

## License

MIT. The reusable golf model and rules retain the license of the original CrossPoint
project. FBInk is a separate executable installed on the device and is not linked into
this program.
