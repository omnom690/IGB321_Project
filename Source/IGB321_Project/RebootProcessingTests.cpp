#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "RebootProcessingController.h"
#include "RebootProcessingState.h"
#include "RebootGameInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/BodyInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessingSequenceTest,"REBOOT.ProcessingControl.CircuitsAndAutomaticBoardingCycle",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessingSequenceTest::RunTest(const FString& Parameters)
{
    FRebootProcessingState State;
    State.Advance(60);
    TestEqual(TEXT("Unpowered processing machinery stays at its lower stop"),State.LiftFraction(),0.0f);
    TestFalse(TEXT("Circuit B cannot skip A"),State.EnableCircuitB());
    TestFalse(TEXT("The terminal cannot skip both circuits"),State.CompleteSystem());
    TestTrue(TEXT("The first valid A operation powers machinery"),State.PowerUp());
    State.Advance(1.9f);
    TestEqual(TEXT("A starts a full two-second boarding stop"),State.LiftFraction(),0.0f);
    State.Advance(.6f);
    TestTrue(TEXT("The lift then rises over two seconds"),FMath::IsNearlyEqual(State.LiftFraction(),.25f,.001f));
    const double PoweredPhase=State.CycleTime;
    TestFalse(TEXT("Repeated A does not retrigger power"),State.PowerUp());
    TestEqual(TEXT("Repeated A does not restart lift travel"),State.CycleTime,PoweredPhase);
    TestFalse(TEXT("Powering A alone cannot complete System 2"),State.CompleteSystem());
    State.Advance(1.5f);
    TestTrue(TEXT("The upper landing is reached after two seconds of travel"),FMath::IsNearlyEqual(State.LiftFraction(),1.0f,.001f));
    State.Advance(1.9f);
    TestTrue(TEXT("The upper landing has a full two-second stop"),FMath::IsNearlyEqual(State.LiftFraction(),1.0f,.001f));
    State.Advance(.6f);
    TestTrue(TEXT("The lift returns automatically after that stop"),FMath::IsNearlyEqual(State.LiftFraction(),.75f,.001f));
    State.Advance(1.5f);
    TestTrue(TEXT("The eight-second cycle returns to its boarding stop"),FMath::IsNearlyZero(State.LiftFraction(),.001f));
    const double Before=State.CycleTime;State.Advance(-10);
    TestEqual(TEXT("Negative time cannot rewind the cycle"),State.CycleTime,Before);
    TestTrue(TEXT("B becomes available after A"),State.EnableCircuitB());
    TestFalse(TEXT("B cannot award or reopen twice"),State.EnableCircuitB());
    TestFalse(TEXT("The circuits alone do not automatically finish System 2"),State.bSystemCompleted);
    TestTrue(TEXT("The terminal completes after both circuits"),State.CompleteSystem());
    TestFalse(TEXT("A completed terminal cannot award twice"),State.CompleteSystem());
    State.Advance(80);
    TestTrue(TEXT("Completed circuits survive later machinery cycles"),State.bPowered&&State.bCircuitBEnabled&&State.bSystemCompleted);
    return true;
}

namespace
{
struct FProcessingTestWorld
{
    UWorld* World=nullptr;
    URebootGameInstance* Progress=nullptr;
    UStaticMesh* Cube=nullptr;
    APlayerController* Player=nullptr;
    ACharacter* Pawn=nullptr;
    ARebootProcessingController* Control=nullptr;
    AStaticMeshActor* Lift=nullptr;
    AStaticMeshActor* LiftCollider=nullptr;
    AStaticMeshActor* ReturnCollider=nullptr;
    FActorSpawnParameters Spawn;

    ~FProcessingTestWorld()
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
        Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(Cube);
        Mesh->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
        Actor->SetActorScale3D(Size/100.0);return Actor;
    }
    AStaticMeshActor* Visual(UStaticMesh* Asset,FVector Position,float Yaw=0)
    {
        AStaticMeshActor* Actor=World->SpawnActor<AStaticMeshActor>(Position,FRotator(0,Yaw,0),Spawn);
        UStaticMeshComponent* Mesh=Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(Asset);
        Mesh->SetCollisionProfileName(TEXT("NoCollision"));return Actor;
    }
    ATargetPoint* Target(FVector Position)
    {
        return World->SpawnActor<ATargetPoint>(Position,FRotator::ZeroRotator,Spawn);
    }
    bool Create(FAutomationTestBase& Test)
    {
        const UWorld::InitializationValues Init=UWorld::InitializationValues()
            .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
            .EnableTraceCollision(true).CreateFXSystem(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!Test.TestNotNull(TEXT("An isolated processing physics world is available"),World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        Progress=NewObject<URebootGameInstance>(World);World->SetGameInstance(Progress);
        Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
        UStaticMesh* LiftMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_2/Meshes/SM_lift_platform_300x300.SM_lift_platform_300x300"));
        UStaticMesh* SwitchMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_2/Meshes/SM_switch_pedestal.SM_switch_pedestal"));
        UStaticMesh* BeltMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_2/Meshes/SM_conveyor_belt_surface_360x164.SM_conveyor_belt_surface_360x164"));
        if (!Test.TestNotNull(TEXT("Collision cube exists"),Cube)||!Test.TestNotNull(TEXT("Lift mesh exists"),LiftMesh)
            ||!Test.TestNotNull(TEXT("Switch mesh exists"),SwitchMesh)||!Test.TestNotNull(TEXT("Belt mesh exists"),BeltMesh)) return false;
        Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Box(FVector(0,2500,-10),FVector(2600,2200,20));
        Lift=Visual(LiftMesh,FVector(800,2500,-35));
        LiftCollider=Box(FVector(800,2500,-17.5),FVector(300,300,35));
        LiftCollider->AttachToActor(Lift,FAttachmentTransformRules::KeepWorldTransform);
        AStaticMeshActor* Belt=Visual(BeltMesh,FVector(450,2500,308),180);
        Box(FVector(450,2500,311),FVector(400,164,6));
        Box(FVector(50,2660,301.5),FVector(400,600,25));
        AStaticMeshActor* A=Visual(SwitchMesh,FVector(950,2010,0));
        AStaticMeshActor* B=Visual(SwitchMesh,FVector(180,2620,314),-90);
        Box(FVector(950,2010,110),FVector(70,70,20));
        Box(FVector(180,2620,424),FVector(70,70,20));
        AStaticMeshActor* ButtonA=Box(FVector(950,2010,120),FVector(30,30,8),false);
        AStaticMeshActor* ButtonB=Box(FVector(180,2620,434),FVector(30,30,8),false);
        AStaticMeshActor* Terminal=Box(FVector(50,2830,404),FVector(160,120,180),false);
        Box(FVector(50,2830,404),FVector(160,120,180));
        AStaticMeshActor* Return=Box(FVector(-150,2500,314),FVector(1,1,1),false);
        ReturnCollider=Box(FVector(-150,2500,413.5),FVector(24,238,199));
        ReturnCollider->AttachToActor(Return,FAttachmentTransformRules::KeepWorldTransform);
        Player=World->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Player->SetAsLocalPlayerController();
        Pawn=World->SpawnActor<ACharacter>(FVector(950,1880,62.5),FRotator::ZeroRotator,Spawn);
        Pawn->GetCapsuleComponent()->SetCapsuleSize(33.373f,60.0f,true);
        Pawn->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
        UCharacterMovementComponent* Movement=Pawn->GetCharacterMovement();
        Movement->bRunPhysicsWithNoController=true;Movement->bUpdateOnlyIfRendered=false;
        Movement->SetMovementMode(MOVE_Walking);Player->Possess(Pawn);
        Control=World->SpawnActor<ARebootProcessingController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Control->LiftActor=Lift;Control->LiftCollisionActor=LiftCollider;Control->BeltActor=Belt;
        Control->SwitchA=A;Control->ButtonA=ButtonA;Control->SwitchATarget=Target(FVector(950,1970,110));
        Control->SwitchB=B;Control->ButtonB=ButtonB;Control->SwitchBTarget=Target(FVector(140,2620,424));
        Control->TerminalActor=Terminal;Control->TerminalTarget=Target(FVector(50,2760,424));
        Control->ReturnDoor=Return;Control->bTransitionEnabled=false;
        World->InitializeActorsForPlay(FURL());World->UpdateWorldComponents(false,false);World->BeginPlay();
        if (!World->HasBegunPlay()) World->GetWorldSettings()->NotifyBeginPlay();
        Test.TestTrue(TEXT("The processing controller ticks natively"),Control->PrimaryActorTick.IsTickFunctionRegistered());
        Test.TestTrue(TEXT("The simulated robot is locally controlled"),Pawn->IsLocallyControlled());
        Frames(2);return true;
    }
    void Place(FVector Position,EMovementMode Mode=MOVE_Walking)
    {
        Pawn->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(Mode);
    }
    void Frames(int32 Count)
    {
        for (int32 Index=0;Index<Count;++Index)
        {
            ++GFrameCounter;UWorld* Previous=GWorld;GWorld=World;
            World->Tick(LEVELTICK_All,1.0f/60.0f);GWorld=Previous;
        }
    }
    bool RobotCollision(FAutomationTestBase& Test)
    {
        UClass* RobotClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/StackOBot/Blueprints/Character/BP_Bot.BP_Bot_C"));
        if (!Test.TestNotNull(TEXT("The actual robot collision defaults exist"),RobotClass)) return false;
        const ACharacter* Defaults=RobotClass->GetDefaultObject<ACharacter>();
        UCapsuleComponent* Capsule=Pawn->GetCapsuleComponent();Capsule->DestroyPhysicsState();
        Capsule->GetBodyInstance()->CopyBodyInstancePropertiesFrom(Defaults->GetCapsuleComponent()->GetBodyInstance());
        Capsule->CreatePhysicsState();
        return Test.TestEqual(TEXT("The rider uses the robot's inline custom collision responses"),Capsule->GetCollisionProfileName(),FName(TEXT("Custom")));
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessingControlTest,"REBOOT.ProcessingControl.ReachableCircuitsAndEarnedTerminalCompletion",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessingControlTest::RunTest(const FString& Parameters)
{
    FProcessingTestWorld Scene;if (!Scene.Create(*this)) return false;
    TestEqual(TEXT("Loading this room does not grant System 1 or System 2"),Scene.Progress->GetCompletedSystemCount(),0);
    Scene.Place(FVector(50,2670,376.5));Scene.Frames(2);
    TestFalse(TEXT("The terminal cannot bypass either circuit"),Scene.Control->CanUseTerminal());
    Scene.Control->ActivateTerminal();TestFalse(TEXT("An early terminal attempt grants no System 2"),Scene.Progress->IsSystemComplete(1));
    Scene.Place(FVector(50,2620,376.5));Scene.Frames(2);
    TestFalse(TEXT("An early B operation cannot bypass A"),Scene.Control->CanUseSwitch(1));
    Scene.Control->ActivateSwitchB();TestFalse(TEXT("An early B attempt leaves its circuit off"),Scene.Control->bCircuitBEnabled);
    TestFalse(TEXT("A cannot be operated from the upper landing"),Scene.Control->CanUseSwitch(0));
    TestFalse(TEXT("Invalid switch identifiers are rejected"),Scene.Control->CanUseSwitch(-1)||Scene.Control->CanUseSwitch(2));
    Scene.Place(FVector(950,1880,62.5));Scene.Frames(2);
    TestTrue(TEXT("A can be reached from its marked front"),Scene.Control->CanUseSwitch(0));
    AStaticMeshActor* Wall=Scene.Box(FVector(950,1930,110),FVector(120,20,200));
    Scene.Control->ActivateSwitchA();TestFalse(TEXT("A cannot power machinery through a wall"),Scene.Control->bPowered);
    Wall->SetActorEnableCollision(false);Scene.Control->ActivateSwitchA();
    TestTrue(TEXT("A powers the lift and conveyor"),Scene.Control->bPowered);
    Scene.Place(FVector(50,2620,376.5));Scene.Frames(2);
    TestTrue(TEXT("Powered B can be reached from its landing"),Scene.Control->CanUseSwitch(1));
    Scene.Control->ActivateSwitchB();Scene.Control->Tick(1.2f);
    TestTrue(TEXT("B enables its own circuit"),Scene.Control->bCircuitBEnabled);
    TestTrue(TEXT("B opens the permanent return barrier"),Scene.Control->ReturnOpenProgress>.99f);
    TestTrue(TEXT("The barrier's collision follows its visible panel"),FMath::IsNearlyEqual(Scene.ReturnCollider->GetActorLocation().Z,618.5,1.0));
    Scene.Place(FVector(50,2670,376.5));Scene.Frames(2);
    TestTrue(TEXT("The powered terminal is reachable from its safe front pad"),Scene.Control->CanUseTerminal());
    AStaticMeshActor* TerminalWall=Scene.Box(FVector(50,2715,424),FVector(220,20,210));
    Scene.Control->ActivateTerminal();TestFalse(TEXT("A wall prevents terminal completion"),Scene.Control->bSystemCompleted);
    TerminalWall->SetActorEnableCollision(false);
    Scene.Place(FVector(50,2670,500),MOVE_Falling);
    TestFalse(TEXT("An airborne robot cannot complete the terminal"),Scene.Control->CanUseTerminal());
    Scene.Place(FVector(50,2670,376.5));Scene.Frames(2);Scene.Control->ActivateTerminal();
    TestTrue(TEXT("The actual terminal completes System 2"),Scene.Control->bSystemCompleted&&Scene.Progress->IsSystemComplete(1));
    TestFalse(TEXT("Completing System 2 never invents earlier or core completion"),Scene.Progress->IsSystemComplete(0)||Scene.Progress->IsSystemComplete(2));
    Scene.Control->ActivateTerminal();Scene.Control->Tick(1.5f);
    TestEqual(TEXT("Repeated terminal input awards System 2 only once"),Scene.Progress->GetCompletedSystemCount(),1);
    TestFalse(TEXT("Unit fixture suppresses travel, not earned completion"),Scene.Control->bTransitionStarted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessingLiftTest,"REBOOT.ProcessingControl.CustomRobotHeadroomAndNativeLiftRoundTrip",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessingLiftTest::RunTest(const FString& Parameters)
{
    FProcessingTestWorld Scene;if (!Scene.Create(*this)||!Scene.RobotCollision(*this)) return false;
    Scene.Control->ActivateSwitchA();TestTrue(TEXT("A powers transport before boarding"),Scene.Control->bPowered);
    Scene.Place(FVector(800,2500,62.5));
    AStaticMeshActor* Ceiling=Scene.Box(FVector(800,2500,240),FVector(180,180,20));
    Scene.Frames(300);
    TestTrue(TEXT("The actual robot's custom responses detect insufficient headroom"),Scene.Control->bLiftObstructed);
    const double Paused=Scene.Lift->GetActorLocation().Z;Scene.Frames(60);
    TestTrue(TEXT("An obstructed lift holds a safe position"),FMath::Abs(Scene.Lift->GetActorLocation().Z-Paused)<.1);
    TestTrue(TEXT("The collision proxy stays attached while paused"),Scene.LiftCollider->GetAttachParentActor()==Scene.Lift);
    Ceiling->SetActorEnableCollision(false);
    bool Upper=false,Returned=false;double MaxFootError=0;
    for (int32 Index=0;Index<650;++Index)
    {
        Scene.Frames(1);
        const double Deck=Scene.Lift->GetActorLocation().Z+35;
        const double Feet=Scene.Pawn->GetActorLocation().Z-Scene.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        MaxFootError=FMath::Max(MaxFootError,FMath::Abs(Feet-Deck));
        Upper|=Scene.Control->LiftPositionFraction>.999f&&Scene.Pawn->GetActorLocation().Z>350;
        if (Upper&&Scene.Control->LiftPositionFraction<.001f) { Returned=true;break; }
    }
    TestTrue(TEXT("Removing the ceiling resumes native carrying to the upper landing"),Upper);
    TestTrue(TEXT("The automatic cycle returns the rider to its lower boarding stop"),Returned);
    TestTrue(FString::Printf(TEXT("The rider remains based on the deck through the trip (%.3f cm)"),MaxFootError),MaxFootError<6);
    TestTrue(TEXT("The rider finishes at ground-floor height"),FMath::Abs(Scene.Pawn->GetActorLocation().Z-62.15)<4);
    TestTrue(TEXT("Lift collision completes the same round trip"),FMath::Abs(Scene.LiftCollider->GetActorLocation().Z+17.5)<1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessingBeltTest,"REBOOT.ProcessingControl.GroundedConveyorAndBlockingCollision",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessingBeltTest::RunTest(const FString& Parameters)
{
    FProcessingTestWorld Scene;if (!Scene.Create(*this)) return false;
    Scene.Place(FVector(550,2500,376.5));Scene.Frames(30);
    TestTrue(TEXT("An unpowered belt does not move the robot"),FMath::Abs(Scene.Pawn->GetActorLocation().X-550)<.1);
    Scene.Place(FVector(950,1880,62.5));Scene.Frames(2);Scene.Control->ActivateSwitchA();
    Scene.Place(FVector(550,2500,376.5));Scene.Frames(10);const double Before=Scene.Pawn->GetActorLocation().X;
    Scene.Frames(30);const double Travel=Before-Scene.Pawn->GetActorLocation().X;
    TestTrue(FString::Printf(TEXT("A powered grounded belt carries the robot west (%.2f cm)"),Travel),Travel>40&&Travel<100);
    Scene.Pawn->GetCharacterMovement()->Velocity=FVector(100,0,0);Scene.Control->Tick(.05f);
    TestTrue(TEXT("Transport preserves the robot's walking velocity"),FMath::IsNearlyEqual(Scene.Pawn->GetCharacterMovement()->Velocity.X,100.0,.01));
    Scene.Place(FVector(550,2500,500),MOVE_Falling);Scene.Control->Tick(.25f);
    TestTrue(TEXT("The belt cannot pull an airborne robot"),FMath::Abs(Scene.Pawn->GetActorLocation().X-550)<.1);
    Scene.Place(FVector(550,2500,62.5));Scene.Control->Tick(.25f);
    TestTrue(TEXT("The belt cannot pull a robot on the floor below it"),FMath::Abs(Scene.Pawn->GetActorLocation().X-550)<.1);
    Scene.Box(FVector(450,2500,414),FVector(20,200,200));
    Scene.Place(FVector(550,2500,376.5));Scene.Frames(1);Scene.Control->Tick(.75f);
    TestTrue(TEXT("Swept belt transport cannot carry the robot through a blocking wall"),Scene.Pawn->GetActorLocation().X>488);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessingRecoveryTest,"REBOOT.ProcessingControl.RecoveryPreservesCircuitsWithoutAwardingCompletion",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessingRecoveryTest::RunTest(const FString& Parameters)
{
    FProcessingTestWorld Scene;if (!Scene.Create(*this)) return false;
    Scene.Control->ActivateSwitchA();
    Scene.Place(FVector(50,2620,376.5));Scene.Frames(2);Scene.Control->ActivateSwitchB();
    Scene.Place(FVector(950,1880,62.5));Scene.Frames(2);
    Scene.Place(FVector(-500,2500,-180),MOVE_Falling);Scene.Frames(90);
    TestEqual(TEXT("A processing-room fall returns to the checkpoint once"),Scene.Control->FallResetCount,1);
    TestTrue(TEXT("Recovery reaches the marked safe front pad"),FVector::Dist2D(Scene.Pawn->GetActorLocation(),FVector(950,1880,0))<2);
    TestTrue(TEXT("Recovery preserves both earned circuit controls"),Scene.Control->bPowered&&Scene.Control->bCircuitBEnabled);
    TestTrue(TEXT("The unlocked return path stays available after a fall"),Scene.Control->ReturnOpenProgress>.99f);
    TestFalse(TEXT("Recovery does not replace the terminal interaction"),Scene.Control->bSystemCompleted||Scene.Progress->IsSystemComplete(1));
    return true;
}

#endif
