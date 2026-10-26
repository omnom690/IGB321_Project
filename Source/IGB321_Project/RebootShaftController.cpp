#include "RebootShaftController.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/OverlapResult.h"
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

namespace
{
void ShaftText(UTextRenderComponent* Component,const FString& Value)
{
    const FText Text=FText::FromString(Value);
    if (!Component->Text.EqualTo(Text)) Component->SetText(Text);
}
}

ARebootShaftController::ARebootShaftController()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PrePhysics;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);
    const auto Volume=[this](FName Name,FVector Position,FVector Extent)
    {
        UBoxComponent* Box=CreateDefaultSubobject<UBoxComponent>(Name);
        Box->SetupAttachment(RootComponent);Box->SetRelativeLocation(Position);Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);Box->SetGenerateOverlapEvents(false);
        return Box;
    };
    PlateVolume=Volume(TEXT("PlateVolume"),FVector(3000,2680,15),FVector(70,70,25));
    DoorwayVolume=Volume(TEXT("DoorwayVolume"),FVector(3000,2940,155),FVector(125,80,155));
    FarSideVolume=Volume(TEXT("FarSideVolume"),FVector(3000,3120,100),FVector(115,80,100));
    BeltMarkers=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BeltMarkers"));
    BeltMarkers->SetupAttachment(RootComponent);BeltMarkers->SetMobility(EComponentMobility::Movable);
    BeltMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);BeltMarkers->SetGenerateOverlapEvents(false);BeltMarkers->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    BeltMarkers->SetStaticMesh(Cube.Object);
    const auto Label=[this](FName Name,FVector Position,float Yaw,float Size)
    {
        UTextRenderComponent* Text=CreateDefaultSubobject<UTextRenderComponent>(Name);
        Text->SetupAttachment(RootComponent);Text->SetRelativeLocation(Position);Text->SetRelativeRotation(FRotator(0,Yaw,0));
        Text->SetWorldSize(Size);Text->SetHorizontalAlignment(EHTA_Center);return Text;
    };
    PromptA=Label(TEXT("PromptA"),FVector(3180,1750,185),-90,17);
    PromptB=Label(TEXT("PromptB"),FVector(3150,3680,185),-90,17);
    GatePrompt=Label(TEXT("GatePrompt"),FVector(3000,2910,375),-90,17);
    LiftPrompt=Label(TEXT("LiftPrompt"),FVector(2780,3720,170),0,17);
}

void ARebootShaftController::BeginPlay()
{
    Super::BeginPlay();
    State.GateHoldSeconds=GateHoldSeconds;
    for (AActor* Actor:{ButtonA.Get(),ButtonB.Get(),GateActor.Get(),PlateTopActor.Get(),LiftActor.Get(),LiftCollisionActor.Get(),CoreDoor.Get()}) MakeMovable(Actor);
    if (IsValid(LiftActor)) LiftLowerLocation=LiftActor->GetActorLocation();
    if (IsValid(LiftCollisionActor)) LiftBase=Cast<UPrimitiveComponent>(LiftCollisionActor->GetRootComponent());
    if (IsValid(GateActor)) GateClosedLocation=GateActor->GetActorLocation();
    if (IsValid(CoreDoor)) CoreClosedLocation=CoreDoor->GetActorLocation();
    if (IsValid(PlateTopActor)) PlateRestLocation=PlateTopActor->GetActorLocation();
    if (IsValid(ButtonA)) ButtonARest=ButtonA->GetActorLocation();
    if (IsValid(ButtonB)) ButtonBRest=ButtonB->GetActorLocation();
    BeltMarkers->ClearInstances();
    if (IsValid(BeltActor)) BeltMarkers->SetWorldTransform(BeltActor->GetActorTransform());
    if (MarkerMaterial) BeltMarkers->SetMaterial(0,MarkerMaterial);
    for (int32 Index=0;Index<8;++Index)
        BeltMarkers->AddInstance(FTransform(FQuat::Identity,FVector(-165+Index*45,0,6.3),FVector(.07,1.5,.006)));
    RefreshPlayer();UpdatePrompts();
}

void ARebootShaftController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);RefreshPlayer();
    const float Step=FMath::Max(0.0f,DeltaSeconds);
    if (IsValid(PlayerController) && PlayerController->WasInputKeyJustPressed(EKeys::E))
    {
        if (CanUseSwitch(0)) ActivateSwitchA();
        else if (CanUseSwitch(1)) ActivateSwitchB();
    }
    const FBox Bounds=IsValid(Character)?Character->GetCapsuleComponent()->Bounds.GetBox():FBox(ForceInit);
    bPlateOccupied=IsPlateOccupied();
    const bool AtDoor=Bounds.IsValid && DoorwayVolume->Bounds.GetBox().Intersect(Bounds);
    const bool Crossed=Bounds.IsValid && IsValid(GateActor) && GateActor->GetActorLocation().Z>Bounds.Max.Z+2
        && Bounds.Min.Y>2980 && FarSideVolume->Bounds.GetBox().IsInside(Character->GetActorLocation());
    FRebootShaftState Candidate=State;
    Candidate.Advance(Step,bPlateOccupied,Crossed,AtDoor);
    UpdateLift(Candidate);
    State=Candidate;
    bConveyorPowered=State.bConveyorPowered;bGateLatched=State.bGateLatched;bLiftPowered=State.bLiftPowered;
    GateCountdown=State.GateHoldRemaining;
    UpdateConveyor(Step);UpdateDoors(Step);UpdateRecovery(Step);UpdatePrompts();
}

bool ARebootShaftController::CanUseSwitch(int32 Index) const
{
    if (Index<0 || Index>1 || !IsValid(PlayerController) || !GetWorld() || !IsValid(BeltActor)
        || !IsValid(LiftActor) || !IsValid(LiftCollisionActor) || !IsValid(CoreDoor) || !IsValid(GateActor)) return false;
    if (Index==0?State.bConveyorPowered:(!State.bConveyorPowered || !State.bGateLatched || State.bLiftPowered)) return false;
    const AActor* Switch=Index==0?SwitchA.Get():SwitchB.Get();
    const APawn* Pawn=PlayerController->GetPawn();
    if (!IsValid(Switch) || !IsValid(Pawn)) return false;
    const FVector Target=Switch->GetActorTransform().TransformPosition(FVector(0,-40,110));
    if (FVector::DistSquared(Pawn->GetActorLocation(),Target)>FMath::Square(FMath::Max(1.0f,InteractionRange))) return false;
    FVector Eye;FRotator Rotation;Pawn->GetActorEyesViewPoint(Eye,Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ShaftSwitch),false,Pawn);Query.AddIgnoredActor(this);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query);
}

void ARebootShaftController::ActivateSwitchA()
{
    if (!CanUseSwitch(0)) return;
    State.PowerConveyor();bConveyorPowered=true;UpdatePrompts();
}
void ARebootShaftController::ActivateSwitchB()
{
    if (!CanUseSwitch(1)) return;
    State.PowerLift();bLiftPowered=State.bLiftPowered;UpdatePrompts();
}

bool ARebootShaftController::IsPlateOccupied() const
{
    if (!GetWorld()) return false;
    FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ShaftPlate),false,this);
    TArray<FOverlapResult> Hits;
    return GetWorld()->OverlapMultiByObjectType(Hits,PlateVolume->GetComponentLocation(),FQuat::Identity,Objects,
        FCollisionShape::MakeBox(PlateVolume->GetScaledBoxExtent()),Query) && !Hits.IsEmpty();
}

bool ARebootShaftController::IsLiftRider() const
{
    if (!IsValid(Character) || !IsValid(LiftActor) || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const FVector Position=Capsule->GetComponentLocation();const FVector Lift=LiftActor->GetActorLocation();
    const double Feet=Position.Z-Capsule->GetScaledCapsuleHalfHeight();
    return FMath::Abs(Feet-(Lift.Z+35))<8 && FMath::Abs(Position.X-Lift.X)<150 && FMath::Abs(Position.Y-Lift.Y)<150;
}

bool ARebootShaftController::IsLiftMoveBlocked(const FVector& Delta,bool Rider) const
{
    if (Delta.IsNearlyZero() || !IsValid(Character) || !IsValid(LiftActor) || !GetWorld()) return false;
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    if (Rider)
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ShaftLiftHeadroom),false,Character);
        Query.AddIgnoredActor(this);Query.AddIgnoredActor(LiftActor);
        TArray<AActor*> AttachedActors;LiftActor->GetAttachedActors(AttachedActors,true,true);
        for (AActor* Child:AttachedActors) Query.AddIgnoredActor(Child);
        FHitResult Hit;const FVector Start=Capsule->GetComponentLocation();
        // Imported robot capsules may use an inline Custom profile with no named
        // entry in CollisionProfile. Query the component's actual channel responses.
        return GetWorld()->SweepSingleByChannel(Hit,Start,Start+Delta,Capsule->GetComponentQuat(),Capsule->GetCollisionObjectType(),
            FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query,
            FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
    }
    if (Delta.Z<0)
    {
        const FVector Old=LiftActor->GetActorLocation();
        return FBox(Old+FVector(-150,-150,Delta.Z),Old+FVector(150,150,35)).Intersect(Capsule->Bounds.GetBox());
    }
    return false;
}

void ARebootShaftController::UpdateLift(FRebootShaftState& Candidate)
{
    if (!IsValid(LiftActor) || !IsValid(LiftBase)) return;
    const FVector Target=LiftLowerLocation+FVector(0,0,LiftTravelHeight*Candidate.LiftFraction());
    const FVector Delta=Target-LiftActor->GetActorLocation();
    const bool Rider=IsLiftRider();
    bLiftObstructed=IsLiftMoveBlocked(Delta,Rider);
    if (bLiftObstructed)
    {
        // A blocked lift pauses its own phase; the pressure-gate timer keeps running.
        Candidate.LiftCycleTime=State.LiftCycleTime;
        return;
    }
    if (Rider && !Delta.IsNearlyZero() && Character->GetMovementBaseObject()!=LiftBase.Get())
    {
        FMovementBaseInterfaceData Base=MovementBaseUtility::GetMovementBaseDataFromPhysicsOwner(LiftBase.Get());
        Character->SetBase(&Base);Character->GetCharacterMovement()->SaveBaseLocation();
    }
    LiftActor->SetActorLocation(Target);LiftPositionFraction=Candidate.LiftFraction();
}

void ARebootShaftController::UpdateConveyor(float DeltaSeconds)
{
    if (!State.bConveyorPowered || !IsValid(BeltActor)) return;
    BeltDistance=FMath::Fmod(BeltDistance+FMath::Max(0.0f,BeltSpeed)*DeltaSeconds,360.0f);
    for (int32 Index=0;Index<8;++Index)
        BeltMarkers->UpdateInstanceTransform(Index,FTransform(FQuat::Identity,
            FVector(-180+FMath::Fmod(15.0+Index*45+BeltDistance,360.0),0,6.3),FVector(.07,1.5,.006)),false,Index==7,true);
    if (!IsValid(Character) || !Character->GetCharacterMovement()->IsMovingOnGround()) return;
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();const FVector Position=Capsule->GetComponentLocation();
    const double Feet=Position.Z-Capsule->GetScaledCapsuleHalfHeight();
    const FVector Local=BeltActor->GetActorTransform().InverseTransformPosition(Position);
    const double Top=BeltActor->GetActorLocation().Z+6*BeltActor->GetActorScale3D().Z;
    if (FMath::Abs(Feet-Top)<8 && FMath::Abs(Local.X)<=200 && FMath::Abs(Local.Y)<=82)
        Character->AddActorWorldOffset(BeltActor->GetActorForwardVector()*FMath::Max(0.0f,BeltSpeed)*DeltaSeconds,true);
}

void ARebootShaftController::UpdateDoors(float DeltaSeconds)
{
    const auto Move=[DeltaSeconds](AActor* Actor,FVector Target,float Speed)
    {
        if (IsValid(Actor)) Actor->SetActorLocation(FMath::VInterpConstantTo(Actor->GetActorLocation(),Target,DeltaSeconds,Speed));
    };
    Move(GateActor,GateClosedLocation+FVector(0,0,State.bGateRequestedOpen?315:0),315);
    Move(CoreDoor,CoreClosedLocation+FVector(0,0,State.bLiftPowered?315:0),315);
    Move(PlateTopActor,PlateRestLocation-FVector(0,0,bPlateOccupied?6:0),30);
    if (State.bConveyorPowered) Move(ButtonA,ButtonARest-FVector(0,0,4),20);
    if (State.bLiftPowered) Move(ButtonB,ButtonBRest-FVector(0,0,4),20);
    if (IsValid(GateActor)) GateOpenProgress=FMath::Clamp(static_cast<float>((GateActor->GetActorLocation().Z-GateClosedLocation.Z)/315),0.0f,1.0f);
    if (IsValid(CoreDoor)) CoreDoorOpenProgress=FMath::Clamp(static_cast<float>((CoreDoor->GetActorLocation().Z-CoreClosedLocation.Z)/315),0.0f,1.0f);
}

void ARebootShaftController::UpdateRecovery(float DeltaSeconds)
{
    if (!IsValid(Character)) return;
    const FVector Position=Character->GetActorLocation();const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const double Feet=Capsule->GetComponentLocation().Z-Capsule->GetScaledCapsuleHalfHeight();
    const bool Within=Position.X>1500 && Position.X<4500 && Position.Y>1500 && Position.Y<4100;
    if (Within && Feet>-30) bOwnsCheckpoint=true;
    else if (Feet>-30 && Character->GetCharacterMovement()->IsMovingOnGround()) bOwnsCheckpoint=false;
    if (State.bLiftPowered && Position.X<1550 && Position.X>1200 && FMath::Abs(Position.Y-2800)<150
        && FMath::Abs(Feet)<8 && Character->GetCharacterMovement()->IsMovingOnGround()) bSectionCompleted=true;
    RecoveryCooldown=FMath::Max(0.0f,RecoveryCooldown-DeltaSeconds);
    if (!bOwnsCheckpoint || Feet>-240 || RecoveryCooldown>0) return;
    TArray<UPhysicsHandleComponent*> Handles;Character->GetComponents(Handles);
    const bool Holding=Handles.ContainsByPredicate([](const UPhysicsHandleComponent* Handle)
        {return IsValid(Handle) && IsValid(Handle->GetGrabbedComponent());});
    if (UFunction* Clear=Character->FindFunction(TEXT("Grab_Clear"));Holding && Clear && Clear->NumParms==0) Character->ProcessEvent(Clear,nullptr);
    for (UPhysicsHandleComponent* Handle:Handles) Handle->ReleaseComponent();
    Character->GetCharacterMovement()->StopMovementImmediately();
    FMovementBaseInterfaceData NoBase;Character->SetBase(&NoBase);
    const double Height=Position.Z-Feet;
    if (Character->TeleportTo(CheckpointGround+FVector(0,0,Height),Character->GetActorRotation(),false,false))
    {
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);++FallResetCount;RecoveryCooldown=.75f;
    }
}

void ARebootShaftController::UpdatePrompts()
{
    ShaftText(PromptA,State.bConveyorPowered?TEXT("CONVEYOR POWERED"):TEXT("E: POWER CONVEYOR"));
    ShaftText(PromptB,State.bLiftPowered?TEXT("LIFT + CORE DOOR POWERED"):
        !State.bConveyorPowered?TEXT("USE SWITCH A FIRST"):!State.bGateLatched?TEXT("CROSS ACCESS GATE FIRST"):TEXT("E: POWER LIFT + CORE DOOR"));
    ShaftText(GatePrompt,State.bGateLatched?TEXT("ACCESS GATE LATCHED"):
        !State.bConveyorPowered?TEXT("USE SWITCH A FIRST"):bPlateOccupied?TEXT("PLATE HELD - TIMER READY"):
        GateCountdown>0?FString::Printf(TEXT("GATE: %.1fs"),GateCountdown):TEXT("PLATE OPENS GATE\n6s AFTER LEAVING"));
    ShaftText(LiftPrompt,!State.bLiftPowered?TEXT("LIFT OFFLINE - USE B"):bLiftObstructed?TEXT("CLEAR LIFT / STEP TO CENTRE"):
        LiftPositionFraction<.001f?TEXT("LOWER STOP - BOARD"):LiftPositionFraction>.999f?TEXT("UPPER STOP - EXIT WEST"):TEXT("LIFT MOVING"));
    PromptA->SetTextRenderColor(State.bConveyorPowered?FColor(65,235,155):FColor(235,235,220));
    PromptB->SetTextRenderColor(State.bLiftPowered?FColor(65,235,155):FColor(235,235,220));
    LiftPrompt->SetTextRenderColor(State.bLiftPowered?FColor(65,235,155):FColor(255,130,65));
    if (IsValid(LiftActor)) LiftPrompt->SetWorldLocation(LiftActor->GetActorLocation()+FVector(180,0,205));
}

void ARebootShaftController::RefreshPlayer()
{
    APlayerController* Current=UGameplayStatics::GetPlayerController(this,0);
    if (Current!=PlayerController)
    {
        if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
        PlayerController=Current;if (IsValid(PlayerController)) AddTickPrerequisiteActor(PlayerController);
    }
    ACharacter* CurrentCharacter=IsValid(PlayerController)?Cast<ACharacter>(PlayerController->GetPawn()):nullptr;
    if (CurrentCharacter!=Character)
    {
        if (IsValid(Character)) Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);
        Character=CurrentCharacter;
        if (IsValid(Character)) Character->GetCharacterMovement()->AddTickPrerequisiteActor(this);
    }
}
void ARebootShaftController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(Character)) Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);
    if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
    Super::EndPlay(EndPlayReason);
}
void ARebootShaftController::MakeMovable(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    TArray<USceneComponent*> Components;Actor->GetComponents(Components);
    for (USceneComponent* Component:Components) Component->SetMobility(EComponentMobility::Movable);
    TArray<AActor*> Children;Actor->GetAttachedActors(Children);
    for (AActor* Child:Children) MakeMovable(Child);
}
