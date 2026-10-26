#pragma once

#include "CoreMinimal.h"

enum class ERebootCoreElectricalPhase : uint8 { Warning,Live,Safe };

struct FRebootCoreState
{
    uint8 RelayMask=0;
    bool bGateLatched=false;
    bool bGateArmed=false;
    bool bGateRequestedOpen=false;
    bool bFinaleStarted=false;
    bool bComplete=false;
    float GateHoldSeconds=6;
    float GateHoldRemaining=0;
    float RebootHeldSeconds=0;
    double Elapsed=0;
    double ShuttleCycleTime=0;
    double FinaleElapsed=0;

    bool RelayActive(int32 Index) const { return Index>=0 && Index<3 && (RelayMask&(1<<Index))!=0; }
    bool AllRelaysActive() const { return RelayMask==7; }
    bool ActivateRelay(int32 Index)
    {
        if (Index<0 || Index>2 || bFinaleStarted || RelayActive(Index) || (Index==2 && !bGateLatched)) return false;
        RelayMask|=1<<Index;return true;
    }
    ERebootCoreElectricalPhase ElectricalPhase() const
    {
        if (RelayActive(1) || bFinaleStarted) return ERebootCoreElectricalPhase::Safe;
        const double Phase=FMath::Fmod(Elapsed,4.0);
        return Phase<1?ERebootCoreElectricalPhase::Warning:Phase<2?ERebootCoreElectricalPhase::Live:ERebootCoreElectricalPhase::Safe;
    }
    float ShuttleFraction() const
    {
        if (ShuttleCycleTime<2) return 0;
        if (ShuttleCycleTime<5) return static_cast<float>((ShuttleCycleTime-2)/3);
        if (ShuttleCycleTime<7) return 1;
        return static_cast<float>((10-ShuttleCycleTime)/3);
    }
    void Advance(float DeltaSeconds,bool Plate,bool Crossed,bool Doorway,bool ConsoleEligible,bool Held)
    {
        const float Step=FMath::Max(0.0f,DeltaSeconds);
        Elapsed+=Step;ShuttleCycleTime=FMath::Fmod(ShuttleCycleTime+Step,10.0);
        GateHoldRemaining=Plate?FMath::Max(0.0f,GateHoldSeconds):FMath::Max(0.0f,GateHoldRemaining-Step);
        bGateArmed|=Plate;
        bGateLatched|=Crossed && bGateArmed;
        bGateRequestedOpen=Plate || GateHoldRemaining>0 || bGateLatched || (bGateArmed && Doorway);
        if (!bGateRequestedOpen) bGateArmed=false;
        if (bFinaleStarted)
        {
            FinaleElapsed=FMath::Min(15.0,FinaleElapsed+Step);
            bComplete=FinaleElapsed>=15.0;
        }
        else
        {
            RebootHeldSeconds=AllRelaysActive() && ConsoleEligible && Held?FMath::Min(1.0f,RebootHeldSeconds+Step):0;
            if (RebootHeldSeconds>=1) { bFinaleStarted=true;FinaleElapsed=0; }
        }
    }
};
