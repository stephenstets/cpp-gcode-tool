; Sample G-code: a small 20 x 20 mm, 3-layer test square.
; Hand-written for testing gcode-tool — not slicer output, but valid G-code.
; Layer height 0.2 mm, 1.75 mm filament, absolute extrusion (M82).
M104 S200 ; set hotend temp
M140 S60  ; set bed temp
G28       ; home all axes
G90       ; absolute positioning
M82       ; absolute extrusion
G1 Z0.2 F300
G1 X10 Y10 F1500
; layer 1: perimeter of a 20x20 square, then infill lines
G1 X30 Y10 E0.8 F1200
G1 X30 Y30 E1.6
G1 X10 Y30 E2.4
G1 X10 Y10 E3.2
G1 X12 Y12 E3.4 F1800
G1 X28 Y12 E3.9
G1 X28 Y14 E4.1
G1 X12 Y14 E4.6
; retract and move up
G1 E4.3 F1800 ; retract 0.3
G1 Z0.4 F300
G92 E0        ; reset extruder
; layer 2
G1 X10 Y10 F1500
G1 X30 Y10 E0.8 F1200
G1 X30 Y30 E1.6
G1 X10 Y30 E2.4
G1 X10 Y10 E3.2
G1 E2.9 F1800 ; retract 0.3
G1 Z0.6 F300
G92 E0
; layer 3
G1 X10 Y10 F1500
G1 X30 Y10 E0.8 F1200
G1 X30 Y30 E1.6
G1 X10 Y30 E2.4
G1 X10 Y10 E3.2
M104 S0 ; hotend off
M140 S0 ; bed off
G28 X Y ; home X and Y
M84     ; motors off
