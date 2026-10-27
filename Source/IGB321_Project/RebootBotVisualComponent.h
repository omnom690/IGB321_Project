#pragma once
#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "UObject/SoftObjectPtr.h"
#include "RebootBotVisualComponent.generated.h"

class ACharacter;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;

// Rendering only: BP_Bot continues to own movement, collision, camera and grabbing.
UCLASS(Config=Game,DefaultConfig,ClassGroup=(REBOOT),meta=(BlueprintSpawnableComponent))
class IGB321_PROJECT_API URebootBotVisualComponent : public UPoseableMeshComponent
{
    GENERATED_BODY()
public:
    URebootBotVisualComponent();
    bool InitializeForCharacter(ACharacter* Character,USkeletalMesh* Mesh);
    virtual void TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(Config,EditAnywhere,Category="Robot Visual") TSoftObjectPtr<USkeletalMesh> RobotMesh;
    UPROPERTY(Config,EditAnywhere,Category="Robot Visual") float VisualYaw=0;
    UPROPERTY(Config,EditAnywhere,Category="Robot Visual",meta=(ClampMin="10")) float TargetHeight=100;
    UPROPERTY(Config,EditAnywhere,Category="Robot Visual",meta=(ClampMin="10")) float TargetWidth=90;
    UPROPERTY(Config,EditAnywhere,Category="Robot Visual") float HoverClearance=16;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Robot Visual") bool bVisualReady=false;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Robot Visual") float MovementBlend=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Robot Visual") float GrabBlend=0;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Robot Visual") FName HeadBone;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Robot Visual") FName LeftArmBone;
    UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Transient,Category="Robot Visual") FName RightArmBone;
private:
    FName FindVisualBone(const FString& Hint) const;
    void ApplyBoneDelta(FName Name,const FTransform& Rest,const FQuat& Delta);
    UPROPERTY(Transient) TObjectPtr<ACharacter> CharacterOwner;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> OriginalMesh;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> HiddenAccessories;
    FVector BaseOffset=FVector::ZeroVector;
    FTransform HeadRest;
    FTransform LeftArmRest;
    FTransform RightArmRest;
    bool bOriginalVisible=true;
    bool bOriginalCastShadow=true;
    EVisibilityBasedAnimTickOption OriginalTickOption=EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    float VisualTime=0;
};
