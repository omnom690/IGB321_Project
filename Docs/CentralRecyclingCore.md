# Level 3 Section 4 — Central Recycling Core

Map: `/Game/Maps/Level_03_RecyclingCore`, Room 12.

The central hub connects three independent relay tests. Complete **A, B, and C in any order**. Approach a relay and press **E** to activate it. Completed relay states survive a fall or electrical reset for the current map session.

- **A — Transport:** use the automatic shuttle and fixed landings to reach the transport relay. Activating A restores a safe bridge for the return trip.
- **B — Isolation:** observe the electrical crossing from its grounded waiting pad. The cycle is one second of warning, one second live, and two seconds safe. Cross when the strip is dark. Live contact resets the robot to the hub. Activating B permanently isolates the local strip.
- **C — Access:** stand on the pressure plate to open the timed gate. Its six-second countdown starts after leaving the plate. Cross the gate and make the marked short jump to the access relay. Activating C restores a safe return bridge.

When all three indicators are active, approach the reboot console from its marked safe pad and **hold E continuously for one second**. Releasing E or leaving interaction range before the hold completes cancels that attempt.

The fifteen-second ending keeps the robot on the safe pad while the local hazards shut down, normal lighting returns, and guarded background machinery restarts. The three system indicators become active and the ending reaches **Facility Reboot Complete**, followed by a stable view of the core. Restarting Play begins a new session.

## Implementation and verified results

`Source/IGB321_Project/RebootCoreController.h` and `.cpp` handle interactions, moving transport, restored routes, recovery, and the ending. `RebootCoreState.h` keeps the relay flags and timing independent; `RebootCoreTests.cpp` provides the `REBOOT.CentralCore` automation tests. The controller enforces range and clear line of sight, and its recovery logic stops once the ending starts. The console interaction target is in front of the physical cabinet rather than inside its screen collider.

Verified on 27 October 2026 in Unreal Engine 5.8.2:

- All **50 REBOOT automation tests passed**, including ten Central Core tests. These cover all six relay orders, shuttle collision and carrying, electrical timing and safe waiting, gate countdown and doorway protection, the 240 cm jump, recovery with progress preserved, interrupted console holds, and the full fifteen-second ending.
- The saved Level 3 map was independently reopened and checked. All **956 protected actors** from the earlier sections were preserved, with one original Player Start. The core layout passed **844 clearance samples**.
- A complete saved-map walkthrough used the actual robot in **C → B → A** order. The robot crossed the timed gate, jumped to C, used both restored bridges, recovered from falls and live electrical contact without losing progress, isolated B permanently, and rode the automatic shuttle to A.
- The production console interaction binding was checked with an interrupted half-second hold followed by a continuous one-second hold. The ending completed after fifteen seconds, with the robot safely locked on the pad, the ending camera active, machinery running behind guards, and **Facility Reboot Complete** displayed.
- After the completed walkthrough, Play was restarted at the hub with **0/3 relays** active, leaving a fresh session ready to try. This preview does not change the saved Player Start or the project's default map.

Verification evidence is stored in the task's `Level3_Section4` folder: `Automation_Verified/index.json`, `core_saved_map_check.json`, `core_build_report.json`, `core_live_report.json`, `core_complete.png`, and `core_hub.png`. The live interaction check calls the same controller binding used by gameplay; physical keyboard input was not simulated by the automation.
