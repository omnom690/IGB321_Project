#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RebootProcessingState.h"
#include "RebootProcessingController.generated.h"

class ACharacter;
class APlayerController;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class UTextRenderComponent;

UCLASS(BlueprintType,Blueprintable)
class IGB321_PROJECT_API ARebootProcessingController : public AActor
{
    GENERATED_BODY()
public:
    ARebootProcessingController();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> LiftActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> LiftCollisionActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> BeltActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> SwitchA;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> ButtonA;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> SwitchATarget;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> SwitchB;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> ButtonB;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> SwitchBTarget;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> TerminalActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> TerminalTarget;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Processing Control") TObjectPtr<AActor> ReturnDoor;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control") TObjectPtr<UMaterialInterface> MarkerMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control") TObjectPtr<UMaterialInterface> OffMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control") TObjectPtr<UMaterialInterface> GreenMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control") int32 TerminalCircuitASlot=INDEX_NONE;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control") int32 TerminalCircuitBSlot=INDEX_NONE;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control") FVector CheckpointGround=FVector(950,1880,2.5);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control",meta=(ClampMin="1")) float InteractionRange=220;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control",meta=(ClampMin="0")) float BeltSpeed=140;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control",meta=(ClampMin="1")) float LiftTravelHeight=314;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control",meta=(ClampMin="1")) float ReturnDoorLiftHeight=205;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control",meta=(ClampMin="0.1")) float TransitionDelay=.8f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control") FName NextLevel=FName(TEXT("/Game/Maps/Level_03_RecyclingCore"));
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Processing Control") bool bTransitionEnabled=true;

    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Processing Control|Visuals") TObjectPtr<UInstancedStaticMeshComponent> BeltMarkers;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Processing Control|Visuals") TObjectPtr<UTextRenderComponent> PromptA;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Processing Control|Visuals") TObjectPtr<UTextRenderComponent> PromptB;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Processing Control|Visuals") TObjectPtr<UTextRenderComponent> TerminalPrompt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Processing Control|Visuals") TObjectPtr<UTextRenderComponent> LiftPrompt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Processing Control|Visuals") TObjectPtr<UTextRenderComponent> ReturnPrompt;

    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Processing Control|State") bool bPowered=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Processing Control|State") bool bCircuitBEnabled=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Processing Control|State") bool bSystemCompleted=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Processing Control|State") bool bTransitionStarted=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Processing Control|State") bool bLiftObstructed=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Processing Control|State") float LiftPositionFraction=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Processing Control|State") float ReturnOpenProgress=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Processing Control|State") int32 FallResetCount=0;

    UFUNCTION(BlueprintPure,Category="Processing Control") bool CanUseSwitch(int32 Index) const;
    UFUNCTION(BlueprintCallable,Category="Processing Control") void ActivateSwitchA();
    UFUNCTION(BlueprintCallable,Category="Processing Control") void ActivateSwitchB();
    UFUNCTION(BlueprintPure,Category="Processing Control") bool CanUseTerminal() const;
    UFUNCTION(BlueprintCallable,Category="Processing Control") void ActivateTerminal();
protected:
    virtual void BeginPlay() override;
private:
    bool BindingsReady() const;
    bool CanReach(const AActor* Interactable,const AActor* TargetActor,FVector LocalTarget) const;
    bool IsLiftRider() const;
    bool IsLiftMoveBlocked(const FVector& Delta,bool Rider) const;
    void RefreshPlayer();
    void UpdateLift(float DeltaSeconds);
    void UpdateConveyor(float DeltaSeconds);
    void UpdateDoors(float DeltaSeconds);
    void UpdateRecovery(float DeltaSeconds);
    void UpdateVisuals();
    static void MakeMovable(AActor* Actor);
    void SetSignal(AActor* Actor,bool Active);

    UPROPERTY(Transient) TObjectPtr<ACharacter> Character;
    UPROPERTY(Transient) TObjectPtr<APlayerController> PlayerController;
    UPROPERTY(Transient) TObjectPtr<UPrimitiveComponent> LiftBase;
    FRebootProcessingState State;
    FVector LiftLowerLocation=FVector(800,2500,-35);
    FVector ReturnClosedLocation=FVector(-150,2500,314);
    FVector ButtonARest;
    FVector ButtonBRest;
    float BeltDistance=0;
    float CompletionElapsed=0;
    float RecoveryCooldown=0;
    bool bOwnsCheckpoint=false;
    bool bEWasDown=false;
    uint8 PreviousVisualState=255;
};
