#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "RebootShaftController.h"
#include "RebootShaftState.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftSequenceTest,"REBOOT.EmergencyShaft.SequenceAndAutomaticLiftTiming",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftSequenceTest::RunTest(const FString& Parameters)
{
    FRebootShaftState State;
    TestFalse(TEXT("Switch B cannot power the lift before A"),State.PowerLift());
    State.Advance(60,true,true,true);
    TestFalse(TEXT("The plate cannot bypass the unpowered conveyor sequence"),State.bGateRequestedOpen);
    TestFalse(TEXT("An early far-side position cannot latch the unpowered gate"),State.bGateLatched);
    State.PowerConveyor();
    State.Advance(20,true,false,false);
    TestTrue(TEXT("A powers the conveyor"),State.bConveyorPowered);
    TestFalse(TEXT("A alone leaves the lift visibly unpowered"),State.bLiftPowered);
    TestEqual(TEXT("A alone leaves the lift parked"),State.LiftFraction(),0.0f);
    TestEqual(TEXT("Holding the plate preserves all six seconds"),State.GateHoldRemaining,6.0f);
    TestFalse(TEXT("B still requires crossing the access gate"),State.PowerLift());
    State.Advance(5.5f,false,false,false);
    TestTrue(TEXT("The timer starts after leaving the plate"),FMath::IsNearlyEqual(State.GateHoldRemaining,.5f,.001f));
    State.Advance(1,false,false,true);
    TestTrue(TEXT("An armed doorway stays safe after the timer expires"),State.bGateRequestedOpen);
    State.Advance(.25f,false,false,false);
    TestFalse(TEXT("A missed gate timer permits a closed-gate retry"),State.bGateRequestedOpen);
    TestFalse(TEXT("An expired, clear doorway disarms proximity opening"),State.bGateArmed);
    State.Advance(.1f,true,false,false);
    State.Advance(.5f,false,true,false);
    TestTrue(TEXT("The real far-side crossing latches the gate"),State.bGateLatched);
    TestTrue(TEXT("B powers the lift after A and the latch"),State.PowerLift());
    State.Advance(1.9f,false,false,false);
    TestEqual(TEXT("The lower boarding stop lasts two seconds"),State.LiftFraction(),0.0f);
    State.Advance(1.6f,false,false,false);
    TestTrue(TEXT("The lift is halfway up after 1.5 seconds of travel"),FMath::IsNearlyEqual(State.LiftFraction(),.5f,.001f));
    State.Advance(1.5f,false,false,false);
    TestTrue(TEXT("Three seconds of travel reaches the upper landing"),FMath::IsNearlyEqual(State.LiftFraction(),1.0f,.001f));
    State.Advance(1.9f,false,false,false);
    TestTrue(TEXT("The upper stop gives a full disembarking pause"),FMath::IsNearlyEqual(State.LiftFraction(),1.0f,.001f));
    State.Advance(1.6f,false,false,false);
    TestTrue(TEXT("The automatic return is halfway down after 1.5 seconds"),FMath::IsNearlyEqual(State.LiftFraction(),.5f,.001f));
    const double Before=State.LiftCycleTime;
    State.PowerConveyor();State.PowerLift();State.Advance(-5,false,false,false);
    TestEqual(TEXT("Repeated controls and negative time cannot restart a moving lift"),State.LiftCycleTime,Before);
    State.Advance(1.5f,false,false,false);
    TestTrue(TEXT("The ten-second cycle returns to the safe boarding stop"),FMath::IsNearlyZero(State.LiftFraction(),.001f));
    TestTrue(TEXT("Completed A, B and gate states persist across the cycle"),State.bConveyorPowered&&State.bLiftPowered&&State.bGateLatched);
    return true;
}

namespace
{
struct FShaftTestWorld
{
    UWorld* World=nullptr;
    UStaticMesh* Cube=nullptr;
    APlayerController* Player=nullptr;
    ACharacter* Pawn=nullptr;
    ARebootShaftController* Shaft=nullptr;
    AStaticMeshActor* Gate=nullptr;
    AStaticMeshActor* GateCollider=nullptr;
    AStaticMeshActor* Lift=nullptr;
    AStaticMeshActor* LiftCollider=nullptr;
    AStaticMeshActor* CoreDoor=nullptr;
    AStaticMeshActor* CoreCollider=nullptr;
    FActorSpawnParameters Spawn;

    ~FShaftTestWorld()
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
    AStaticMeshActor* Switch(UStaticMesh* Mesh,FVector Position)
    {
        AStaticMeshActor* Actor=Visual(Mesh,Position);
        Box(Position+FVector(0,0,7.5),FVector(70,70,15));
        Box(Position+FVector(0,0,57.5),FVector(40,40,85));
        Box(Position+FVector(0,0,110),FVector(70,70,20));
        return Actor;
    }
    bool Create(FAutomationTestBase& Test)
    {
        const UWorld::InitializationValues Init=UWorld::InitializationValues()
            .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
            .EnableTraceCollision(true).CreateFXSystem(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!Test.TestNotNull(TEXT("An isolated shaft physics world is available"),World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
        UStaticMesh* LiftMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_lift_platform_300x300.SM_lift_platform_300x300"));
        UStaticMesh* SwitchMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_switch_pedestal.SM_switch_pedestal"));
        UStaticMesh* BeltMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_conveyor_belt_surface_360x164.SM_conveyor_belt_surface_360x164"));
        if (!Test.TestNotNull(TEXT("Collision cube is available"),Cube)
            ||!Test.TestNotNull(TEXT("Imported lift is available"),LiftMesh)
            ||!Test.TestNotNull(TEXT("Imported pedestal is available"),SwitchMesh)
            ||!Test.TestNotNull(TEXT("Imported belt is available"),BeltMesh)) return false;
        Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Box(FVector(3000,2800,-360),FVector(2860,2460,20));
        Box(FVector(3000,1825,-10),FVector(500,450,20));
        Box(FVector(3000,2250,-3),FVector(164,400,6));
        Box(FVector(3000,2625,-10),FVector(500,350,20));
        Box(FVector(3000,3000,-10),FVector(300,400,20));
        Box(FVector(3000,3700,-10),FVector(500,400,20));
        Box(FVector(2250,3700,290),FVector(400,400,20));
        Box(FVector(2025,2700,-10),FVector(850,800,20));
        for (double X : {2830.0,3170.0}) Box(FVector(X,3000,150),FVector(40,400,300));
        Gate=Box(FVector(3000,2940,2),FVector(1,1,1),false);
        GateCollider=Box(FVector(3000,2940,155),FVector(238,24,306));
        GateCollider->AttachToActor(Gate,FAttachmentTransformRules::KeepWorldTransform);
        for (double X : {2865.0,3135.0}) Box(FVector(X,2940,155),FVector(30,60,310));
        Box(FVector(3000,2940,330),FVector(300,60,40));
        Lift=Visual(LiftMesh,FVector(2600,3720,-35));
        LiftCollider=Box(FVector(2600,3720,-17.5),FVector(300,300,35));
        LiftCollider->AttachToActor(Lift,FAttachmentTransformRules::KeepWorldTransform);
        for (double Y : {3580.0,3860.0})
        {
            AStaticMeshActor* Rail=Box(FVector(2600,Y,55),FVector(300,24,110));
            Rail->AttachToActor(Lift,FAttachmentTransformRules::KeepWorldTransform);
        }
        AStaticMeshActor* Belt=Visual(BeltMesh,FVector(3000,2250,-6),90);
        AStaticMeshActor* A=Switch(SwitchMesh,FVector(3180,1800,0));
        AStaticMeshActor* B=Switch(SwitchMesh,FVector(3150,3730,0));
        AStaticMeshActor* ButtonA=Box(FVector(3180,1800,120),FVector(30,30,8),false);
        AStaticMeshActor* ButtonB=Box(FVector(3150,3730,120),FVector(30,30,8),false);
        AStaticMeshActor* Plate=Box(FVector(3000,2680,-7.5),FVector(118,118,8),false);
        CoreDoor=Box(FVector(1585,2800,2),FVector(1,1,1),false);
        CoreCollider=Box(FVector(1585,2800,155),FVector(24,238,306));
        CoreCollider->AttachToActor(CoreDoor,FAttachmentTransformRules::KeepWorldTransform);
        Player=World->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Player->SetAsLocalPlayerController();
        Pawn=World->SpawnActor<ACharacter>(FVector(3000,1800,62.5),FRotator::ZeroRotator,Spawn);
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
        Shaft=World->SpawnActor<ARebootShaftController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Shaft->SwitchA=A;Shaft->ButtonA=ButtonA;Shaft->SwitchB=B;Shaft->ButtonB=ButtonB;
        Shaft->GateActor=Gate;Shaft->PlateTopActor=Plate;
        Shaft->LiftActor=Lift;Shaft->LiftCollisionActor=LiftCollider;Shaft->BeltActor=Belt;Shaft->CoreDoor=CoreDoor;
        World->InitializeActorsForPlay(FURL());
        World->UpdateWorldComponents(false,false);
        World->BeginPlay();
        if (!World->HasBegunPlay()) World->GetWorldSettings()->NotifyBeginPlay();
        Test.TestTrue(TEXT("The shaft controller is registered for native ticking"),Shaft->PrimaryActorTick.IsTickFunctionRegistered());
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
        UWorld* Previous=GWorld;GWorld=World;
        World->Tick(LEVELTICK_All,1.0f/60.0f);
        GWorld=Previous;
    }
    void Frames(int32 Count) { for (int32 Index=0;Index<Count;++Index) Frame(); }
    bool PowerA(FAutomationTestBase& Test)
    {
        if (Shaft->bConveyorPowered) return true;
        Place(FVector(3180,1650,62.5));Frames(2);
        if (!Test.TestTrue(TEXT("The entrance control is reachable from its front"),Shaft->CanUseSwitch(0))) return false;
        Shaft->ActivateSwitchA();
        return Test.TestTrue(TEXT("Switch A powers the conveyor"),Shaft->bConveyorPowered);
    }
    bool LatchGate(FAutomationTestBase& Test)
    {
        if (!PowerA(Test)) return false;
        Place(FVector(3000,2680,62.5));Frames(75);
        if (!Test.TestTrue(TEXT("The occupied plate raises the actual access panel"),Shaft->GateOpenProgress>.99f)) return false;
        Place(FVector(3000,3120,62.5));Frames(4);
        return Test.TestTrue(TEXT("The native far-side trigger latches the access gate"),Shaft->bGateLatched);
    }
    bool PowerB(FAutomationTestBase& Test)
    {
        if (!LatchGate(Test)) return false;
        Place(FVector(3000,3600,62.5));Frames(2);
        if (!Test.TestTrue(TEXT("Switch B is reachable on its far fixed landing"),Shaft->CanUseSwitch(1))) return false;
        Shaft->ActivateSwitchB();
        return Test.TestTrue(TEXT("Switch B powers the lift"),Shaft->bLiftPowered);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftSwitchAccessTest,"REBOOT.EmergencyShaft.SwitchSequenceRangeAndLineOfSight",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftSwitchAccessTest::RunTest(const FString& Parameters)
{
    FShaftTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Place(FVector(3000,3600,62.5));
    TestFalse(TEXT("Reaching B early cannot skip Switch A"),Scene.Shaft->CanUseSwitch(1));
    Scene.Shaft->ActivateSwitchB();
    TestFalse(TEXT("An early B attempt leaves the lift unpowered"),Scene.Shaft->bLiftPowered);
    TestFalse(TEXT("A cannot be operated from the far landing"),Scene.Shaft->CanUseSwitch(0));
    Scene.Place(FVector(3000,2680,62.5));Scene.Frames(90);
    TestTrue(TEXT("The robot actually occupies the plate before A"),Scene.Shaft->bPlateOccupied);
    TestTrue(TEXT("An unpowered plate cannot raise the access gate"),Scene.Shaft->GateOpenProgress<.01f);
    Scene.Place(FVector(3180,1650,62.5));
    TestTrue(TEXT("A is accessible from its front display"),Scene.Shaft->CanUseSwitch(0));
    AStaticMeshActor* Wall=Scene.Box(FVector(3180,1705,110),FVector(120,20,200));
    TestFalse(TEXT("A cannot be used through a wall"),Scene.Shaft->CanUseSwitch(0));
    Scene.Shaft->ActivateSwitchA();
    TestFalse(TEXT("Blocked A leaves the conveyor unpowered"),Scene.Shaft->bConveyorPowered);
    Wall->SetActorEnableCollision(false);
    Scene.Shaft->ActivateSwitchA();Scene.Frames(300);
    TestTrue(TEXT("A powers the conveyor after valid front interaction"),Scene.Shaft->bConveyorPowered);
    TestFalse(TEXT("A does not power the lift"),Scene.Shaft->bLiftPowered);
    TestTrue(TEXT("The unpowered lift remains at its lower stop"),Scene.Shaft->LiftPositionFraction<.01f);
    TestTrue(TEXT("The core door remains closed before B"),Scene.Shaft->CoreDoorOpenProgress<.01f);
    Scene.Place(FVector(3000,3600,62.5));
    TestFalse(TEXT("B also requires latching the access gate"),Scene.Shaft->CanUseSwitch(1));
    if (!Scene.LatchGate(*this)) return false;
    Scene.Place(FVector(3150,3590,62.5));
    TestTrue(TEXT("B becomes usable after the gate sequence"),Scene.Shaft->CanUseSwitch(1));
    AStaticMeshActor* BWall=Scene.Box(FVector(3150,3640,110),FVector(120,20,200));
    Scene.Shaft->ActivateSwitchB();
    TestFalse(TEXT("A wall also blocks B's interaction"),Scene.Shaft->bLiftPowered);
    BWall->SetActorEnableCollision(false);
    Scene.Shaft->ActivateSwitchB();Scene.Frames(90);
    TestTrue(TEXT("B powers the lift"),Scene.Shaft->bLiftPowered);
    TestTrue(TEXT("B raises the core entrance door"),Scene.Shaft->CoreDoorOpenProgress>.99f);
    TestTrue(TEXT("Core door collision follows its moving panel"),FMath::IsNearlyEqual(Scene.CoreCollider->GetActorLocation().Z,470.0,1.0));
    TestFalse(TEXT("A completed switch cannot retrigger"),Scene.Shaft->CanUseSwitch(0));
    TestFalse(TEXT("B completed switch cannot retrigger"),Scene.Shaft->CanUseSwitch(1));
    TestFalse(TEXT("Invalid switch indices cannot activate controls"),Scene.Shaft->CanUseSwitch(-1)||Scene.Shaft->CanUseSwitch(2));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftGateTest,"REBOOT.EmergencyShaft.NativeGateTimerDoorwayGuardAndFastLatch",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftGateTest::RunTest(const FString& Parameters)
{
    FShaftTestWorld Scene;
    if (!Scene.Create(*this) || !Scene.PowerA(*this)) return false;
    Scene.Place(FVector(3000,2890,62.5));Scene.Frames(90);
    TestTrue(TEXT("Unarmed doorway proximity cannot open the access gate"),Scene.Shaft->GateOpenProgress<.01f);
    Scene.Place(FVector(3000,3120,62.5));Scene.Frames(2);
    TestFalse(TEXT("A far-side position alone cannot bypass the plate"),Scene.Shaft->bGateLatched);
    Scene.Place(FVector(3000,2680,62.5));Scene.Frames(480);
    TestEqual(TEXT("Eight seconds on the plate preserves the full six-second countdown"),Scene.Shaft->GateCountdown,6.0f);
    TestTrue(TEXT("The plate holds the actual access gate open"),Scene.Shaft->GateOpenProgress>.99f);
    TestTrue(TEXT("Access panel collision stays attached"),Scene.GateCollider->GetAttachParentActor()==Scene.Gate);
    TestTrue(TEXT("The access collider rises with the panel"),FMath::IsNearlyEqual(Scene.GateCollider->GetActorLocation().Z,470.0,1.0));
    Scene.Place(FVector(3000,2820,62.5));Scene.Frames(300);
    TestTrue(TEXT("The countdown runs only after leaving the plate"),Scene.Shaft->GateCountdown>.9f&&Scene.Shaft->GateCountdown<1.1f);
    Scene.Place(FVector(3000,2920,62.5));Scene.Frames(120);
    TestEqual(TEXT("The timer expires while the doorway is occupied"),Scene.Shaft->GateCountdown,0.0f);
    TestTrue(TEXT("An armed gate stays open over the occupied doorway"),Scene.Shaft->GateOpenProgress>.99f);
    TestFalse(TEXT("Standing in the doorway does not falsely latch the gate"),Scene.Shaft->bGateLatched);
    Scene.Place(FVector(3000,2800,62.5));Scene.Frames(75);
    TestTrue(TEXT("A missed timer closes the clear gate for another attempt"),Scene.Shaft->GateOpenProgress<.01f);
    TestTrue(TEXT("The collider returns to its closed position"),FMath::IsNearlyEqual(Scene.GateCollider->GetActorLocation().Z,155.0,1.0));
    Scene.Place(FVector(3000,2680,62.5));Scene.Frames(2);
    for (int32 Index=0;Index<90&&!Scene.Shaft->bGateLatched;++Index) Scene.Frame(FVector::YAxisVector);
    Scene.Pawn->GetCharacterMovement()->StopMovementImmediately();
    TestTrue(TEXT("A walking robot latches the gate on its first fast crossing"),Scene.Shaft->bGateLatched);
    TestTrue(TEXT("The legitimate crossing registers before the panel finishes rising"),Scene.Shaft->GateOpenProgress<.95f);
    TestTrue(TEXT("The whole capsule has crossed the access gate plane"),Scene.Pawn->GetActorLocation().Y>3013);
    Scene.Frames(480);
    TestTrue(TEXT("The completed gate remains latched beyond later timers"),Scene.Shaft->bGateLatched&&Scene.Shaft->GateOpenProgress>.99f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftBeltTest,"REBOOT.EmergencyShaft.PoweredGroundedConveyorTransport",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftBeltTest::RunTest(const FString& Parameters)
{
    FShaftTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Place(FVector(3000,2210,62.5));Scene.Frames(30);
    TestTrue(TEXT("An unpowered belt remains stationary"),FMath::IsNearlyEqual(Scene.Pawn->GetActorLocation().Y,2210.0,1.0));
    if (!Scene.PowerA(*this)) return false;
    Scene.Place(FVector(3000,2210,62.5));Scene.Frames(10);
    const double Before=Scene.Pawn->GetActorLocation().Y;
    Scene.Frames(30);
    const double Travel=Scene.Pawn->GetActorLocation().Y-Before;
    TestTrue(FString::Printf(TEXT("The powered belt carries a grounded robot north (%.2f cm)"),Travel),Travel>40&&Travel<100);
    Scene.Pawn->GetCharacterMovement()->Velocity=FVector(0,100,0);
    Scene.Shaft->Tick(.05f);
    TestTrue(TEXT("Conveyor transport preserves the robot's own movement velocity"),FMath::IsNearlyEqual(Scene.Pawn->GetCharacterMovement()->Velocity.Y,100.0,.01));
    Scene.Place(FVector(3000,2250,150),MOVE_Falling);Scene.Shaft->Tick(.25f);
    TestTrue(TEXT("The belt does not pull an airborne robot"),FMath::IsNearlyEqual(Scene.Pawn->GetActorLocation().Y,2250.0,.01));
    Scene.Place(FVector(3000,2250,-20));Scene.Shaft->Tick(.25f);
    TestTrue(TEXT("The belt does not pull a robot below its walking surface"),FMath::IsNearlyEqual(Scene.Pawn->GetActorLocation().Y,2250.0,.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftGapWalkTest,"REBOOT.EmergencyShaft.ShortJumpRejectsWalking",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftGapWalkTest::RunTest(const FString& Parameters)
{
    FShaftTestWorld Scene;
    if (!Scene.Create(*this) || !Scene.LatchGate(*this)) return false;
    Scene.Place(FVector(3000,3175,62.5));Scene.Frames(1);
    Scene.Pawn->GetCharacterMovement()->Velocity=FVector(0,800,0);
    bool LandedFar=false;
    for (int32 Index=0;Index<100&&Scene.Shaft->FallResetCount==0;++Index)
    {
        Scene.Frame(FVector::YAxisVector);
        LandedFar|=Scene.Pawn->GetActorLocation().Y>=3500&&Scene.Pawn->GetCharacterMovement()->IsMovingOnGround();
    }
    TestFalse(TEXT("The mandatory short gap cannot be walked across"),LandedFar);
    TestEqual(TEXT("Walking into the gap triggers entrance recovery"),Scene.Shaft->FallResetCount,1);
    TestTrue(TEXT("A missed jump preserves conveyor power and the gate latch"),Scene.Shaft->bConveyorPowered&&Scene.Shaft->bGateLatched);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftGapJumpTest,"REBOOT.EmergencyShaft.NormalJumpReachesSwitchBLanding",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftGapJumpTest::RunTest(const FString& Parameters)
{
    FShaftTestWorld Scene;
    if (!Scene.Create(*this) || !Scene.LatchGate(*this)) return false;
    Scene.Place(FVector(3000,3175,62.5));Scene.Frames(1);
    Scene.Pawn->GetCharacterMovement()->Velocity=FVector(0,800,0);Scene.Pawn->Jump();
    bool Airborne=false,Landed=false;
    for (int32 Index=0;Index<100&&Scene.Shaft->FallResetCount==0;++Index)
    {
        Scene.Frame(FVector::YAxisVector);
        Airborne|=Scene.Pawn->GetCharacterMovement()->IsFalling();
        if (Airborne&&Scene.Pawn->GetActorLocation().Y>3500&&Scene.Pawn->GetCharacterMovement()->IsMovingOnGround()) {Landed=true;break;}
    }
    TestTrue(TEXT("The robot leaves the gate landing in a real jump"),Airborne);
    TestTrue(TEXT("A normal robot jump reaches the fixed Switch B landing"),Landed);
    TestEqual(TEXT("A successful short jump avoids recovery"),Scene.Shaft->FallResetCount,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftLiftRiderTest,"REBOOT.EmergencyShaft.NativeLiftRiderAutomaticRoundTrip",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftLiftRiderTest::RunTest(const FString& Parameters)
{
    FShaftTestWorld Scene;
    if (!Scene.Create(*this) || !Scene.PowerB(*this)) return false;
    Scene.Place(FVector(2600,3720,62.5));
    double MaxDeck=0,MaxPawn=0,MaxFootError=0;
    for (int32 Index=0;Index<610;++Index)
    {
        Scene.Frame();
        const double Deck=Scene.Lift->GetActorLocation().Z+35;
        const double Feet=Scene.Pawn->GetActorLocation().Z-Scene.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        MaxDeck=FMath::Max(MaxDeck,Deck);MaxPawn=FMath::Max(MaxPawn,Scene.Pawn->GetActorLocation().Z);
        MaxFootError=FMath::Max(MaxFootError,FMath::Abs(Feet-Deck));
    }
    TestTrue(TEXT("B powers the lift to its 300cm upper landing"),MaxDeck>299);
    TestTrue(TEXT("Native movement carries the robot up with the deck"),MaxPawn>359);
    TestTrue(FString::Printf(TEXT("The rider stays based on the moving lift (max foot error %.3f cm)"),MaxFootError),MaxFootError<6);
    TestTrue(TEXT("The automatic cycle returns the platform to its lower stop"),FMath::Abs(Scene.Lift->GetActorLocation().Z+35)<.1);
    TestTrue(TEXT("The robot returns on the safe lower deck"),FMath::Abs(Scene.Pawn->GetActorLocation().Z-62.15)<3);
    TestTrue(TEXT("Attached lift collision completes the same return trip"),FMath::Abs(Scene.LiftCollider->GetActorLocation().Z+17.5)<.1);
    TestEqual(TEXT("An ordinary lift ride does not trigger fall recovery"),Scene.Shaft->FallResetCount,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftLiftHeadroomTest,"REBOOT.EmergencyShaft.LiftHeadroomPausesAndResumes",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftLiftHeadroomTest::RunTest(const FString& Parameters)
{
    FShaftTestWorld Scene;
    if (!Scene.Create(*this) || !Scene.PowerB(*this)) return false;
    Scene.Place(FVector(2600,3720,62.5));
    // Copy the imported robot's actual inline responses; changing one channel
    // on a generic pawn does not necessarily remove its named Pawn profile.
    UClass* RobotClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/StackOBot/Blueprints/Character/BP_Bot.BP_Bot_C"));
    if (!TestNotNull(TEXT("The real robot collision defaults are available"),RobotClass)) return false;
    const ACharacter* RobotDefaults=RobotClass->GetDefaultObject<ACharacter>();
    Scene.Pawn->GetCapsuleComponent()->DestroyPhysicsState();
    Scene.Pawn->GetCapsuleComponent()->GetBodyInstance()->CopyBodyInstancePropertiesFrom(
        RobotDefaults->GetCapsuleComponent()->GetBodyInstance());
    Scene.Pawn->GetCapsuleComponent()->CreatePhysicsState();
    TestEqual(TEXT("The rider exercises the robot's inline Custom collision profile"),
        Scene.Pawn->GetCapsuleComponent()->GetCollisionProfileName(),FName(TEXT("Custom")));
    AStaticMeshActor* Ceiling=Scene.Box(FVector(2600,3720,220),FVector(180,180,20));
    for (int32 Index=0;Index<360&&!Scene.Shaft->bLiftObstructed;++Index) Scene.Frame();
    TestTrue(TEXT("Insufficient rider headroom pauses the lift"),Scene.Shaft->bLiftObstructed);
    const double Paused=Scene.Lift->GetActorLocation().Z;
    const float GateTimeBefore=Scene.Shaft->GateCountdown;
    TestTrue(TEXT("The independent gate countdown is still running when the lift first pauses"),GateTimeBefore>1.5f);
    Scene.Frames(60);
    TestTrue(TEXT("The blocked lift holds its safe travel position"),FMath::Abs(Scene.Lift->GetActorLocation().Z-Paused)<.1);
    TestTrue(TEXT("Pausing the lift does not pause the pressure gate countdown"),
        FMath::IsNearlyEqual(GateTimeBefore-Scene.Shaft->GateCountdown,1.0f,.05f));
    Ceiling->SetActorEnableCollision(false);Scene.Frames(120);
    TestFalse(TEXT("Clearing headroom resumes lift travel"),Scene.Shaft->bLiftObstructed);
    TestTrue(TEXT("The lift resumes its held phase toward the upper landing"),Scene.Lift->GetActorLocation().Z>Paused+100);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftLiftDescendingTest,"REBOOT.EmergencyShaft.DescendingLiftWaitsForClearance",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftLiftDescendingTest::RunTest(const FString& Parameters)
{
    FShaftTestWorld Scene;
    if (!Scene.Create(*this) || !Scene.PowerB(*this)) return false;
    Scene.Frames(310);
    TestTrue(TEXT("The empty lift reaches its upper stop"),Scene.Shaft->LiftPositionFraction>.99f);
    Scene.Box(FVector(2600,3720,-10),FVector(300,300,20));
    Scene.Place(FVector(2600,3720,62.5));Scene.Frames(420);
    TestTrue(TEXT("A robot under the returning platform pauses descent"),Scene.Shaft->bLiftObstructed);
    TestTrue(TEXT("The held lift remains above the robot's capsule"),Scene.Lift->GetActorLocation().Z>Scene.Pawn->GetActorLocation().Z+57);
    const double Held=Scene.Lift->GetActorLocation().Z;
    Scene.Frames(60);
    TestTrue(TEXT("An occupied underside keeps the platform stationary"),FMath::Abs(Scene.Lift->GetActorLocation().Z-Held)<.1);
    Scene.Place(FVector(3000,3600,62.5));Scene.Frames(180);
    TestFalse(TEXT("Clearing the underside resumes the return"),Scene.Shaft->bLiftObstructed);
    TestTrue(TEXT("The cleared platform completes its automatic return"),Scene.Shaft->LiftPositionFraction<.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaftFallPersistenceTest,"REBOOT.EmergencyShaft.FallRecoveryPreservesCompletedControls",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShaftFallPersistenceTest::RunTest(const FString& Parameters)
{
    FShaftTestWorld Scene;
    if (!Scene.Create(*this) || !Scene.PowerB(*this)) return false;
    Scene.Place(FVector(3500,2600,-180),MOVE_Falling);Scene.Frames(90);
    TestEqual(TEXT("A shaft fall triggers one entrance recovery"),Scene.Shaft->FallResetCount,1);
    TestTrue(TEXT("The robot returns to the shaft entrance checkpoint"),FVector::Dist2D(Scene.Pawn->GetActorLocation(),FVector(3000,1800,0))<2);
    TestTrue(TEXT("Recovery preserves completed Switch A"),Scene.Shaft->bConveyorPowered);
    TestTrue(TEXT("Recovery preserves the access gate's latch"),Scene.Shaft->bGateLatched);
    TestTrue(TEXT("Recovery preserves completed Switch B"),Scene.Shaft->bLiftPowered);
    TestTrue(TEXT("The persistent core entrance remains open"),Scene.Shaft->CoreDoorOpenProgress>.99f);
    Scene.Frames(600);
    TestTrue(TEXT("The lift continues cycling after recovery"),Scene.Shaft->bLiftPowered);
    TestTrue(TEXT("The latched gate remains open after later cycles"),Scene.Shaft->GateOpenProgress>.99f);
    return true;
}

#endif
