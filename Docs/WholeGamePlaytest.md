# REBOOT — whole-game playtest

27 October 2026 · Unreal Engine 5.8.2 · IGB321_Project

**The game cannot currently be completed as one connected playthrough. Level 2 Section 4 is the main blocker.** Level 1 and its automatic transition into Level 2 worked. Level 2 Sections 1–3 worked, but the Processing Control room has no connected gameplay or working transition to Level 3. Level 3 was loaded explicitly to continue testing its four sections and ending.

## Findings, in repair order

### 1. Level 2 Section 4: controls and completion are not implemented — blocker

The actual robot reached Switch A and pressed E. After ten seconds, the lift remained at its original Z=-35 cm: **zero movement**. The saved room has no gameplay controller. Its authored set consists of static meshes, lights, target points and text.

The robot could walk up the permanent return ramp to the upper landing, then reach Switch B and the System 2 terminal. E at B and the terminal caused no completion response. The game remained in Level 2.

This is consistent with the section's build report: it describes asset placement and explicitly lists the switches, conveyor, lift, terminal circuit response and System 2 completion as unconnected. A green lamp or screen in this room is currently a static visual, not evidence of completed gameplay.

Reproduction: play through Level 2 Sections 1–3, enter Processing Control, approach A at approximately (950,1880), press E and observe the lift at (800,2500). The alternate ramp reaches B at approximately (50,2620) and the terminal at (50,2670); neither completes the system.

Evidence: `level2_full_route_report.json`, `level2_control_room_check_report.json`, `inventory_Level_02_ProcessingPlant.json`, and the control-room screenshots below.

### 2. Level 2 has no working transition to Level 3 — blocker

The control-room terminal does not initiate travel, and no transition controller was present in the saved map's actor inventory. The original room layout also ends here. The Level 1 → Level 2 transition did work, so map travel itself is functioning elsewhere.

**The Level 3 test used an explicit QA map load. This was a bypass of the blocker, not a successful Level 2 → Level 3 transition.** The reason and source map are recorded in `level3_manual_load_reason.json`.

### 3. The ending assumes Systems 1 and 2 were completed — progression defect

After loading Level 3 from the unfinished Level 2 room, the core already reported **two active systems**. Its code initializes `ActiveSystemCount` to 2 and replaces that with 3 solely when the local A/B/C relays are complete. Reboot eligibility checks the local relays and console interaction, without checking earlier systems.

The project uses the base GameInstance; there is no native earned-progress store connecting the levels. The ending therefore reports all three systems online even though System 2 was never completed.

Source: `Source/IGB321_Project/RebootCoreController.h:82`, `RebootCoreController.cpp:165`, the `CanReboot` function, and its completion text. `global_progress_audit.md` records the source audit; `level3_entire_route_and_finale_report.json` records the initial system count and resulting ending.

### 4. Level 3 bridge fall recovery raises three Blueprint errors — confirmed

A separate recovery probe placed the empty-handed robot over the bridge's catch area. The robot did return to its checkpoint, but `Grab_Clear` generated three runtime errors for missing grabbed component, `FX_Grab`, and `GrabAudio`.

The bridge calls `Grab_Clear` unconditionally. Later controllers first check whether the robot is actually holding a component. This should receive the same guard.

Source: `Source/IGB321_Project/RebootBridgeController.cpp:189`. Runtime evidence: the three `PIE: Error: Blueprint Runtime Error` entries at 04:14:15 UTC in `Saved/Logs/WholeGameQA.log`. The report marks this as a separate fault-injection probe; the following section-to-section route used normal movement.

### 5. Transfer Shaft lift uses an unavailable collision profile — confirmed warning, safety check needs repair

The Level 2 Section 3 lift did carry the actual robot to its upper landing, and the return route worked. While carrying it, however, the game logged **298 warnings** that collision profile `Custom` could not be found.

`IsLiftMoveBlocked` sweeps using the capsule's named profile. The robot has custom collision responses rather than a registered profile with that name. This makes the rider headroom check unreliable. The later shaft/core implementations use the capsule's actual channel and response settings instead.

Source: `Source/IGB321_Project/RebootTransferController.cpp:149`. Normal transport passed; a live overhead obstruction was not introduced in this playtest, so this report does not claim an observed crush or headroom collision.

### Minor usability observation: sorting switch approach

Level 1 Section 2 rejected E from an oblique approach around (2248,101), despite a switch distance of 157 cm, because its button sight line was blocked. Walking directly in front, around (2250,220), worked normally. A clearer front interaction mark or a better exposed target would make this less confusing. This did not block the playthrough.

## Coverage

| Level | Section | Result in the saved game |
|---|---|---|
| 1 | Arrival | Stairs, normal jump, E switch, moving door and exit passed. |
| 1 | Sorting Belt | Entrance ramp, front E interaction, passive belt transport and far exit passed. |
| 1 | Inspection Gate | Entry ramp, plate, timed gate, normal jump and far landing passed. |
| 1 | Local Power | E at A, actual shuttle ride, terminal E, restoration and automatic travel to Level 2 passed. |
| 2 | Crusher Approach | Real hazard recovery, both timed crossings, middle checkpoint, E shutdown and exit passed. |
| 2 | Steam Gallery | Plate/gate, both timed steam crossings, safe pad, E isolation and north corridor passed. |
| 2 | Transfer Shaft | E at A/B, actual lift ride, conveyor, both doors, descending ramp and control-room join passed; collision warning noted above. |
| 2 | Processing Control | Failed: controls, lift response, System 2 completion and next-level transition are absent. |
| 3 | Broken Service Bridge | E power, shuttle carry, conveyor, normal jump and corridor join passed; recovery errors noted above. |
| 3 | Electrical Bypass | Safe-phase crossing, plate/gate, E isolation and shaft corridor passed. |
| 3 | Emergency Access Shaft | E at A/B, belt, plate/gate, normal jump, actual lift ride, ramp and core join passed. |
| 3 | Central Recycling Core | C → B → A, both restored bridges, shuttle ride, console hold and full ending checked in the continuous Level 3 run. |

The existing **50 REBOOT automation tests also passed**, with zero failures and zero test warnings. Those tests do not cover the missing Processing Control implementation or whole-game progression, so their success does not mean the connected game is complete.

## Test method and limits

The saved maps ran in a separate Play in Editor window with the actual Stack O Bot pawn, normal movement, jumping and collision. E was generated through Unreal's built-in gameplay key simulation, reaching the normal PlayerController key state and the original Blueprint/native handlers. This mechanism is described in [Epic's Enhanced Input documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine).

No puzzle completion flags were forced. Sections were connected by ordinary movement, except for the explicit Level 3 map load required by the Level 2 blocker. A separate bridge fall fault-injection probe and deliberate crusher contact tested recovery. Some test waypoints and one actor lookup were corrected before continuing; those harness corrections are not listed as game defects. The early report named `harness_reentrancy_invalid_run.json` is invalid and excluded from the results.

This was an editor playtest, not a packaged-build, controller-device, restart-persistence or exhaustive performance test. The Level 1 return bridge and sorting side walkway were not separately exercised in this run. The earlier open editor window was preserved.

## Screenshots

### Unresponsive System 2 terminal

![Actual robot at the unresponsive System 2 terminal](C:/Users/omnom/.codex/visualizations/2026/10/23/01a167e6-6074-7652-ab74-0ad60672e280/WholeGameQA/control_terminal_visual.png)

### Processing Control switch A

![Actual robot at Processing Control switch A](C:/Users/omnom/.codex/visualizations/2026/10/23/01a167e6-6074-7652-ab74-0ad60672e280/WholeGameQA/control_room_after_A_visual.png)

### Completed core ending after the Level 3 QA load

![Level 3 ending](C:/Users/omnom/.codex/visualizations/2026/10/23/01a167e6-6074-7652-ab74-0ad60672e280/WholeGameQA/whole_game_finale_visual.png)

## Evidence files

- `level1_full_route_report.json`: arrival route after the clean restart; ends at a test waypoint later corrected to the actual corridor centre.
- `level1_route_continued_report.json` and `sorting_interaction_probe.json`: angled sorting interaction and sight-line diagnosis.
- `level1_sorting_front_retry_report.json`: sorting, inspection and entry to Local Power; ends at a corrected test actor lookup.
- `level1_local_power_to_exit_report.json`: shuttle, terminal and observed automatic Level 1 → Level 2 travel.
- `level2_full_route_report.json`: first three Level 2 sections and arrival at Processing Control.
- `level2_control_room_check_report.json`: stationary control-room lift and unresponsive B/terminal.
- `level3_entire_route_and_finale_report.json`: bridge recovery probe followed by connected Level 3 route and ending.
- `Automation/index.json`: all 50 current native tests.
- `game_files_before.json` and `completion.json`: source/config/map preservation and final results.
- Project log: `D:/Unreal Projects/IGB321_Project/Saved/Logs/WholeGameQA.log`.
