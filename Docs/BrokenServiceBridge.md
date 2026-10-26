# Level 3 Section 1 — Broken Service Bridge

Open `Content/Maps/Level_03_RecyclingCore` and start in the first room.

- Press **E** at the entrance switch to power the shuttle and conveyor.
- Board the shuttle at the marked opening. It waits two seconds at each stop and takes three seconds to travel across the gap.
- Step north onto the fixed rest platform, then move east onto the conveyor.
- Jump from the end of the belt to the far fixed landing. Follow the arrows south and then east into the next section.
- Falls return the robot to the entrance with transport power preserved. The catch floor prevents a long fall.
- The switch requires a clear line of sight within 220 cm. The shuttle pauses if a rider's path is obstructed.

Design source: the existing expanded three-level design, Broken Service Bridge segment. Room 09 retains its provisional outer footprint; the traversal route fits inside it.

Implementation: `ARebootBridgeController`. The shuttle uses native character movement for riders, with attached moving collision and guards. Conveyor movement adds to walking control and stops affecting the robot while airborne.
