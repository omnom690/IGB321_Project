# Level 3 Section 3 — Emergency Access Shaft

Map: `/Game/Maps/Level_03_RecyclingCore`, Room 11.

Enter from Electrical Bypass through the south corridor. Approach switch **A** and press **E** to power the conveyor. A powers the belt; the lift and core door remain offline.

Use the conveyor to reach the fixed middle landing and its pressure plate. Standing on the plate opens the access gate and keeps its six-second countdown ready. The countdown begins after stepping off. Cross to the far trigger to latch the gate open, then make the marked 300 cm jump to the switch B landing.

Approach switch **B** and press **E** to power the lift and open the core door. Board the lift at its lower stop, ride up, then follow the upper route west toward the core entrance. Each lift cycle gives two seconds at the lower stop, three seconds rising, two seconds at the upper stop, and three seconds returning.

Falls return the robot to the section entrance. Completed switch A, gate latch, and switch B states persist for the current map session; restarting Play resets them. Wait at a safe fixed landing when the lift is moving. If its clearance prompt appears, clear the obstruction or move to the centre of the deck.

## Implementation and validation

`Source/IGB321_Project/RebootShaftController.h` and `.cpp` handle the two controls, conveyor, pressure plate, gate and core door movement, lift riding, obstruction checks, and fall recovery. `RebootShaftState.h` contains the independent gate countdown and lift cycle. `RebootShaftTests.cpp` covers the `REBOOT.EmergencyShaft` automation tests.

The lift uses the robot's native moving-base support. A blocked lift pauses its own motion cycle while the gate countdown continues. Gate collision and lift rails must remain attached to their moving actors. Fall recovery calls the robot's grab cleanup only when a physics handle is holding an object.

Validation completed: all 40 REBOOT automation tests passed with zero failures and warnings, including ten Emergency Shaft tests. The lift headroom test uses the imported robot's real collision defaults, configured before creating the fixture's physics body. Clearance queries use the capsule's actual collision responses, including inline Custom settings.

The actual Stack O Bot passed the saved-map conveyor ride, fixed middle landing, seven-second held plate, missed-timer retry, doorway guard, far-side latch, normal 300 cm jump, switch B, native lift ride, upper landing, full ramp descent, and west core exit. Both A-only and completed A/B/gate states survived fall recovery. The final route produced no Blueprint runtime errors or missing collision-profile warnings.

An independent saved-map readback confirmed all 540 protected actors, Section 1/2 bindings, lift rail attachments, moving gate collision, and PlayerStart. Live testing did not alter the map file. The project PlayerStart remains at Section 1; the prepared preview opens the robot at Section 3's entrance with controls reset.
