#include "RebootProcessingController.h"
#include "RebootGameInstance.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interfaces/MovementBaseInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "UObject/Class.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
void ProcessingText(UTextRenderComponent* Component,const TCHAR* Value)
{
    const FText Text=FText::FromString(Value);
    if (!Component->Text.EqualTo(Text)) Component->SetText(Text);
}
}

ARebootProcessingController::ARebootProcessingController()
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
    const auto Label=[this](FName Name,FVector Position,float Yaw,float Size)
    {
        UTextRenderComponent* Text=CreateDefaultSubobject<UTextRenderComponent>(Name);
        Text->SetupAttachment(RootComponent);
        Text->SetRelativeLocation(Position);
        Text->SetRelativeRotation(FRotator(0,Yaw,0));
        Text->SetWorldSize(Size);
        Text->SetHorizontalAlignment(EHTA_Center);
        return Text;
    };
    PromptA=Label(TEXT("PromptA"),FVector(950,1947,185),-90,17);
    PromptB=Label(TEXT("PromptB"),FVector(117,2620,499),180,17);
    TerminalPrompt=Label(TEXT("TerminalPrompt"),FVector(50,2735,585),-90,18);
    LiftPrompt=Label(TEXT("LiftPrompt"),FVector(800,2325,170),-90,17);
    ReturnPrompt=Label(TEXT("ReturnPrompt"),FVector(-175,2500,519),180,17);
}

void ARebootProcessingController::BeginPlay()
{
    Super::BeginPlay();
    for (AActor* Actor:{LiftActor.Get(),LiftCollisionActor.Get(),ButtonA.Get(),ButtonB.Get(),ReturnDoor.Get()}) MakeMovable(Actor);
    if (IsValid(LiftActor)) LiftLowerLocation=LiftActor->GetActorLocation();
    if (IsValid(LiftCollisionActor))
    {
        LiftBase=Cast<UPrimitiveComponent>(LiftCollisionActor->GetRootComponent());
        if (IsValid(LiftActor) && LiftCollisionActor->GetAttachParentActor()!=LiftActor.Get())
            LiftCollisionActor->AttachToActor(LiftActor,FAttachmentTransformRules::KeepWorldTransform);
    }
    if (IsValid(ReturnDoor)) ReturnClosedLocation=ReturnDoor->GetActorLocation();
    if (IsValid(ButtonA)) ButtonARest=ButtonA->GetActorLocation();
    if (IsValid(ButtonB)) ButtonBRest=ButtonB->GetActorLocation();
    BeltMarkers->ClearInstances();
    if (IsValid(BeltActor)) BeltMarkers->SetWorldTransform(BeltActor->GetActorTransform());
    if (MarkerMaterial) BeltMarkers->SetMaterial(0,MarkerMaterial);
    for (int32 Index=0;Index<8;++Index)
        BeltMarkers->AddInstance(FTransform(FQuat::Identity,FVector(-165+Index*45,0,6.3),FVector(.07,1.5,.006)));
    RefreshPlayer();UpdateVisuals();
}

void ARebootProcessingController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);RefreshPlayer();
    const float Step=FMath::Max(0.0f,DeltaSeconds);
    // Adjacent B and terminal controls must consume separate key presses.
    const bool EDown=IsValid(PlayerController) && PlayerController->IsInputKeyDown(EKeys::E);
    if (IsValid(PlayerController) && !bEWasDown && PlayerController->WasInputKeyJustPressed(EKeys::E))
    {
        if (CanUseSwitch(0)) ActivateSwitchA();
        else if (CanUseSwitch(1)) ActivateSwitchB();
        else if (CanUseTerminal()) ActivateTerminal();
    }
    bEWasDown=EDown;
    UpdateLift(Step);UpdateConveyor(Step);UpdateDoors(Step);UpdateRecovery(Step);UpdateVisuals();
    if (State.bSystemCompleted && !bTransitionStarted)
    {
        CompletionElapsed+=Step;
        if (bTransitionEnabled && !NextLevel.IsNone() && CompletionElapsed>=FMath::Max(.1f,TransitionDelay))
        {
            // Completion is written to the persistent game instance before travel.
            // OpenLevel is deferred until the completion feedback has been visible.
            bTransitionStarted=true;
            UGameplayStatics::OpenLevel(this,NextLevel);
        }
    }
}

bool ARebootProcessingController::BindingsReady() const
{
    return IsValid(LiftActor) && IsValid(LiftCollisionActor) && IsValid(LiftBase) && IsValid(BeltActor)
        && IsValid(SwitchA) && IsValid(SwitchB) && IsValid(TerminalActor) && IsValid(ReturnDoor);
}

bool ARebootProcessingController::CanReach(const AActor* Interactable,const AActor* TargetActor,FVector LocalTarget) const
{
    if (!IsValid(Interactable) || !IsValid(Character) || !GetWorld()
        || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
    const FVector Target=IsValid(TargetActor)?TargetActor->GetActorLocation():Interactable->GetActorTransform().TransformPosition(LocalTarget);
    if (FVector::DistSquared(Character->GetActorLocation(),Target)>FMath::Square(FMath::Max(1.0f,InteractionRange))) return false;
    const FVector Front=Interactable->GetActorTransform().TransformVectorNoScale(FVector(0,-1,0));
    if (FVector::DotProduct(Character->GetActorLocation()-Interactable->GetActorLocation(),Front)<10) return false;
    FVector Eye;FRotator Rotation;Character->GetActorEyesViewPoint(Eye,Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ProcessingInteraction),false,Character);
    Query.AddIgnoredActor(this);Query.AddIgnoredActor(Interactable);
    if (IsValid(TargetActor)) Query.AddIgnoredActor(TargetActor);
    TArray<AActor*> AttachedActors;Interactable->GetAttachedActors(AttachedActors,true,true);
    for (AActor* Child:AttachedActors) Query.AddIgnoredActor(Child);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query);
}

bool ARebootProcessingController::CanUseSwitch(int32 Index) const
{
    if (Index<0 || Index>1 || !BindingsReady() || State.bSystemCompleted || !IsValid(PlayerController)) return false;
    if (Index==0?State.bPowered:(!State.bPowered || State.bCircuitBEnabled)) return false;
    return Index==0?CanReach(SwitchA,SwitchATarget,FVector(0,-60,110)):
        CanReach(SwitchB,SwitchBTarget,FVector(0,-60,110));
}

void ARebootProcessingController::ActivateSwitchA()
{
    if (!CanUseSwitch(0)) return;
    State.PowerUp();bPowered=State.bPowered;UpdateVisuals();
}

void ARebootProcessingController::ActivateSwitchB()
{
    if (!CanUseSwitch(1)) return;
    State.EnableCircuitB();bCircuitBEnabled=State.bCircuitBEnabled;UpdateVisuals();
}

bool ARebootProcessingController::CanUseTerminal() const
{
    return BindingsReady() && State.bPowered && State.bCircuitBEnabled && !State.bSystemCompleted
        && IsValid(Cast<URebootGameInstance>(UGameplayStatics::GetGameInstance(this)))
        && CanReach(TerminalActor,TerminalTarget,FVector(0,-90,110));
}

void ARebootProcessingController::ActivateTerminal()
{
    if (!CanUseTerminal()) return;
    URebootGameInstance* Progress=Cast<URebootGameInstance>(UGameplayStatics::GetGameInstance(this));
    if (!IsValid(Progress) || !State.CompleteSystem()) return;
    Progress->CompleteSystem(1);
    bSystemCompleted=State.bSystemCompleted;CompletionElapsed=0;UpdateVisuals();
}

bool ARebootProcessingController::IsLiftRider() const
{
    if (!IsValid(Character) || !IsValid(LiftActor) || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const FVector Position=Capsule->GetComponentLocation();const FVector Lift=LiftActor->GetActorLocation();
    const double Feet=Position.Z-Capsule->GetScaledCapsuleHalfHeight();
    return FMath::Abs(Feet-(Lift.Z+35))<8 && FMath::Abs(Position.X-Lift.X)<150 && FMath::Abs(Position.Y-Lift.Y)<150;
}

bool ARebootProcessingController::IsLiftMoveBlocked(const FVector& Delta,bool Rider) const
{
    if (Delta.IsNearlyZero() || !IsValid(Character) || !IsValid(LiftActor) || !GetWorld()) return false;
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    if (Rider)
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ProcessingLiftHeadroom),false,Character);
        Query.AddIgnoredActor(this);Query.AddIgnoredActor(LiftActor);Query.AddIgnoredActor(LiftCollisionActor);
        TArray<AActor*> AttachedActors;LiftActor->GetAttachedActors(AttachedActors,true,true);
        for (AActor* Child:AttachedActors) Query.AddIgnoredActor(Child);
        FHitResult Hit;const FVector Start=Capsule->GetComponentLocation();
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

void ARebootProcessingController::UpdateLift(float DeltaSeconds)
{
    if (!IsValid(LiftActor) || !IsValid(LiftBase)) return;
    FRebootProcessingState Candidate=State;Candidate.Advance(DeltaSeconds);
    const FVector Target=LiftLowerLocation+FVector(0,0,LiftTravelHeight*Candidate.LiftFraction());
    const FVector Delta=Target-LiftActor->GetActorLocation();const bool Rider=IsLiftRider();
    bLiftObstructed=IsLiftMoveBlocked(Delta,Rider);
    if (bLiftObstructed) return;
    if (Rider && !Delta.IsNearlyZero() && Character->GetMovementBaseObject()!=LiftBase.Get())
    {
        FMovementBaseInterfaceData Base=MovementBaseUtility::GetMovementBaseDataFromPhysicsOwner(LiftBase.Get());
        Character->SetBase(&Base);Character->GetCharacterMovement()->SaveBaseLocation();
    }
    State=Candidate;LiftActor->SetActorLocation(Target);LiftPositionFraction=State.LiftFraction();
}

void ARebootProcessingController::UpdateConveyor(float DeltaSeconds)
{
    if (!State.bPowered || !IsValid(BeltActor)) return;
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

void ARebootProcessingController::UpdateDoors(float DeltaSeconds)
{
    const auto Move=[DeltaSeconds](AActor* Actor,FVector Target,float Speed)
    {
        if (IsValid(Actor)) Actor->SetActorLocation(FMath::VInterpConstantTo(Actor->GetActorLocation(),Target,DeltaSeconds,Speed));
    };
    if (State.bPowered) Move(ButtonA,ButtonARest-FVector(0,0,4),20);
    if (State.bCircuitBEnabled)
    {
        Move(ButtonB,ButtonBRest-FVector(0,0,4),20);
        Move(ReturnDoor,ReturnClosedLocation+FVector(0,0,ReturnDoorLiftHeight),ReturnDoorLiftHeight);
    }
    if (IsValid(ReturnDoor)) ReturnOpenProgress=FMath::Clamp(static_cast<float>((ReturnDoor->GetActorLocation().Z-ReturnClosedLocation.Z)/FMath::Max(1.0f,ReturnDoorLiftHeight)),0.0f,1.0f);
}

void ARebootProcessingController::UpdateRecovery(float DeltaSeconds)
{
    if (!IsValid(Character)) return;
    const FVector Position=Character->GetActorLocation();const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const double Feet=Capsule->GetComponentLocation().Z-Capsule->GetScaledCapsuleHalfHeight();
    const bool Within=Position.X>-1300 && Position.X<1250 && Position.Y>1400 && Position.Y<3600;
    if (Within && Feet>-30) bOwnsCheckpoint=true;
    else if (Feet>-30 && Character->GetCharacterMovement()->IsMovingOnGround()) bOwnsCheckpoint=false;
    RecoveryCooldown=FMath::Max(0.0f,RecoveryCooldown-DeltaSeconds);
    if (State.bSystemCompleted || !bOwnsCheckpoint || Feet>-240 || RecoveryCooldown>0) return;
    TArray<UPhysicsHandleComponent*> Handles;Character->GetComponents(Handles);
    const bool Holding=Handles.ContainsByPredicate([](const UPhysicsHandleComponent* Handle)
        {return IsValid(Handle) && IsValid(Handle->GetGrabbedComponent());});
    if (UFunction* Clear=Character->FindFunction(TEXT("Grab_Clear"));Holding && Clear && Clear->NumParms==0) Character->ProcessEvent(Clear,nullptr);
    for (UPhysicsHandleComponent* Handle:Handles) if (IsValid(Handle)) Handle->ReleaseComponent();
    Character->GetCharacterMovement()->StopMovementImmediately();
    FMovementBaseInterfaceData NoBase;Character->SetBase(&NoBase);
    const double Height=Position.Z-Feet;
    if (Character->TeleportTo(CheckpointGround+FVector(0,0,Height),Character->GetActorRotation(),false,false))
    {
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);++FallResetCount;RecoveryCooldown=.75f;
    }
}

void ARebootProcessingController::SetSignal(AActor* Actor,bool Active)
{
    if (!IsValid(Actor)) return;
    UMaterialInterface* Material=Active?GreenMaterial.Get():OffMaterial.Get();
    if (!IsValid(Material)) return;
    TArray<UStaticMeshComponent*> Meshes;Actor->GetComponents(Meshes);
    for (UStaticMeshComponent* Mesh:Meshes)
    {
        const TArray<FName> Names=Mesh->GetMaterialSlotNames();
        for (int32 Index=0;Index<Names.Num();++Index)
            if (Names[Index].ToString().Contains(TEXT("signal"),ESearchCase::IgnoreCase)) Mesh->SetMaterial(Index,Material);
    }
}

void ARebootProcessingController::UpdateVisuals()
{
    ProcessingText(PromptA,State.bPowered?TEXT("LIFT + BELT POWERED"):TEXT("E: POWER LIFT + BELT"));
    ProcessingText(PromptB,State.bCircuitBEnabled?TEXT("CIRCUIT B + RETURN ENABLED"):
        State.bPowered?TEXT("E: ENABLE CIRCUIT B + RETURN"):TEXT("USE SWITCH A FIRST"));
    ProcessingText(TerminalPrompt,State.bSystemCompleted?TEXT("SYSTEM 2 ONLINE - LOADING CORE"):
        State.bCircuitBEnabled?TEXT("E: RESTORE SYSTEM 2"):TEXT("ENABLE CIRCUITS A + B FIRST"));
    ProcessingText(ReturnPrompt,State.bCircuitBEnabled?TEXT("RETURN ROUTE OPEN"):TEXT("SWITCH B OPENS RETURN"));
    ProcessingText(LiftPrompt,!State.bPowered?TEXT("LIFT OFFLINE - USE A"):bLiftObstructed?TEXT("CLEAR LIFT / STEP TO CENTRE"):
        LiftPositionFraction<.001f?TEXT("LOWER STOP - BOARD"):LiftPositionFraction>.999f?TEXT("UPPER STOP - EXIT WEST"):TEXT("LIFT MOVING"));
    const FColor Green(65,235,155),Waiting(235,235,220);
    PromptA->SetTextRenderColor(State.bPowered?Green:Waiting);
    PromptB->SetTextRenderColor(State.bCircuitBEnabled?Green:Waiting);
    TerminalPrompt->SetTextRenderColor(State.bSystemCompleted?Green:Waiting);
    ReturnPrompt->SetTextRenderColor(State.bCircuitBEnabled?Green:Waiting);
    if (IsValid(LiftActor)) LiftPrompt->SetWorldLocation(LiftActor->GetActorLocation()+FVector(0,-175,205));
    const uint8 VisualState=static_cast<uint8>(State.bPowered)|(static_cast<uint8>(State.bCircuitBEnabled)<<1)|(static_cast<uint8>(State.bSystemCompleted)<<2);
    if (VisualState==PreviousVisualState) return;
    PreviousVisualState=VisualState;SetSignal(ButtonA,State.bPowered);SetSignal(ButtonB,State.bCircuitBEnabled);
    if (IsValid(TerminalActor))
    {
        if (UStaticMeshComponent* Mesh=TerminalActor->FindComponentByClass<UStaticMeshComponent>())
        {
            const auto Circuit=[Mesh,this](int32 Slot,bool Active)
            {
                UMaterialInterface* Material=Active?GreenMaterial.Get():OffMaterial.Get();
                if (Slot>=0 && Slot<Mesh->GetNumMaterials() && IsValid(Material)) Mesh->SetMaterial(Slot,Material);
            };
            Circuit(TerminalCircuitASlot,State.bPowered);Circuit(TerminalCircuitBSlot,State.bCircuitBEnabled);
        }
    }
}

void ARebootProcessingController::RefreshPlayer()
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

void ARebootProcessingController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(Character)) Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);
    if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
    Super::EndPlay(EndPlayReason);
}

void ARebootProcessingController::MakeMovable(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    TArray<USceneComponent*> Components;Actor->GetComponents(Components);
    for (USceneComponent* Component:Components) Component->SetMobility(EComponentMobility::Movable);
    TArray<AActor*> Children;Actor->GetAttachedActors(Children);
    for (AActor* Child:Children) MakeMovable(Child);
}
