#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "RebootBridgeController.h"
#include "RebootBridgeState.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeTimingTest,"REBOOT.BrokenBridge.BoardingStopsAndPowerLatch",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBridgeTimingTest::RunTest(const FString& Parameters)
{
    FRebootBridgeState State;
    State.Advance(25);
    TestEqual(TEXT("Transport remains parked until its entrance switch is used"),State.ShuttleFraction(),0.0f);
    State.PowerUp();
    State.Advance(1.9f);
    TestEqual(TEXT("The near stop gives the robot two seconds to board"),State.ShuttleFraction(),0.0f);
    State.Advance(1.6f);
    TestTrue(TEXT("The powered shuttle traverses halfway in 1.5 seconds"),FMath::IsNearlyEqual(State.ShuttleFraction(),.5f,.001f));
    State.Advance(1.5f);
    TestTrue(TEXT("Three seconds of travel reaches the rest-platform stop"),FMath::IsNearlyEqual(State.ShuttleFraction(),1.0f,.001f));
    State.Advance(1.9f);
    TestTrue(TEXT("The far stop provides its full disembarking pause"),FMath::IsNearlyEqual(State.ShuttleFraction(),1.0f,.001f));
    State.Advance(1.6f);
    TestTrue(TEXT("The shuttle returns after the far pause"),FMath::IsNearlyEqual(State.ShuttleFraction(),.5f,.001f));
    const double Before=State.CycleTime;
    State.PowerUp();
    State.Advance(-5);
    TestEqual(TEXT("Repeated activation and negative time cannot restart the cycle"),State.CycleTime,Before);
    State.Advance(1.5f);
    TestTrue(TEXT("A complete cycle returns to the boarding stop"),FMath::IsNearlyZero(State.ShuttleFraction(),.001f));
    TestTrue(TEXT("Completed transport power persists across cycles"),State.bPowered);
    return true;
}

namespace
{
struct FBridgeTestWorld
{
    UWorld* World=nullptr;
    UStaticMesh* Cube=nullptr;
    APlayerController* Player=nullptr;
    ACharacter* Pawn=nullptr;
    ARebootBridgeController* Bridge=nullptr;
    AStaticMeshActor* Shuttle=nullptr;
    AStaticMeshActor* ShuttleCollider=nullptr;
    FActorSpawnParameters Spawn;

    ~FBridgeTestWorld()
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
        if (!Test.TestNotNull(TEXT("An isolated bridge physics world is available"),World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
        UStaticMesh* ShuttleMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_shuttle_platform_300x200.SM_shuttle_platform_300x200"));
        UStaticMesh* SwitchMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_switch_pedestal.SM_switch_pedestal"));
        UStaticMesh* BeltMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_conveyor_belt_surface_360x164.SM_conveyor_belt_surface_360x164"));
        if (!Test.TestNotNull(TEXT("Engine collision cube is available"),Cube)
            ||!Test.TestNotNull(TEXT("The imported shuttle is available"),ShuttleMesh)
            ||!Test.TestNotNull(TEXT("The imported control is available"),SwitchMesh)
            ||!Test.TestNotNull(TEXT("The imported conveyor is available"),BeltMesh)) return false;
        Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Box(FVector(0,0,-360),FVector(2460,2060,20));
        Box(FVector(-800,-650,-10),FVector(800,700,20));
        Box(FVector(-600,650,-12.5),FVector(400,300,25));
        Box(FVector(-200,650,-3),FVector(400,164,6));
        Box(FVector(750,0,-10),FVector(900,2000,20));
        Shuttle=Visual(ShuttleMesh,FVector(-600,-200,-35));
        ShuttleCollider=Box(FVector(-600,-200,-17.5),FVector(300,200,35));
        ShuttleCollider->AttachToActor(Shuttle,FAttachmentTransformRules::KeepWorldTransform);
        for (const double X : {-740.0,-460.0})
        {
            AStaticMeshActor* Rail=Box(FVector(X,-200,55),FVector(24,200,110));
            Rail->AttachToActor(Shuttle,FAttachmentTransformRules::KeepWorldTransform);
        }
        AStaticMeshActor* Belt=Visual(BeltMesh,FVector(-200,650,-6));
        AStaticMeshActor* Switch=Visual(SwitchMesh,FVector(-1000,-470,0));
        Box(FVector(-1000,-470,110),FVector(70,70,20));
        AStaticMeshActor* Button=Box(FVector(-1000,-470,120),FVector(30,30,8),false);
        Player=World->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Player->SetAsLocalPlayerController();
        Pawn=World->SpawnActor<ACharacter>(FVector(-1050,-650,62.5),FRotator::ZeroRotator,Spawn);
        Pawn->GetCapsuleComponent()->SetCapsuleSize(33.373f,60.0f,true);
        Pawn->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
        UCharacterMovementComponent* Movement=Pawn->GetCharacterMovement();
        Movement->bRunPhysicsWithNoController=true;
        Movement->bUpdateOnlyIfRendered=false;
        Movement->MaxWalkSpeed=800;
        Movement->JumpZVelocity=400;
        Movement->GravityScale=2;
        Movement->MaxStepHeight=45;
        Movement->PerchRadiusThreshold=0;
        Movement->AirControl=1;
        Movement->SetMovementMode(MOVE_Walking);
        Player->Possess(Pawn);
        Bridge=World->SpawnActor<ARebootBridgeController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Bridge->SwitchActor=Switch;
        Bridge->ButtonActor=Button;
        Bridge->ShuttleActor=Shuttle;
        Bridge->ShuttleCollisionActor=ShuttleCollider;
        Bridge->BeltActor=Belt;
        World->InitializeActorsForPlay(FURL());
        World->UpdateWorldComponents(false,false);
        World->BeginPlay();
        if (!World->HasBegunPlay()) World->GetWorldSettings()->NotifyBeginPlay();
        Test.TestTrue(TEXT("The bridge controller is registered for native ticking"),Bridge->PrimaryActorTick.IsTickFunctionRegistered());
        Test.TestTrue(TEXT("The simulated robot is locally controlled"),Pawn->IsLocallyControlled());
        Test.TestTrue(TEXT("Character movement is registered for native ticking"),Movement->PrimaryComponentTick.IsTickFunctionRegistered());
        return true;
    }
    void Place(FVector Position,EMovementMode Mode=MOVE_Walking)
    {
        Pawn->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->SetMovementMode(Mode);
    }
    void Frame(FVector Input=FVector::ZeroVector)
    {
        if (!Input.IsNearlyZero()) Pawn->AddMovementInput(Input,1);
        ++GFrameCounter;
        UWorld* Previous=GWorld;
        GWorld=World;
        World->Tick(LEVELTICK_All,1.0f/60.0f);
        GWorld=Previous;
    }
    void Frames(int32 Count)
    {
        for (int32 Index=0; Index<Count; ++Index) Frame();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgePowerAccessTest,"REBOOT.BrokenBridge.EntranceControlAccess",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBridgePowerAccessTest::RunTest(const FString& Parameters)
{
    FBridgeTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Place(FVector(600,650,62.5));
    TestFalse(TEXT("Transport power cannot be activated from the far bank"),Scene.Bridge->CanActivatePower());
    Scene.Place(FVector(-1050,-650,62.5));
    TestTrue(TEXT("The entrance control can be reached from its front"),Scene.Bridge->CanActivatePower());
    AStaticMeshActor* Wall=Scene.Box(FVector(-1020,-570,115),FVector(140,20,200));
    TestFalse(TEXT("A wall blocks use of the power control"),Scene.Bridge->CanActivatePower());
    Scene.Bridge->ActivatePower();
    TestFalse(TEXT("An obstructed control leaves transport unpowered"),Scene.Bridge->bPowered);
    Wall->SetActorEnableCollision(false);
    Scene.Bridge->ActivatePower();
    TestTrue(TEXT("The reachable entrance switch latches transport power"),Scene.Bridge->bPowered);
    TestFalse(TEXT("A completed control cannot be retriggered"),Scene.Bridge->CanActivatePower());
    Scene.Frames(210);
    const double Position=Scene.Shuttle->GetActorLocation().Y;
    Scene.Bridge->ActivatePower();
    TestTrue(TEXT("Repeated activation does not teleport a moving shuttle"),FMath::IsNearlyEqual(Scene.Shuttle->GetActorLocation().Y,Position,.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeRiderTest,"REBOOT.BrokenBridge.NativeShuttleRiderRoundTrip",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBridgeRiderTest::RunTest(const FString& Parameters)
{
    FBridgeTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Bridge->ActivatePower();
    TestTrue(TEXT("The shuttle is powered before boarding"),Scene.Bridge->bPowered);
    Scene.Place(FVector(-600,-200,62.5));
    double MaxPawnY=-200,MaxShuttleY=-200,MaxDrift=0;
    for (int32 Index=0; Index<610; ++Index)
    {
        Scene.Frame();
        MaxPawnY=FMath::Max(MaxPawnY,Scene.Pawn->GetActorLocation().Y);
        MaxShuttleY=FMath::Max(MaxShuttleY,Scene.Shuttle->GetActorLocation().Y);
        MaxDrift=FMath::Max(MaxDrift,FMath::Abs(Scene.Pawn->GetActorLocation().Y-Scene.Shuttle->GetActorLocation().Y));
    }
    TestTrue(TEXT("The shuttle reaches the rest-platform boarding stop"),MaxShuttleY>399);
    TestTrue(TEXT("Native based movement carries the robot to the far stop"),MaxPawnY>398);
    TestTrue(FString::Printf(TEXT("A passive rider stays on the shuttle through both directions (drift %.3f cm)"),MaxDrift),MaxDrift<4);
    TestTrue(TEXT("The full cycle returns the platform to its entry stop"),FMath::Abs(Scene.Shuttle->GetActorLocation().Y+200)<.1);
    TestTrue(TEXT("The passive rider returns to the same entry stop"),FMath::Abs(Scene.Pawn->GetActorLocation().Y+200)<4);
    TestTrue(TEXT("The collision proxy follows the shuttle through the full trip"),Scene.ShuttleCollider->GetActorLocation().Equals(FVector(-600,-200,-17.5),.1));
    TestEqual(TEXT("A valid shuttle ride does not trigger fall recovery"),Scene.Bridge->FallResetCount,0);
    AddInfo(FString::Printf(TEXT("Shuttle: max platform Y %.3f, max rider Y %.3f, maximum relative drift %.3f"),MaxShuttleY,MaxPawnY,MaxDrift));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeConveyorTest,"REBOOT.BrokenBridge.GroundedConveyorTransport",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBridgeConveyorTest::RunTest(const FString& Parameters)
{
    FBridgeTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Bridge->ActivatePower();
    Scene.Place(FVector(-300,650,62.5));
    Scene.Frames(5);
    const double Before=Scene.Pawn->GetActorLocation().X;
    Scene.Frames(30);
    const double Distance=Scene.Pawn->GetActorLocation().X-Before;
    TestTrue(FString::Printf(TEXT("The belt carries a grounded robot east (%.2f cm)"),Distance),Distance>40&&Distance<100);
    Scene.Pawn->GetCharacterMovement()->Velocity=FVector(100,0,0);
    Scene.Bridge->Tick(.05f);
    TestTrue(TEXT("Belt transport preserves player walking velocity"),FMath::IsNearlyEqual(Scene.Pawn->GetCharacterMovement()->Velocity.X,100.0,.01));
    Scene.Place(FVector(-200,650,200),MOVE_Falling);
    Scene.Bridge->Tick(.25f);
    TestTrue(TEXT("The belt does not pull an airborne robot"),FMath::IsNearlyEqual(Scene.Pawn->GetActorLocation().X,-200.0,.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeRecoveryTest,"REBOOT.BrokenBridge.FallRecoveryPreservesPower",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBridgeRecoveryTest::RunTest(const FString& Parameters)
{
    FBridgeTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Bridge->ActivatePower();
    Scene.Frames(130);
    const float Before=Scene.Bridge->ShuttlePositionFraction;
    Scene.Place(FVector(0,0,62.5),MOVE_Falling);
    for (int32 Index=0; Index<100&&Scene.Bridge->FallResetCount==0; ++Index) Scene.Frame();
    TestEqual(TEXT("A fall into the catch zone causes one prompt reset"),Scene.Bridge->FallResetCount,1);
    TestTrue(TEXT("Fall recovery returns the robot to the entrance bank"),Scene.Pawn->GetActorLocation().Equals(FVector(-1050,-650,62.5),4));
    TestTrue(TEXT("Recovery preserves transport power"),Scene.Bridge->bPowered);
    TestTrue(TEXT("Recovery does not restart the shuttle's moving cycle"),Scene.Bridge->ShuttlePositionFraction>Before+.05f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeGapWalkTest,"REBOOT.BrokenBridge.JumpGapRejectsWalking",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBridgeGapWalkTest::RunTest(const FString& Parameters)
{
    FBridgeTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Bridge->ActivatePower();
    Scene.Place(FVector(-170,650,62.5));
    Scene.Pawn->GetCharacterMovement()->Velocity=FVector(800,0,0);
    bool bLandedFar=false;
    for (int32 Index=0; Index<150&&Scene.Bridge->FallResetCount==0; ++Index)
    {
        Scene.Frame(FVector::ForwardVector);
        bLandedFar|=Scene.Pawn->GetActorLocation().X>=300&&Scene.Pawn->GetCharacterMovement()->IsMovingOnGround();
    }
    TestFalse(TEXT("The mandatory gap cannot be walked across at full robot speed"),bLandedFar);
    TestEqual(TEXT("Walking off the belt provides catch-zone recovery"),Scene.Bridge->FallResetCount,1);
    TestTrue(TEXT("A missed jump preserves the powered transport"),Scene.Bridge->bPowered);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeGapJumpTest,"REBOOT.BrokenBridge.JumpReachesFarLanding",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBridgeGapJumpTest::RunTest(const FString& Parameters)
{
    FBridgeTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Bridge->ActivatePower();
    Scene.Place(FVector(-25,650,62.5));
    Scene.Frames(1);
    Scene.Pawn->GetCharacterMovement()->Velocity=FVector(800,0,0);
    Scene.Pawn->Jump();
    bool bAirborne=false,bLanded=false;
    for (int32 Index=0; Index<100&&Scene.Bridge->FallResetCount==0; ++Index)
    {
        Scene.Frame(FVector::ForwardVector);
        bAirborne|=Scene.Pawn->GetCharacterMovement()->IsFalling();
        if (bAirborne&&Scene.Pawn->GetActorLocation().X>300&&Scene.Pawn->GetCharacterMovement()->IsMovingOnGround()) { bLanded=true; break; }
    }
    TestTrue(TEXT("The robot leaves the conveyor in a real jump"),bAirborne);
    TestTrue(TEXT("A forward jump reaches the far fixed landing"),bLanded);
    TestEqual(TEXT("A successful jump avoids fall recovery"),Scene.Bridge->FallResetCount,0);
    Scene.Frames(2);
    TestTrue(TEXT("The powered section records reaching its far landing"),Scene.Bridge->bSectionCompleted);
    return true;
}

#endif
