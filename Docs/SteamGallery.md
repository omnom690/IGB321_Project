# Level 2 Section 2 — Steam Gallery

Save any open editor work and restart Unreal to load the new native gameplay code. Then open `Content/Maps/Level_02_ProcessingPlant` and enter the second room.

- Step onto the pressure plate to open the gate. It stays open while occupied and for six seconds after release.
- Fully crossing the gate latches it open for the rest of the map session. The doorway prevents it closing onto the robot.
- The two steam crossings alternate: two seconds active, two seconds clear. Red lights and visible steam mark active hazards; green lights mark clear vents.
- Steam contact returns the robot to the safe landing beyond the gate. Reaching the marked middle waiting pad updates that reset point. Movement stops and held physics objects are released.
- Reach the isolation valve and press **E** within 220 cm with a clear line of sight. The wheel turns and both vents remain isolated for the rest of the map session.

This adds gameplay only to Section 2. Other sections retain their current functionality and asset placement.

Implementation: `ARebootSteamGalleryController`, with independently tested pressure-gate and steam-cycle state. Automated tests run without opening a Play session.
