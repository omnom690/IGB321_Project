#include "RebootCoreController.h"
#include "RebootGameInstance.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/Class.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
void CoreText(UTextRenderComponent* Component,const FString& Value)
{
    const FText Text=FText::FromString(Value);
    if (!Component->Text.EqualTo(Text)) Component->SetText(Text);
}
void CoreSignal(UMaterialInstanceDynamic* Material,FLinearColor Color,float Strength)
{
    if (!Material) return;
    Material->SetVectorParameterValue(TEXT("ArcColor"),Color);
    Material->SetScalarParameterValue(TEXT("ArcStrength"),Strength);
}
}

ARebootCoreController::ARebootCoreController()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));RootComponent->SetMobility(EComponentMobility::Movable);
    const auto Volume=[this](FName Name,FVector Position,FVector Extent)
    {
        UBoxComponent* Box=CreateDefaultSubobject<UBoxComponent>(Name);Box->SetupAttachment(RootComponent);
        Box->SetRelativeLocation(Position);Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);Box->SetGenerateOverlapEvents(false);return Box;
    };
    ElectricalVolume=Volume(TEXT("ElectricalVolume"),FVector(0,3710,12),FVector(150,240,12));
    PlateVolume=Volume(TEXT("PlateVolume"),FVector(0,2350,15),FVector(70,70,25));
    DoorwayVolume=Volume(TEXT("DoorwayVolume"),FVector(0,2190,90),FVector(125,80,90));
    FarSideVolume=Volume(TEXT("FarSideVolume"),FVector(0,2050,100),FVector(115,55,100));
    ConsolePadVolume=Volume(TEXT("ConsolePadVolume"),FVector(550,2580,70),FVector(120,120,100));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    const auto Instances=[this](FName Name)
    {
        UInstancedStaticMeshComponent* Mesh=CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        Mesh->SetupAttachment(RootComponent);Mesh->SetStaticMesh(Cube.Object);Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetGenerateOverlapEvents(false);Mesh->SetCastShadow(false);return Mesh;
    };
    Arcs=Instances(TEXT("Arcs"));
    for (int32 Index=0;Index<3;++Index) RelayCables.Add(Instances(*FString::Printf(TEXT("RelayCable%d"),Index)));
    const auto Label=[this](FName Name,FVector Position,float Yaw,float Size)
    {
        UTextRenderComponent* Text=CreateDefaultSubobject<UTextRenderComponent>(Name);Text->SetupAttachment(RootComponent);
        Text->SetRelativeLocation(Position);Text->SetRelativeRotation(FRotator(0,Yaw,0));Text->SetWorldSize(Size);
        Text->SetHorizontalAlignment(EHTA_Center);return Text;
    };
    for (int32 Index=0;Index<3;++Index) RelayPrompts.Add(Label(*FString::Printf(TEXT("RelayPrompt%d"),Index),FVector::ZeroVector,0,18));
    ConsolePrompt=Label(TEXT("ConsolePrompt"),FVector(550,2715,240),-90,20);
    GatePrompt=Label(TEXT("GatePrompt"),FVector(0,2220,260),90,16);
    ElectricalPrompt=Label(TEXT("ElectricalPrompt"),FVector(0,3450,250),-90,18);
    CompletionText=Label(TEXT("CompletionText"),FVector(550,2800,380),0,34);CompletionText->SetVisibility(false);
    SystemText=Label(TEXT("SystemText"),FVector(550,2800,310),0,21);SystemText->SetVisibility(false);
    HazardLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("HazardLight"));HazardLight->SetupAttachment(RootComponent);
    HazardLight->SetRelativeLocation(FVector(0,3710,150));HazardLight->SetAttenuationRadius(550);HazardLight->SetCastShadows(false);
    WarningAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("WarningAudio"));WarningAudio->SetupAttachment(RootComponent);
    WarningAudio->SetRelativeLocation(FVector(0,3710,70));WarningAudio->bAutoActivate=false;WarningAudio->bOverrideAttenuation=true;
    WarningAudio->AttenuationOverrides.bAttenuate=true;WarningAudio->AttenuationOverrides.AttenuationShapeExtents=FVector(300,0,0);
    WarningAudio->AttenuationOverrides.FalloffDistance=800;WarningAudio->SetVolumeMultiplier(.25f);
}

void ARebootCoreController::BeginPlay()
{
    Super::BeginPlay();
    for (AActor* Actor:{ShuttleActor.Get(),ShuttleCollisionActor.Get(),GateActor.Get(),PlateTopActor.Get(),BackgroundRotor.Get()}) MakeMovable(Actor);
    if (IsValid(ShuttleActor)) ShuttleNear=ShuttleActor->GetActorLocation();
    if (IsValid(ShuttleCollisionActor)) ShuttleBase=Cast<UPrimitiveComponent>(ShuttleCollisionActor->GetRootComponent());
    if (IsValid(GateActor)) GateClosed=GateActor->GetActorLocation();
    if (IsValid(PlateTopActor)) PlateRest=PlateTopActor->GetActorLocation();
    if (SignalMaterial)
    {
        ArcMaterial=UMaterialInstanceDynamic::Create(SignalMaterial,this);Arcs->SetMaterial(0,ArcMaterial);
        CoreMaterial=UMaterialInstanceDynamic::Create(SignalMaterial,this);
    }
    for (int32 Index=0;Index<3;++Index)
    {
        UMaterialInstanceDynamic* Material=SignalMaterial?UMaterialInstanceDynamic::Create(SignalMaterial,this):nullptr;
        RelayMaterials.Add(Material);if (Material) RelayCables[Index]->SetMaterial(0,Material);
        if (RelayActors.IsValidIndex(Index) && IsValid(RelayActors[Index]))
        {
            const FVector Front=RelayActors[Index]->GetActorTransform().TransformVectorNoScale(FVector(0,-1,0));
            RelayPrompts[Index]->SetWorldLocation(RelayActors[Index]->GetActorLocation()+Front*60+FVector(0,0,210));
            RelayPrompts[Index]->SetWorldRotation(Front.Rotation());
            if (UStaticMeshComponent* Mesh=RelayActors[Index]->FindComponentByClass<UStaticMeshComponent>())
                for (const FName Slot:Mesh->GetMaterialSlotNames())
                    if (Slot.ToString().EndsWith(TEXT("signal_green"))) Mesh->SetMaterial(Mesh->GetMaterialIndex(Slot),Material?static_cast<UMaterialInterface*>(Material):OffMaterial.Get());
        }
    }
    if (IsValid(ConsoleActor))
        if (UStaticMeshComponent* Mesh=ConsoleActor->FindComponentByClass<UStaticMeshComponent>())
            for (int32 Index=0;Index<3;++Index)
                for (const FName Slot:Mesh->GetMaterialSlotNames())
                    if (Slot.ToString().EndsWith(FString::Printf(TEXT("relay_status_%c"),'A'+Index)))
                        Mesh->SetMaterial(Mesh->GetMaterialIndex(Slot),RelayMaterials[Index]?static_cast<UMaterialInterface*>(RelayMaterials[Index].Get()):OffMaterial.Get());
    if (IsValid(CoreActor) && CoreMaterial)
        if (UStaticMeshComponent* Mesh=CoreActor->FindComponentByClass<UStaticMeshComponent>())
            for (const FName Slot:Mesh->GetMaterialSlotNames())
                if (Slot.ToString().EndsWith(TEXT("core_blue"))) Mesh->SetMaterial(Mesh->GetMaterialIndex(Slot),CoreMaterial);
    const TArray<TArray<FVector>> Paths={
        {FVector(450,2800,1),FVector(-650,2800,1),FVector(-1550,2800,1),FVector(-1810,2800,1)},
        {FVector(550,2870,1),FVector(100,3150,1),FVector(100,3950,1),FVector(0,4060,1)},
        {FVector(450,2740,1),FVector(-100,2350,1),FVector(-100,2010,1),FVector(-100,1770,1),FVector(0,1590,1)}};
    for (int32 Index=0;Index<3;++Index)
    {
        RelayCables[Index]->ClearInstances();
        for (int32 Segment=1;Segment<Paths[Index].Num();++Segment)
        {
            const FVector A=Paths[Index][Segment-1],B=Paths[Index][Segment],Delta=B-A;
            RelayCables[Index]->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::XAxisVector,Delta.GetSafeNormal()),(A+B)*.5,FVector(Delta.Size()/100,.025,.015)));
        }
    }
    Arcs->ClearInstances();
    for (int32 Strip=0;Strip<8;++Strip)
        for (int32 Segment=0;Segment<4;++Segment)
        {
            const FVector A(-150+Segment*75,3500+Strip*60,Segment%2?65:5);
            const FVector B(-75+Segment*75,3500+Strip*60,Segment%2?5:65);const FVector Delta=B-A;
            Arcs->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::XAxisVector,Delta.GetSafeNormal()),(A+B)*.5,FVector(Delta.Size()/100,.018,.018)));
        }
    RefreshPlayer();UpdateVisuals();
}

void ARebootCoreController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);RefreshPlayer();const float Step=FMath::Max(0.0f,DeltaSeconds);
    if (!State.bFinaleStarted && IsValid(PlayerController) && PlayerController->WasInputKeyJustPressed(EKeys::E))
        for (int32 Index=0;Index<3;++Index) if (CanActivateRelay(Index)) { ActivateRelay(Index);break; }
    const FBox Bounds=IsValid(Character)?Character->GetCapsuleComponent()->Bounds.GetBox():FBox(ForceInit);
    const bool Plate=IsPlateOccupied();const bool AtDoor=Bounds.IsValid && DoorwayVolume->Bounds.GetBox().Intersect(Bounds);
    const FVector Direction=GateCrossingDirection.GetSafeNormal2D();
    const double Radius=Bounds.IsValid?FVector::DotProduct(Bounds.GetExtent(),Direction.GetAbs()):0;
    const bool Crossed=Bounds.IsValid && IsValid(GateActor) && GateActor->GetActorLocation().Z>Bounds.Max.Z+2
        && FVector::DotProduct(Character->GetActorLocation()-GateClosed,Direction)>40+Radius
        && FarSideVolume->Bounds.GetBox().IsInside(Character->GetActorLocation());
    FRebootCoreState Candidate=State;
    Candidate.Advance(Step,Plate,Crossed,AtDoor,CanReboot(),bBoundRebootHeld || (IsValid(PlayerController)&&PlayerController->IsInputKeyDown(EKeys::E)));
    UpdateShuttle(Candidate);const bool Starting=!State.bFinaleStarted && Candidate.bFinaleStarted;State=Candidate;
    RelayMask=State.RelayMask;CompletedRelayCount=static_cast<int32>(State.RelayActive(0))+State.RelayActive(1)+State.RelayActive(2);
    bGateLatched=State.bGateLatched;GateCountdown=State.GateHoldRemaining;RebootHoldProgress=State.RebootHeldSeconds;
    bRebootStarted=State.bFinaleStarted;bFacilityRebootComplete=State.bComplete;FinaleProgress=static_cast<float>(State.FinaleElapsed/15.0);
    if (URebootGameInstance* Progress=Cast<URebootGameInstance>(GetGameInstance()))
    {
        if (State.AllRelaysActive()) Progress->CompleteSystem(2);
        ActiveSystemCount=Progress->GetCompletedSystemCount();
    }
    else ActiveSystemCount=0;
    if (Starting) StartFinale();
    UpdateGate(Step);UpdateRecovery(Step);UpdateFinale(Step);UpdateVisuals();
}

bool ARebootCoreController::CanReach(const AActor* Actor,FVector LocalTarget) const
{
    if (!IsValid(Actor) || !IsValid(Character) || !GetWorld()) return false;
    const FVector Target=Actor->GetActorTransform().TransformPosition(LocalTarget);
    if (FVector::DistSquared(Character->GetActorLocation(),Target)>FMath::Square(FMath::Max(1.0f,InteractionRange))) return false;
    FVector Eye;FRotator Rotation;Character->GetActorEyesViewPoint(Eye,Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CoreInteraction),false,Character);Query.AddIgnoredActor(this);FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query);
}
bool ARebootCoreController::CanActivateRelay(int32 Index) const
{
    if (Index<0 || Index>2 || RelayActors.Num()!=3 || State.bFinaleStarted || State.RelayActive(Index)
        || !IsValid(Character) || !Character->GetCharacterMovement()->IsMovingOnGround()
        || !IsValid(ReturnBridgeA) || !IsValid(ReturnBridgeC) || !IsValid(GateActor) || (Index==2 && !State.bGateLatched)) return false;
    if (Index==1 && ElectricalVolume->Bounds.GetBox().Intersect(Character->GetCapsuleComponent()->Bounds.GetBox())) return false;
    return CanReach(RelayActors[Index],FVector(0,-50,110));
}
void ARebootCoreController::ActivateRelay(int32 Index)
{
    if (!CanActivateRelay(Index)) return;
    State.ActivateRelay(Index);RelayMask=State.RelayMask;
    CompletedRelayCount=static_cast<int32>(State.RelayActive(0))+State.RelayActive(1)+State.RelayActive(2);
    if (URebootGameInstance* Progress=Cast<URebootGameInstance>(GetGameInstance()))
    {
        if (State.AllRelaysActive()) Progress->CompleteSystem(2);
        ActiveSystemCount=Progress->GetCompletedSystemCount();
    }
    UpdateVisuals();
}
bool ARebootCoreController::ArePreviousSystemsComplete() const
{
    const URebootGameInstance* Progress=Cast<URebootGameInstance>(GetGameInstance());
    return Progress && Progress->IsSystemComplete(0) && Progress->IsSystemComplete(1);
}
bool ARebootCoreController::CanReboot() const
{
    if (!ArePreviousSystemsComplete() || !State.AllRelaysActive() || State.bFinaleStarted || !IsValid(Character) || !IsValid(FinaleCameraActor)
        || !Character->GetCharacterMovement()->IsMovingOnGround() || !ConsolePadVolume->Bounds.GetBox().IsInside(Character->GetActorLocation())) return false;
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const double Feet=Capsule->GetComponentLocation().Z-Capsule->GetScaledCapsuleHalfHeight();
    return FMath::Abs(Feet-ConsolePadGround.Z)<10 && CanReach(ConsoleActor,FVector(0,-70,110));
}
void ARebootCoreController::SetRebootInteractionHeld(bool Held) { bBoundRebootHeld=Held; }
bool ARebootCoreController::IsPlateOccupied() const
{
    if (!GetWorld()) return false;FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CorePlate),false,this);TArray<FOverlapResult> Hits;
    return GetWorld()->OverlapMultiByObjectType(Hits,PlateVolume->GetComponentLocation(),FQuat::Identity,Objects,
        FCollisionShape::MakeBox(PlateVolume->GetScaledBoxExtent()),Query) && !Hits.IsEmpty();
}

void ARebootCoreController::UpdateShuttle(FRebootCoreState& Candidate)
{
    if (!IsValid(ShuttleActor) || !IsValid(ShuttleBase)) return;
    const FVector Target=ShuttleNear+ShuttleTravel*Candidate.ShuttleFraction(),Delta=Target-ShuttleActor->GetActorLocation();
    bool Rider=false;bShuttleObstructed=false;
    if (IsValid(Character))
    {
        const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();const FVector Position=Capsule->GetComponentLocation(),Deck=ShuttleActor->GetActorLocation();
        const double Feet=Position.Z-Capsule->GetScaledCapsuleHalfHeight();
        Rider=Character->GetCharacterMovement()->IsMovingOnGround() && FMath::Abs(Feet-(Deck.Z+35))<8
            && FMath::Abs(Position.X-Deck.X)<150 && FMath::Abs(Position.Y-Deck.Y)<100;
        if (!Delta.IsNearlyZero() && Rider)
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(CoreShuttleClearance),false,Character);Query.AddIgnoredActor(this);Query.AddIgnoredActor(ShuttleActor);
            TArray<AActor*> AttachedActors;ShuttleActor->GetAttachedActors(AttachedActors,true,true);for (AActor* Child:AttachedActors) Query.AddIgnoredActor(Child);
            FHitResult Hit;bShuttleObstructed=GetWorld()->SweepSingleByChannel(Hit,Position,Position+Delta,Capsule->GetComponentQuat(),Capsule->GetCollisionObjectType(),
                FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query,FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
        }
        else if (!Delta.IsNearlyZero() && Feet<Deck.Z+30)
        {
            const FBox A(Deck+FVector(-150,-100,0),Deck+FVector(150,100,35));
            const FBox B(Deck+Delta+FVector(-150,-100,0),Deck+Delta+FVector(150,100,35));
            bShuttleObstructed=(A+B).Intersect(Capsule->Bounds.GetBox());
        }
    }
    if (bShuttleObstructed) { Candidate.ShuttleCycleTime=State.ShuttleCycleTime;return; }
    if (Rider && !Delta.IsNearlyZero() && Character->GetMovementBaseObject()!=ShuttleBase.Get())
    {
        FMovementBaseInterfaceData Base=MovementBaseUtility::GetMovementBaseDataFromPhysicsOwner(ShuttleBase.Get());Character->SetBase(&Base);Character->GetCharacterMovement()->SaveBaseLocation();
    }
    ShuttleActor->SetActorLocation(Target);ShuttlePositionFraction=Candidate.ShuttleFraction();
}
void ARebootCoreController::UpdateGate(float DeltaSeconds)
{
    if (IsValid(GateActor))
    {
        GateActor->SetActorLocation(FMath::VInterpConstantTo(GateActor->GetActorLocation(),GateClosed+FVector(0,0,State.bGateRequestedOpen?315:0),DeltaSeconds,315));
        GateOpenProgress=FMath::Clamp(static_cast<float>((GateActor->GetActorLocation().Z-GateClosed.Z)/315),0.0f,1.0f);
    }
    if (IsValid(PlateTopActor)) PlateTopActor->SetActorLocation(FMath::VInterpConstantTo(PlateTopActor->GetActorLocation(),PlateRest-FVector(0,0,IsPlateOccupied()?6:0),DeltaSeconds,30));
}
void ARebootCoreController::ClearGrab()
{
    if (!IsValid(Character)) return;TArray<UPhysicsHandleComponent*> Handles;Character->GetComponents(Handles);
    const bool Holding=Handles.ContainsByPredicate([](const UPhysicsHandleComponent* Handle){return IsValid(Handle)&&IsValid(Handle->GetGrabbedComponent());});
    if (UFunction* Clear=Character->FindFunction(TEXT("Grab_Clear"));Holding&&Clear&&Clear->NumParms==0) Character->ProcessEvent(Clear,nullptr);
    for (UPhysicsHandleComponent* Handle:Handles) Handle->ReleaseComponent();
}
void ARebootCoreController::ResetRobot(bool Electrical)
{
    if (!IsValid(Character)) return;ClearGrab();Character->GetCharacterMovement()->StopMovementImmediately();
    FMovementBaseInterfaceData NoBase;Character->SetBase(&NoBase);
    const double Height=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (Character->TeleportTo(HubCheckpoint+FVector(0,0,Height),Character->GetActorRotation(),false,false))
    {
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);RecoveryCooldown=.75f;
        if (Electrical) ++HazardResetCount;else ++FallResetCount;
    }
}
void ARebootCoreController::UpdateRecovery(float DeltaSeconds)
{
    if (!IsValid(Character) || State.bFinaleStarted) return;
    const FVector Position=Character->GetActorLocation();const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    const double Feet=Position.Z-Capsule->GetScaledCapsuleHalfHeight();
    const bool Within=Position.X>-2200 && Position.X<1200 && Position.Y>1300 && Position.Y<4300;
    if (Within && Feet>-30) bOwnsCheckpoint=true;
    else if (Feet>-30 && Character->GetCharacterMovement()->IsMovingOnGround()) bOwnsCheckpoint=false;
    RecoveryCooldown=FMath::Max(0.0f,RecoveryCooldown-DeltaSeconds);
    if (RecoveryCooldown>0) return;
    if (State.ElectricalPhase()==ERebootCoreElectricalPhase::Live && ElectricalVolume->Bounds.GetBox().Intersect(Capsule->Bounds.GetBox())) ResetRobot(true);
    else if (bOwnsCheckpoint && Feet<-240) ResetRobot(false);
}
void ARebootCoreController::StartFinale()
{
    if (!IsValid(Character) || !IsValid(PlayerController)) return;ClearGrab();
    Character->GetCharacterMovement()->StopMovementImmediately();FMovementBaseInterfaceData NoBase;Character->SetBase(&NoBase);
    LockedPosition=ConsolePadGround+FVector(0,0,Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    LockedRotation=IsValid(ConsoleActor)?(ConsoleActor->GetActorLocation()-LockedPosition).Rotation():FRotator::ZeroRotator;LockedRotation.Pitch=0;LockedRotation.Roll=0;
    Character->SetActorLocation(LockedPosition,false,nullptr,ETeleportType::TeleportPhysics);Character->SetActorRotation(LockedRotation);
    Character->GetCharacterMovement()->DisableMovement();PlayerController->SetControlRotation(LockedRotation);
    PlayerController->SetIgnoreMoveInput(true);PlayerController->SetIgnoreLookInput(true);
    if (IsValid(FinaleCameraActor)) PlayerController->SetViewTargetWithBlend(FinaleCameraActor,2.0f);
    bRobotLocked=true;WarningAudio->Stop();
}
void ARebootCoreController::UpdateFinale(float DeltaSeconds)
{
    if (State.bFinaleStarted && IsValid(Character))
    {
        Character->GetCharacterMovement()->StopMovementImmediately();Character->SetActorLocation(LockedPosition,false,nullptr,ETeleportType::TeleportPhysics);
        Character->SetActorRotation(LockedRotation);
        if (IsValid(BackgroundRotor) && State.FinaleElapsed>2) BackgroundRotor->AddActorLocalRotation(FRotator(0,DeltaSeconds*60,0));
    }
    const float Normal=State.bFinaleStarted?FMath::Clamp(static_cast<float>(State.FinaleElapsed/6),0.0f,1.0f):0;
    for (AActor* Actor:EmergencyLights)
        if (IsValid(Actor))
            if (ULightComponent* Light=Actor->FindComponentByClass<ULightComponent>())
            {
                Light->SetLightColor(FMath::Lerp(FLinearColor(.7f,.06f,.025f),FLinearColor(.75f,.85f,1),Normal));
                Light->SetIntensity(FMath::Lerp(600.0f,3200.0f,Normal));
            }
}

void ARebootCoreController::UpdateVisuals()
{
    if (PreviousVisualMask!=State.RelayMask)
    {
        PreviousVisualMask=State.RelayMask;SetRouteActive(ReturnBridgeA,State.RelayActive(0));SetRouteActive(ReturnBridgeC,State.RelayActive(2));
        for (int32 Index=0;Index<RelayMaterials.Num();++Index)
            CoreSignal(RelayMaterials[Index],State.RelayActive(Index)?FLinearColor(.05f,1,.25f):FLinearColor(.015f,.02f,.025f),State.RelayActive(Index)?4:.15f);
    }
    const ERebootCoreElectricalPhase Phase=State.ElectricalPhase();bElectricalWarning=Phase==ERebootCoreElectricalPhase::Warning;bElectricalLive=Phase==ERebootCoreElectricalPhase::Live;
    ElectricalCycleSeconds=static_cast<float>(FMath::Fmod(State.Elapsed,4.0));Arcs->SetVisibility(bElectricalWarning||bElectricalLive);
    const FLinearColor ElectricColor=bElectricalWarning?FLinearColor(1,.35f,0):FLinearColor(.08f,.55f,1);
    CoreSignal(ArcMaterial,ElectricColor,bElectricalLive?8:bElectricalWarning?1.5f:0);
    HazardLight->SetIntensity(bElectricalLive?700:bElectricalWarning?200:0);HazardLight->SetLightColor(ElectricColor);
    if (bElectricalWarning&&!bWasWarning) PlayWarning();bWasWarning=bElectricalWarning;
    CoreSignal(CoreMaterial,State.bFinaleStarted?FLinearColor(.08f,.55f,1):FLinearColor(.6f,.035f,.01f),State.bFinaleStarted?4:.8f);
    for (int32 Index=0;Index<3;++Index)
    {
        CoreText(RelayPrompts[Index],State.RelayActive(Index)?FString::Printf(TEXT("RELAY %c ONLINE - RETURN OPEN"),'A'+Index):FString::Printf(TEXT("E: ACTIVATE RELAY %c"),'A'+Index));
        RelayPrompts[Index]->SetTextRenderColor(State.RelayActive(Index)?FColor(65,235,155):FColor(235,235,220));
    }
    CoreText(ConsolePrompt,State.bComplete?TEXT("FACILITY REBOOT COMPLETE"):State.bFinaleStarted?TEXT("REBOOT IN PROGRESS"):
        State.AllRelaysActive()?!ArePreviousSystemsComplete()?TEXT("RESTORE SYSTEMS 1 AND 2 FIRST\nSTART FROM LEVEL 1"):FString::Printf(TEXT("HOLD E: REBOOT  %.0f%%"),State.RebootHeldSeconds*100):FString::Printf(TEXT("ACTIVATE RELAYS A / B / C\n%d / 3 ONLINE - ANY ORDER"),CompletedRelayCount));
    ConsolePrompt->SetVisibility(!State.bFinaleStarted);
    CoreText(GatePrompt,State.bGateLatched?TEXT("C ACCESS LATCHED OPEN"):GateCountdown>0?FString::Printf(TEXT("C GATE: %.1fs"),GateCountdown):TEXT("C PLATE OPENS GATE\n6s AFTER LEAVING"));
    CoreText(ElectricalPrompt,State.RelayActive(1)?TEXT("B STRIP ISOLATED"):bElectricalWarning?TEXT("B WARNING - WAIT"):bElectricalLive?TEXT("B LIVE - STAY ON PAD"):TEXT("B DARK - CROSS NOW"));
    CompletionText->SetVisibility(State.bFinaleStarted);SystemText->SetVisibility(State.bFinaleStarted);
    CoreText(CompletionText,State.bComplete?TEXT("FACILITY REBOOT COMPLETE"):TEXT("RESTORING FACILITY..."));
    const URebootGameInstance* Progress=Cast<URebootGameInstance>(GetGameInstance());
    CoreText(SystemText,FString::Printf(TEXT("SYSTEM 1 %s | SYSTEM 2 %s | SYSTEM 3 %s"),
        Progress && Progress->IsSystemComplete(0)?TEXT("ONLINE"):TEXT("OFFLINE"),
        Progress && Progress->IsSystemComplete(1)?TEXT("ONLINE"):TEXT("OFFLINE"),
        Progress && Progress->IsSystemComplete(2)?TEXT("ONLINE"):TEXT("OFFLINE")));
    CompletionText->SetTextRenderColor(State.bComplete?FColor(65,235,155):FColor(235,240,255));
    SystemText->SetTextRenderColor(FColor(65,235,155));
    if (IsValid(FinaleCameraActor))
    {
        CompletionText->SetWorldRotation((FinaleCameraActor->GetActorLocation()-CompletionText->GetComponentLocation()).Rotation());
        SystemText->SetWorldRotation((FinaleCameraActor->GetActorLocation()-SystemText->GetComponentLocation()).Rotation());
    }
}
void ARebootCoreController::PlayWarning()
{
    WarningAudio->Stop();WarningWave=NewObject<USoundWaveProcedural>(this);constexpr int32 Rate=22050,Count=Rate/3;
    WarningWave->SetSampleRate(Rate);WarningWave->NumChannels=1;WarningWave->Duration=1.0f/3;
    TArray<int16> Samples;Samples.SetNumUninitialized(Count);
    for (int32 Index=0;Index<Count;++Index)
    {
        const float Time=static_cast<float>(Index)/Rate,Local=FMath::Fmod(Time,.18f);
        const float Envelope=Local<.12f?FMath::Min(1.0f,Local/.008f)*FMath::Min(1.0f,(.12f-Local)/.012f):0;
        Samples[Index]=static_cast<int16>(FMath::Sin(2*PI*880*Time)*Envelope*7000);
    }
    WarningWave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Count*sizeof(int16));WarningAudio->SetSound(WarningWave);WarningAudio->Play();
}
void ARebootCoreController::SetRouteActive(AActor* Actor,bool Active)
{
    if (!IsValid(Actor)) return;Actor->SetActorHiddenInGame(!Active);Actor->SetActorEnableCollision(Active);
    TArray<AActor*> AttachedActors;Actor->GetAttachedActors(AttachedActors);for (AActor* Child:AttachedActors) SetRouteActive(Child,Active);
}
void ARebootCoreController::RefreshPlayer()
{
    APlayerController* Current=UGameplayStatics::GetPlayerController(this,0);
    if (Current!=PlayerController)
    {
        if (IsValid(PlayerController)) RemoveTickPrerequisiteActor(PlayerController);PlayerController=Current;
        if (IsValid(PlayerController)) AddTickPrerequisiteActor(PlayerController);
    }
    ACharacter* CurrentCharacter=IsValid(PlayerController)?Cast<ACharacter>(PlayerController->GetPawn()):nullptr;
    if (CurrentCharacter!=Character)
    {
        if (IsValid(Character)) Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);Character=CurrentCharacter;
        if (IsValid(Character)) Character->GetCharacterMovement()->AddTickPrerequisiteActor(this);
    }
}
void ARebootCoreController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(Character)) Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);
    if (IsValid(PlayerController))
    {
        RemoveTickPrerequisiteActor(PlayerController);
        if (bRobotLocked) { PlayerController->SetIgnoreMoveInput(false);PlayerController->SetIgnoreLookInput(false); }
    }
    Super::EndPlay(EndPlayReason);
}
void ARebootCoreController::MakeMovable(AActor* Actor)
{
    if (!IsValid(Actor)) return;TArray<USceneComponent*> Components;Actor->GetComponents(Components);
    for (USceneComponent* Component:Components) Component->SetMobility(EComponentMobility::Movable);
    TArray<AActor*> AttachedActors;Actor->GetAttachedActors(AttachedActors);for (AActor* Child:AttachedActors) MakeMovable(Child);
}
