# circuit-synth upstream bug reports

Bugs found in [circuit-synth](https://github.com/circuit-synth/circuit-synth)
while generating this board's schematic, and what became of them.

## Filed

### #619: pins sharing a name collapse into one connection

https://github.com/circuit-synth/circuit-synth/issues/619 (filed 2026-09-27)

`src/circuit_synth/kicad/sch_gen/circuit_loader.py` identifies a connected
pin by its **name** before its **number**. Names repeat (USB-C: two `D+`,
two `D-`; a Pico module: eight `GND`), so all same-named pins of a component
collapse into one connection and only one of them gets wired. Reproduced on
v0.12.1 (unchanged on `main` 3aaff18) with a USB-C connector: pin B6 (second
`D+`) is left unconnected, confirmed by KiCad 10 ERC. Fix: prefer the pin
number; verified locally. Probably also the root cause of upstream #35.

## Not filed

### Sheet pin placed 1.27 mm from its hierarchical label (false alarm)

`schematic_writer.py` writes each sheet pin 1.27 mm inside the sheet's right
edge while its hierarchical label sits on the edge. This looked like broken
connectivity, but KiCad snaps sheet pins onto the sheet border when it loads
the file, so the pin and label do coincide. A two-sheet reproduction on
v0.12.1 passes KiCad 10 ERC without any sheet-pin errors, and moving the
label to the pin's written position actually breaks the connection. Harmless
as far as KiCad is concerned; not reported.

### Fixed upstream / out of scope

Found on the old PyPI release 0.1.0 and fixed by v0.12.1: pin-level labels
of the wrong type, a `(ratsnest ...)` token KiCad can't open, and a
duplicated `(paper ...)` token. Default paper size and placement overflow
belong to the PCB/placement code, which v0.12.1 no longer ships in the open
version.
