#include "RebootBridgeController.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interfaces/MovementBaseInterface.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "UObject/Class.h"
#include "UObject/ConstructorHelpers.h"

ARebootBridgeController::ARebootBridgeController()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PrePhysics;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);
    BeltMarkers=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BeltMarkers"));
    BeltMarkers->SetupAttachment(RootComponent);
    BeltMarkers->SetMobility(EComponentMobility::Movable);
    BeltMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BeltMarkers->SetGenerateOverlapEvents(false);
    BeltMarkers->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    BeltMarkers->SetStaticMesh(Cube.Object);
    for (int32 Index=0;Index<8;++Index)
        BeltMarkers->AddInstance(FTransform(FQuat::Identity,FVector(-385+Index*50,650,.3),FVector(.07,1.5,.006)));
    PowerPrompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("PowerPrompt"));
    PowerPrompt->SetupAttachment(RootComponent);
    PowerPrompt->SetRelativeLocation(FVector(-1000,-512,185));
    PowerPrompt->SetRelativeRotation(FRotator(0,-90,0));
    PowerPrompt->SetWorldSize(18);
    PowerPrompt->SetHorizontalAlignment(EHTA_Center);
    PowerPrompt->SetText(FText::FromString(TEXT("E: POWER SHUTTLE + BELT")));
    ShuttlePrompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("ShuttlePrompt"));
    ShuttlePrompt->SetupAttachment(RootComponent);
    ShuttlePrompt->SetRelativeLocation(FVector(-600,-310,170));
    ShuttlePrompt->SetRelativeRotation(FRotator(0,-90,0));
    ShuttlePrompt->SetWorldSize(18);
    ShuttlePrompt->SetHorizontalAlignment(EHTA_Center);
    ShuttlePrompt->SetText(FText::FromString(TEXT("SHUTTLE OFFLINE")));
    PowerLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("PowerLight"));
    PowerLight->SetupAttachment(RootComponent);
    PowerLight->SetRelativeLocation(FVector(-1000,-510,145));
    PowerLight->SetMobility(EComponentMobility::Movable);
    PowerLight->SetIntensity(100);
    PowerLight->SetAttenuationRadius(200);
    PowerLight->SetCastShadows(false);
}

void ARebootBridgeController::BeginPlay()
{
    Super::BeginPlay();
    MakeMovable(ShuttleActor);
    MakeMovable(ShuttleCollisionActor);
    MakeMovable(ButtonActor);
    if (IsValid(ShuttleActor)) ShuttleNearLocation=ShuttleActor->GetActorLocation();
    if (IsValid(ShuttleCollisionActor)) ShuttleBase=Cast<UPrimitiveComponent>(ShuttleCollisionActor->GetRootComponent());
    if (IsValid(ButtonActor)) ButtonRest=ButtonActor->GetActorLocation();
    if (MarkerMaterial) BeltMarkers->SetMaterial(0,MarkerMaterial);
    RefreshPlayer();
    UpdatePrompts();
}

void ARebootBridgeController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshPlayer();
    const float Step=FMath::Max(0.0f,DeltaSeconds);
    if (IsValid(PlayerController) && PlayerController->WasInputKeyJustPressed(EKeys::E) && CanActivatePower()) ActivatePower();
    UpdateShuttle(Step);
    UpdateConveyor(Step);
    UpdateRecovery(Step);
    if (State.bPowered && IsValid(ButtonActor))
        ButtonActor->SetActorLocation(FMath::VInterpConstantTo(ButtonActor->GetActorLocation(),ButtonRest-FVector(0,0,4),Step,20));
    UpdatePrompts();
}

bool ARebootBridgeController::CanActivatePower() const
{
    if (State.bPowered || !IsValid(PlayerController) || !IsValid(SwitchActor) || !IsValid(ShuttleActor)
        || !IsValid(ShuttleCollisionActor) || !IsValid(BeltActor) || !GetWorld()) return false;
    const APawn* Pawn=PlayerController->GetPawn();
    if (!IsValid(Pawn)) return false;
    const FVector Target=SwitchActor->GetActorTransform().TransformPosition(FVector(0,-40,110));
    if (FVector::DistSquared(Pawn->GetActorLocation(),Target)>FMath::Square(FMath::Max(1.0f,InteractionRange))) return false;
    FVector Eye;FRotator Rotation;
    Pawn->GetActorEyesViewPoint(Eye,Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BridgePowerSwitch),false,Pawn);
    Query.AddIgnoredActor(this);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query);
}

void ARebootBridgeController::ActivatePower()
{
    if (!CanActivatePower()) return;
    State.PowerUp();
    bPowered=true;
    UpdatePrompts();
}

bool ARebootBridgeController::IsRider() const
{
    if (!IsValid(Character) || !IsValid(ShuttleActor) || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const FVector Position=Capsule->GetComponentLocation();
    const FVector Shuttle=ShuttleActor->GetActorLocation();
    const double Feet=Position.Z-Capsule->GetScaledCapsuleHalfHeight();
    return FMath::Abs(Feet-(Shuttle.Z+35))<8 && FMath::Abs(Position.X-Shuttle.X)<150 && FMath::Abs(Position.Y-Shuttle.Y)<100;
}

void ARebootBridgeController::UpdateShuttle(float DeltaSeconds)
{
    if (!IsValid(ShuttleActor) || !IsValid(ShuttleBase)) return;
    FRebootBridgeState Candidate=State;
    Candidate.Advance(DeltaSeconds);
    const FVector Target=ShuttleNearLocation+ShuttleTravel*Candidate.ShuttleFraction();
    const FVector Delta=Target-ShuttleActor->GetActorLocation();
    const bool bRider=IsRider();
    bShuttleObstructed=false;
    if (bRider && !Delta.IsNearlyZero())
    {
        const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(BridgeShuttleClearance),false,Character);
        Query.AddIgnoredActor(this);
        Query.AddIgnoredActor(ShuttleActor);
        TArray<AActor*> Attached;
        ShuttleActor->GetAttachedActors(Attached,true,true);
        for (AActor* Child:Attached) Query.AddIgnoredActor(Child);
        FHitResult Hit;
        const FVector Start=Capsule->GetComponentLocation();
        bShuttleObstructed=GetWorld()->SweepSingleByProfile(Hit,Start,Start+Delta,Capsule->GetComponentQuat(),
            Capsule->GetCollisionProfileName(),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query);
    }
    if (bShuttleObstructed) return;
    if (bRider && !Delta.IsNearlyZero() && Character->GetMovementBaseObject()!=ShuttleBase.Get())
    {
        FMovementBaseInterfaceData Base=MovementBaseUtility::GetMovementBaseDataFromPhysicsOwner(ShuttleBase.Get());
        Character->SetBase(&Base);
        Character->GetCharacterMovement()->SaveBaseLocation();
    }
    State=Candidate;
    ShuttleActor->SetActorLocation(Target);
    ShuttlePositionFraction=State.ShuttleFraction();
}

void ARebootBridgeController::UpdateConveyor(float DeltaSeconds)
{
    if (!State.bPowered || !IsValid(BeltActor)) return;
    BeltDistance=FMath::Fmod(BeltDistance+FMath::Max(0.0f,BeltSpeed)*DeltaSeconds,400.0f);
    for (int32 Index=0;Index<8;++Index)
    {
        const double X=-400+FMath::Fmod(15.0+Index*50+BeltDistance,400.0);
        BeltMarkers->UpdateInstanceTransform(Index,FTransform(FQuat::Identity,FVector(X,650,.3),FVector(.07,1.5,.006)),false,Index==7,true);
    }
    if (!IsValid(Character) || !Character->GetCharacterMovement()->IsMovingOnGround()) return;
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const FVector Position=Capsule->GetComponentLocation();
    const double Feet=Position.Z-Capsule->GetScaledCapsuleHalfHeight();
    FVector Center,Extent;BeltActor->GetActorBounds(false,Center,Extent);
    if (FMath::Abs(Feet-(Center.Z+Extent.Z))<8 && Position.X>=Center.X-Extent.X-20
        && Position.X<=Center.X+Extent.X+20 && FMath::Abs(Position.Y-Center.Y)<=Extent.Y)
        Character->AddActorWorldOffset(BeltActor->GetActorForwardVector()*FMath::Max(0.0f,BeltSpeed)*DeltaSeconds,true);
}

void ARebootBridgeController::UpdateRecovery(float DeltaSeconds)
{
    if (!IsValid(Character)) return;
    const FVector Position=Character->GetActorLocation();
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const double Feet=Capsule->GetComponentLocation().Z-Capsule->GetScaledCapsuleHalfHeight();
    const bool bWithinSection=Position.X>-1300 && Position.X<1400 && FMath::Abs(Position.Y)<1100;
    if (bWithinSection && Feet>-30) bOwnsCheckpoint=true;
    else if (Feet>-30 && Character->GetCharacterMovement()->IsMovingOnGround()) bOwnsCheckpoint=false;
    if (State.bPowered && bWithinSection && Position.X>=300 && FMath::Abs(Feet)<8
        && Character->GetCharacterMovement()->IsMovingOnGround()) bSectionCompleted=true;
    RecoveryCooldown=FMath::Max(0.0f,RecoveryCooldown-DeltaSeconds);
    if (!bOwnsCheckpoint || Feet>-240 || RecoveryCooldown>0) return;
    if (UFunction* ClearGrab=Character->FindFunction(TEXT("Grab_Clear"));ClearGrab && ClearGrab->NumParms==0) Character->ProcessEvent(ClearGrab,nullptr);
    TArray<UPhysicsHandleComponent*> Handles;Character->GetComponents(Handles);
    for (UPhysicsHandleComponent* Handle:Handles) Handle->ReleaseComponent();
    Character->GetCharacterMovement()->StopMovementImmediately();
    FMovementBaseInterfaceData NoBase;
    Character->SetBase(&NoBase);
    const double Height=Position.Z-Feet;
    if (Character->TeleportTo(CheckpointGround+FVector(0,0,Height),Character->GetActorRotation(),false,false))
    {
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        ++FallResetCount;
        RecoveryCooldown=.75f;
    }
}

void ARebootBridgeController::UpdatePrompts()
{
    const auto Text=[](UTextRenderComponent* Component,const TCHAR* Value)
    {
        const FText Message=FText::FromString(Value);
        if (!Component->Text.EqualTo(Message)) Component->SetText(Message);
    };
    Text(PowerPrompt,State.bPowered?TEXT("SHUTTLE + BELT POWERED"):TEXT("E: POWER SHUTTLE + BELT"));
    Text(ShuttlePrompt,!State.bPowered?TEXT("SHUTTLE OFFLINE"):bShuttleObstructed?TEXT("CLEAR SHUTTLE ROUTE"):
        ShuttlePositionFraction<.001f?TEXT("ENTRY STOP - BOARD"):ShuttlePositionFraction>.999f?TEXT("REST PLATFORM - EXIT NORTH"):TEXT("SHUTTLE MOVING"));
    const FLinearColor Color=State.bPowered?FLinearColor(.15f,1,.4f):FLinearColor(1,.06f,.02f);
    PowerLight->SetLightColor(Color);
    if (State.bPowered) PowerPrompt->SetTextRenderColor(FColor(65,235,155));
    if (IsValid(ShuttleActor)) ShuttlePrompt->SetRelativeLocation(ShuttleActor->GetActorLocation()+FVector(0,-110,205));
}

void ARebootBridgeController::RefreshPlayer()
{
    APlayerController* Current=UGameplayStatics::GetPlayerController(this,0);
    if (Current!=PlayerController)
    {
        if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
        PlayerController=Current;
        if (IsValid(PlayerController)) AddTickPrerequisiteActor(PlayerController);
    }
    ACharacter* CurrentCharacter=IsValid(PlayerController)?Cast<ACharacter>(PlayerController->GetPawn()):nullptr;
    if (CurrentCharacter!=Character)
    {
        if (IsValid(Character)) Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);
        Character=CurrentCharacter;
        if (IsValid(Character)) Character->GetCharacterMovement()->AddTickPrerequisiteActor(this);
    }
}

void ARebootBridgeController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(Character)) Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);
    if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
    Super::EndPlay(EndPlayReason);
}

void ARebootBridgeController::MakeMovable(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    TArray<USceneComponent*> Components;Actor->GetComponents(Components);
    for (USceneComponent* Component:Components) Component->SetMobility(EComponentMobility::Movable);
    TArray<AActor*> Children;Actor->GetAttachedActors(Children);
    for (AActor* Child:Children) MakeMovable(Child);
}
