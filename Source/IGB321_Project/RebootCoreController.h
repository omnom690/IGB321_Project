#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RebootCoreState.h"
#include "RebootCoreController.generated.h"

class ACharacter;
class APlayerController;
class UAudioComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UPrimitiveComponent;
class USoundWaveProcedural;
class UTextRenderComponent;

UCLASS(BlueprintType,Blueprintable)
class IGB321_PROJECT_API ARebootCoreController : public AActor
{
    GENERATED_BODY()
public:
    ARebootCoreController();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TArray<TObjectPtr<AActor>> RelayActors;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> ConsoleActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> CoreActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> ShuttleActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> ShuttleCollisionActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> ReturnBridgeA;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> ReturnBridgeC;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> GateActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> PlateTopActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> FinaleCameraActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<AActor> BackgroundRotor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Recycling Core") TArray<TObjectPtr<AActor>> EmergencyLights;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<UMaterialInterface> OffMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<UMaterialInterface> GreenMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<UMaterialInterface> YellowMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recycling Core") TObjectPtr<UMaterialInterface> SignalMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recycling Core") FVector HubCheckpoint=FVector(550,2580,2.5);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recycling Core") FVector ConsolePadGround=FVector(550,2580,2.5);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recycling Core") FVector ShuttleTravel=FVector(-600,0,0);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recycling Core") FVector GateCrossingDirection=FVector(0,-1,0);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Recycling Core",meta=(ClampMin="1")) float InteractionRange=220;

    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Volumes") TObjectPtr<UBoxComponent> ElectricalVolume;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Volumes") TObjectPtr<UBoxComponent> PlateVolume;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Volumes") TObjectPtr<UBoxComponent> DoorwayVolume;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Volumes") TObjectPtr<UBoxComponent> FarSideVolume;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Volumes") TObjectPtr<UBoxComponent> ConsolePadVolume;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Visuals") TObjectPtr<UInstancedStaticMeshComponent> Arcs;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Visuals") TArray<TObjectPtr<UInstancedStaticMeshComponent>> RelayCables;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Visuals") TArray<TObjectPtr<UTextRenderComponent>> RelayPrompts;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Visuals") TObjectPtr<UTextRenderComponent> ConsolePrompt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Visuals") TObjectPtr<UTextRenderComponent> GatePrompt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Visuals") TObjectPtr<UTextRenderComponent> ElectricalPrompt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Visuals") TObjectPtr<UTextRenderComponent> CompletionText;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Visuals") TObjectPtr<UTextRenderComponent> SystemText;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Recycling Core|Visuals") TObjectPtr<UPointLightComponent> HazardLight;

    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") uint8 RelayMask=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") int32 CompletedRelayCount=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") bool bGateLatched=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") float GateCountdown=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") float GateOpenProgress=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") bool bElectricalWarning=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") bool bElectricalLive=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") float ElectricalCycleSeconds=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") float ShuttlePositionFraction=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") bool bShuttleObstructed=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") int32 FallResetCount=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") int32 HazardResetCount=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") float RebootHoldProgress=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") bool bRebootStarted=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") bool bRobotLocked=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") bool bFacilityRebootComplete=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") float FinaleProgress=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Recycling Core|State") int32 ActiveSystemCount=2;

    UFUNCTION(BlueprintPure,Category="Recycling Core") bool CanActivateRelay(int32 Index) const;
    UFUNCTION(BlueprintCallable,Category="Recycling Core") void ActivateRelay(int32 Index);
    UFUNCTION(BlueprintPure,Category="Recycling Core") bool CanReboot() const;
    // Input-action binding for Blueprint/controller interactions; the keyboard
    // path and this binding share the same pad checks and timed hold.
    UFUNCTION(BlueprintCallable,Category="Recycling Core|Input") void SetRebootInteractionHeld(bool Held);
protected:
    virtual void BeginPlay() override;
private:
    bool CanReach(const AActor* Actor,FVector LocalTarget) const;
    bool IsPlateOccupied() const;
    void RefreshPlayer();
    void UpdateShuttle(FRebootCoreState& Candidate);
    void UpdateGate(float DeltaSeconds);
    void UpdateRecovery(float DeltaSeconds);
    void ResetRobot(bool Electrical);
    void ClearGrab();
    void StartFinale();
    void UpdateFinale(float DeltaSeconds);
    void UpdateVisuals();
    void PlayWarning();
    static void SetRouteActive(AActor* Actor,bool Active);
    static void MakeMovable(AActor* Actor);

    UPROPERTY(Transient) TObjectPtr<ACharacter> Character;
    UPROPERTY(Transient) TObjectPtr<APlayerController> PlayerController;
    UPROPERTY(Transient) TObjectPtr<UPrimitiveComponent> ShuttleBase;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ArcMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> RelayMaterials;
    UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> WarningWave;
    UPROPERTY() TObjectPtr<UAudioComponent> WarningAudio;
    FRebootCoreState State;
    FVector ShuttleNear=FVector(-800,2800,-35);
    FVector GateClosed=FVector(0,2190,2);
    FVector PlateRest=FVector(0,2350,-7.5);
    FVector LockedPosition;
    FRotator LockedRotation;
    bool bOwnsCheckpoint=false;
    bool bWasWarning=false;
    bool bBoundRebootHeld=false;
    uint8 PreviousVisualMask=255;
    float RecoveryCooldown=0;
};
