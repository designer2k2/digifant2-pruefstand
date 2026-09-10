# circuit-synth upstream bug reports (draft)

Both confirmed present in v0.12.1 source (`git f52f491`, "Bump version to 0.12.1"),
in the free/MIT schematic-generation path. Checked against
`src/circuit_synth/kicad/sch_gen/`. No existing duplicate issue found
(searched hierarchical-label / sheet-pin / pin-number issues; closest are
#517, #539, #551, #554, #562 — all different).

Repo: https://github.com/circuit-synth/circuit-synth/issues/new

---

## Issue 1 — Sheet pin and its hierarchical label are placed 1.27 mm apart, breaking sheet connectivity

**File:** `src/circuit_synth/kicad/sch_gen/schematic_writer.py`, in the sheet-pin
creation loop (~lines 1719–1745 in v0.12.1).

The sheet pin is created at `pin_x - 1.27`:

```python
sheet_pin = SheetPin(
    ...
    position=Point(pin_x - 1.27, pin_y),
)
```

but the hierarchical `Label` meant to land on it is created at `pin_x`:

```python
label_x = pin_x            # <-- should be pin_x - 1.27
label = Label(
    ...
    position=Point(label_x, pin_y),
    label_type=LabelType.HIERARCHICAL,
)
```

KiCad only treats two items as electrically connected when their connection
points are exactly coincident (or joined by a wire). A 1.27 mm (50 mil) gap
means every sheet pin is left unconnected to its label, so no net crosses the
sheet boundary. ERC reports the nets as unconnected / not driven on both sides.

**Fix:** `label_x = pin_x - 1.27` (match the sheet-pin x), or draw a wire
segment between the two points.

**Repro:** generate any multi-sheet (non-flattened) project and open it in
KiCad 7/8/9 — inter-sheet nets show as unconnected; the label sits one grid
step off the pin.

---

## Issue 2 — Net rebuild prefers pin *name* over pin *number*, collapsing multi-pin nets (e.g. all GND pins of a module)

**File:** `src/circuit_synth/kicad/sch_gen/circuit_loader.py`, ~lines 282–304 in v0.12.1.

```python
# Enhanced pin identification - store the most specific identifier available
pin_identifier = None
if "name" in pin_data and pin_data["name"] != "~":
    pin_identifier = pin_data["name"]          # <-- name chosen first
elif "number" in pin_data:
    pin_identifier = str(pin_data["number"])
```

Pin **name** is not a unique identifier — many real symbols have several pins
with the same name (a Raspberry Pi Pico module symbol has 8 pins named `GND`;
regulators, connectors, FPGAs similarly). Pin **number** is always unique.

Because connections are appended as `(comp_ref, pin_identifier)` tuples, using
the name makes all same-named pins collapse to a single tuple. Only one of the
module's GND pins ends up connected; the rest are silently dropped and show up
in ERC as `power_pin_not_driven` / unconnected.

**Fix:** prefer `number` when present, fall back to `name`, then `pin_id`:

```python
if "number" in pin_data and pin_data["number"] not in (None, ""):
    pin_identifier = str(pin_data["number"])
elif "name" in pin_data and pin_data["name"] != "~":
    pin_identifier = pin_data["name"]
else:
    pin_identifier = str(pin_data.get("pin_id", ""))
```

**Repro:** create a `Component` for any symbol with ≥2 identically-named pins,
connect all of them to one net, generate the project — only one pin is wired in
the output `.kicad_sch`.
