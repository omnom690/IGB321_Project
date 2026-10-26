#include "RebootSteamGalleryController.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Class.h"

ARebootSteamGalleryController::ARebootSteamGalleryController()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);
    const auto Volume = [this](FName Name, FVector Center, FVector Extent)
    {
        UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(Name);
        Box->SetupAttachment(RootComponent);
        Box->SetRelativeLocation(Center);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Box->SetGenerateOverlapEvents(false);
        return Box;
    };
    PlateVolume = Volume(TEXT("PlateVolume"),FVector(2020,0,20),FVector(70,70,25));
    DoorwayVolume = Volume(TEXT("DoorwayVolume"),FVector(2315,0,155),FVector(50,125,155));
    FarSideVolume = Volume(TEXT("FarSideVolume"),FVector(2490,0,130),FVector(80,190,130));
    SafePadVolume = Volume(TEXT("SafePadVolume"),FVector(3175,300,70),FVector(120,105,100));
    SteamVolumes.Add(Volume(TEXT("SteamHazard1"),FVector(2825,0,120),FVector(80,190,120)));
    SteamVolumes.Add(Volume(TEXT("SteamHazard2"),FVector(3525,0,120),FVector(80,190,120)));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for (int32 Index=0; Index<24; ++Index)
    {
        UStaticMeshComponent* Puff = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("SteamPuff_%02d"),Index));
        Puff->SetupAttachment(RootComponent);
        Puff->SetStaticMesh(Sphere.Object);
        Puff->SetMobility(EComponentMobility::Movable);
        Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Puff->SetGenerateOverlapEvents(false);
        Puff->SetCastShadow(false);
        Puff->SetVisibility(false);
        SteamPuffs.Add(Puff);
    }
    for (int32 Index=0; Index<2; ++Index)
    {
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("VentLight%d"),Index+1));
        Light->SetupAttachment(RootComponent);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetRelativeLocation(FVector(2825+Index*700,0,80));
        Light->SetIntensity(120.0f);
        Light->SetAttenuationRadius(300.0f);
        Light->SetCastShadows(false);
        VentLights.Add(Light);
    }
    ValvePrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ValvePrompt"));
    ValvePrompt->SetupAttachment(RootComponent);
    ValvePrompt->SetRelativeLocation(FVector(3725,450,190));
    ValvePrompt->SetRelativeRotation(FRotator(0,180,0));
    ValvePrompt->SetWorldSize(19);
    ValvePrompt->SetHorizontalAlignment(EHTA_Center);
    ValvePrompt->SetText(FText::FromString(TEXT("E: ISOLATE STEAM")));
    GatePrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("GatePrompt"));
    GatePrompt->SetupAttachment(RootComponent);
    GatePrompt->SetRelativeLocation(FVector(2328,-450,110));
    GatePrompt->SetRelativeRotation(FRotator(0,180,0));
    GatePrompt->SetWorldSize(18);
    GatePrompt->SetHorizontalAlignment(EHTA_Center);
    GatePrompt->SetText(FText::FromString(TEXT("STEP ON PLATE\n6 SECOND HOLD")));
}

void ARebootSteamGalleryController::BeginPlay()
{
    Super::BeginPlay();
    State.GateHoldSeconds = GateHoldSeconds;
    State.SteamActiveSeconds = SteamActiveSeconds;
    State.SteamClearSeconds = SteamClearSeconds;
    MakeMovable(GateActor);
    MakeMovable(PlateTopActor);
    MakeMovable(ValvePivotActor);
    if (IsValid(GateActor)) GateClosedLocation = GateActor->GetActorLocation();
    if (IsValid(PlateTopActor)) PlateRestLocation = PlateTopActor->GetActorLocation();
    if (IsValid(ValvePivotActor)) ValveRestRotation = ValvePivotActor->GetActorRotation();
    for (UStaticMeshComponent* Puff : SteamPuffs)
    {
        if (SteamMaterial) Puff->SetMaterial(0,SteamMaterial);
    }
    RefreshPlayerController();
    UpdateSteamVisuals();
}

void ARebootSteamGalleryController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshPlayerController();
    APawn* Pawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
    const FBox Bounds = IsValid(Pawn) ? PawnBounds(Pawn) : FBox(ForceInit);
    bPlateOccupied = IsPlateOccupied();
    const bool bAtDoor = Bounds.IsValid && DoorwayVolume->Bounds.GetBox().Intersect(Bounds);
    const bool bBeyondGate = Bounds.IsValid && Bounds.Min.X>2380
        && FarSideVolume->Bounds.GetBox().IsInside(Pawn->GetActorLocation());
    State.Advance(DeltaSeconds,bPlateOccupied,bBeyondGate,bAtDoor);
    bGateLatched = State.bGateLatched;
    GateCountdown = State.GateHoldRemaining;

    if (IsValid(GateActor))
    {
        const FVector Target = GateClosedLocation+FVector(0,0,State.bGateRequestedOpen?315:0);
        GateActor->SetActorLocation(FMath::VInterpConstantTo(GateActor->GetActorLocation(),Target,DeltaSeconds,FMath::Max(1.0f,GateSpeed)));
        GateOpenProgress = FMath::Clamp(static_cast<float>((GateActor->GetActorLocation().Z-GateClosedLocation.Z)/315.0),0.0f,1.0f);
    }
    if (IsValid(PlateTopActor))
    {
        const FVector Target = PlateRestLocation-FVector(0,0,bPlateOccupied?6:0);
        PlateTopActor->SetActorLocation(FMath::VInterpConstantTo(PlateTopActor->GetActorLocation(),Target,DeltaSeconds,30));
    }
    if (IsValid(PlayerController) && PlayerController->WasInputKeyJustPressed(EKeys::E) && CanInteractWithValve())
    {
        ActivateIsolationValve();
    }
    ValveTurn = FMath::FInterpConstantTo(ValveTurn,bSteamIsolated?90.0f:0.0f,DeltaSeconds,90.0f);
    if (IsValid(ValvePivotActor))
    {
        const FQuat Turn(FVector::YAxisVector,FMath::DegreesToRadians(ValveTurn));
        ValvePivotActor->SetActorRotation(ValveRestRotation.Quaternion()*Turn);
    }
    GatePrompt->SetText(FText::FromString(bGateLatched?TEXT("GATE LATCHED OPEN"):
        bPlateOccupied?TEXT("PLATE HELD"):
        GateCountdown>0?FString::Printf(TEXT("GATE: %.1fs"),GateCountdown):TEXT("STEP ON PLATE\n6 SECOND HOLD")));
    ValvePrompt->SetText(FText::FromString(bSteamIsolated?TEXT("STEAM ISOLATED"):TEXT("E: ISOLATE STEAM")));
    ValvePrompt->SetTextRenderColor(bSteamIsolated?FColor(70,235,155):FColor(230,235,225));

    if (IsValid(Pawn) && State.bGateLatched && SafePadVolume->Bounds.GetBox().IsInside(Pawn->GetActorLocation()))
    {
        CheckpointGround = FVector(3175,300,2.5);
    }
    ResetCooldown = FMath::Max(0.0f,ResetCooldown-DeltaSeconds);
    if (IsValid(Pawn) && ResetCooldown<=0)
    {
        for (int32 Index=0; Index<SteamVolumes.Num(); ++Index)
        {
            if (State.IsSteamActive(Index) && SteamVolumes[Index]->Bounds.GetBox().Intersect(Bounds))
            {
                ResetPawnFromSteam(Pawn);
                break;
            }
        }
    }
    UpdateSteamVisuals();
}

bool ARebootSteamGalleryController::IsPlateOccupied() const
{
    if (!GetWorld()) return false;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SteamPlate),false,this);
    TArray<FOverlapResult> Hits;
    return GetWorld()->OverlapMultiByObjectType(Hits,PlateVolume->GetComponentLocation(),FQuat::Identity,
        Objects,FCollisionShape::MakeBox(PlateVolume->GetScaledBoxExtent()),Query) && !Hits.IsEmpty();
}

bool ARebootSteamGalleryController::CanInteractWithValve() const
{
    if (bSteamIsolated || !IsValid(ValvePivotActor) || !IsValid(PlayerController) || !GetWorld()) return false;
    const APawn* Pawn = PlayerController->GetPawn();
    if (!IsValid(Pawn)) return false;
    const FVector Target = ValvePivotActor->GetActorLocation();
    if (FVector::DistSquared(Pawn->GetActorLocation(),Target)>FMath::Square(FMath::Max(1.0f,InteractionRange))) return false;
    FVector Eye; FRotator View;
    Pawn->GetActorEyesViewPoint(Eye,View);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SteamValve),false,Pawn);
    Query.AddIgnoredActor(this);
    Query.AddIgnoredActor(ValvePivotActor);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query);
}

void ARebootSteamGalleryController::ActivateIsolationValve()
{
    if (!CanInteractWithValve()) return;
    State.IsolateSteam();
    bSteamIsolated = true;
    UpdateSteamVisuals();
}

bool ARebootSteamGalleryController::IsVentActive(int32 VentIndex) const
{
    return VentIndex>=0 && VentIndex<2 && State.IsSteamActive(VentIndex);
}

void ARebootSteamGalleryController::UpdateSteamVisuals()
{
    for (int32 Index=0; Index<SteamPuffs.Num(); ++Index)
    {
        const int32 Vent = Index/12;
        UStaticMeshComponent* Puff = SteamPuffs[Index];
        Puff->SetVisibility(State.IsSteamActive(Vent));
        const int32 Local = Index%12;
        const float T = FMath::Fmod(static_cast<float>(State.Elapsed)*0.7f+(Local%3)/3.0f,1.0f);
        const float S = 0.75f+T*0.25f;
        Puff->SetRelativeLocation(FVector(2825+Vent*700+FMath::Sin(T*6.28f+Local)*12,
            -135+(Local/3)*90,25+T*200));
        Puff->SetRelativeScale3D(FVector(1.8f,1.2f,1.5f)*S);
    }
    for (int32 Index=0; Index<VentLights.Num(); ++Index)
    {
        VentLights[Index]->SetLightColor(State.IsSteamActive(Index)?FLinearColor(1,.12f,.03f):FLinearColor(.15f,.8f,.4f));
    }
}

void ARebootSteamGalleryController::ResetPawnFromSteam(APawn* Pawn)
{
    // Let the robot clear its own grab state when its existing no-argument
    // cleanup event is present; keep the native handle fallback for other pawns.
    if (UFunction* ClearGrab = Pawn->FindFunction(TEXT("Grab_Clear")); ClearGrab && ClearGrab->NumParms==0)
    {
        Pawn->ProcessEvent(ClearGrab,nullptr);
    }
    TArray<UPhysicsHandleComponent*> Handles;
    Pawn->GetComponents(Handles);
    for (UPhysicsHandleComponent* Handle : Handles) Handle->ReleaseComponent();
    if (ACharacter* Character = Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->StopMovementImmediately();
    const FBox Bounds = PawnBounds(Pawn);
    const float CenterAboveFeet = static_cast<float>(Pawn->GetActorLocation().Z-Bounds.Min.Z);
    if (Pawn->TeleportTo(CheckpointGround+FVector(0,0,CenterAboveFeet),Pawn->GetActorRotation(),false,false))
    {
        ++HazardResetCount;
        ResetCooldown = 0.75f;
    }
}

FBox ARebootSteamGalleryController::PawnBounds(const APawn* Pawn)
{
    if (const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>()) return Capsule->Bounds.GetBox();
    FVector Center,Extent;
    Pawn->GetActorBounds(true,Center,Extent);
    return FBox(Center-Extent,Center+Extent);
}

void ARebootSteamGalleryController::RefreshPlayerController()
{
    APlayerController* Current = UGameplayStatics::GetPlayerController(this,0);
    if (Current == PlayerController) return;
    if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
    PlayerController = Current;
    if (IsValid(PlayerController)) AddTickPrerequisiteActor(PlayerController);
}

void ARebootSteamGalleryController::MakeMovable(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    TArray<USceneComponent*> Components;
    Actor->GetComponents(Components);
    for (USceneComponent* Component : Components) Component->SetMobility(EComponentMobility::Movable);
    TArray<AActor*> Children;
    Actor->GetAttachedActors(Children);
    for (AActor* Child : Children) MakeMovable(Child);
}
