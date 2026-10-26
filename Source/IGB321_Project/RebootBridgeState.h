#pragma once
#include "CoreMinimal.h"

struct FRebootBridgeState
{
    bool bPowered=false;
    double CycleTime=0.0;
    void PowerUp() { if (!bPowered) { bPowered=true; CycleTime=0; } }
    void Advance(float DeltaSeconds) { if (bPowered) CycleTime=FMath::Fmod(CycleTime+FMath::Max(0.0f,DeltaSeconds),10.0); }
    float ShuttleFraction() const
    {
        if (!bPowered || CycleTime<2) return 0;
        if (CycleTime<5) return static_cast<float>((CycleTime-2)/3);
        if (CycleTime<7) return 1;
        return static_cast<float>((10-CycleTime)/3);
    }
};
