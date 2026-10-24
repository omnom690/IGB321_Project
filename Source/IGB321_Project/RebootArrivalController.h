#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RebootArrivalController.generated.h"

class APlayerController;
class UMaterialInstanceDynamic;
class USoundBase;

UCLASS(BlueprintType, Blueprintable)
class IGB321_PROJECT_API ARebootArrivalController : public AActor
{
    GENERATED_BODY()

public:
    ARebootArrivalController();

    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Arrival")
    TObjectPtr<AActor> DoorActor;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Arrival")
    TObjectPtr<AActor> SwitchActor;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Arrival")
    TObjectPtr<AActor> ButtonActor;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Arrival")
    TArray<TObjectPtr<AActor>> StatusActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrival", meta=(ClampMin="1.0", Units="cm"))
    float InteractionRange = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrival", meta=(ClampMin="1.0", Units="cm"))
    float DoorLiftHeight = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrival", meta=(ClampMin="1.0", Units="cm/s"))
    float DoorSpeed = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrival", meta=(ClampMin="0.0", Units="cm"))
    float ButtonTravel = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrival")
    TObjectPtr<USoundBase> ActivationSound;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Arrival")
    bool bSwitchActivated = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Arrival")
    bool bDoorOpened = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Arrival")
    float DoorOpenProgress = 0.0f;

    UFUNCTION(BlueprintCallable, Category="Arrival")
    void ActivateSwitch();

    UFUNCTION(BlueprintPure, Category="Arrival")
    bool CanInteract() const;

protected:
    virtual void BeginPlay() override;

private:
    void ResolveActors();
    void PrepareStatusMaterials();
    void RefreshPlayerController();
    static void MakeMovable(AActor* Actor);
    bool HasSwitchLineOfSight(const class APawn* Pawn, const FVector& Target) const;
    FVector GetSwitchTarget() const;

    UPROPERTY(Transient)
    TObjectPtr<APlayerController> PlayerController;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> StatusMaterials;

    FVector DoorClosedLocation = FVector::ZeroVector;
    bool bDoorLocationReady = false;
};
