# Level 3 Section 2 — Electrical Bypass

Map: `/Game/Maps/Level_03_RecyclingCore`, Room 10.

Enter from the west after Broken Service Bridge. Follow the marked service lane to the grounded waiting pad. The crossing has one second of amber warning, one second live, and two seconds safe. Cross when the rails go dark. Live electrical contact returns the robot to the grounded entry pad.

The pressure plate is on fixed ground after the crossing. Stand on it to raise the timed gate. Its six-second countdown starts when you leave the plate; standing on it keeps the full countdown available. Cross to the far side to latch the gate open. The doorway guard prevents an armed gate from closing on the robot.

Approach the isolation switch beyond the gate and press **E**. Isolation permanently disables this crossing and raises the north shaft entrance gate for the current map session. Follow the arrows east out of the lane, north, west, then north through the shaft doorway. Restarting Play resets the section.

## Layout

- Grounded pad: `(1950, 0)`, 240 × 240 cm, flush with the floor.
- Electrical crossing: eight adjacent strips covering `X 2150–2630`, `Y −150–150`, floor level. The hazard footprint matches these strips.
- Pressure plate: `(2850, 0)`.
- Timed gate: `(3200, 0)`, 240 cm clear opening within its frame.
- Isolation switch: `(3750, 0)`, facing west.
- Shaft entrance gate: `(3000, 1115)`, leading to the existing north corridor.

Level 3 Section 1 and its starting position are preserved. Other maps are outside this section's builder scope.

## Implementation and checks

`Source/IGB321_Project/RebootElectricalController.h` and `.cpp` own the electrical phases, plate, gate movement, far-side latch, interaction, reset, and warning effects. `RebootElectricalState.h` holds the timing state; `RebootElectricalTests.cpp` provides the `REBOOT.ElectricalBypass` automation tests.

The builder uses the existing Level 3 industrial meshes and textured material library. Gate collision follows the moving panel through attached proxies. The arc effect uses one instanced mesh component and a dedicated local material; it does not modify shared materials.

Validation passed: all 30 REBOOT automation tests, including seven Electrical Bypass tests, completed with no failures or warnings. The actual Stack O Bot walked the saved main map and passed the full phase observation, live-contact reset, safe crossing, seven-second plate hold, gate timeout, doorway protection, far-side latch, isolation, persistent shutdown, and north exit checks. Empty-handed recovery no longer calls the robot's grab-effect cleanup.

The saved map was reopened independently: all 296 protected actors, Section 1 bindings, PlayerStart, and attached moving gate collision remained intact. Live testing did not change the saved map. A fresh editor running module `2608` is left at the Section 2 entry pad with a new puzzle session. The project PlayerStart remains at Section 1.
