#pragma once

#include "CoreMinimal.h"

// Runtime state is independent of meshes, player input and editor tooling.
struct FRebootSteamGalleryState
{
    double Elapsed = 0.0;
    float GateHoldRemaining = 0.0f;
    float GateHoldSeconds = 6.0f;
    float SteamActiveSeconds = 2.0f;
    float SteamClearSeconds = 2.0f;
    bool bGateLatched = false;
    bool bSteamIsolated = false;
    bool bGateRequestedOpen = false;

    void Advance(float DeltaSeconds, bool bPlateOccupied, bool bCrossedGate, bool bDoorwayOccupied)
    {
        const float Step = FMath::Max(0.0f, DeltaSeconds);
        Elapsed += Step;
        bGateLatched |= bCrossedGate;
        GateHoldRemaining = bPlateOccupied ? FMath::Max(0.0f, GateHoldSeconds)
            : FMath::Max(0.0f, GateHoldRemaining - Step);
        bGateRequestedOpen = bPlateOccupied || GateHoldRemaining > 0.0f
            || bGateLatched || bDoorwayOccupied;
    }

    bool IsSteamActive(int32 VentIndex) const
    {
        if (bSteamIsolated) return false;
        const double Active = FMath::Max(0.01f, SteamActiveSeconds);
        const double Clear = FMath::Max(0.01f, SteamClearSeconds);
        const double Phase = FMath::Fmod(Elapsed + (VentIndex % 2) * Active, Active + Clear);
        return Phase >= Clear;
    }

    void IsolateSteam() { bSteamIsolated = true; }
};
