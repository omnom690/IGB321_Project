#pragma once

#include "CoreMinimal.h"

enum class ERebootElectricalPhase : uint8 { Warning, Live, Safe };

struct FRebootElectricalState
{
    double Elapsed = 0.0;
    float GateHoldSeconds = 6.0f;
    float GateHoldRemaining = 0.0f;
    bool bGateLatched = false;
    bool bIsolated = false;
    bool bGateRequestedOpen = false;
    bool bGateArmed = false;

    void Advance(float DeltaSeconds, bool bPlateOccupied, bool bCrossedGate, bool bDoorwayOccupied)
    {
        const float Step = FMath::Max(0.0f, DeltaSeconds);
        Elapsed += Step;
        GateHoldRemaining = bPlateOccupied ? FMath::Max(0.0f, GateHoldSeconds)
            : FMath::Max(0.0f, GateHoldRemaining-Step);
        bGateArmed |= bPlateOccupied;
        // The caller supplies a crossing only after the real panel clears the opening.
        bGateLatched |= bCrossedGate && bGateArmed;
        bGateRequestedOpen = bPlateOccupied || GateHoldRemaining>0 || bGateLatched
            || (bGateArmed && bDoorwayOccupied);
        if (!bGateRequestedOpen) bGateArmed = false;
    }

    ERebootElectricalPhase Phase() const
    {
        if (bIsolated) return ERebootElectricalPhase::Safe;
        const double Time = FMath::Fmod(Elapsed,4.0);
        return Time<1.0 ? ERebootElectricalPhase::Warning
            : Time<2.0 ? ERebootElectricalPhase::Live : ERebootElectricalPhase::Safe;
    }
    void Isolate() { if (bGateLatched) bIsolated = true; }
};
