#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "RebootTransferController.h"
#include "RebootTransferState.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/BodyInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransferTimingTest,"REBOOT.TransferShaft.BoardingStopsAndTravel",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTransferTimingTest::RunTest(const FString& Parameters)
{
    FRebootTransferState State;
    State.Advance(20);
    TestEqual(TEXT("An unpowered lift remains at its lower stop"),State.LiftFraction(),0.0f);
    State.PowerUp();
    State.Advance(1.9f);
    TestEqual(TEXT("Powering A gives a full boarding pause"),State.LiftFraction(),0.0f);
    State.Advance(.6f);
    TestTrue(TEXT("The lift travels after the two-second lower pause"),FMath::IsNearlyEqual(State.LiftFraction(),.25f,.001f));
    State.Advance(1.5f);
    TestTrue(TEXT("Two seconds of ascent reaches the upper stop"),FMath::IsNearlyEqual(State.LiftFraction(),1.0f,.001f));
    State.Advance(1.9f);
    TestTrue(TEXT("The upper stop provides a two-second disembarking pause"),FMath::IsNearlyEqual(State.LiftFraction(),1.0f,.001f));
    State.Advance(.6f);
    TestTrue(TEXT("The lift descends after the upper pause"),FMath::IsNearlyEqual(State.LiftFraction(),.75f,.001f));
    State.Advance(1.5f);
    TestTrue(TEXT("The lift returns to a lower boarding stop"),FMath::IsNearlyZero(State.LiftFraction(),.001f));
    const double Previous=State.CycleTime;
    State.Advance(-4);
    TestEqual(TEXT("Negative time cannot rewind the lift"),State.CycleTime,Previous);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransferOrderingTest,"REBOOT.TransferShaft.SwitchOrderAndPermanentRoute",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTransferOrderingTest::RunTest(const FString& Parameters)
{
    FRebootTransferState State;
    TestFalse(TEXT("B cannot bypass the missing A circuit"),State.UnlockRoute());
    TestFalse(TEXT("Attempting B early leaves the return path locked"),State.bRouteUnlocked);
    State.PowerUp();
    State.Advance(3);
    const double Previous=State.CycleTime;
    State.PowerUp();
    TestEqual(TEXT("Repeated A does not restart the moving lift"),State.CycleTime,Previous);
    TestTrue(TEXT("B unlocks the route after A"),State.UnlockRoute());
    State.Advance(1001);
    TestTrue(TEXT("The return path stays unlocked across later lift cycles"),State.bRouteUnlocked);
    TestTrue(TEXT("B leaves the lift and belt powered"),State.bPowered);
    TestTrue(TEXT("Repeated B remains safe"),State.UnlockRoute());
    return true;
}

namespace
{
struct FTransferTestWorld
{
    UWorld* World=nullptr;
    UStaticMesh* Cube=nullptr;
    APlayerController* Player=nullptr;
    ACharacter* Pawn=nullptr;
    ARebootTransferController* Gallery=nullptr;
    AStaticMeshActor* Lift=nullptr;
    AStaticMeshActor* LiftCollider=nullptr;
    AStaticMeshActor* ExitCollider=nullptr;
    AStaticMeshActor* ReturnCollider=nullptr;
    FActorSpawnParameters Spawn;

    ~FTransferTestWorld()
    {
        if (World)
        {
            World->EndPlay(EEndPlayReason::Quit);
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
    }
    AStaticMeshActor* Box(FVector Position,FVector Size,bool Collision=true)
    {
        AStaticMeshActor* Actor=World->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator,Spawn);
        UStaticMeshComponent* Mesh=Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
        Actor->SetActorScale3D(Size/100.0);
        return Actor;
    }
    AStaticMeshActor* Visual(UStaticMesh* Asset,FVector Position,float Yaw=0)
    {
        AStaticMeshActor* Actor=World->SpawnActor<AStaticMeshActor>(Position,FRotator(0,Yaw,0),Spawn);
        Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Actor->GetStaticMeshComponent()->SetStaticMesh(Asset);
        Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("NoCollision"));
        return Actor;
    }
    bool Create(FAutomationTestBase& Test)
    {
        const UWorld::InitializationValues Init=UWorld::InitializationValues()
            .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
            .EnableTraceCollision(true).CreateFXSystem(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!Test.TestNotNull(TEXT("Isolated transfer physics world is available"),World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
        UStaticMesh* LiftMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_2/Meshes/SM_lift_platform_300x300.SM_lift_platform_300x300"));
        UStaticMesh* SwitchMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_2/Meshes/SM_switch_pedestal.SM_switch_pedestal"));
        UStaticMesh* BeltMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_2/Meshes/SM_conveyor_belt_surface_360x164.SM_conveyor_belt_surface_360x164"));
        if (!Test.TestNotNull(TEXT("Cube is available"),Cube)||!Test.TestNotNull(TEXT("Lift mesh is available"),LiftMesh)
            ||!Test.TestNotNull(TEXT("Switch mesh is available"),SwitchMesh)||!Test.TestNotNull(TEXT("Belt mesh is available"),BeltMesh)) return false;
        Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Box(FVector(2900,2500,-10),FVector(2600,2200,20));
        Lift=Visual(LiftMesh,FVector(3550,2500,-35));
        LiftCollider=Box(FVector(3550,2500,-17.5),FVector(300,300,35));
        LiftCollider->AttachToActor(Lift,FAttachmentTransformRules::KeepWorldTransform);
        AStaticMeshActor* Belt=Visual(BeltMesh,FVector(3200,2500,308),180);
        Box(FVector(3200,2500,311),FVector(400,164,6));
        Box(FVector(2800,2500,301.5),FVector(400,300,25));
        AStaticMeshActor* A=Visual(SwitchMesh,FVector(3250,2050,0));
        AStaticMeshActor* B=Visual(SwitchMesh,FVector(2800,2580,314),90);
        Box(FVector(3250,2050,110),FVector(70,70,20));
        Box(FVector(2800,2580,424),FVector(70,70,20));
        AStaticMeshActor* ButtonA=Box(FVector(3250,2050,120),FVector(30,30,8),false);
        AStaticMeshActor* ButtonB=Box(FVector(2800,2580,434),FVector(30,30,8),false);
        AStaticMeshActor* Exit=Box(FVector(1730,2500,2),FVector(24,238,306),false);
        ExitCollider=Box(FVector(1730,2500,155),FVector(24,238,306));
        ExitCollider->AttachToActor(Exit,FAttachmentTransformRules::KeepWorldTransform);
        AStaticMeshActor* Return=Box(FVector(2620,2500,316),FVector(24,238,198.9),false);
        ReturnCollider=Box(FVector(2620,2500,415.45),FVector(24,238,198.9));
        ReturnCollider->AttachToActor(Return,FAttachmentTransformRules::KeepWorldTransform);
        Player=World->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Player->SetAsLocalPlayerController();
        Pawn=World->SpawnActor<ACharacter>(FVector(3250,1900,62.5),FRotator::ZeroRotator,Spawn);
        Pawn->GetCapsuleComponent()->SetCapsuleSize(33.373f,60.0f,true);
        Pawn->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
        Pawn->GetCharacterMovement()->bRunPhysicsWithNoController=true;
        Pawn->GetCharacterMovement()->bUpdateOnlyIfRendered=false;
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Player->Possess(Pawn);
        Gallery=World->SpawnActor<ARebootTransferController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Gallery->LiftActor=Lift;
        Gallery->LiftCollisionActor=LiftCollider;
        Gallery->BeltActor=Belt;
        Gallery->SwitchA=A;
        Gallery->ButtonA=ButtonA;
        Gallery->SwitchB=B;
        Gallery->ButtonB=ButtonB;
        Gallery->ExitDoor=Exit;
        Gallery->ReturnDoor=Return;
        World->InitializeActorsForPlay(FURL());
        World->UpdateWorldComponents(false,false);
        World->BeginPlay();
        if (!World->HasBegunPlay()) World->GetWorldSettings()->NotifyBeginPlay();
        Test.TestTrue(TEXT("The controller tick is registered"),Gallery->PrimaryActorTick.IsTickFunctionRegistered());
        Test.TestTrue(TEXT("The controller tick is enabled"),Gallery->IsActorTickEnabled());
        Test.TestTrue(TEXT("Character movement ticking is enabled"),Pawn->GetCharacterMovement()->IsComponentTickEnabled());
        Test.TestTrue(TEXT("Character movement ticking is registered"),Pawn->GetCharacterMovement()->PrimaryComponentTick.IsTickFunctionRegistered());
        Test.TestTrue(TEXT("The simulated player controls a local character"),Pawn->IsLocallyControlled());
        return true;
    }
    void Place(FVector Position,EMovementMode Mode=MOVE_Walking)
    {
        Pawn->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->SetMovementMode(Mode);
    }
    void Frames(int32 Count)
    {
        // Automation executes this loop inside one outer editor frame. Native
        // tick tasks deduplicate by GFrameCounter, so each simulated frame must
        // have its own counter, just as frames driven by the engine loop do.
        for (int32 Index=0; Index<Count; ++Index)
        {
            ++GFrameCounter;
            UWorld* PreviousWorld=GWorld;
            GWorld=World;
            World->Tick(LEVELTICK_All,1.0f/60.0f);
            GWorld=PreviousWorld;
        }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransferControlTest,"REBOOT.TransferShaft.AccessAndRouteGates",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTransferControlTest::RunTest(const FString& Parameters)
{
    FTransferTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Place(FVector(2950,2580,376.5));
    TestFalse(TEXT("Reaching B early cannot bypass A"),Scene.Gallery->CanUseSwitch(1));
    Scene.Gallery->ActivateSwitchB();
    TestFalse(TEXT("An early B attempt leaves both route gates locked"),Scene.Gallery->bRouteUnlocked);
    TestFalse(TEXT("A cannot be operated remotely"),Scene.Gallery->CanUseSwitch(0));
    Scene.Place(FVector(3250,1900,62.5));
    TestTrue(TEXT("A is reachable from its front display"),Scene.Gallery->CanUseSwitch(0));
    AStaticMeshActor* Wall=Scene.Box(FVector(3250,1960,115),FVector(120,20,200));
    TestFalse(TEXT("A cannot be operated through a wall"),Scene.Gallery->CanUseSwitch(0));
    Scene.Gallery->ActivateSwitchA();
    TestFalse(TEXT("Blocked A does not power the machinery"),Scene.Gallery->bPowered);
    Wall->SetActorEnableCollision(false);
    Scene.Gallery->ActivateSwitchA();
    TestTrue(TEXT("A powers the lift and belt"),Scene.Gallery->bPowered);
    Scene.Place(FVector(2950,2580,376.5));
    TestTrue(TEXT("Powered B is reachable from the upper landing"),Scene.Gallery->CanUseSwitch(1));
    Scene.Gallery->ActivateSwitchB();
    Scene.Gallery->Tick(1.2f);
    TestTrue(TEXT("B unlocks the return route"),Scene.Gallery->bRouteUnlocked);
    TestTrue(TEXT("The main exit opens fully"),Scene.Gallery->ExitOpenProgress>.99f);
    TestTrue(TEXT("The ramp barrier opens fully"),Scene.Gallery->ReturnOpenProgress>.99f);
    TestTrue(TEXT("Exit collision follows its sliding panel"),FMath::IsNearlyEqual(Scene.ExitCollider->GetActorLocation().Z,470.0,.1));
    TestTrue(TEXT("Ramp barrier collision follows its panel"),FMath::IsNearlyEqual(Scene.ReturnCollider->GetActorLocation().Z,620.45,.1));
    Scene.Gallery->Tick(20);
    TestTrue(TEXT("Both return gates remain open on later lift cycles"),Scene.Gallery->ExitOpenProgress>.99f&&Scene.Gallery->ReturnOpenProgress>.99f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransferRiderTest,"REBOOT.TransferShaft.NativeLiftRiderRoundTrip",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTransferRiderTest::RunTest(const FString& Parameters)
{
    FTransferTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Gallery->ActivateSwitchA();
    TestTrue(TEXT("A is powered before the rider simulation"),Scene.Gallery->bPowered);
    Scene.Place(FVector(3550,2500,62.5));
    float MaxDeck=0;
    float MaxPawn=0;
    float MaxFootError=0;
    for (int32 Frame=0; Frame<490; ++Frame)
    {
        Scene.Frames(1);
        const float Deck=Scene.Lift->GetActorLocation().Z+35;
        const float Feet=Scene.Pawn->GetActorLocation().Z-Scene.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        MaxDeck=FMath::Max(MaxDeck,Deck);
        MaxPawn=FMath::Max(MaxPawn,static_cast<float>(Scene.Pawn->GetActorLocation().Z));
        MaxFootError=FMath::Max(MaxFootError,FMath::Abs(Feet-Deck));
    }
    TestTrue(TEXT("The powered platform reaches its upper landing"),MaxDeck>313);
    TestTrue(TEXT("Native based movement carries the robot up with it"),MaxPawn>373);
    TestTrue(FString::Printf(TEXT("The rider stays on the deck through ascent and descent (max error %.3f cm)"),MaxFootError),MaxFootError<6);
    TestTrue(TEXT("The full cycle returns the platform to its lower stop"),FMath::Abs(Scene.Lift->GetActorLocation().Z+35)<.1);
    TestTrue(TEXT("The robot completes the return trip on the lower deck"),FMath::Abs(Scene.Pawn->GetActorLocation().Z-62.15)<3);
    TestTrue(TEXT("The collision proxy completes the same trip"),FMath::Abs(Scene.LiftCollider->GetActorLocation().Z+17.5)<.1);
    TestTrue(TEXT("The simulated world advances through the full trip"),Scene.World->GetTimeSeconds()>8);
    AddInfo(FString::Printf(TEXT("Rider simulation: world %.3fs, max deck %.3f, max pawn %.3f, max feet error %.3f, powered %d, blocked %d"),
        Scene.World->GetTimeSeconds(),MaxDeck,MaxPawn,MaxFootError,Scene.Gallery->bPowered,Scene.Gallery->bLiftObstructed));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransferBeltTest,"REBOOT.TransferShaft.GroundedConveyorTransport",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTransferBeltTest::RunTest(const FString& Parameters)
{
    FTransferTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Gallery->ActivateSwitchA();
    TestTrue(TEXT("A is powered before the conveyor simulation"),Scene.Gallery->bPowered);
    Scene.Place(FVector(3320,2500,376.5));
    Scene.Frames(10);
    const double Before=Scene.Pawn->GetActorLocation().X;
    Scene.Frames(30);
    const double Travel=Before-Scene.Pawn->GetActorLocation().X;
    TestTrue(TEXT("Conveyor test world ticks advance time"),Scene.World->GetTimeSeconds()>.6);
    AddInfo(FString::Printf(TEXT("Conveyor simulation: world %.3fs, travel %.3f, feet %.3f, movement mode %d, powered %d"),
        Scene.World->GetTimeSeconds(),Travel,Scene.Pawn->GetActorLocation().Z-60,static_cast<int32>(Scene.Pawn->GetCharacterMovement()->MovementMode),Scene.Gallery->bPowered));
    TestTrue(FString::Printf(TEXT("The powered belt carries a standing robot west (%.2f cm)"),Travel),Travel>40&&Travel<100);
    Scene.Pawn->GetCharacterMovement()->Velocity=FVector(100,0,0);
    Scene.Gallery->Tick(.05f);
    TestTrue(TEXT("Belt transport does not overwrite the robot walking velocity"),FMath::IsNearlyEqual(Scene.Pawn->GetCharacterMovement()->Velocity.X,100.0,.01));
    Scene.Place(FVector(3320,2500,500),MOVE_Falling);
    Scene.Gallery->Tick(.25f);
    TestTrue(TEXT("The belt does not pull an airborne robot"),FMath::IsNearlyEqual(Scene.Pawn->GetActorLocation().X,3320.0,.01));
    Scene.Place(FVector(3320,2500,62.5));
    Scene.Gallery->Tick(.25f);
    TestTrue(TEXT("The belt does not pull a robot on the floor beneath it"),FMath::IsNearlyEqual(Scene.Pawn->GetActorLocation().X,3320.0,.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransferObstructionTest,"REBOOT.TransferShaft.LiftHeadroomPauseAndResume",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTransferObstructionTest::RunTest(const FString& Parameters)
{
    FTransferTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    UClass* RobotClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/StackOBot/Blueprints/Character/BP_Bot.BP_Bot_C"));
    if (!TestNotNull(TEXT("Actual robot collision defaults are available"),RobotClass)) return false;
    const ACharacter* Defaults=RobotClass->GetDefaultObject<ACharacter>();
    UCapsuleComponent* Capsule=Scene.Pawn->GetCapsuleComponent();
    Capsule->DestroyPhysicsState();
    Capsule->GetBodyInstance()->CopyBodyInstancePropertiesFrom(Defaults->GetCapsuleComponent()->GetBodyInstance());
    Capsule->CreatePhysicsState();
    TestEqual(TEXT("The headroom regression uses the robot's inline Custom collision settings"),Capsule->GetCollisionProfileName(),FName(TEXT("Custom")));
    Scene.Gallery->ActivateSwitchA();
    TestTrue(TEXT("A is powered before the headroom simulation"),Scene.Gallery->bPowered);
    Scene.Place(FVector(3550,2500,62.5));
    AStaticMeshActor* Ceiling=Scene.Box(FVector(3550,2500,240),FVector(180,180,20));
    Scene.Frames(300);
    TestTrue(TEXT("Insufficient rider headroom pauses lift travel"),Scene.Gallery->bLiftObstructed);
    const double Paused=Scene.Lift->GetActorLocation().Z;
    Scene.Frames(60);
    TestTrue(TEXT("A blocked lift stays at its safe position"),FMath::Abs(Scene.Lift->GetActorLocation().Z-Paused)<.1);
    Ceiling->SetActorEnableCollision(false);
    Scene.Frames(120);
    TestFalse(TEXT("Removing the obstruction resumes travel"),Scene.Gallery->bLiftObstructed);
    TestTrue(TEXT("The lift resumes toward its upper landing without skipping the held travel"),Scene.Lift->GetActorLocation().Z>Paused+100);
    return true;
}

#endif
