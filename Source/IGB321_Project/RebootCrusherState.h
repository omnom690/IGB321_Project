#pragma once

#include "CoreMinimal.h"

enum class ERebootCrusherPhase : uint8 { Open, Warning, Closed };

struct FRebootCrusherState
{
    double Elapsed = 0.35;
    bool bStopped = false;

    void Advance(float DeltaSeconds) { Elapsed += FMath::Max(0.0f,DeltaSeconds); }
    void StopPermanently() { bStopped = true; }
    double CycleTime(int32 Index) const { return FMath::Fmod(Elapsed+(Index%2)*2.0,4.0); }
    ERebootCrusherPhase Phase(int32 Index) const
    {
        if (bStopped) return ERebootCrusherPhase::Open;
        const double Time = CycleTime(Index);
        return Time<2.0 ? ERebootCrusherPhase::Open : Time<3.0 ? ERebootCrusherPhase::Warning : ERebootCrusherPhase::Closed;
    }
    float RaisedFraction(int32 Index) const
    {
        if (bStopped) return 1.0f;
        const double Time = CycleTime(Index);
        if (Time<2.0) return static_cast<float>(FMath::Clamp(Time/0.35,0.0,1.0));
        if (Time<2.65) return 1.0f;
        if (Time<3.0) return static_cast<float>((3.0-Time)/0.35);
        return 0.0f;
    }
};
