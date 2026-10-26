# Level 2 Section 3 — Transfer Shaft

Save open editor work and restart Unreal to load the new native gameplay code. Open `Content/Maps/Level_02_ProcessingPlant` and enter the third room.

- Press **E** at Switch **A** to power the lift and the westbound conveyor.
- Board the lift at its lower stop. It waits two seconds at each stop and takes two seconds to travel between floors. Exit west at the upper stop.
- Stay near the centre of the platform. The lift pauses if the robot has insufficient clearance, or if the robot is beneath a descending platform. Clear the obstruction to resume.
- The powered conveyor carries a grounded robot west while preserving walking control. Jumping leaves the belt's influence.
- Reach Switch **B** on the upper landing and press **E** to open the main exit and the gate onto the permanent return ramp. B requires A to be powered first.
- Both switches require a clear line of sight and a distance of at most 220 cm. Completed controls remain active for the map session.

Implementation: `ARebootTransferController`, with native character movement on the lift, swept conveyor transport, and gates whose collision proxies move with their meshes. Automated tests use an isolated world without opening a Play session.
