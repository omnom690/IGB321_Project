#include "RebootBotVisualComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

namespace
{
FString BoneKey(FString Value)
{
    Value=Value.ToLower();Value.ReplaceInline(TEXT("-"),TEXT(""));
    Value.ReplaceInline(TEXT("_"),TEXT(""));Value.ReplaceInline(TEXT("."),TEXT(""));return Value;
}
}

URebootBotVisualComponent::URebootBotVisualComponent()
{
    PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostPhysics;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);SetCastShadow(true);
    RobotMesh=FSoftObjectPath(TEXT("/Game/REBOOT/Redesign/Robot/SKM_Robot2.SKM_Robot2"));
}

FName URebootBotVisualComponent::FindVisualBone(const FString& Hint) const
{
    const FString Key=BoneKey(Hint);
    for (int32 Index=0;Index<GetNumBones();++Index)
        if (BoneKey(GetBoneName(Index).ToString())==Key) return GetBoneName(Index);
    return NAME_None;
}

bool URebootBotVisualComponent::InitializeForCharacter(ACharacter* InCharacter,USkeletalMesh* Mesh)
{
    if (!IsValid(InCharacter) || !IsValid(Mesh) || !IsValid(InCharacter->GetMesh())) return false;
    CharacterOwner=InCharacter;OriginalMesh=InCharacter->GetMesh();
    SetSkinnedAssetAndUpdate(Mesh);
    RefreshBoneTransforms();
    if (GetNumBones()==0) return false;
    const FBoxSphereBounds MeshBounds=Mesh->GetBounds();
    const float Height=FMath::Max(1.0f,static_cast<float>(MeshBounds.BoxExtent.Z*2));
    const float Width=FMath::Max(1.0f,static_cast<float>(FMath::Max(MeshBounds.BoxExtent.X,MeshBounds.BoxExtent.Y)*2));
    const float Scale=FMath::Min(FMath::Max(10.0f,TargetHeight)/Height,FMath::Max(10.0f,TargetWidth)/Width);
    const double Bottom=MeshBounds.Origin.Z-MeshBounds.BoxExtent.Z;
    BaseOffset=FVector(0,0,-InCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+HoverClearance-Bottom*Scale);
    SetRelativeScale3D(FVector(Scale));SetRelativeLocation(BaseOffset);SetRelativeRotation(FRotator(0,VisualYaw,0));
    SetBoundsScale(1.5f);
    HeadBone=FindVisualBone(TEXT("DEF-Head"));
    LeftArmBone=FindVisualBone(TEXT("DEF-upper_arm.L"));
    RightArmBone=FindVisualBone(TEXT("DEF-upper_arm.R"));
    if (!HeadBone.IsNone()) HeadRest=GetBoneTransformByName(HeadBone,EBoneSpaces::ComponentSpace);
    if (!LeftArmBone.IsNone()) LeftArmRest=GetBoneTransformByName(LeftArmBone,EBoneSpaces::ComponentSpace);
    if (!RightArmBone.IsNone()) RightArmRest=GetBoneTransformByName(RightArmBone,EBoneSpaces::ComponentSpace);

    bOriginalVisible=OriginalMesh->IsVisible();bOriginalCastShadow=OriginalMesh->CastShadow;
    OriginalTickOption=OriginalMesh->VisibilityBasedAnimTickOption;
    OriginalMesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    OriginalMesh->SetVisibility(false,false);OriginalMesh->SetCastShadow(false);
    TArray<UStaticMeshComponent*> Accessories;InCharacter->GetComponents(Accessories);
    for (UStaticMeshComponent* Part:Accessories)
    {
        FString Name=Part->GetName().ToLower();
        if (Part->GetStaticMesh()) Name+=Part->GetStaticMesh()->GetName().ToLower();
        if (Part->IsVisible() && (Name.Contains(TEXT("backpack")) || Name.Contains(TEXT("jetpack")) || Name.Contains(TEXT("hat"))))
        {
            HiddenAccessories.Add(Part);Part->SetVisibility(false,false);
        }
    }
    AddTickPrerequisiteComponent(OriginalMesh);bVisualReady=true;return true;
}

void URebootBotVisualComponent::ApplyBoneDelta(FName Name,const FTransform& Rest,const FQuat& Delta)
{
    if (Name.IsNone()) return;
    FTransform Pose=Rest;Pose.SetRotation((Delta*Rest.GetRotation()).GetNormalized());
    SetBoneTransformByName(Name,Pose,EBoneSpaces::ComponentSpace);
}

void URebootBotVisualComponent::TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    if (bVisualReady && IsValid(CharacterOwner))
    {
        if (IsValid(OriginalMesh) && OriginalMesh->IsVisible()) OriginalMesh->SetVisibility(false,false);
        for (UStaticMeshComponent* Part:HiddenAccessories) if (IsValid(Part) && Part->IsVisible()) Part->SetVisibility(false,false);
        const float Step=FMath::Clamp(DeltaSeconds,0.0f,.1f);VisualTime+=Step;
        const UCharacterMovementComponent* Movement=CharacterOwner->GetCharacterMovement();
        const float Speed=CharacterOwner->GetVelocity().Size2D();
        MovementBlend=FMath::FInterpTo(MovementBlend,FMath::Clamp(Speed/FMath::Max(1.0f,Movement->MaxWalkSpeed),0.0f,1.0f),Step,8);
        TArray<UPhysicsHandleComponent*> Handles;CharacterOwner->GetComponents(Handles);
        const bool Holding=Handles.ContainsByPredicate([](const UPhysicsHandleComponent* Handle)
            {return IsValid(Handle) && IsValid(Handle->GetGrabbedComponent());});
        GrabBlend=FMath::FInterpTo(GrabBlend,Holding?1.0f:0.0f,Step,10);
        const float Bob=FMath::Sin(VisualTime*(3.0f+MovementBlend*5.0f))*(1.1f+MovementBlend*.8f);
        SetRelativeLocation(BaseOffset+FVector(0,0,Bob));
        SetRelativeRotation(FRotator(-MovementBlend*5.0f-(Movement->IsFalling()?3.0f:0.0f),VisualYaw,0));
        float LookYaw=0;
        if (const AController* Controller=CharacterOwner->GetController())
            LookYaw=FMath::Clamp(FMath::FindDeltaAngleDegrees(CharacterOwner->GetActorRotation().Yaw,Controller->GetControlRotation().Yaw),-35.0f,35.0f);
        ApplyBoneDelta(HeadBone,HeadRest,FQuat(FVector::UpVector,FMath::DegreesToRadians(LookYaw*.6f)));
        const float Swing=FMath::Sin(VisualTime*8.0f)*MovementBlend*4.0f;
        ApplyBoneDelta(LeftArmBone,LeftArmRest,FQuat(FVector::YAxisVector,FMath::DegreesToRadians(-GrabBlend*55.0f+Swing)));
        ApplyBoneDelta(RightArmBone,RightArmRest,FQuat(FVector::YAxisVector,FMath::DegreesToRadians(-GrabBlend*55.0f-Swing)));
    }
    Super::TickComponent(DeltaSeconds,TickType,ThisTickFunction);
}

void URebootBotVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(OriginalMesh) && bVisualReady)
    {
        RemoveTickPrerequisiteComponent(OriginalMesh);
        OriginalMesh->SetVisibility(bOriginalVisible,false);OriginalMesh->SetCastShadow(bOriginalCastShadow);
        OriginalMesh->VisibilityBasedAnimTickOption=OriginalTickOption;
    }
    for (UStaticMeshComponent* Part:HiddenAccessories) if (IsValid(Part)) Part->SetVisibility(true,false);
    Super::EndPlay(EndPlayReason);
}
