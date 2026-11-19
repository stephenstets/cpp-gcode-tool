# cpp-gcode-tool

A G-code analyzer for 3D printing, written in C++. Point it at a `.gcode`
file and it reports what the print will actually cost you in time and
filament — before you start the printer.

## What it reports

- **Filament used**, in millimetres and grams (1.75 mm PLA defaults)
- **Layer count** and average layer height
- **Estimated print time**, from move distances and feedrates
- **Print vs travel distance**, total moves, and the bounding box

## Build and run

```bash
make            # builds ./gcode-tool (g++, C++17)
./gcode-tool sample.gcode
```

Or without make:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -o gcode-tool gcode-tool.cpp
./gcode-tool my-print.gcode
```

## What the parser handles

- Comments (`;` to end of line) stripped before parsing
- Absolute (`G90`) and relative (`G91`) positioning
- Absolute (`M82`) and relative (`M83`) extrusion
- `G92 E0` extruder resets (common after retractions)
- Homing (`G28`) for all or individual axes
- Retractions: E decreasing is *not* counted as filament used — only forward
  extrusion adds to the total

## Sample output

```
File:            sample.gcode
Filament used:   11.00 mm  (0.03 g PLA)
Layers:          3  (avg layer height 0.20 mm)
Print height:    0.60 mm
Est. print time: 0h 0m 14s
Print distance:  276.83 mm (16 extrusion moves)
Travel distance: 19.21 mm
Total moves:     21
Bounding box:    X 10.00 to 30.00  Y 10.00 to 30.00 mm
```

## Honest limits

The time estimate divides each move's distance by its feedrate and ignores
acceleration, jerk, and heating/cooling pauses — real prints run longer than
the estimate, usually by 10–30%. The filament figure is exact for what the
file extrudes; it doesn't include purge lines or prime towers the slicer adds
outside the model.

## License

MIT — see `LICENSE`.
