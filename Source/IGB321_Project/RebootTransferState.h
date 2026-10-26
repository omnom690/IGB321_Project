#pragma once
#include "CoreMinimal.h"

struct FRebootTransferState
{
    bool bPowered = false;
    bool bRouteUnlocked = false;
    double CycleTime = 0.0;
    void PowerUp() { if (!bPowered) { bPowered=true; CycleTime=0.0; } }
    bool UnlockRoute() { if (!bPowered) return false; bRouteUnlocked=true; return true; }
    void Advance(float DeltaSeconds)
    {
        if (bPowered) CycleTime=FMath::Fmod(CycleTime+FMath::Max(0.0f,DeltaSeconds),8.0);
    }
    float LiftFraction() const
    {
        if (!bPowered || CycleTime<2.0) return 0.0f;
        if (CycleTime<4.0) return static_cast<float>((CycleTime-2.0)/2.0);
        if (CycleTime<6.0) return 1.0f;
        return static_cast<float>((8.0-CycleTime)/2.0);
    }
};
