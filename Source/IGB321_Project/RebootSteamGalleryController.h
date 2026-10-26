#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RebootSteamGalleryState.h"
#include "RebootSteamGalleryController.generated.h"

class APlayerController;
class APawn;
class UBoxComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class IGB321_PROJECT_API ARebootSteamGalleryController : public AActor
{
    GENERATED_BODY()

public:
    ARebootSteamGalleryController();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Steam Gallery")
    TObjectPtr<AActor> GateActor;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Steam Gallery")
    TObjectPtr<AActor> PlateTopActor;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Steam Gallery")
    TObjectPtr<AActor> ValvePivotActor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steam Gallery")
    TObjectPtr<UMaterialInterface> SteamMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steam Gallery", meta=(ClampMin="0.0"))
    float GateHoldSeconds = 6.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steam Gallery", meta=(ClampMin="0.01"))
    float SteamActiveSeconds = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steam Gallery", meta=(ClampMin="0.01"))
    float SteamClearSeconds = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steam Gallery", meta=(ClampMin="1.0"))
    float InteractionRange = 220.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steam Gallery", meta=(ClampMin="1.0"))
    float GateSpeed = 315.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steam Gallery|Volumes")
    TObjectPtr<UBoxComponent> PlateVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steam Gallery|Volumes")
    TObjectPtr<UBoxComponent> DoorwayVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steam Gallery|Volumes")
    TObjectPtr<UBoxComponent> FarSideVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steam Gallery|Volumes")
    TObjectPtr<UBoxComponent> SafePadVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steam Gallery|Volumes")
    TArray<TObjectPtr<UBoxComponent>> SteamVolumes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steam Gallery|Visuals")
    TArray<TObjectPtr<UStaticMeshComponent>> SteamPuffs;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steam Gallery|Visuals")
    TArray<TObjectPtr<UPointLightComponent>> VentLights;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steam Gallery|Visuals")
    TObjectPtr<UTextRenderComponent> ValvePrompt;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steam Gallery|Visuals")
    TObjectPtr<UTextRenderComponent> GatePrompt;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Steam Gallery|State")
    bool bPlateOccupied = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Steam Gallery|State")
    bool bGateLatched = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Steam Gallery|State")
    bool bSteamIsolated = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Steam Gallery|State")
    float GateOpenProgress = 0.0f;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Steam Gallery|State")
    float GateCountdown = 0.0f;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Steam Gallery|State")
    int32 HazardResetCount = 0;

    UFUNCTION(BlueprintPure, Category="Steam Gallery")
    bool CanInteractWithValve() const;
    UFUNCTION(BlueprintCallable, Category="Steam Gallery")
    void ActivateIsolationValve();
    UFUNCTION(BlueprintPure, Category="Steam Gallery")
    bool IsVentActive(int32 VentIndex) const;

protected:
    virtual void BeginPlay() override;

private:
    bool IsPlateOccupied() const;
    void UpdateSteamVisuals();
    void ResetPawnFromSteam(APawn* Pawn);
    void RefreshPlayerController();
    static FBox PawnBounds(const APawn* Pawn);
    static void MakeMovable(AActor* Actor);

    UPROPERTY(Transient)
    TObjectPtr<APlayerController> PlayerController;
    FRebootSteamGalleryState State;
    FVector GateClosedLocation = FVector(2315,0,2);
    FVector PlateRestLocation = FVector(2020,0,-7.5);
    FVector CheckpointGround = FVector(2550,0,2.5);
    FRotator ValveRestRotation;
    float ValveTurn = 0.0f;
    float ResetCooldown = 0.0f;
};
