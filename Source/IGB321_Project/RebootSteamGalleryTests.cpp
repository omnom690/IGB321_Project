#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RebootSteamGalleryState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamGalleryPlateTimerTest,
    "REBOOT.SteamGallery.PlateReleaseAndRepress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSteamGalleryPlateTimerTest::RunTest(const FString& Parameters)
{
    FRebootSteamGalleryState State;
    State.Advance(0.0f, false, false, false);
    TestFalse(TEXT("An untouched plate leaves the gate closed"), State.bGateRequestedOpen);

    State.Advance(0.0f, true, false, false);
    TestTrue(TEXT("Stepping onto the plate opens the gate immediately"), State.bGateRequestedOpen);
    State.Advance(30.0f, true, false, false);
    TestTrue(TEXT("A long plate hold never consumes the release window"), State.bGateRequestedOpen);
    TestEqual(TEXT("The full release window remains after holding"), State.GateHoldRemaining, 6.0f);

    State.Advance(5.75f, false, false, false);
    TestTrue(TEXT("The robot still has passage just before the six-second deadline"), State.bGateRequestedOpen);
    State.Advance(0.25f, false, false, false);
    TestFalse(TEXT("The unlatched gate closes at the six-second deadline"), State.bGateRequestedOpen);
    State.Advance(10.0f, false, false, false);
    TestEqual(TEXT("Waiting longer cannot make the countdown negative"), State.GateHoldRemaining, 0.0f);

    State.Advance(0.0f, true, false, false);
    State.Advance(4.0f, false, false, false);
    State.Advance(0.0f, true, false, false);
    TestEqual(TEXT("Repressing the plate restores a full six seconds"), State.GateHoldRemaining, 6.0f);
    State.Advance(5.5f, false, false, false);
    TestTrue(TEXT("A fresh release receives a fresh window"), State.bGateRequestedOpen);
    State.Advance(0.5f, false, false, false);
    TestFalse(TEXT("The refreshed window also expires normally"), State.bGateRequestedOpen);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamGalleryDoorwayTest,
    "REBOOT.SteamGallery.DoorwayProtectionAndLatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSteamGalleryDoorwayTest::RunTest(const FString& Parameters)
{
    FRebootSteamGalleryState State;
    State.Advance(0.0f, true, false, false);
    State.Advance(7.0f, false, false, true);
    TestTrue(TEXT("An occupied doorway keeps the expired gate open"), State.bGateRequestedOpen);
    TestEqual(TEXT("Doorway protection does not freeze the plate countdown"), State.GateHoldRemaining, 0.0f);
    TestFalse(TEXT("Standing in the doorway does not permanently latch it"), State.bGateLatched);
    State.Advance(0.0f, false, false, false);
    TestFalse(TEXT("The expired gate closes once the doorway clears"), State.bGateRequestedOpen);

    State.Advance(0.0f, false, true, false);
    TestTrue(TEXT("Fully crossing the far side latches the gate"), State.bGateLatched);
    TestTrue(TEXT("Crossing opens the return route immediately"), State.bGateRequestedOpen);
    State.Advance(120.0f, false, false, false);
    TestTrue(TEXT("The return route remains open after leaving every trigger"), State.bGateRequestedOpen);
    State.Advance(1.0f, true, false, true);
    State.Advance(20.0f, false, false, false);
    TestTrue(TEXT("Re-entering and leaving cannot cancel the latch"), State.bGateLatched);
    TestTrue(TEXT("A latched gate ignores later plate expiry"), State.bGateRequestedOpen);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamGallerySteamCycleTest,
    "REBOOT.SteamGallery.StaggeredSteamWindows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSteamGallerySteamCycleTest::RunTest(const FString& Parameters)
{
    FRebootSteamGalleryState State;
    TestFalse(TEXT("The first crossing starts with a clear window"), State.IsSteamActive(0));
    TestTrue(TEXT("The second vent starts on the opposite phase"), State.IsSteamActive(1));

    State.Advance(1.5f, false, false, false);
    TestFalse(TEXT("First vent remains clear before two seconds"), State.IsSteamActive(0));
    TestTrue(TEXT("Second vent remains active before two seconds"), State.IsSteamActive(1));
    State.Advance(0.5f, false, false, false);
    TestTrue(TEXT("First vent activates at two seconds"), State.IsSteamActive(0));
    TestFalse(TEXT("Second vent clears at two seconds"), State.IsSteamActive(1));
    State.Advance(1.5f, false, false, false);
    TestTrue(TEXT("First vent stays active for the full active window"), State.IsSteamActive(0));
    TestFalse(TEXT("Second vent stays clear for the full clear window"), State.IsSteamActive(1));
    State.Advance(0.5f, false, false, false);
    TestFalse(TEXT("The first vent clears at the next four-second boundary"), State.IsSteamActive(0));
    TestTrue(TEXT("The second vent repeats its active phase"), State.IsSteamActive(1));

    State.Advance(10.0f, false, false, false);
    TestTrue(TEXT("Skipping several cycles still yields the correct first-vent phase"), State.IsSteamActive(0));
    TestFalse(TEXT("Skipping several cycles preserves the second-vent offset"), State.IsSteamActive(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamGalleryIsolationTest,
    "REBOOT.SteamGallery.PermanentIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSteamGalleryIsolationTest::RunTest(const FString& Parameters)
{
    FRebootSteamGalleryState State;
    State.Advance(2.5f, true, true, false);
    TestTrue(TEXT("The first vent is active before isolation"), State.IsSteamActive(0));
    State.IsolateSteam();
    TestFalse(TEXT("Isolation stops an active first vent immediately"), State.IsSteamActive(0));
    TestFalse(TEXT("Isolation keeps the other vent disabled"), State.IsSteamActive(1));
    TestTrue(TEXT("Isolation preserves the completed gate latch"), State.bGateLatched);
    for (int32 Step = 0; Step < 12; ++Step)
    {
        State.Advance(0.5f, false, false, false);
        TestFalse(TEXT("The first vent cannot restart on later phase boundaries"), State.IsSteamActive(0));
        TestFalse(TEXT("The second vent cannot restart on later phase boundaries"), State.IsSteamActive(1));
    }
    State.IsolateSteam();
    State.Advance(1000.0f, false, false, false);
    TestTrue(TEXT("Repeated isolation remains permanent"), State.bSteamIsolated);
    TestTrue(TEXT("The gate's return route remains available"), State.bGateRequestedOpen);
    TestFalse(TEXT("Large elapsed times do not reactivate the first vent"), State.IsSteamActive(0));
    TestFalse(TEXT("Large elapsed times do not reactivate the second vent"), State.IsSteamActive(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamGalleryNegativeDeltaTest,
    "REBOOT.SteamGallery.NonnegativeTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSteamGalleryNegativeDeltaTest::RunTest(const FString& Parameters)
{
    FRebootSteamGalleryState State;
    State.Advance(0.0f, true, false, false);
    State.Advance(2.5f, false, false, false);
    const double ElapsedBefore = State.Elapsed;
    const float HoldBefore = State.GateHoldRemaining;
    const bool bSteamBefore = State.IsSteamActive(0);
    State.Advance(-5.0f, false, false, false);
    TestEqual(TEXT("A negative delta cannot rewind steam time"), State.Elapsed, ElapsedBefore);
    TestEqual(TEXT("A negative delta cannot extend the release deadline"), State.GateHoldRemaining, HoldBefore);
    TestEqual(TEXT("A negative delta cannot change the current steam phase"), State.IsSteamActive(0), bSteamBefore);
    State.Advance(3.5f, false, false, false);
    TestFalse(TEXT("The original release deadline is still honored"), State.bGateRequestedOpen);
    return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RebootSteamGalleryController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamGalleryRuntimeTest,
    "REBOOT.SteamGallery.RuntimeController",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSteamGalleryRuntimeTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues()
        .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
        .EnableTraceCollision(true).CreateFXSystem(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,
        ERHIFeatureLevel::Num,&Init);
    if (!TestNotNull(TEXT("Isolated physics world created"),World)) return false;
    struct FWorldCleanup
    {
        UWorld* World;
        ~FWorldCleanup() { World->DestroyWorld(false); }
    } Cleanup{World};

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Engine cube is available"),Cube)) return false;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const auto MeshActor = [&](FVector Position,FVector Size,bool Collision)
    {
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator,Spawn);
        UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetStaticMesh(Cube);
        Component->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
        Actor->SetActorScale3D(Size/100.0);
        return Actor;
    };
    MeshActor(FVector(3000,0,-10),FVector(4000,4000,20),true);
    AStaticMeshActor* Gate = MeshActor(FVector(2315,0,2),FVector(24,238,306),false);
    AStaticMeshActor* GateCollider = MeshActor(FVector(2315,0,155),FVector(24,238,306),true);
    GateCollider->AttachToActor(Gate,FAttachmentTransformRules::KeepWorldTransform);
    AStaticMeshActor* Plate = MeshActor(FVector(2020,0,-7.5),FVector(118,118,8),false);
    AStaticMeshActor* Valve = MeshActor(FVector(3733,450,110),FVector(1),false);
    APlayerController* Player = World->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
    ACharacter* Pawn = World->SpawnActor<ACharacter>(FVector(2020,0,62.5),FRotator::ZeroRotator,Spawn);
    Pawn->GetCapsuleComponent()->SetCapsuleSize(33.373f,60.0f,true);
    Pawn->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
    Player->Possess(Pawn);
    ARebootSteamGalleryController* Gallery = World->SpawnActor<ARebootSteamGalleryController>(
        FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
    Gallery->GateActor=Gate;
    Gallery->PlateTopActor=Plate;
    Gallery->ValvePivotActor=Valve;
    World->InitializeActorsForPlay(FURL());
    Gallery->DispatchBeginPlay();
    World->UpdateWorldComponents(false,false);

    Gallery->Tick(0.1f);
    TestTrue(TEXT("Real pawn collision occupies the pressure plate"),Gallery->bPlateOccupied);
    Gallery->Tick(1.0f);
    TestTrue(TEXT("The pressure plate raises the gate fully"),FMath::IsNearlyEqual(Gallery->GateOpenProgress,1.0f));
    TestTrue(TEXT("The attached gate collider moves with the door"),FMath::IsNearlyEqual(GateCollider->GetActorLocation().Z,470.0,0.1));
    Pawn->SetActorLocation(FVector(2180,0,62.5));
    Gallery->Tick(5.5f);
    TestTrue(TEXT("Leaving the plate retains the timed passage"),Gallery->GateOpenProgress>0.99f);
    Gallery->Tick(1.5f);
    TestTrue(TEXT("After expiry the gate returns to its closed position"),Gallery->GateOpenProgress<0.01f);

    Pawn->SetActorLocation(FVector(2490,0,62.5));
    Gallery->Tick(1.0f);
    TestTrue(TEXT("Fully passing the far-side volume latches the gate"),Gallery->bGateLatched);
    for (int32 Step=0; Step<8 && !Gallery->IsVentActive(0); ++Step) Gallery->Tick(0.5f);
    TestTrue(TEXT("An active first-vent window is available for the hazard test"),Gallery->IsVentActive(0));
    Pawn->SetActorLocation(FVector(2825,0,62.5));
    Gallery->Tick(0.0f);
    TestEqual(TEXT("Active steam performs one hazard reset"),Gallery->HazardResetCount,1);
    TestTrue(TEXT("The first reset returns to the entry-side safe ground"),Pawn->GetActorLocation().Equals(FVector(2550,0,62.5),0.5));
    Pawn->SetActorLocation(FVector(3175,300,62.5));
    Gallery->Tick(1.0f);
    Pawn->SetActorLocation(FVector(Gallery->IsVentActive(0)?2825:3525,0,62.5));
    Gallery->Tick(0.0f);
    TestEqual(TEXT("A later active vent performs another reset"),Gallery->HazardResetCount,2);
    TestTrue(TEXT("Reaching the waiting pad updates the reset location"),Pawn->GetActorLocation().Equals(FVector(3175,300,62.5),0.5));

    Pawn->SetActorLocation(FVector(2550,0,62.5));
    TestFalse(TEXT("The valve cannot be used remotely"),Gallery->CanInteractWithValve());
    Pawn->SetActorLocation(FVector(3630,450,62.5));
    TestTrue(TEXT("An unobstructed nearby robot can reach the valve"),Gallery->CanInteractWithValve());
    AStaticMeshActor* Blocker = MeshActor(FVector(3680,450,115),FVector(20,180,200),true);
    TestFalse(TEXT("A wall blocks valve interaction"),Gallery->CanInteractWithValve());
    Gallery->ActivateIsolationValve();
    TestFalse(TEXT("Blocked activation cannot isolate the vents"),Gallery->bSteamIsolated);
    Blocker->SetActorEnableCollision(false);
    Gallery->ActivateIsolationValve();
    TestTrue(TEXT("The reachable valve isolates both vents"),Gallery->bSteamIsolated);
    Gallery->Tick(10.0f);
    TestFalse(TEXT("First steam vent remains disabled"),Gallery->IsVentActive(0));
    TestFalse(TEXT("Second steam vent remains disabled"),Gallery->IsVentActive(1));
    TestTrue(TEXT("Isolation keeps the latched return gate open"),Gallery->bGateLatched);
    return true;
}

#endif
