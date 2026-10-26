#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RebootElectricalState.h"
#include "RebootElectricalController.generated.h"

class APlayerController;
class APawn;
class UAudioComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USoundWaveProcedural;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class IGB321_PROJECT_API ARebootElectricalController : public AActor
{
    GENERATED_BODY()
public:
    ARebootElectricalController();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Electrical Bypass") TObjectPtr<AActor> GateActor;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Electrical Bypass") TObjectPtr<AActor> ShaftGateActor;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Electrical Bypass") TObjectPtr<AActor> PlateTopActor;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Electrical Bypass") TObjectPtr<AActor> SwitchActor;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Electrical Bypass") TObjectPtr<AActor> ButtonActor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Electrical Bypass") TObjectPtr<UMaterialInterface> ArcMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Electrical Bypass", meta=(ClampMin="0")) float GateHoldSeconds=6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Electrical Bypass", meta=(ClampMin="1")) float GateSpeed=315;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Electrical Bypass", meta=(ClampMin="1")) float InteractionRange=220;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electrical Bypass|Volumes") TObjectPtr<UBoxComponent> ElectricalVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electrical Bypass|Volumes") TObjectPtr<UBoxComponent> PlateVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electrical Bypass|Volumes") TObjectPtr<UBoxComponent> DoorwayVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electrical Bypass|Volumes") TObjectPtr<UBoxComponent> FarSideVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electrical Bypass|Visuals") TObjectPtr<UInstancedStaticMeshComponent> Arcs;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electrical Bypass|Visuals") TObjectPtr<UPointLightComponent> HazardLight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electrical Bypass|Visuals") TObjectPtr<UTextRenderComponent> PhasePrompt;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electrical Bypass|Visuals") TObjectPtr<UTextRenderComponent> GatePrompt;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electrical Bypass|Visuals") TObjectPtr<UTextRenderComponent> IsolationPrompt;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") bool bPlateOccupied=false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") bool bGateLatched=false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") bool bIsolated=false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") bool bElectricalLive=false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") bool bElectricalWarning=false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") float CyclePhaseSeconds=0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") float GateCountdown=0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") float GateOpenProgress=0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") float ShaftGateOpenProgress=0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Electrical Bypass|State") int32 HazardResetCount=0;

    UFUNCTION(BlueprintPure, Category="Electrical Bypass") bool CanActivateIsolation() const;
    UFUNCTION(BlueprintCallable, Category="Electrical Bypass") void ActivateIsolation();

protected:
    virtual void BeginPlay() override;
private:
    bool IsPlateOccupied() const;
    void RefreshPlayerController();
    void UpdateVisuals();
    void PlayWarning();
    void ResetPawn(APawn* Pawn);
    static FBox PawnBounds(const APawn* Pawn);
    static void MakeMovable(AActor* Actor);

    UPROPERTY(Transient) TObjectPtr<APlayerController> PlayerController;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ArcInstance;
    UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> WarningWave;
    UPROPERTY() TObjectPtr<UAudioComponent> WarningAudio;
    FRebootElectricalState State;
    FVector GateClosedLocation=FVector(3200,0,2);
    FVector ShaftClosedLocation=FVector(3000,1115,2);
    FVector PlateRestLocation=FVector(2850,0,-7.5);
    FVector ButtonRestLocation=FVector(3750,0,120);
    float ResetCooldown=0;
    bool bWasWarning=false;
};
