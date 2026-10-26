#include "RebootCrusherController.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "UObject/Class.h"

namespace
{
bool IntersectsCapsule(const FBox& Box,const UCapsuleComponent* Capsule)
{
    const FVector Center = Capsule->GetComponentLocation();
    const double Radius = Capsule->GetScaledCapsuleRadius();
    const double SpineHalf = Capsule->GetScaledCapsuleHalfHeight()-Radius;
    const double DX = FMath::Max3(Box.Min.X-Center.X,0.0,Center.X-Box.Max.X);
    const double DY = FMath::Max3(Box.Min.Y-Center.Y,0.0,Center.Y-Box.Max.Y);
    const double DZ = FMath::Max3(Box.Min.Z-(Center.Z+SpineHalf),0.0,(Center.Z-SpineHalf)-Box.Max.Z);
    return DX*DX+DY*DY+DZ*DZ<=Radius*Radius;
}
}

ARebootCrusherController::ARebootCrusherController()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);
    WaitingPocket = CreateDefaultSubobject<UBoxComponent>(TEXT("WaitingPocket"));
    WaitingPocket->SetupAttachment(RootComponent);
    WaitingPocket->SetRelativeLocation(FVector(100,435,70));
    WaitingPocket->SetBoxExtent(FVector(120,150,110));
    WaitingPocket->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaitingPocket->SetGenerateOverlapEvents(false);
    SwitchPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SwitchPrompt"));
    SwitchPrompt->SetupAttachment(RootComponent);
    SwitchPrompt->SetRelativeLocation(FVector(887,410,260));
    SwitchPrompt->SetRelativeRotation(FRotator(0,180,0));
    SwitchPrompt->SetWorldSize(18);
    SwitchPrompt->SetHorizontalAlignment(EHTA_Center);
    SwitchPrompt->SetText(FText::FromString(TEXT("E: STOP CRUSHERS\nOPEN EXIT")));
    for (int32 Index=0; Index<2; ++Index)
    {
        UTextRenderComponent* Label = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("CrusherPhase%d"),Index+1));
        Label->SetupAttachment(RootComponent);
        Label->SetRelativeLocation(FVector(-320+Index*800,-510,300));
        Label->SetRelativeRotation(FRotator(0,180,0));
        Label->SetWorldSize(18);
        Label->SetHorizontalAlignment(EHTA_Center);
        Label->SetText(FText::FromString(TEXT("WAIT FOR GREEN")));
        PhaseLabels.Add(Label);
    }
}

void ARebootCrusherController::BeginPlay()
{
    Super::BeginPlay();
    for (AActor* Head : CrusherHeads)
    {
        MakeMovable(Head);
        RaisedLocations.Add(IsValid(Head)?Head->GetActorLocation():FVector::ZeroVector);
    }
    MakeMovable(ExitDoor);
    MakeMovable(ButtonActor);
    for (AActor* Light : WarningLights) MakeMovable(Light);
    for (AActor* Beacon : WarningBeacons) MakeMovable(Beacon);
    if (IsValid(ExitDoor)) DoorClosedLocation = ExitDoor->GetActorLocation();
    if (IsValid(ButtonActor)) ButtonRestLocation = ButtonActor->GetActorLocation();
    RefreshPlayerController();
    UpdateWarnings();
}

void ARebootCrusherController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshPlayerController();
    APawn* Pawn = IsValid(PlayerController)?PlayerController->GetPawn():nullptr;
    if (IsValid(Pawn) && WaitingPocket->Bounds.GetBox().IsInside(Pawn->GetActorLocation())) bSafePocketReached = true;
    if (IsValid(PlayerController) && PlayerController->WasInputKeyJustPressed(EKeys::E) && CanInteractWithSwitch()) ActivateSafetySwitch();
    // Substeps preserve the descending head sweep even during a slow frame.
    float Remaining = FMath::Max(0.0f,DeltaSeconds);
    do
    {
        const float Step = FMath::Min(Remaining,0.05f);
        SimulateStep(Step,Pawn);
        Remaining -= Step;
    } while (Remaining>KINDA_SMALL_NUMBER);
    UpdateWarnings();
}

void ARebootCrusherController::SimulateStep(float DeltaSeconds,APawn* Pawn)
{
    State.Advance(DeltaSeconds);
    ResetCooldown = FMath::Max(0.0f,ResetCooldown-DeltaSeconds);
    for (int32 Index=0; Index<CrusherHeads.Num() && Index<RaisedLocations.Num(); ++Index)
    {
        AActor* Head = CrusherHeads[Index];
        if (!IsValid(Head)) continue;
        const FVector Previous = Head->GetActorLocation();
        FVector Position = RaisedLocations[Index]-FVector(0,0,270*(1-State.RaisedFraction(Index)));
        if (State.bStopped) Position = FMath::VInterpConstantTo(Previous,RaisedLocations[Index],DeltaSeconds,900);
        const bool bCrushing = !State.bStopped && (Position.Z<Previous.Z-0.001 || State.Phase(Index)==ERebootCrusherPhase::Closed);
        if (bCrushing && IsValid(Pawn) && ResetCooldown<=0)
        {
            const FBox Sweep(FVector(Position.X-110,Position.Y-125,FMath::Min(Previous.Z,Position.Z)),
                FVector(Position.X+110,Position.Y+125,FMath::Max(Previous.Z,Position.Z)+90));
            const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>();
            if (Capsule?IntersectsCapsule(Sweep,Capsule):Sweep.Intersect(PawnBounds(Pawn))) ResetCrushedPawn(Pawn);
        }
        Head->SetActorLocation(Position);
    }
    if (IsValid(ExitDoor) && State.bStopped)
    {
        const FVector Target = DoorClosedLocation+FVector(0,0,315);
        ExitDoor->SetActorLocation(FMath::VInterpConstantTo(ExitDoor->GetActorLocation(),Target,DeltaSeconds,315));
        ExitOpenProgress = FMath::Clamp(static_cast<float>((ExitDoor->GetActorLocation().Z-DoorClosedLocation.Z)/315.0),0.0f,1.0f);
    }
    if (IsValid(ButtonActor) && State.bStopped)
    {
        ButtonActor->SetActorLocation(FMath::VInterpConstantTo(ButtonActor->GetActorLocation(),ButtonRestLocation-FVector(0,0,4),DeltaSeconds,20));
    }
}

bool ARebootCrusherController::CanInteractWithSwitch() const
{
    if (State.bStopped || !IsValid(PlayerController) || !IsValid(SwitchActor) || !IsValid(ExitDoor)
        || CrusherHeads.Num()!=2 || !IsValid(CrusherHeads[0]) || !IsValid(CrusherHeads[1]) || !GetWorld()) return false;
    const APawn* Pawn = PlayerController->GetPawn();
    if (!IsValid(Pawn)) return false;
    const FVector Target = SwitchTarget();
    if (FVector::DistSquared(Pawn->GetActorLocation(),Target)>FMath::Square(FMath::Max(1.0f,InteractionRange))) return false;
    FVector Eye; FRotator View;
    Pawn->GetActorEyesViewPoint(Eye,View);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CrusherSafetySwitch),false,Pawn);
    Query.AddIgnoredActor(this);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query);
}

void ARebootCrusherController::ActivateSafetySwitch()
{
    if (!CanInteractWithSwitch()) return;
    State.StopPermanently();
    bCrushersStopped = true;
    SwitchPrompt->SetText(FText::FromString(TEXT("CRUSHERS STOPPED\nEXIT OPEN")));
    SwitchPrompt->SetTextRenderColor(FColor(65,235,155));
    UpdateWarnings();
}

float ARebootCrusherController::GetCrusherRaisedFraction(int32 Index) const
{
    if (!CrusherHeads.IsValidIndex(Index) || !RaisedLocations.IsValidIndex(Index) || !IsValid(CrusherHeads[Index])) return 0;
    return FMath::Clamp(static_cast<float>((CrusherHeads[Index]->GetActorLocation().Z-(RaisedLocations[Index].Z-270))/270.0),0.0f,1.0f);
}

void ARebootCrusherController::UpdateWarnings()
{
    for (int32 Index=0; Index<2; ++Index)
    {
        const ERebootCrusherPhase Phase = State.Phase(Index);
        const bool bWarning = Phase==ERebootCrusherPhase::Warning;
        const bool bGreen = State.bStopped || (Phase==ERebootCrusherPhase::Open && GetCrusherRaisedFraction(Index)>.99f);
        const bool bFlash = FMath::Fmod(State.Elapsed,0.25)<0.125;
        const FLinearColor Color = bGreen?FLinearColor(.12f,1,.35f):bWarning&&bFlash?FLinearColor(1,.6f,.02f):FLinearColor(1,.03f,.01f);
        UMaterialInterface* Material = bGreen?GreenMaterial.Get():bWarning&&bFlash?YellowMaterial.Get():RedMaterial.Get();
        if (WarningLights.IsValidIndex(Index) && IsValid(WarningLights[Index]))
        {
            if (UPointLightComponent* Light = WarningLights[Index]->FindComponentByClass<UPointLightComponent>()) Light->SetLightColor(Color);
        }
        if (Material && WarningBeacons.IsValidIndex(Index) && IsValid(WarningBeacons[Index]))
        {
            if (UStaticMeshComponent* Mesh = WarningBeacons[Index]->FindComponentByClass<UStaticMeshComponent>())
                if (Mesh->GetMaterial(0)!=Material) Mesh->SetMaterial(0,Material);
        }
        if (PhaseLabels.IsValidIndex(Index))
        {
            const FText Text = FText::FromString(State.bStopped?TEXT("SAFE - STOPPED"):bGreen?TEXT("GREEN - CROSS"):bWarning?TEXT("WARNING - CLOSING"):TEXT("STOP - WAIT"));
            if (!PhaseLabels[Index]->Text.EqualTo(Text)) PhaseLabels[Index]->SetText(Text);
            PhaseLabels[Index]->SetTextRenderColor(Color.ToFColor(true));
        }
    }
}

void ARebootCrusherController::ResetCrushedPawn(APawn* Pawn)
{
    if (UFunction* ClearGrab = Pawn->FindFunction(TEXT("Grab_Clear")); ClearGrab && ClearGrab->NumParms==0) Pawn->ProcessEvent(ClearGrab,nullptr);
    TArray<UPhysicsHandleComponent*> Handles;
    Pawn->GetComponents(Handles);
    for (UPhysicsHandleComponent* Handle : Handles) Handle->ReleaseComponent();
    if (ACharacter* Character = Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->StopMovementImmediately();
    const FBox Bounds = PawnBounds(Pawn);
    const double Height = Pawn->GetActorLocation().Z-Bounds.Min.Z;
    const FVector Ground = bSafePocketReached?FVector(100,435,2.5):FVector(-750,0,2.5);
    if (Pawn->TeleportTo(Ground+FVector(0,0,Height),Pawn->GetActorRotation(),false,false))
    {
        ++HazardResetCount;
        ResetCooldown = 0.75f;
    }
}

FVector ARebootCrusherController::SwitchTarget() const
{
    return IsValid(SwitchActor)?SwitchActor->GetActorTransform().TransformPosition(FVector(0,-40,110)):GetActorLocation();
}

FBox ARebootCrusherController::PawnBounds(const APawn* Pawn)
{
    if (const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>()) return Capsule->Bounds.GetBox();
    FVector Center,Extent; Pawn->GetActorBounds(true,Center,Extent);
    return FBox(Center-Extent,Center+Extent);
}

void ARebootCrusherController::RefreshPlayerController()
{
    APlayerController* Current = UGameplayStatics::GetPlayerController(this,0);
    if (Current==PlayerController) return;
    if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
    PlayerController = Current;
    if (IsValid(PlayerController)) AddTickPrerequisiteActor(PlayerController);
}

void ARebootCrusherController::MakeMovable(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    TArray<USceneComponent*> Components; Actor->GetComponents(Components);
    for (USceneComponent* Component : Components) Component->SetMobility(EComponentMobility::Movable);
    TArray<AActor*> Children; Actor->GetAttachedActors(Children);
    for (AActor* Child : Children) MakeMovable(Child);
}
