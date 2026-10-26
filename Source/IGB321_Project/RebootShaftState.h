#pragma once

#include "CoreMinimal.h"

struct FRebootShaftState
{
    bool bConveyorPowered=false;
    bool bGateLatched=false;
    bool bLiftPowered=false;
    bool bGateArmed=false;
    bool bGateRequestedOpen=false;
    float GateHoldSeconds=6;
    float GateHoldRemaining=0;
    double LiftCycleTime=0;

    void PowerConveyor() { bConveyorPowered=true; }
    bool PowerLift()
    {
        if (!bConveyorPowered || !bGateLatched) return false;
        if (!bLiftPowered) { bLiftPowered=true;LiftCycleTime=0; }
        return true;
    }
    void Advance(float DeltaSeconds,bool bPlateOccupied,bool bCrossedGate,bool bDoorwayOccupied)
    {
        const float Step=FMath::Max(0.0f,DeltaSeconds);
        if (bConveyorPowered)
        {
            GateHoldRemaining=bPlateOccupied?FMath::Max(0.0f,GateHoldSeconds):FMath::Max(0.0f,GateHoldRemaining-Step);
            bGateArmed|=bPlateOccupied;
            bGateLatched|=bCrossedGate && bGateArmed;
            bGateRequestedOpen=bPlateOccupied || GateHoldRemaining>0 || bGateLatched || (bGateArmed && bDoorwayOccupied);
            if (!bGateRequestedOpen) bGateArmed=false;
        }
        if (bLiftPowered) LiftCycleTime=FMath::Fmod(LiftCycleTime+Step,10.0);
    }
    float LiftFraction() const
    {
        if (!bLiftPowered || LiftCycleTime<2) return 0;
        if (LiftCycleTime<5) return static_cast<float>((LiftCycleTime-2)/3);
        if (LiftCycleTime<7) return 1;
        return static_cast<float>((10-LiftCycleTime)/3);
    }
};
