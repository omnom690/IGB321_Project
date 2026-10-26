#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RebootShaftState.h"
#include "RebootShaftController.generated.h"

class ACharacter;
class APlayerController;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class UTextRenderComponent;

UCLASS(BlueprintType,Blueprintable)
class IGB321_PROJECT_API ARebootShaftController : public AActor
{
    GENERATED_BODY()
public:
    ARebootShaftController();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> SwitchA;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> ButtonA;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> SwitchB;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> ButtonB;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> GateActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> PlateTopActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> LiftActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> LiftCollisionActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> BeltActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<AActor> CoreDoor;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Emergency Shaft") TObjectPtr<UMaterialInterface> MarkerMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Emergency Shaft",meta=(ClampMin="1")) float InteractionRange=220;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Emergency Shaft",meta=(ClampMin="0")) float BeltSpeed=140;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Emergency Shaft",meta=(ClampMin="1")) float LiftTravelHeight=300;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Emergency Shaft",meta=(ClampMin="0")) float GateHoldSeconds=6;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Emergency Shaft") FVector CheckpointGround=FVector(3000,1800,2.5);

    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Emergency Shaft|Volumes") TObjectPtr<UBoxComponent> PlateVolume;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Emergency Shaft|Volumes") TObjectPtr<UBoxComponent> DoorwayVolume;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Emergency Shaft|Volumes") TObjectPtr<UBoxComponent> FarSideVolume;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Emergency Shaft|Visuals") TObjectPtr<UInstancedStaticMeshComponent> BeltMarkers;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Emergency Shaft|Visuals") TObjectPtr<UTextRenderComponent> PromptA;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Emergency Shaft|Visuals") TObjectPtr<UTextRenderComponent> PromptB;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Emergency Shaft|Visuals") TObjectPtr<UTextRenderComponent> LiftPrompt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Emergency Shaft|Visuals") TObjectPtr<UTextRenderComponent> GatePrompt;

    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") bool bConveyorPowered=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") bool bGateLatched=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") bool bLiftPowered=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") bool bLiftObstructed=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") bool bPlateOccupied=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") bool bSectionCompleted=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") float GateCountdown=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") float GateOpenProgress=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") float LiftPositionFraction=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") float CoreDoorOpenProgress=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Emergency Shaft|State") int32 FallResetCount=0;

    UFUNCTION(BlueprintPure,Category="Emergency Shaft") bool CanUseSwitch(int32 SwitchIndex) const;
    UFUNCTION(BlueprintCallable,Category="Emergency Shaft") void ActivateSwitchA();
    UFUNCTION(BlueprintCallable,Category="Emergency Shaft") void ActivateSwitchB();
protected:
    virtual void BeginPlay() override;
private:
    void RefreshPlayer();
    void UpdateLift(FRebootShaftState& Candidate);
    void UpdateConveyor(float DeltaSeconds);
    void UpdateDoors(float DeltaSeconds);
    void UpdateRecovery(float DeltaSeconds);
    void UpdatePrompts();
    bool IsPlateOccupied() const;
    bool IsLiftRider() const;
    bool IsLiftMoveBlocked(const FVector& Delta,bool bRider) const;
    static void MakeMovable(AActor* Actor);

    UPROPERTY(Transient) TObjectPtr<APlayerController> PlayerController;
    UPROPERTY(Transient) TObjectPtr<ACharacter> Character;
    UPROPERTY(Transient) TObjectPtr<UPrimitiveComponent> LiftBase;
    FRebootShaftState State;
    FVector LiftLowerLocation=FVector(2600,3720,-35);
    FVector GateClosedLocation=FVector(3000,2940,2);
    FVector CoreClosedLocation=FVector(1585,2800,2);
    FVector PlateRestLocation=FVector(3000,2680,-7.5);
    FVector ButtonARest;
    FVector ButtonBRest;
    float BeltDistance=0;
    float RecoveryCooldown=0;
    bool bOwnsCheckpoint=false;
};
