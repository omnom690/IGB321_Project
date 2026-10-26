#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "RebootElectricalController.h"
#include "RebootElectricalState.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectricalPhaseTest,"REBOOT.ElectricalBypass.WarningLiveSafeCycle",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FElectricalPhaseTest::RunTest(const FString& Parameters)
{
    FRebootElectricalState State;
    TestTrue(TEXT("The strip starts with a warning"),State.Phase()==ERebootElectricalPhase::Warning);
    State.Advance(.5f,false,false,false);
    TestTrue(TEXT("The first half-second is warning only"),State.Phase()==ERebootElectricalPhase::Warning);
    State.Advance(.5f,false,false,false);
    TestTrue(TEXT("Live begins after exactly one second of warning"),State.Phase()==ERebootElectricalPhase::Live);
    State.Advance(.5f,false,false,false);
    TestTrue(TEXT("The second half of the live second remains dangerous"),State.Phase()==ERebootElectricalPhase::Live);
    State.Advance(.5f,false,false,false);
    TestTrue(TEXT("The dark safe window begins at two seconds"),State.Phase()==ERebootElectricalPhase::Safe);
    State.Advance(1,false,false,false);
    TestTrue(TEXT("The safe window lasts a complete two seconds"),State.Phase()==ERebootElectricalPhase::Safe);
    const double Before=State.Elapsed;
    State.Advance(-50,false,false,false);
    TestEqual(TEXT("Negative time cannot rewind the electrical phase"),State.Elapsed,Before);
    State.Advance(1,false,false,false);
    TestTrue(TEXT("The next four-second cycle starts with warning"),State.Phase()==ERebootElectricalPhase::Warning);
    State.Isolate();
    TestFalse(TEXT("Isolation requires completing the gate first"),State.bIsolated);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectricalGateStateTest,"REBOOT.ElectricalBypass.SixSecondHoldDoorGuardAndLatch",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FElectricalGateStateTest::RunTest(const FString& Parameters)
{
    FRebootElectricalState State;
    State.Advance(.25f,false,true,true);
    TestFalse(TEXT("Approaching an unarmed gate cannot open it"),State.bGateRequestedOpen);
    TestFalse(TEXT("A far-side location alone cannot latch an unarmed gate"),State.bGateLatched);
    State.Advance(100,true,false,false);
    TestEqual(TEXT("Remaining on the plate never consumes the six-second hold"),State.GateHoldRemaining,6.0f);
    TestTrue(TEXT("An occupied plate requests the gate open"),State.bGateRequestedOpen);
    State.Advance(-10,false,false,false);
    TestEqual(TEXT("Negative time cannot increase or spend gate time"),State.GateHoldRemaining,6.0f);
    State.Advance(5.5f,false,false,false);
    TestTrue(TEXT("Five and a half seconds after departure leaves half a second"),FMath::IsNearlyEqual(State.GateHoldRemaining,.5f,.001f));
    State.Advance(1,false,false,true);
    TestEqual(TEXT("The gate timer expires even while a robot occupies the doorway"),State.GateHoldRemaining,0.0f);
    TestTrue(TEXT("An armed gate stays raised rather than closing on the robot"),State.bGateRequestedOpen);
    State.Advance(.25f,false,false,false);
    TestFalse(TEXT("An unlatched expired gate closes once its doorway is clear"),State.bGateRequestedOpen);
    TestFalse(TEXT("Closing disarms proximity-based opening"),State.bGateArmed);
    State.Advance(50,true,false,false);
    State.Advance(.5f,false,true,false);
    TestTrue(TEXT("Crossing a legitimately opened gate latches it"),State.bGateLatched);
    State.Advance(120,false,false,false);
    TestTrue(TEXT("The gate remains open after its timer expires when latched"),State.bGateRequestedOpen);
    State.Isolate();
    State.Advance(120,false,false,false);
    TestTrue(TEXT("Completed isolation is permanent within the session"),State.bIsolated);
    TestTrue(TEXT("An isolated strip remains safe across later cycles"),State.Phase()==ERebootElectricalPhase::Safe);
    return true;
}

namespace
{
struct FElectricalTestWorld
{
    UWorld* World=nullptr;
    UStaticMesh* Cube=nullptr;
    APlayerController* Player=nullptr;
    ACharacter* Pawn=nullptr;
    ARebootElectricalController* Electrical=nullptr;
    AStaticMeshActor* Gate=nullptr;
    AStaticMeshActor* GateCollider=nullptr;
    AStaticMeshActor* ShaftGate=nullptr;
    AStaticMeshActor* ShaftCollider=nullptr;
    FActorSpawnParameters Spawn;

    ~FElectricalTestWorld()
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
    bool Create(FAutomationTestBase& Test)
    {
        const UWorld::InitializationValues Init=UWorld::InitializationValues()
            .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
            .EnableTraceCollision(true).CreateFXSystem(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!Test.TestNotNull(TEXT("An isolated electrical physics world is available"),World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
        UStaticMesh* SwitchMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_switch_pedestal.SM_switch_pedestal"));
        if (!Test.TestNotNull(TEXT("Engine collision cube is available"),Cube)
            || !Test.TestNotNull(TEXT("Imported isolation pedestal is available"),SwitchMesh)) return false;
        Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Box(FVector(3000,300,-10),FVector(3200,2400,20));
        for (double Y : {-170.0,170.0}) Box(FVector(2600,Y,150),FVector(1800,40,300));
        Gate=Box(FVector(3200,0,2),FVector(1,1,1),false);
        GateCollider=Box(FVector(3200,0,155),FVector(24,238,306));
        GateCollider->AttachToActor(Gate,FAttachmentTransformRules::KeepWorldTransform);
        for (double Y : {-135.0,135.0}) Box(FVector(3200,Y,155),FVector(60,30,310));
        Box(FVector(3200,0,330),FVector(60,300,40));
        ShaftGate=Box(FVector(3000,1115,2),FVector(1,1,1),false);
        ShaftCollider=Box(FVector(3000,1115,155),FVector(238,24,306));
        ShaftCollider->AttachToActor(ShaftGate,FAttachmentTransformRules::KeepWorldTransform);
        AStaticMeshActor* Switch=World->SpawnActor<AStaticMeshActor>(FVector(3750,0,0),FRotator(0,-90,0),Spawn);
        Switch->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Switch->GetStaticMeshComponent()->SetStaticMesh(SwitchMesh);
        Switch->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("NoCollision"));
        Box(FVector(3750,0,7.5),FVector(70,70,15));
        Box(FVector(3750,0,57.5),FVector(40,40,85));
        Box(FVector(3750,0,110),FVector(70,70,20));
        AStaticMeshActor* Button=Box(FVector(3750,0,120),FVector(30,30,8),false);
        AStaticMeshActor* Plate=Box(FVector(2850,0,-7.5),FVector(118,118,8),false);
        Player=World->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Player->SetAsLocalPlayerController();
        Pawn=World->SpawnActor<ACharacter>(FVector(1950,0,62.5),FRotator::ZeroRotator,Spawn);
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
        Electrical=World->SpawnActor<ARebootElectricalController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Electrical->GateActor=Gate;
        Electrical->ShaftGateActor=ShaftGate;
        Electrical->PlateTopActor=Plate;
        Electrical->SwitchActor=Switch;
        Electrical->ButtonActor=Button;
        World->InitializeActorsForPlay(FURL());
        World->UpdateWorldComponents(false,false);
        World->BeginPlay();
        if (!World->HasBegunPlay()) World->GetWorldSettings()->NotifyBeginPlay();
        Test.TestTrue(TEXT("The electrical controller is registered for native ticking"),Electrical->PrimaryActorTick.IsTickFunctionRegistered());
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
    void Frames(int32 Count) { for (int32 Index=0;Index<Count;++Index) Frame(); }
    bool LatchGate(FAutomationTestBase& Test)
    {
        Place(FVector(2850,0,62.5));
        Frames(75);
        Test.TestTrue(TEXT("The occupied plate raises the actual panel"),Electrical->GateOpenProgress>.99f);
        Place(FVector(3340,0,62.5));
        Frames(4);
        return Test.TestTrue(TEXT("The native far-side trigger latches the opened gate"),Electrical->bGateLatched);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectricalHazardResetTest,"REBOOT.ElectricalBypass.NativeContactResetAndGroundedPad",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FElectricalHazardResetTest::RunTest(const FString& Parameters)
{
    FElectricalTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Frames(4);
    TestTrue(TEXT("Native ticking reports the warning phase"),Scene.Electrical->bElectricalWarning);
    TestTrue(TEXT("Warning arcs are visible"),Scene.Electrical->Arcs->IsVisible());
    Scene.Place(FVector(2390,0,62.5));
    Scene.Frames(20);
    TestEqual(TEXT("A warning is not damaging electrical contact"),Scene.Electrical->HazardResetCount,0);
    Scene.Frames(95);
    TestEqual(TEXT("Actual live-strip contact resets the robot once"),Scene.Electrical->HazardResetCount,1);
    TestTrue(TEXT("Electrical recovery returns to the grounded entry pad"),
        FVector::Dist2D(Scene.Pawn->GetActorLocation(),FVector(1950,0,0))<2);
    Scene.Frames(480);
    TestEqual(TEXT("The grounded pad remains safe over complete electrical cycles"),Scene.Electrical->HazardResetCount,1);
    TestFalse(TEXT("A hazard reset does not grant gate progression"),Scene.Electrical->bGateLatched);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectricalPhysicalGateTest,"REBOOT.ElectricalBypass.NativeTimerAndAttachedGateCollision",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FElectricalPhysicalGateTest::RunTest(const FString& Parameters)
{
    FElectricalTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Place(FVector(2850,0,62.5));
    Scene.Frames(480);
    TestTrue(TEXT("The robot actually occupies the plate"),Scene.Electrical->bPlateOccupied);
    TestEqual(TEXT("Waiting eight seconds on the plate preserves all six gate seconds"),Scene.Electrical->GateCountdown,6.0f);
    TestTrue(TEXT("The actual gate is fully open while the plate is held"),Scene.Electrical->GateOpenProgress>.99f);
    TestTrue(TEXT("Gate collision stays attached to its moving panel"),Scene.GateCollider->GetAttachParentActor()==Scene.Gate);
    TestTrue(TEXT("The gate's collider rises with its panel"),FMath::IsNearlyEqual(Scene.GateCollider->GetActorLocation().Z,470.0,1.0));
    Scene.Place(FVector(3030,0,62.5));
    Scene.Frames(300);
    TestTrue(TEXT("Leaving the plate begins the countdown"),Scene.Electrical->GateCountdown>.9f && Scene.Electrical->GateCountdown<1.1f);
    Scene.Place(FVector(3180,0,62.5));
    Scene.Frames(120);
    TestEqual(TEXT("The six-second countdown completes"),Scene.Electrical->GateCountdown,0.0f);
    TestTrue(TEXT("The raised panel protects an occupied doorway after timeout"),Scene.Electrical->GateOpenProgress>.99f);
    TestFalse(TEXT("Stopping inside the doorway does not count as crossing it"),Scene.Electrical->bGateLatched);
    Scene.Place(FVector(3060,0,62.5));
    Scene.Frames(75);
    TestTrue(TEXT("The expired unlatched panel closes after the robot leaves"),Scene.Electrical->GateOpenProgress<.01f);
    TestTrue(TEXT("Its collider returns to the closed position too"),FMath::IsNearlyEqual(Scene.GateCollider->GetActorLocation().Z,155.0,1.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectricalProximityTest,"REBOOT.ElectricalBypass.ClosedDoorProximityCannotBypassPlate",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FElectricalProximityTest::RunTest(const FString& Parameters)
{
    FElectricalTestWorld Scene;
    if (!Scene.Create(*this)) return false;
    Scene.Place(FVector(3145,0,62.5));
    Scene.Frames(120);
    TestTrue(TEXT("Approaching the closed door cannot raise the panel"),Scene.Electrical->GateOpenProgress<.01f);
    TestFalse(TEXT("The plate has not been occupied"),Scene.Electrical->bPlateOccupied);
    TestFalse(TEXT("The unarmed gate is not latched"),Scene.Electrical->bGateLatched);
    Scene.Place(FVector(3340,0,62.5));
    Scene.Frames(4);
    TestFalse(TEXT("A far-side position without opening the gate cannot grant its latch"),Scene.Electrical->bGateLatched);
    Scene.Place(FVector(3630,0,62.5));
    Scene.Electrical->ActivateIsolation();
    TestFalse(TEXT("The isolation switch cannot skip the plate-and-gate sequence"),Scene.Electrical->bIsolated);
    // A robot can clear the opening as soon as the panel rises above its capsule.
    // Requiring full panel travel would miss a fast legitimate crossing entirely.
    Scene.Place(FVector(2850,0,62.5));
    Scene.Frames(2);
    int32 FramesToLatch=0;
    for (;FramesToLatch<90 && !Scene.Electrical->bGateLatched;++FramesToLatch)
        Scene.Frame(FVector::XAxisVector);
    Scene.Pawn->GetCharacterMovement()->StopMovementImmediately();
    TestTrue(TEXT("A real walking robot latches the gate on its first fast crossing"),Scene.Electrical->bGateLatched);
    TestTrue(TEXT("A legitimate crossing latches before the panel reaches full height"),Scene.Electrical->GateOpenProgress<.95f);
    TestTrue(TEXT("The robot actually passed the gate plane"),Scene.Pawn->GetActorLocation().X>3273);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectricalIsolationAccessTest,"REBOOT.ElectricalBypass.IsolationRequiresRangeAndLineOfSight",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FElectricalIsolationAccessTest::RunTest(const FString& Parameters)
{
    FElectricalTestWorld Scene;
    if (!Scene.Create(*this) || !Scene.LatchGate(*this)) return false;
    Scene.Place(FVector(1950,0,62.5));
    TestFalse(TEXT("Isolation cannot be activated remotely from the waiting pad"),Scene.Electrical->CanActivateIsolation());
    Scene.Electrical->ActivateIsolation();
    TestFalse(TEXT("An out-of-range attempt leaves the hazard enabled"),Scene.Electrical->bIsolated);
    Scene.Place(FVector(3630,0,62.5));
    TestTrue(TEXT("The imported pedestal is usable from its west-facing front"),Scene.Electrical->CanActivateIsolation());
    AStaticMeshActor* Wall=Scene.Box(FVector(3670,0,110),FVector(20,160,180));
    TestFalse(TEXT("Solid geometry blocks using the isolation switch through a wall"),Scene.Electrical->CanActivateIsolation());
    Scene.Electrical->ActivateIsolation();
    TestFalse(TEXT("A blocked attempt leaves the strip live"),Scene.Electrical->bIsolated);
    Wall->SetActorEnableCollision(false);
    TestTrue(TEXT("Removing the obstruction restores valid front interaction"),Scene.Electrical->CanActivateIsolation());
    Scene.Electrical->ActivateIsolation();
    TestTrue(TEXT("A valid interaction isolates the local electrical strip"),Scene.Electrical->bIsolated);
    TestFalse(TEXT("Completed isolation cannot be retriggered"),Scene.Electrical->CanActivateIsolation());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectricalIsolationPersistenceTest,"REBOOT.ElectricalBypass.PersistentIsolationOpensShaftEntrance",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FElectricalIsolationPersistenceTest::RunTest(const FString& Parameters)
{
    FElectricalTestWorld Scene;
    if (!Scene.Create(*this) || !Scene.LatchGate(*this)) return false;
    TestTrue(TEXT("The shaft entrance stays shut before isolation"),Scene.Electrical->ShaftGateOpenProgress<.01f);
    Scene.Place(FVector(3630,0,62.5));
    Scene.Electrical->ActivateIsolation();
    Scene.Frames(90);
    TestTrue(TEXT("Isolation lifts the actual shaft entrance panel"),Scene.Electrical->ShaftGateOpenProgress>.99f);
    TestTrue(TEXT("The shaft panel carries its collider upward"),FMath::IsNearlyEqual(Scene.ShaftCollider->GetActorLocation().Z,470.0,1.0));
    Scene.Place(FVector(2390,0,62.5));
    Scene.Frames(600);
    TestTrue(TEXT("Isolation persists across several later hazard cycles"),Scene.Electrical->bIsolated);
    TestFalse(TEXT("Isolation permanently removes the live state"),Scene.Electrical->bElectricalLive);
    TestFalse(TEXT("Isolation permanently removes warning flashes"),Scene.Electrical->bElectricalWarning);
    TestFalse(TEXT("An isolated strip is visibly dark"),Scene.Electrical->Arcs->IsVisible());
    TestEqual(TEXT("The isolated strip's hazard light is off"),Scene.Electrical->HazardLight->Intensity,0.0f);
    TestEqual(TEXT("Returning through an isolated crossing does not reset the robot"),Scene.Electrical->HazardResetCount,0);
    TestTrue(TEXT("The first gate's completed latch persists"),Scene.Electrical->bGateLatched);
    TestTrue(TEXT("The shaft remains open for the next section"),Scene.Electrical->ShaftGateOpenProgress>.99f);
    return true;
}

#endif
