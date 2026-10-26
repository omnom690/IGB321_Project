#include "RebootElectricalController.h"

#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/Class.h"
#include "UObject/ConstructorHelpers.h"

ARebootElectricalController::ARebootElectricalController()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);
    const auto Volume=[this](FName Name,FVector Center,FVector Extent)
    {
        UBoxComponent* Box=CreateDefaultSubobject<UBoxComponent>(Name);
        Box->SetupAttachment(RootComponent);
        Box->SetRelativeLocation(Center);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Box->SetGenerateOverlapEvents(false);
        return Box;
    };
    ElectricalVolume=Volume(TEXT("ElectricalVolume"),FVector(2390,0,12),FVector(240,150,12));
    PlateVolume=Volume(TEXT("PlateVolume"),FVector(2850,0,15),FVector(70,70,25));
    DoorwayVolume=Volume(TEXT("DoorwayVolume"),FVector(3200,0,155),FVector(80,125,155));
    FarSideVolume=Volume(TEXT("FarSideVolume"),FVector(3370,0,100),FVector(90,115,100));
    Arcs=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Arcs"));
    Arcs->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Arcs->SetStaticMesh(Cube.Object);
    Arcs->SetMobility(EComponentMobility::Movable);
    Arcs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Arcs->SetGenerateOverlapEvents(false);
    Arcs->SetCastShadow(false);
    HazardLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("HazardLight"));
    HazardLight->SetupAttachment(RootComponent);
    HazardLight->SetMobility(EComponentMobility::Movable);
    HazardLight->SetRelativeLocation(FVector(2390,0,160));
    HazardLight->SetAttenuationRadius(550);
    HazardLight->SetCastShadows(false);
    const auto Prompt=[this](FName Name,FVector Position,float Size)
    {
        UTextRenderComponent* Text=CreateDefaultSubobject<UTextRenderComponent>(Name);
        Text->SetupAttachment(RootComponent);
        Text->SetRelativeLocation(Position);
        Text->SetRelativeRotation(FRotator(0,180,0));
        Text->SetWorldSize(Size);
        Text->SetHorizontalAlignment(EHTA_Center);
        return Text;
    };
    PhasePrompt=Prompt(TEXT("PhasePrompt"),FVector(2135,0,310),18);
    GatePrompt=Prompt(TEXT("GatePrompt"),FVector(3180,0,375),17);
    IsolationPrompt=Prompt(TEXT("IsolationPrompt"),FVector(3680,0,195),17);
    WarningAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("WarningAudio"));
    WarningAudio->SetupAttachment(RootComponent);
    WarningAudio->SetRelativeLocation(FVector(2390,0,70));
    WarningAudio->bAutoActivate=false;
    WarningAudio->bAllowSpatialization=true;
    WarningAudio->bOverrideAttenuation=true;
    WarningAudio->AttenuationOverrides.bAttenuate=true;
    WarningAudio->AttenuationOverrides.AttenuationShapeExtents=FVector(350,0,0);
    WarningAudio->AttenuationOverrides.FalloffDistance=1100;
    WarningAudio->SetVolumeMultiplier(.25f);
}

void ARebootElectricalController::BeginPlay()
{
    Super::BeginPlay();
    State.GateHoldSeconds=GateHoldSeconds;
    MakeMovable(GateActor);MakeMovable(ShaftGateActor);MakeMovable(PlateTopActor);MakeMovable(ButtonActor);
    if (IsValid(GateActor)) GateClosedLocation=GateActor->GetActorLocation();
    if (IsValid(ShaftGateActor)) ShaftClosedLocation=ShaftGateActor->GetActorLocation();
    if (IsValid(PlateTopActor)) PlateRestLocation=PlateTopActor->GetActorLocation();
    if (IsValid(ButtonActor)) ButtonRestLocation=ButtonActor->GetActorLocation();
    if (ArcMaterial)
    {
        ArcInstance=UMaterialInstanceDynamic::Create(ArcMaterial,this);
        Arcs->SetMaterial(0,ArcInstance);
    }
    Arcs->ClearInstances();
    for (int32 Strip=0;Strip<8;++Strip)
        for (int32 Segment=0;Segment<4;++Segment)
        {
            const double X=2180+60*Strip;
            const FVector A(X, -150+Segment*75, Segment%2?65:5);
            const FVector B(X, -75+Segment*75, Segment%2?5:65);
            const FVector Delta=B-A;
            Arcs->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::XAxisVector,Delta.GetSafeNormal()),
                (A+B)*.5,FVector(Delta.Size()/100,.018,.018)));
        }
    RefreshPlayerController();
    UpdateVisuals();
}

void ARebootElectricalController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshPlayerController();
    APawn* Pawn=IsValid(PlayerController)?PlayerController->GetPawn():nullptr;
    const FBox Bounds=IsValid(Pawn)?PawnBounds(Pawn):FBox(ForceInit);
    bPlateOccupied=IsPlateOccupied();
    const bool AtDoor=Bounds.IsValid && DoorwayVolume->Bounds.GetBox().Intersect(Bounds);
    const bool Crossed=Bounds.IsValid && IsValid(GateActor) && GateActor->GetActorLocation().Z>Bounds.Max.Z+2 && Bounds.Min.X>3240
        && FarSideVolume->Bounds.GetBox().IsInside(Pawn->GetActorLocation());
    State.Advance(DeltaSeconds,bPlateOccupied,Crossed,AtDoor);
    bGateLatched=State.bGateLatched;
    GateCountdown=State.GateHoldRemaining;
    const auto MoveGate=[DeltaSeconds,this](AActor* Actor,FVector Closed,bool Open)
    {
        if (!IsValid(Actor)) return 0.0f;
        const FVector Target=Closed+FVector(0,0,Open?315:0);
        Actor->SetActorLocation(FMath::VInterpConstantTo(Actor->GetActorLocation(),Target,DeltaSeconds,FMath::Max(1.0f,GateSpeed)));
        return FMath::Clamp(static_cast<float>((Actor->GetActorLocation().Z-Closed.Z)/315.0),0.0f,1.0f);
    };
    GateOpenProgress=MoveGate(GateActor,GateClosedLocation,State.bGateRequestedOpen);
    ShaftGateOpenProgress=MoveGate(ShaftGateActor,ShaftClosedLocation,State.bIsolated);
    if (IsValid(PlateTopActor)) PlateTopActor->SetActorLocation(FMath::VInterpConstantTo(
        PlateTopActor->GetActorLocation(),PlateRestLocation-FVector(0,0,bPlateOccupied?6:0),DeltaSeconds,30));
    if (IsValid(PlayerController) && PlayerController->WasInputKeyJustPressed(EKeys::E)) ActivateIsolation();
    if (IsValid(ButtonActor)) ButtonActor->SetActorLocation(FMath::VInterpConstantTo(
        ButtonActor->GetActorLocation(),ButtonRestLocation-FVector(0,0,bIsolated?4:0),DeltaSeconds,20));
    ResetCooldown=FMath::Max(0.0f,ResetCooldown-DeltaSeconds);
    if (IsValid(Pawn) && ResetCooldown<=0 && State.Phase()==ERebootElectricalPhase::Live
        && ElectricalVolume->Bounds.GetBox().Intersect(Bounds)) ResetPawn(Pawn);
    UpdateVisuals();
}

bool ARebootElectricalController::IsPlateOccupied() const
{
    if (!GetWorld()) return false;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ElectricalPlate),false,this);
    TArray<FOverlapResult> Hits;
    return GetWorld()->OverlapMultiByObjectType(Hits,PlateVolume->GetComponentLocation(),FQuat::Identity,Objects,
        FCollisionShape::MakeBox(PlateVolume->GetScaledBoxExtent()),Query) && !Hits.IsEmpty();
}

bool ARebootElectricalController::CanActivateIsolation() const
{
    if (bIsolated || !State.bGateLatched || !IsValid(SwitchActor) || !IsValid(ShaftGateActor)
        || !IsValid(PlayerController) || !GetWorld()) return false;
    const APawn* Pawn=PlayerController->GetPawn();
    if (!IsValid(Pawn)) return false;
    const FVector Target=SwitchActor->GetActorTransform().TransformPosition(FVector(0,-40,110));
    if (FVector::DistSquared(Pawn->GetActorLocation(),Target)>FMath::Square(FMath::Max(1.0f,InteractionRange))) return false;
    FVector Eye;FRotator View;Pawn->GetActorEyesViewPoint(Eye,View);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ElectricalIsolation),false,Pawn);
    Query.AddIgnoredActor(this);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query);
}

void ARebootElectricalController::ActivateIsolation()
{
    if (!CanActivateIsolation()) return;
    State.Isolate();bIsolated=State.bIsolated;
    UpdateVisuals();
}

void ARebootElectricalController::UpdateVisuals()
{
    const ERebootElectricalPhase Phase=State.Phase();
    bElectricalWarning=Phase==ERebootElectricalPhase::Warning;
    bElectricalLive=Phase==ERebootElectricalPhase::Live;
    CyclePhaseSeconds=static_cast<float>(FMath::Fmod(State.Elapsed,4.0));
    bIsolated=State.bIsolated;
    const FLinearColor Color=bElectricalWarning?FLinearColor(1.0f,.35f,0.0f):FLinearColor(.08f,.55f,1.0f);
    Arcs->SetVisibility(bElectricalWarning || bElectricalLive);
    HazardLight->SetIntensity(bElectricalLive?700:bElectricalWarning?200:0);
    HazardLight->SetLightColor(Color);
    if (ArcInstance)
    {
        ArcInstance->SetVectorParameterValue(TEXT("ArcColor"),Color);
        ArcInstance->SetScalarParameterValue(TEXT("ArcStrength"),bElectricalLive?8.0f:bElectricalWarning?1.5f:0.0f);
    }
    if (bElectricalWarning && !bWasWarning) PlayWarning();
    bWasWarning=bElectricalWarning;
    PhasePrompt->SetText(FText::FromString(bIsolated?TEXT("ISOLATED - SAFE"):
        bElectricalWarning?TEXT("WARNING - WAIT"):
        bElectricalLive?TEXT("LIVE - STAY ON PAD"):TEXT("DARK - CROSS NOW")));
    PhasePrompt->SetTextRenderColor(bElectricalLive?FColor(80,175,255):bElectricalWarning?FColor(255,175,35):FColor(65,235,155));
    GatePrompt->SetText(FText::FromString(bGateLatched?TEXT("GATE LATCHED OPEN"):bPlateOccupied?TEXT("PLATE HELD - TIMER READY"):
        GateCountdown>0?FString::Printf(TEXT("GATE: %.1fs"),GateCountdown):TEXT("PLATE OPENS GATE\n6s AFTER LEAVING")));
    IsolationPrompt->SetText(FText::FromString(bIsolated?TEXT("STRIP ISOLATED\nSHAFT OPEN"):TEXT("E: ISOLATE STRIP\nOPEN SHAFT")));
    IsolationPrompt->SetTextRenderColor(bIsolated?FColor(65,235,155):FColor(235,235,220));
}

void ARebootElectricalController::PlayWarning()
{
    // Small local two-tone warning, generated as PCM rather than another asset dependency.
    WarningAudio->Stop();
    WarningWave=NewObject<USoundWaveProcedural>(this);
    constexpr int32 SampleRate=22050;
    constexpr int32 Count=SampleRate/3;
    WarningWave->SetSampleRate(SampleRate);
    WarningWave->NumChannels=1;
    WarningWave->Duration=1.0f/3;
    TArray<int16> Samples;Samples.SetNumUninitialized(Count);
    for (int32 Index=0;Index<Count;++Index)
    {
        const float Time=static_cast<float>(Index)/SampleRate;
        const float Local=FMath::Fmod(Time,.18f);
        const float Envelope=Local<.12f?FMath::Min(1.0f,Local/.008f)*FMath::Min(1.0f,(.12f-Local)/.012f):0;
        Samples[Index]=static_cast<int16>(FMath::Sin(2*PI*880*Time)*Envelope*7000);
    }
    WarningWave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Count*sizeof(int16));
    WarningAudio->SetSound(WarningWave);
    WarningAudio->Play();
}

void ARebootElectricalController::ResetPawn(APawn* Pawn)
{
    TArray<UPhysicsHandleComponent*> Handles;Pawn->GetComponents(Handles);
    // Stack O Bot's cleanup assumes a running grab effect and audio component.
    // An empty-handed reset must not call that Blueprint event.
    const bool Holding=Handles.ContainsByPredicate([](const UPhysicsHandleComponent* Handle)
        {return IsValid(Handle) && IsValid(Handle->GetGrabbedComponent());});
    if (UFunction* Clear=Pawn->FindFunction(TEXT("Grab_Clear"));Holding && Clear && Clear->NumParms==0)
        Pawn->ProcessEvent(Clear,nullptr);
    for (UPhysicsHandleComponent* Handle:Handles) Handle->ReleaseComponent();
    if (ACharacter* Character=Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->StopMovementImmediately();
    const FBox Bounds=PawnBounds(Pawn);
    const double Height=Pawn->GetActorLocation().Z-Bounds.Min.Z;
    if (Pawn->TeleportTo(FVector(1950,0,2.5+Height),Pawn->GetActorRotation(),false,false))
    {
        if (ACharacter* Character=Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        ++HazardResetCount;ResetCooldown=.75f;
    }
}

FBox ARebootElectricalController::PawnBounds(const APawn* Pawn)
{
    if (const UCapsuleComponent* Capsule=Pawn->FindComponentByClass<UCapsuleComponent>()) return Capsule->Bounds.GetBox();
    FVector Center,Extent;Pawn->GetActorBounds(true,Center,Extent);return FBox(Center-Extent,Center+Extent);
}

void ARebootElectricalController::RefreshPlayerController()
{
    APlayerController* Current=UGameplayStatics::GetPlayerController(this,0);
    if (Current==PlayerController) return;
    if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);
    PlayerController=Current;
    if (IsValid(PlayerController)) AddTickPrerequisiteActor(PlayerController);
}

void ARebootElectricalController::MakeMovable(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    TArray<USceneComponent*> Components;Actor->GetComponents(Components);
    for (USceneComponent* Component:Components) Component->SetMobility(EComponentMobility::Movable);
    TArray<AActor*> Children;Actor->GetAttachedActors(Children);
    for (AActor* Child:Children) MakeMovable(Child);
}
