# Level 2 Section 1 — Crusher Approach

Save open editor work and restart Unreal to load the new native gameplay code. Open `Content/Maps/Level_02_ProcessingPlant`; this section is the first room.

- Watch each crusher: two seconds open, one second warning, one second closed. The second crusher has a two-second offset.
- Green means the head is fully raised. Amber/red flashes warn of closing; red means wait. Keep clear of moving heads.
- A descending or lowered head returns the robot to the entrance. Reaching the marked middle waiting pocket updates that reset point. Held objects are released and movement stops.
- Reach the far control and press **E**. It permanently raises both crushers, disables their hazard, and opens the exit for the rest of the map session.
- The control requires a clear line of sight and a distance of at most 220 cm.

Implementation: `ARebootCrusherController`. Both crusher collision proxies and the exit collision proxy move with their visual meshes. Automated timing and runtime collision tests run without opening a Play session.
