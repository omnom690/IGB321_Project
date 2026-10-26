#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RebootTransferState.h"
#include "RebootTransferController.generated.h"

class ACharacter;
class APlayerController;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class UTextRenderComponent;

UCLASS(BlueprintType,Blueprintable)
class IGB321_PROJECT_API ARebootTransferController : public AActor
{
    GENERATED_BODY()
public:
    ARebootTransferController();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<AActor> LiftActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<AActor> LiftCollisionActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<AActor> BeltActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<AActor> SwitchA;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<AActor> ButtonA;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<AActor> SwitchB;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<AActor> ButtonB;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<AActor> ExitDoor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<AActor> ReturnDoor;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Transfer Shaft")
    TObjectPtr<UMaterialInterface> BeltMarkerMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Transfer Shaft",meta=(ClampMin="1.0"))
    float InteractionRange = 220.0f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Transfer Shaft",meta=(ClampMin="0.0"))
    float BeltSpeed = 140.0f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Transfer Shaft")
    TObjectPtr<UInstancedStaticMeshComponent> BeltMarkers;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Transfer Shaft")
    TObjectPtr<UTextRenderComponent> PromptA;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Transfer Shaft")
    TObjectPtr<UTextRenderComponent> PromptB;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Transfer Shaft")
    TObjectPtr<UTextRenderComponent> LiftPrompt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Transfer Shaft")
    TObjectPtr<UTextRenderComponent> ReturnPrompt;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Transfer Shaft|State")
    bool bPowered = false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Transfer Shaft|State")
    bool bRouteUnlocked = false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Transfer Shaft|State")
    bool bLiftObstructed = false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Transfer Shaft|State")
    float LiftPositionFraction = 0.0f;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Transfer Shaft|State")
    float ExitOpenProgress = 0.0f;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Transfer Shaft|State")
    float ReturnOpenProgress = 0.0f;
    UFUNCTION(BlueprintPure,Category="Transfer Shaft")
    bool CanUseSwitch(int32 SwitchIndex) const;
    UFUNCTION(BlueprintCallable,Category="Transfer Shaft")
    void ActivateSwitchA();
    UFUNCTION(BlueprintCallable,Category="Transfer Shaft")
    void ActivateSwitchB();
protected:
    virtual void BeginPlay() override;
private:
    void RefreshPlayer();
    void UpdateLift(float DeltaSeconds);
    void UpdateConveyor(float DeltaSeconds);
    void UpdateDoorsAndControls(float DeltaSeconds);
    void UpdatePrompts();
    bool IsLiftRider(const ACharacter* Character) const;
    bool IsLiftMoveBlocked(const FVector& Delta,const ACharacter* Rider) const;
    static void MakeMovable(AActor* Actor);
    UPROPERTY(Transient)
    TObjectPtr<APlayerController> PlayerController;
    UPROPERTY(Transient)
    TObjectPtr<ACharacter> TrackedCharacter;
    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> LiftBase;
    FRebootTransferState State;
    FVector LiftLowerLocation = FVector(3550,2500,-35);
    FVector ExitClosedLocation;
    FVector ReturnClosedLocation;
    FVector ButtonARest;
    FVector ButtonBRest;
    float BeltDistance = 0.0f;
};
