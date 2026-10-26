#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RebootCrusherState.h"
#include "RebootCrusherController.generated.h"

class APlayerController;
class APawn;
class UBoxComponent;
class UMaterialInterface;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class IGB321_PROJECT_API ARebootCrusherController : public AActor
{
    GENERATED_BODY()
public:
    ARebootCrusherController();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Crusher Approach")
    TArray<TObjectPtr<AActor>> CrusherHeads;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Crusher Approach")
    TArray<TObjectPtr<AActor>> WarningLights;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Crusher Approach")
    TArray<TObjectPtr<AActor>> WarningBeacons;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Crusher Approach")
    TObjectPtr<AActor> ExitDoor;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Crusher Approach")
    TObjectPtr<AActor> SwitchActor;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Crusher Approach")
    TObjectPtr<AActor> ButtonActor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crusher Approach")
    TObjectPtr<UMaterialInterface> GreenMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crusher Approach")
    TObjectPtr<UMaterialInterface> YellowMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crusher Approach")
    TObjectPtr<UMaterialInterface> RedMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crusher Approach",meta=(ClampMin="1.0"))
    float InteractionRange = 220.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crusher Approach")
    TObjectPtr<UBoxComponent> WaitingPocket;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crusher Approach")
    TObjectPtr<UTextRenderComponent> SwitchPrompt;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crusher Approach")
    TArray<TObjectPtr<UTextRenderComponent>> PhaseLabels;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Crusher Approach|State")
    bool bCrushersStopped = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Crusher Approach|State")
    bool bSafePocketReached = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Crusher Approach|State")
    float ExitOpenProgress = 0.0f;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Crusher Approach|State")
    int32 HazardResetCount = 0;

    UFUNCTION(BlueprintPure, Category="Crusher Approach")
    bool CanInteractWithSwitch() const;
    UFUNCTION(BlueprintCallable, Category="Crusher Approach")
    void ActivateSafetySwitch();
    UFUNCTION(BlueprintPure, Category="Crusher Approach")
    float GetCrusherRaisedFraction(int32 Index) const;

protected:
    virtual void BeginPlay() override;
private:
    void SimulateStep(float DeltaSeconds,APawn* Pawn);
    void RefreshPlayerController();
    void UpdateWarnings();
    void ResetCrushedPawn(APawn* Pawn);
    FVector SwitchTarget() const;
    static FBox PawnBounds(const APawn* Pawn);
    static void MakeMovable(AActor* Actor);
    UPROPERTY(Transient)
    TObjectPtr<APlayerController> PlayerController;
    FRebootCrusherState State;
    TArray<FVector> RaisedLocations;
    FVector DoorClosedLocation;
    FVector ButtonRestLocation;
    float ResetCooldown = 0.0f;
};
