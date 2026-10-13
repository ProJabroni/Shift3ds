# Shift 3DS

A Nintendo 3DS port of the black-and-white puzzle platformer *Shift*, with stereoscopic 3D,
a chiptune soundtrack and all 37 original room layouts.

* Rules, physics and levels follow the Atari 8-bit version of Shift by **Haplo**
  (https://h4plo.itch.io/shift), which is based on the original game by **Antony Lavelle** /
  Armor Games. The game logic in `source/game.c` was written to reproduce that version's
  behaviour frame for frame (checked against an emulation of the original program),
  and `source/atari_data.c` contains its level layouts and tile graphics.
  This is a fan project for personal use - please don't redistribute the built game.
* Graphics are redrawn for the 3DS top screen (400x240 = 40x24 tiles of 10 px) with layered
  depth for the 3D slider. Music and sound effects are synthesised on the console at startup
  (needs the DSP firmware dump, `sdmc:/3ds/dspfirm.cdc`, otherwise the game is silent).

## How it plays

The room is black and white. You walk on the black; white is air. **Shift** turns the whole
room upside-down and swaps the colours, so you end up standing on what used to be sky.
You can only shift while standing on the ground (the original rule), and not for a moment
after landing. Reach the door. Spikes kill, the pickaxe opens the dotted force fields, and
buttons swing the hinged bars. Levels 1-2 are tutorials (no shifting yet).

## Controls

| Action  | Input                                        |
|---------|----------------------------------------------|
| Move    | Circle Pad or D-Pad                          |
| Jump    | A or Up (hold to keep hopping)               |
| Shift   | B, X, L, R, ZL/ZR (New 3DS) or tap the bottom screen |
| Restart | Y (or the RESTART button)                    |
| Menu    | Start                                        |

## Building

Requirements: [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `3ds-dev`
group, `DEVKITARM` set, and for the .cia also `makerom` and `bannertool` in PATH.
The GitHub Actions workflow in `.github/workflows/build.yml` does all of this in the cloud and
uploads `shift3ds.cia` and `shift3ds.3dsx` as a build artifact.

```
python3 tools/make_assets.py   # icon, banner, audio placeholder
make        # -> shift3ds.3dsx  (Homebrew Launcher)
make cia    # -> shift3ds.cia    (install with FBI on a custom-firmware 3DS)
```

## Host-side tools

```
gcc -O2 -o solve test/solve.c source/game.c source/atari_data.c && ./solve   # every level beatable?
python3 tools/atari/export.py shift.xex source/atari_data.c                   # regenerate level data
```
