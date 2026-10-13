# Shift 3DS

An original Nintendo 3DS puzzle-platformer inspired by the browser game *Shift*:
flip the world between black and white, and the blocks that are solid flip with it.
Eight levels, touch-screen shift button, custom controls.

## Controls

| Action  | Input                                        |
|---------|----------------------------------------------|
| Move    | Circle Pad or D-Pad                          |
| Jump    | A (hold to keep hopping) or Up               |
| Shift   | B, X, L, R, ZL/ZR (New 3DS) or tap the bottom screen |
| Restart | Y (or the RESTART button)                    |
| Menu    | Start                                        |

Rules: `W` blocks are solid in the white world, `K` blocks only in the black world,
grey blocks always, red spikes always kill, green door is the exit.
A shift is refused (player flashes red) if it would put you inside a block.
You can shift in mid-air, which most levels need.

## Building

Requirements: [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `3ds-dev`
group (`dkp-pacman -S 3ds-dev`), `DEVKITARM` set, and for the .cia also
`makerom` (Project_CTR) and `bannertool` somewhere in your PATH.

```
python3 tools/make_assets.py   # only if assets/ is missing (icon, banner, audio)
make        # -> shift3ds.3dsx  (Homebrew Launcher: copy to sdmc:/3ds/)
make cia    # -> shift3ds.cia   (install with FBI on a custom-firmware 3DS)
```

A .cia only runs on a 3DS-family console with custom firmware. A plain DS / DSi / DS Lite
needs a `.nds` build instead, which is a different codebase (libnds).

## Editing levels

Levels live in `tools/levels.py` as small drawing scripts. After editing:

```
python3 tools/levels.py        # regenerates source/levels.c
gcc -O2 -o solve test/solve.c source/game.c source/levels.c && ./solve
```

`solve` runs a breadth-first search over every possible input sequence using the same
integer physics as the console build, and tells you if each level is beatable.

## Status

* Game logic and all 8 levels: compiled and solver-verified on a PC.
* `main.c` (rendering/input), `Makefile` and `shift3ds.rsf`: written against the
  libctru/citro2d APIs but **not compiled with devkitARM or run on hardware** yet.
  If `make cia` complains, the `.rsf` (makerom config) is the most likely culprit;
  any known-good homebrew `.rsf` can be swapped in (keep the Title/UniqueId lines).
