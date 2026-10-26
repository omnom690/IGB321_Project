#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RebootBridgeState.h"
#include "RebootBridgeController.generated.h"

class ACharacter;
class APlayerController;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UPrimitiveComponent;
class UTextRenderComponent;

UCLASS(BlueprintType,Blueprintable)
class IGB321_PROJECT_API ARebootBridgeController : public AActor
{
    GENERATED_BODY()
public:
    ARebootBridgeController();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Broken Bridge")
    TObjectPtr<AActor> SwitchActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Broken Bridge")
    TObjectPtr<AActor> ButtonActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Broken Bridge")
    TObjectPtr<AActor> ShuttleActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Broken Bridge")
    TObjectPtr<AActor> ShuttleCollisionActor;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Broken Bridge")
    TObjectPtr<AActor> BeltActor;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Broken Bridge")
    TObjectPtr<UMaterialInterface> MarkerMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Broken Bridge")
    FVector ShuttleTravel=FVector(0,600,0);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Broken Bridge")
    FVector CheckpointGround=FVector(-1050,-650,2.5);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Broken Bridge",meta=(ClampMin="1.0"))
    float InteractionRange=220;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Broken Bridge",meta=(ClampMin="0.0"))
    float BeltSpeed=140;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Broken Bridge")
    TObjectPtr<UInstancedStaticMeshComponent> BeltMarkers;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Broken Bridge")
    TObjectPtr<UTextRenderComponent> PowerPrompt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Broken Bridge")
    TObjectPtr<UTextRenderComponent> ShuttlePrompt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Broken Bridge")
    TObjectPtr<UPointLightComponent> PowerLight;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Broken Bridge|State")
    bool bPowered=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Broken Bridge|State")
    bool bShuttleObstructed=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Broken Bridge|State")
    bool bSectionCompleted=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Broken Bridge|State")
    float ShuttlePositionFraction=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Broken Bridge|State")
    int32 FallResetCount=0;
    UFUNCTION(BlueprintPure,Category="Broken Bridge")
    bool CanActivatePower() const;
    UFUNCTION(BlueprintCallable,Category="Broken Bridge")
    void ActivatePower();
protected:
    virtual void BeginPlay() override;
private:
    void RefreshPlayer();
    void UpdateShuttle(float DeltaSeconds);
    void UpdateConveyor(float DeltaSeconds);
    void UpdateRecovery(float DeltaSeconds);
    bool IsRider() const;
    void UpdatePrompts();
    static void MakeMovable(AActor* Actor);
    UPROPERTY(Transient)
    TObjectPtr<APlayerController> PlayerController;
    UPROPERTY(Transient)
    TObjectPtr<ACharacter> Character;
    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> ShuttleBase;
    FRebootBridgeState State;
    FVector ShuttleNearLocation=FVector(-600,-200,-35);
    FVector ButtonRest;
    float BeltDistance=0;
    float RecoveryCooldown=0;
    bool bOwnsCheckpoint=false;
};
