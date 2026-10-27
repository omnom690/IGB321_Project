#pragma once
#include "CoreMinimal.h"

struct FRebootProcessingState
{
    bool bPowered=false;
    bool bCircuitBEnabled=false;
    bool bSystemCompleted=false;
    double CycleTime=0;

    bool PowerUp()
    {
        if (bPowered || bSystemCompleted) return false;
        bPowered=true;CycleTime=0;return true;
    }
    bool EnableCircuitB()
    {
        if (!bPowered || bCircuitBEnabled || bSystemCompleted) return false;
        bCircuitBEnabled=true;return true;
    }
    bool CompleteSystem()
    {
        if (!bPowered || !bCircuitBEnabled || bSystemCompleted) return false;
        bSystemCompleted=true;return true;
    }
    void Advance(float DeltaSeconds)
    {
        if (bPowered) CycleTime=FMath::Fmod(CycleTime+FMath::Max(0.0f,DeltaSeconds),8.0);
    }
    float LiftFraction() const
    {
        if (!bPowered || CycleTime<2) return 0;
        if (CycleTime<4) return static_cast<float>((CycleTime-2)/2);
        if (CycleTime<6) return 1;
        return static_cast<float>((8-CycleTime)/2);
    }
};
