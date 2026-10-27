#include "RebootTransferController.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "UObject/ConstructorHelpers.h"

namespace
{
void TransferText(UTextRenderComponent* Component,const TCHAR* Value)
{
    const FText Text=FText::FromString(Value);
    if (!Component->Text.EqualTo(Text)) Component->SetText(Text);
}
}

ARebootTransferController::ARebootTransferController()
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
    for (int32 Index=0; Index<8; ++Index)
        BeltMarkers->AddInstance(FTransform(FQuat::Identity,FVector(3015+Index*50,2500,314.3),FVector(.07,1.5,.006)));
    const auto Label=[this](FName Name,FVector Position,float Yaw,const TCHAR* Value)
    {
        UTextRenderComponent* Text=CreateDefaultSubobject<UTextRenderComponent>(Name);
        Text->SetupAttachment(RootComponent);
        Text->SetRelativeLocation(Position);
        Text->SetRelativeRotation(FRotator(0,Yaw,0));
        Text->SetWorldSize(18);
        Text->SetHorizontalAlignment(EHTA_Center);
        Text->SetText(FText::FromString(Value));
        return Text;
    };
    PromptA=Label(TEXT("PromptA"),FVector(3250,2007,185),-90,TEXT("E: POWER LIFT + BELT"));
    PromptB=Label(TEXT("PromptB"),FVector(2843,2580,499),0,TEXT("POWER SWITCH A FIRST"));
    LiftPrompt=Label(TEXT("LiftPrompt"),FVector(3550,2325,170),-90,TEXT("LIFT OFFLINE"));
    ReturnPrompt=Label(TEXT("ReturnPrompt"),FVector(2640,2500,580),0,TEXT("SWITCH B OPENS RETURN"));
}

void ARebootTransferController::BeginPlay()
{
    Super::BeginPlay();
    for (AActor* Actor : {LiftActor.Get(),LiftCollisionActor.Get(),ButtonA.Get(),ButtonB.Get(),ExitDoor.Get(),ReturnDoor.Get()}) MakeMovable(Actor);
    if (IsValid(LiftActor)) LiftLowerLocation=LiftActor->GetActorLocation();
    if (IsValid(LiftCollisionActor)) LiftBase=Cast<UPrimitiveComponent>(LiftCollisionActor->GetRootComponent());
    if (IsValid(ExitDoor)) ExitClosedLocation=ExitDoor->GetActorLocation();
    if (IsValid(ReturnDoor)) ReturnClosedLocation=ReturnDoor->GetActorLocation();
    if (IsValid(ButtonA)) ButtonARest=ButtonA->GetActorLocation();
    if (IsValid(ButtonB)) ButtonBRest=ButtonB->GetActorLocation();
    if (BeltMarkerMaterial) BeltMarkers->SetMaterial(0,BeltMarkerMaterial);
    RefreshPlayer();
    UpdatePrompts();
}

void ARebootTransferController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshPlayer();
    const float Step=FMath::Max(0.0f,DeltaSeconds);
    if (IsValid(PlayerController) && PlayerController->WasInputKeyJustPressed(EKeys::E))
    {
        if (CanUseSwitch(0)) ActivateSwitchA();
        else if (CanUseSwitch(1)) ActivateSwitchB();
    }
    UpdateLift(Step);
    UpdateConveyor(Step);
    UpdateDoorsAndControls(Step);
    UpdatePrompts();
}

bool ARebootTransferController::CanUseSwitch(int32 SwitchIndex) const
{
    if (SwitchIndex<0 || SwitchIndex>1 || !IsValid(PlayerController) || !GetWorld()
        || !IsValid(LiftActor) || !IsValid(LiftCollisionActor) || !IsValid(BeltActor)
        || !IsValid(ExitDoor) || !IsValid(ReturnDoor)) return false;
    if (SwitchIndex==0?State.bPowered:(!State.bPowered || State.bRouteUnlocked)) return false;
    AActor* Switch=SwitchIndex==0?SwitchA.Get():SwitchB.Get();
    const APawn* Pawn=PlayerController->GetPawn();
    if (!IsValid(Switch) || !IsValid(Pawn)) return false;
    const FVector Target=Switch->GetActorTransform().TransformPosition(FVector(0,-40,110));
    if (FVector::DistSquared(Pawn->GetActorLocation(),Target)>FMath::Square(FMath::Max(1.0f,InteractionRange))) return false;
    FVector Eye; FRotator Rotation;
    Pawn->GetActorEyesViewPoint(Eye,Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TransferSwitch),false,Pawn);
    Query.AddIgnoredActor(this);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query);
}

void ARebootTransferController::ActivateSwitchA()
{
    if (!CanUseSwitch(0)) return;
    State.PowerUp();
    bPowered=true;
    UpdatePrompts();
}

void ARebootTransferController::ActivateSwitchB()
{
    if (!CanUseSwitch(1)) return;
    State.UnlockRoute();
    bRouteUnlocked=true;
    UpdatePrompts();
}

bool ARebootTransferController::IsLiftRider(const ACharacter* Character) const
{
    if (!IsValid(Character) || !IsValid(LiftActor) || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const FVector Position=Capsule->GetComponentLocation();
    const FVector Lift=LiftActor->GetActorLocation();
    const double Feet=Position.Z-Capsule->GetScaledCapsuleHalfHeight();
    return FMath::Abs(Feet-(Lift.Z+35))<8 && FMath::Abs(Position.X-Lift.X)<150 && FMath::Abs(Position.Y-Lift.Y)<150;
}

bool ARebootTransferController::IsLiftMoveBlocked(const FVector& Delta,const ACharacter* Rider) const
{
    if (Delta.IsNearlyZero() || !IsValid(TrackedCharacter) || !GetWorld()) return false;
    const UCapsuleComponent* Capsule=TrackedCharacter->GetCapsuleComponent();
    if (IsValid(Rider))
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(TransferLiftHeadroom),false,Rider);
        Query.AddIgnoredActor(this);
        Query.AddIgnoredActor(LiftActor);
        Query.AddIgnoredActor(LiftCollisionActor);
        FHitResult Hit;
        const FVector Start=Capsule->GetComponentLocation();
        // Custom collision responses on the robot need not have a registered profile name.
        return GetWorld()->SweepSingleByChannel(Hit,Start,Start+Delta,Capsule->GetComponentQuat(),Capsule->GetCollisionObjectType(),
            FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query,
            FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
    }
    if (Delta.Z<0)
    {
        const FVector Old=LiftActor->GetActorLocation();
        const FBox Sweep(Old+FVector(-150,-150,Delta.Z),Old+FVector(150,150,35));
        return Sweep.Intersect(Capsule->Bounds.GetBox());
    }
    return false;
}

void ARebootTransferController::UpdateLift(float DeltaSeconds)
{
    if (!IsValid(LiftActor) || !IsValid(LiftBase)) return;
    FRebootTransferState Candidate=State;
    Candidate.Advance(DeltaSeconds);
    const FVector Target=LiftLowerLocation+FVector(0,0,314*Candidate.LiftFraction());
    const FVector Delta=Target-LiftActor->GetActorLocation();
    ACharacter* Rider=IsLiftRider(TrackedCharacter)?TrackedCharacter.Get():nullptr;
    bLiftObstructed=IsLiftMoveBlocked(Delta,Rider);
    if (bLiftObstructed) return;
    if (IsValid(Rider) && !Delta.IsNearlyZero() && Rider->GetMovementBaseObject()!=LiftBase.Get())
    {
        FMovementBaseInterfaceData Base=MovementBaseUtility::GetMovementBaseDataFromPhysicsOwner(LiftBase.Get());
        Rider->SetBase(&Base);
        Rider->GetCharacterMovement()->SaveBaseLocation();
    }
    State=Candidate;
    LiftActor->SetActorLocation(Target);
    LiftPositionFraction=State.LiftFraction();
}

void ARebootTransferController::UpdateConveyor(float DeltaSeconds)
{
    if (!State.bPowered || !IsValid(BeltActor)) return;
    BeltDistance=FMath::Fmod(BeltDistance+FMath::Max(0.0f,BeltSpeed)*DeltaSeconds,400.0f);
    for (int32 Index=0; Index<8; ++Index)
    {
        const double X=3000+FMath::Fmod(415.0+Index*50-BeltDistance,400.0);
        BeltMarkers->UpdateInstanceTransform(Index,FTransform(FQuat::Identity,FVector(X,2500,314.3),FVector(.07,1.5,.006)),false,Index==7,true);
    }
    if (!IsValid(TrackedCharacter) || !TrackedCharacter->GetCharacterMovement()->IsMovingOnGround()) return;
    const UCapsuleComponent* Capsule=TrackedCharacter->GetCapsuleComponent();
    const FVector Position=Capsule->GetComponentLocation();
    const double Feet=Position.Z-Capsule->GetScaledCapsuleHalfHeight();
    FVector Center,Extent; BeltActor->GetActorBounds(false,Center,Extent);
    if (FMath::Abs(Feet-(Center.Z+Extent.Z))<=8 && Position.X>=Center.X-Extent.X-20
        && Position.X<=Center.X+Extent.X+20 && FMath::Abs(Position.Y-Center.Y)<=Extent.Y)
    {
        TrackedCharacter->AddActorWorldOffset(BeltActor->GetActorForwardVector()*FMath::Max(0.0f,BeltSpeed)*DeltaSeconds,true);
    }
}

void ARebootTransferController::UpdateDoorsAndControls(float DeltaSeconds)
{
    const auto Move=[DeltaSeconds](AActor* Actor,const FVector& Target,float Speed)
    {
        if (IsValid(Actor)) Actor->SetActorLocation(FMath::VInterpConstantTo(Actor->GetActorLocation(),Target,DeltaSeconds,Speed));
    };
    if (State.bPowered) Move(ButtonA,ButtonARest-FVector(0,0,4),20);
    if (State.bRouteUnlocked)
    {
        Move(ButtonB,ButtonBRest-FVector(0,0,4),20);
        Move(ExitDoor,ExitClosedLocation+FVector(0,0,315),315);
        Move(ReturnDoor,ReturnClosedLocation+FVector(0,0,205),205);
        if (IsValid(ExitDoor)) ExitOpenProgress=FMath::Clamp(static_cast<float>((ExitDoor->GetActorLocation().Z-ExitClosedLocation.Z)/315.0),0.0f,1.0f);
        if (IsValid(ReturnDoor)) ReturnOpenProgress=FMath::Clamp(static_cast<float>((ReturnDoor->GetActorLocation().Z-ReturnClosedLocation.Z)/205.0),0.0f,1.0f);
    }
}

void ARebootTransferController::UpdatePrompts()
{
    TransferText(PromptA,State.bPowered?TEXT("LIFT + BELT POWERED"):TEXT("E: POWER LIFT + BELT"));
    TransferText(PromptB,State.bRouteUnlocked?TEXT("EXIT + RETURN OPEN"):State.bPowered?TEXT("E: OPEN EXIT + RETURN"):TEXT("POWER SWITCH A FIRST"));
    TransferText(ReturnPrompt,State.bRouteUnlocked?TEXT("RETURN ROUTE OPEN"):TEXT("SWITCH B OPENS RETURN"));
    TransferText(LiftPrompt,!State.bPowered?TEXT("LIFT OFFLINE"):bLiftObstructed?TEXT("CLEAR LIFT / STEP TO CENTRE"):
        LiftPositionFraction<.001f?TEXT("LOWER STOP - BOARD"):LiftPositionFraction>.999f?TEXT("UPPER STOP - EXIT WEST"):TEXT("LIFT MOVING"));
    const FColor Green(65,235,155);
    if (State.bPowered) PromptA->SetTextRenderColor(Green);
    if (State.bRouteUnlocked) { PromptB->SetTextRenderColor(Green); ReturnPrompt->SetTextRenderColor(Green); }
    if (IsValid(LiftActor)) LiftPrompt->SetRelativeLocation(FVector(3550,2325,LiftActor->GetActorLocation().Z+205));
}

void ARebootTransferController::RefreshPlayer()
{
    APlayerController* Current=UGameplayStatics::GetPlayerController(this,0);
    if (Current!=PlayerController)
    {
        if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
        PlayerController=Current;
        if (IsValid(PlayerController)) AddTickPrerequisiteActor(PlayerController);
    }
    ACharacter* Character=IsValid(PlayerController)?Cast<ACharacter>(PlayerController->GetPawn()):nullptr;
    if (Character!=TrackedCharacter)
    {
        if (IsValid(TrackedCharacter)) TrackedCharacter->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);
        TrackedCharacter=Character;
        if (IsValid(TrackedCharacter)) TrackedCharacter->GetCharacterMovement()->AddTickPrerequisiteActor(this);
    }
}

void ARebootTransferController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(TrackedCharacter)) TrackedCharacter->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);
    if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
    Super::EndPlay(EndPlayReason);
}

void ARebootTransferController::MakeMovable(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    TArray<USceneComponent*> Components; Actor->GetComponents(Components);
    for (USceneComponent* Component:Components) Component->SetMobility(EComponentMobility::Movable);
    TArray<AActor*> Children; Actor->GetAttachedActors(Children);
    for (AActor* Child:Children) MakeMovable(Child);
}
