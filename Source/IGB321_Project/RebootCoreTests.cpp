#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "RebootCoreController.h"
#include "RebootCoreState.h"
#include "RebootGameInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "InputCoreTypes.h"
#include "PhysicsEngine/BodyInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreRelayOrdersTest,"REBOOT.CentralCore.AllRelayOrdersAndContinuousRebootHold",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreRelayOrdersTest::RunTest(const FString& Parameters)
{
    const int32 Orders[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    for (int32 OrderIndex=0;OrderIndex<6;++OrderIndex)
    {
        FRebootCoreState State;
        TestFalse(TEXT("Invalid relay indices are rejected"),State.ActivateRelay(-1)||State.ActivateRelay(3));
        TestFalse(TEXT("Access relay C requires its own gate latch"),State.ActivateRelay(2));
        uint8 ExpectedMask=0;
        for (int32 Position=0;Position<3;++Position)
        {
            const int32 Relay=Orders[OrderIndex][Position];
            if (Relay==2)
            {
                State.Advance(0,true,false,false,false,false);
                State.Advance(0,false,true,false,false,false);
            }
            TestTrue(FString::Printf(TEXT("Order %d accepts relay %d independently"),OrderIndex,Relay),State.ActivateRelay(Relay));
            ExpectedMask|=1<<Relay;
            TestEqual(TEXT("Completing a relay preserves all earlier relay flags"),State.RelayMask,ExpectedMask);
            TestFalse(TEXT("An activated relay cannot be triggered twice"),State.ActivateRelay(Relay));
            TestEqual(TEXT("All-relay readiness changes only after the third relay"),State.AllRelaysActive(),Position==2);
            if (Position<2)
            {
                State.Advance(2,false,false,false,true,true);
                TestEqual(TEXT("Holding the console cannot reboot before all three relays"),State.RebootHeldSeconds,0.0f);
                TestFalse(TEXT("An incomplete relay set cannot start the ending"),State.bFinaleStarted);
            }
        }
        State.Advance(.5f,false,false,false,true,true);
        TestFalse(TEXT("A short half-second press does not start the ending"),State.bFinaleStarted);
        State.Advance(0,false,false,false,true,false);
        TestEqual(TEXT("Releasing the key cancels an unfinished hold"),State.RebootHeldSeconds,0.0f);
        State.Advance(.5f,false,false,false,true,true);
        State.Advance(.5f,false,false,false,false,true);
        TestEqual(TEXT("Leaving the console pad cancels an unfinished hold"),State.RebootHeldSeconds,0.0f);
        State.Advance(.5f,false,false,false,true,true);
        const double ElapsedBefore=State.Elapsed;
        State.Advance(-10,false,false,false,true,true);
        TestEqual(TEXT("Negative time cannot advance the core clock"),State.Elapsed,ElapsedBefore);
        TestEqual(TEXT("Negative time cannot complete a reboot hold"),State.RebootHeldSeconds,.5f);
        State.Advance(.5f,false,false,false,true,true);
        TestTrue(TEXT("A continuous one-second eligible hold starts the ending"),State.bFinaleStarted);
        TestEqual(TEXT("The fifteen-second ending starts after the hold finishes"),State.FinaleElapsed,0.0);
        TestFalse(TEXT("The ending is not complete as soon as it starts"),State.bComplete);
        State.Advance(14.75f,false,false,false,false,false);
        TestFalse(TEXT("The ending does not finish before fifteen seconds"),State.bComplete);
        State.Advance(.25f,false,false,false,false,false);
        TestTrue(TEXT("The ending completes after its full fifteen seconds"),State.bComplete);
        TestEqual(TEXT("Ending completion preserves all three independent relays"),State.RelayMask,static_cast<uint8>(7));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreElectricalPhaseTest,"REBOOT.CentralCore.ElectricalWarningLiveSafeAndIsolation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreElectricalPhaseTest::RunTest(const FString& Parameters)
{
    FRebootCoreState State;
    TestTrue(TEXT("The crossing starts with one second of warning"),State.ElectricalPhase()==ERebootCoreElectricalPhase::Warning);
    State.Advance(.5f,false,false,false,false,false);
    TestTrue(TEXT("The warning remains non-live for its first half-second"),State.ElectricalPhase()==ERebootCoreElectricalPhase::Warning);
    State.Advance(.5f,false,false,false,false,false);
    TestTrue(TEXT("The strip becomes live after one second"),State.ElectricalPhase()==ERebootCoreElectricalPhase::Live);
    State.Advance(1,false,false,false,false,false);
    TestTrue(TEXT("The dark two-second safe window begins at two seconds"),State.ElectricalPhase()==ERebootCoreElectricalPhase::Safe);
    State.Advance(1.5f,false,false,false,false,false);
    TestTrue(TEXT("The safe window remains clear until the cycle ends"),State.ElectricalPhase()==ERebootCoreElectricalPhase::Safe);
    State.Advance(.5f,false,false,false,false,false);
    TestTrue(TEXT("The four-second cycle returns to warning"),State.ElectricalPhase()==ERebootCoreElectricalPhase::Warning);
    TestTrue(TEXT("Isolation relay B can be completed before either other relay"),State.ActivateRelay(1));
    State.Advance(40,false,false,false,false,false);
    TestTrue(TEXT("Completed B permanently isolates its crossing"),State.ElectricalPhase()==ERebootCoreElectricalPhase::Safe);
    TestFalse(TEXT("Isolation does not grant the transport relay"),State.RelayActive(0));
    TestFalse(TEXT("Isolation does not grant the access relay"),State.RelayActive(2));
    return true;
}

namespace
{
struct FCoreTestWorld
{
    UWorld* World=nullptr;
    URebootGameInstance* Progress=nullptr;
    UStaticMesh* Cube=nullptr;
    APlayerController* Player=nullptr;
    ACharacter* Pawn=nullptr;
    ARebootCoreController* Core=nullptr;
    AStaticMeshActor* Shuttle=nullptr;
    AStaticMeshActor* ShuttleCollider=nullptr;
    AStaticMeshActor* BridgeA=nullptr;
    AStaticMeshActor* BridgeC=nullptr;
    AStaticMeshActor* Gate=nullptr;
    AStaticMeshActor* GateCollider=nullptr;
    AStaticMeshActor* Rotor=nullptr;
    ACameraActor* Camera=nullptr;
    APointLight* EmergencyLight=nullptr;
    FActorSpawnParameters Spawn;

    ~FCoreTestWorld()
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
        Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
        Actor->GetStaticMeshComponent()->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
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
    AStaticMeshActor* Relay(UStaticMesh* Asset,FVector Position,float Yaw)
    {
        AStaticMeshActor* Actor=Visual(Asset,Position,Yaw);
        Box(Position+FVector(0,0,8),FVector(100,90,16));
        const FVector Body=FMath::IsNearlyEqual(FMath::Abs(Yaw),90.0f)?FVector(65,80,152):FVector(80,65,152);
        Box(Position+FVector(0,0,92),Body);
        return Actor;
    }
    bool Create(FAutomationTestBase& Test)
    {
        const UWorld::InitializationValues Init=UWorld::InitializationValues()
            .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
            .EnableTraceCollision(true).CreateFXSystem(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!Test.TestNotNull(TEXT("An isolated core physics world is available"),World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        Progress=NewObject<URebootGameInstance>(World);World->SetGameInstance(Progress);
        Progress->CompleteSystem(0);Progress->CompleteSystem(1);
        Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
        UStaticMesh* ShuttleMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_shuttle_platform_300x200.SM_shuttle_platform_300x200"));
        UStaticMesh* RelayMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_power_relay.SM_power_relay"));
        UStaticMesh* ConsoleMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_3/Meshes/SM_core_reboot_station_180.SM_core_reboot_station_180"));
        if (!Test.TestNotNull(TEXT("Collision cube is available"),Cube)
            ||!Test.TestNotNull(TEXT("Imported shuttle is available"),ShuttleMesh)
            ||!Test.TestNotNull(TEXT("Imported relay is available"),RelayMesh)
            ||!Test.TestNotNull(TEXT("Imported reboot console is available"),ConsoleMesh)) return false;
        Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Box(FVector(-500,2800,-360),FVector(3260,2860,20));
        Box(FVector(225,2800,-10),FVector(1750,700,20));
        Box(FVector(-1825,2800,-10),FVector(550,700,20));
        Box(FVector(0,3310,-10),FVector(600,320,20));
        Box(FVector(0,3710,-10),FVector(300,480,20));
        Box(FVector(0,4075,-10),FVector(600,250,20));
        Box(FVector(0,2350,-10),FVector(600,200,20));
        Box(FVector(0,2130,-10),FVector(300,240,20));
        Box(FVector(0,1585,-10),FVector(600,370,20));
        BridgeA=Box(FVector(-1100,3050,-10),FVector(900,200,20));
        BridgeC=Box(FVector(400,1975,-10),FVector(200,950,20));
        Shuttle=Visual(ShuttleMesh,FVector(-800,2800,-35));
        ShuttleCollider=Box(FVector(-800,2800,-17.5),FVector(300,200,35));
        ShuttleCollider->AttachToActor(Shuttle,FAttachmentTransformRules::KeepWorldTransform);
        for (double Y : {2710.0,2890.0})
        {
            AStaticMeshActor* Rail=Box(FVector(-800,Y,55),FVector(300,20,110));
            Rail->AttachToActor(Shuttle,FAttachmentTransformRules::KeepWorldTransform);
        }
        Gate=Box(FVector(0,2190,2),FVector(1,1,1),false);
        GateCollider=Box(FVector(0,2190,78.5),FVector(238,24,153));
        GateCollider->AttachToActor(Gate,FAttachmentTransformRules::KeepWorldTransform);
        for (double X : {-135.0,135.0}) Box(FVector(X,2190,77.5),FVector(30,60,155));
        Box(FVector(0,2190,165),FVector(300,60,20));
        for (double X : {-170.0,170.0}) Box(FVector(X,2130,150),FVector(40,240,300));
        AStaticMeshActor* Plate=Box(FVector(0,2350,-7.5),FVector(118,118,8),false);
        AStaticMeshActor* A=Relay(RelayMesh,FVector(-1860,2800,0),90);
        AStaticMeshActor* B=Relay(RelayMesh,FVector(0,4110,0),0);
        AStaticMeshActor* C=Relay(RelayMesh,FVector(0,1540,0),180);
        AStaticMeshActor* Console=Visual(ConsoleMesh,FVector(550,2800,0));
        Box(FVector(550,2800,7.5),FVector(180,120,15));
        Box(FVector(550,2800,90),FVector(150,100,150));
        Box(FVector(550,2800,172.5),FVector(180,120,15));
        AStaticMeshActor* Column=Box(FVector(850,3020,250),FVector(196,196,500));
        Rotor=Box(FVector(750,3650,210),FVector(180,180,20),false);
        Camera=World->SpawnActor<ACameraActor>(FVector(950,2250,650),FRotator(-25,120,0),Spawn);
        EmergencyLight=World->SpawnActor<APointLight>(FVector(0,3400,600),FRotator::ZeroRotator,Spawn);
        Player=World->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Player->SetAsLocalPlayerController();
        Pawn=World->SpawnActor<ACharacter>(FVector(550,2580,62.5),FRotator::ZeroRotator,Spawn);
        Pawn->GetCapsuleComponent()->SetCapsuleSize(33.373f,60.0f,true);
        Pawn->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
        UCharacterMovementComponent* Movement=Pawn->GetCharacterMovement();
        Movement->bRunPhysicsWithNoController=true;Movement->bUpdateOnlyIfRendered=false;
        Movement->MaxWalkSpeed=800;Movement->JumpZVelocity=400;Movement->GravityScale=2;
        Movement->MaxStepHeight=45;Movement->PerchRadiusThreshold=0;Movement->AirControl=1;
        Movement->SetMovementMode(MOVE_Walking);Player->Possess(Pawn);
        Core=World->SpawnActor<ARebootCoreController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        Core->RelayActors={A,B,C};Core->ConsoleActor=Console;Core->CoreActor=Column;
        Core->ShuttleActor=Shuttle;Core->ShuttleCollisionActor=ShuttleCollider;
        Core->ReturnBridgeA=BridgeA;Core->ReturnBridgeC=BridgeC;Core->GateActor=Gate;Core->PlateTopActor=Plate;
        Core->FinaleCameraActor=Camera;Core->BackgroundRotor=Rotor;Core->EmergencyLights.Add(EmergencyLight);
        World->InitializeActorsForPlay(FURL());World->UpdateWorldComponents(false,false);World->BeginPlay();
        if (!World->HasBegunPlay()) World->GetWorldSettings()->NotifyBeginPlay();
        Test.TestTrue(TEXT("The core controller ticks natively"),Core->PrimaryActorTick.IsTickFunctionRegistered());
        Test.TestTrue(TEXT("The simulated robot is locally controlled"),Pawn->IsLocallyControlled());
        return true;
    }
    void Place(FVector Position,EMovementMode Mode=MOVE_Walking)
    {
        Pawn->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(Mode);
    }
    void Frame(FVector Input=FVector::ZeroVector)
    {
        if (!Input.IsNearlyZero()) Pawn->AddMovementInput(Input,1);
        ++GFrameCounter;UWorld* Previous=GWorld;GWorld=World;
        World->Tick(LEVELTICK_All,1.0f/60.0f);GWorld=Previous;
    }
    void Frames(int32 Count) { for (int32 Index=0;Index<Count;++Index) Frame(); }
    void Key(bool Pressed)
    {
        // Exercise the production input binding without a viewport or synthetic
        // PlayerInput object on this headless controller.
        Core->SetRebootInteractionHeld(Pressed);
    }
    bool LatchGate(FAutomationTestBase& Test)
    {
        Place(FVector(0,2350,62.5));Frames(75);
        if (!Test.TestTrue(TEXT("The core plate raises its actual panel"),Core->GateOpenProgress>.99f)) return false;
        Place(FVector(0,2050,62.5));Frames(4);
        return Test.TestTrue(TEXT("The southbound far trigger latches relay C's gate"),Core->bGateLatched);
    }
    bool Activate(int32 RelayIndex,FAutomationTestBase& Test)
    {
        if (RelayIndex==2&&!Core->bGateLatched&&!LatchGate(Test)) return false;
        const FVector Fronts[3]={FVector(-1680,2800,62.5),FVector(0,4015,62.5),FVector(0,1700,62.5)};
        Place(Fronts[RelayIndex]);Frames(2);
        if (!Test.TestTrue(FString::Printf(TEXT("Relay %d is reachable from its fixed landing"),RelayIndex),Core->CanActivateRelay(RelayIndex))) return false;
        Core->ActivateRelay(RelayIndex);Frames(1);
        return Test.TestTrue(TEXT("The reached relay records its own progress"),(Core->RelayMask&(1<<RelayIndex))!=0);
    }
    bool ActivateAll(FAutomationTestBase& Test)
    {
        return Activate(0,Test)&&Activate(1,Test)&&Activate(2,Test);
    }
    bool RobotCollision(FAutomationTestBase& Test)
    {
        UClass* RobotClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/StackOBot/Blueprints/Character/BP_Bot.BP_Bot_C"));
        if (!Test.TestNotNull(TEXT("The actual robot collision defaults are available"),RobotClass)) return false;
        const ACharacter* Defaults=RobotClass->GetDefaultObject<ACharacter>();
        UCapsuleComponent* Capsule=Pawn->GetCapsuleComponent();
        Capsule->DestroyPhysicsState();
        Capsule->GetBodyInstance()->CopyBodyInstancePropertiesFrom(Defaults->GetCapsuleComponent()->GetBodyInstance());
        Capsule->CreatePhysicsState();
        return Test.TestEqual(TEXT("The shuttle rider uses the robot's inline Custom collision settings"),Capsule->GetCollisionProfileName(),FName(TEXT("Custom")));
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreRelayAccessTest,"REBOOT.CentralCore.ScopedRelayAccessAndRestoredReturns",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreRelayAccessTest::RunTest(const FString& Parameters)
{
    FCoreTestWorld Scene;if (!Scene.Create(*this)) return false;
    TestFalse(TEXT("The transport return bridge begins unavailable"),Scene.BridgeA->GetActorEnableCollision());
    TestFalse(TEXT("The access return bridge begins unavailable"),Scene.BridgeC->GetActorEnableCollision());
    Scene.Place(FVector(0,1700,62.5));Scene.Frames(2);
    Scene.Core->ActivateRelay(2);
    TestEqual(TEXT("Relay C cannot skip its gate sequence"),Scene.Core->RelayMask,static_cast<uint8>(0));
    Scene.Place(FVector(550,2580,62.5));
    TestFalse(TEXT("Relays cannot be activated remotely from the hub"),Scene.Core->CanActivateRelay(0));
    Scene.Place(FVector(-1680,2800,150),MOVE_Falling);
    TestFalse(TEXT("Relay operation requires a safe grounded landing"),Scene.Core->CanActivateRelay(0));
    Scene.Place(FVector(-1680,2800,62.5));Scene.Frames(2);
    TestTrue(TEXT("Relay A can be completed before either other relay"),Scene.Core->CanActivateRelay(0));
    AStaticMeshActor* Wall=Scene.Box(FVector(-1750,2800,110),FVector(20,150,190));
    Scene.Core->ActivateRelay(0);
    TestEqual(TEXT("A wall blocks relay interaction"),Scene.Core->RelayMask,static_cast<uint8>(0));
    Wall->SetActorEnableCollision(false);Scene.Core->ActivateRelay(0);Scene.Frames(1);
    TestTrue(TEXT("A restores its safe bridge return"),Scene.BridgeA->GetActorEnableCollision()&&!Scene.BridgeA->IsHidden());
    TestFalse(TEXT("A does not restore C's independent route"),Scene.BridgeC->GetActorEnableCollision());
    if (!Scene.Activate(1,*this)||!Scene.Activate(2,*this)) return false;
    TestEqual(TEXT("All three relays remain independently completed"),Scene.Core->RelayMask,static_cast<uint8>(7));
    TestEqual(TEXT("The console counts three completed relays"),Scene.Core->CompletedRelayCount,3);
    TestTrue(TEXT("C restores its safe bridge return"),Scene.BridgeC->GetActorEnableCollision()&&!Scene.BridgeC->IsHidden());
    TestFalse(TEXT("A completed relay cannot retrigger"),Scene.Core->CanActivateRelay(2));
    TestFalse(TEXT("Invalid relay indices are rejected"),Scene.Core->CanActivateRelay(-1)||Scene.Core->CanActivateRelay(3));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreShuttleTest,"REBOOT.CentralCore.NativeShuttleCustomRiderAndClearance",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreShuttleTest::RunTest(const FString& Parameters)
{
    FCoreTestWorld Scene;if (!Scene.Create(*this)||!Scene.RobotCollision(*this)) return false;
    Scene.Place(FVector(-800,2800,62.5));
    double MinPawnX=-800,MaxDrift=0;
    for (int32 Index=0;Index<610;++Index)
    {
        Scene.Frame();MinPawnX=FMath::Min(MinPawnX,Scene.Pawn->GetActorLocation().X);
        MaxDrift=FMath::Max(MaxDrift,FMath::Abs(Scene.Pawn->GetActorLocation().X-Scene.Shuttle->GetActorLocation().X));
    }
    TestTrue(TEXT("The automatic shuttle carries the actual collision shape to A's landing"),MinPawnX<-1390);
    TestTrue(FString::Printf(TEXT("Native based movement keeps the rider on the shuttle (%.3f cm drift)"),MaxDrift),MaxDrift<6);
    TestTrue(TEXT("The shuttle returns automatically to the hub boarding stop"),FMath::Abs(Scene.Shuttle->GetActorLocation().X+800)<.1);
    TestTrue(TEXT("The robot returns on the shuttle"),FMath::Abs(Scene.Pawn->GetActorLocation().X+800)<3);
    TestTrue(TEXT("Collision stays attached to the moving shuttle"),Scene.ShuttleCollider->GetAttachParentActor()==Scene.Shuttle);
    AStaticMeshActor* Wall=Scene.Box(FVector(-1100,2800,90),FVector(20,150,200));
    for (int32 Index=0;Index<420&&!Scene.Core->bShuttleObstructed;++Index) Scene.Frame();
    TestTrue(TEXT("The shuttle pauses before carrying its rider through an obstacle"),Scene.Core->bShuttleObstructed);
    const double Held=Scene.Shuttle->GetActorLocation().X;Scene.Frames(60);
    TestTrue(TEXT("The obstructed shuttle holds its safe phase"),FMath::Abs(Scene.Shuttle->GetActorLocation().X-Held)<.1);
    Wall->SetActorEnableCollision(false);Scene.Frames(90);
    TestFalse(TEXT("Clearing the rider path resumes the shuttle"),Scene.Core->bShuttleObstructed);
    TestTrue(TEXT("The held travel resumes toward the far landing"),Scene.Shuttle->GetActorLocation().X<Held-100);
    TestEqual(TEXT("A normal shuttle trip does not reset the robot"),Scene.Core->FallResetCount,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreElectricResetTest,"REBOOT.CentralCore.LiveContactSafePadAndPermanentIsolation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreElectricResetTest::RunTest(const FString& Parameters)
{
    FCoreTestWorld Scene;if (!Scene.Create(*this)) return false;
    Scene.Place(FVector(0,3270,62.5));Scene.Frames(264);
    TestEqual(TEXT("The grounded waiting pad is safe through a full electrical cycle"),Scene.Core->HazardResetCount,0);
    for (int32 Index=0;Index<240&&!Scene.Core->bElectricalLive;++Index) Scene.Frame();
    TestTrue(TEXT("The native strip reaches its live phase"),Scene.Core->bElectricalLive);
    Scene.Place(FVector(0,3710,62.5));Scene.Frames(2);
    TestEqual(TEXT("Live electrical contact triggers one hub recovery"),Scene.Core->HazardResetCount,1);
    TestTrue(TEXT("Electrical recovery returns the robot to the safe hub"),FVector::Dist2D(Scene.Pawn->GetActorLocation(),FVector(550,2580,0))<2);
    Scene.Place(FVector(0,3900,62.5));
    TestFalse(TEXT("Relay B cannot be activated early while the capsule still overlaps the field"),Scene.Core->CanActivateRelay(1));
    Scene.Core->ActivateRelay(1);
    TestEqual(TEXT("The premature field-side attempt grants no relay progress"),Scene.Core->RelayMask,static_cast<uint8>(0));
    if (!Scene.Activate(1,*this)) return false;
    Scene.Place(FVector(0,3710,62.5));Scene.Frames(300);
    TestEqual(TEXT("Completed isolation prevents later electrical resets"),Scene.Core->HazardResetCount,1);
    TestFalse(TEXT("The isolated strip is no longer live"),Scene.Core->bElectricalLive);
    TestFalse(TEXT("The isolated strip stops warning"),Scene.Core->bElectricalWarning);
    TestFalse(TEXT("The isolated strip is visibly dark"),Scene.Core->Arcs->IsVisible());
    TestEqual(TEXT("The isolated strip's hazard light is off"),Scene.Core->HazardLight->Intensity,0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreGateTest,"REBOOT.CentralCore.PlateTimerDoorwayGuardAndFastAccessLatch",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreGateTest::RunTest(const FString& Parameters)
{
    FCoreTestWorld Scene;if (!Scene.Create(*this)) return false;
    Scene.Place(FVector(0,2240,62.5));Scene.Frames(90);
    TestTrue(TEXT("An unarmed doorway cannot open its own gate"),Scene.Core->GateOpenProgress<.01f);
    Scene.Place(FVector(0,2050,62.5));Scene.Frames(2);
    TestFalse(TEXT("A far-side location alone cannot grant the access latch"),Scene.Core->bGateLatched);
    Scene.Place(FVector(0,2350,62.5));Scene.Frames(480);
    TestEqual(TEXT("Standing on the plate does not spend its six-second hold"),Scene.Core->GateCountdown,6.0f);
    TestTrue(TEXT("The plate raises the actual panel"),Scene.Core->GateOpenProgress>.99f);
    TestTrue(TEXT("The half-height panel's collision rises with it"),FMath::IsNearlyEqual(Scene.GateCollider->GetActorLocation().Z,393.5,1.0));
    Scene.Place(FVector(0,2500,62.5));Scene.Frames(300);
    TestTrue(TEXT("The timer counts down after leaving the plate"),Scene.Core->GateCountdown>.9f&&Scene.Core->GateCountdown<1.1f);
    Scene.Place(FVector(0,2205,62.5));Scene.Frames(120);
    TestEqual(TEXT("The gate timer expires while its doorway is occupied"),Scene.Core->GateCountdown,0.0f);
    TestTrue(TEXT("The armed gate protects its occupied doorway"),Scene.Core->GateOpenProgress>.99f);
    TestFalse(TEXT("Doorway protection does not falsely grant the far latch"),Scene.Core->bGateLatched);
    Scene.Place(FVector(0,2500,62.5));Scene.Frames(75);
    TestTrue(TEXT("An unlatched clear gate closes for a retry"),Scene.Core->GateOpenProgress<.01f);
    Scene.Place(FVector(0,2350,62.5));Scene.Frames(2);
    for (int32 Index=0;Index<90&&!Scene.Core->bGateLatched;++Index) Scene.Frame(-FVector::YAxisVector);
    Scene.Pawn->GetCharacterMovement()->StopMovementImmediately();
    TestTrue(TEXT("A walking robot latches its first legitimate southbound crossing"),Scene.Core->bGateLatched);
    TestTrue(TEXT("The crossing registers before the panel finishes rising"),Scene.Core->GateOpenProgress<.95f);
    Scene.Frames(480);
    TestTrue(TEXT("The access latch persists beyond later gate timers"),Scene.Core->bGateLatched&&Scene.Core->GateOpenProgress>.99f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreJumpTest,"REBOOT.CentralCore.AccessGapRequiresAndAcceptsNormalJump",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreJumpTest::RunTest(const FString& Parameters)
{
    {
        FCoreTestWorld Scene;if (!Scene.Create(*this)||!Scene.LatchGate(*this)) return false;
        Scene.Place(FVector(0,2045,62.5));Scene.Frames(1);
        Scene.Pawn->GetCharacterMovement()->Velocity=FVector(0,-800,0);
        bool WalkedAcross=false;
        for (int32 Index=0;Index<100&&Scene.Core->FallResetCount==0;++Index)
        {
            Scene.Frame(-FVector::YAxisVector);
            WalkedAcross|=Scene.Pawn->GetActorLocation().Y<1770&&Scene.Pawn->GetCharacterMovement()->IsMovingOnGround();
        }
        TestFalse(TEXT("The access gap cannot be walked across without jumping"),WalkedAcross);
        TestEqual(TEXT("Walking into the access gap triggers hub recovery"),Scene.Core->FallResetCount,1);
    }
    {
        FCoreTestWorld Scene;if (!Scene.Create(*this)||!Scene.LatchGate(*this)) return false;
        Scene.Place(FVector(0,2045,62.5));Scene.Frames(1);
        Scene.Pawn->GetCharacterMovement()->Velocity=FVector(0,-800,0);Scene.Pawn->Jump();
        bool Airborne=false,Landed=false;
        for (int32 Index=0;Index<100&&Scene.Core->FallResetCount==0;++Index)
        {
            Scene.Frame(-FVector::YAxisVector);Airborne|=Scene.Pawn->GetCharacterMovement()->IsFalling();
            if (Airborne&&Scene.Pawn->GetActorLocation().Y<1770&&Scene.Pawn->GetCharacterMovement()->IsMovingOnGround()) {Landed=true;break;}
        }
        TestTrue(TEXT("The access route includes a real airborne jump"),Airborne);
        TestTrue(TEXT("The robot's normal jump reaches C's fixed landing"),Landed);
        TestEqual(TEXT("A successful short jump avoids recovery"),Scene.Core->FallResetCount,0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreRecoveryTest,"REBOOT.CentralCore.RecoveryPreservesIndependentRelayProgress",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreRecoveryTest::RunTest(const FString& Parameters)
{
    FCoreTestWorld Scene;if (!Scene.Create(*this)||!Scene.Activate(0,*this)||!Scene.Activate(2,*this)) return false;
    Scene.Place(FVector(-1100,2500,-180),MOVE_Falling);Scene.Frames(90);
    TestEqual(TEXT("A core fall returns once to the safe hub"),Scene.Core->FallResetCount,1);
    TestEqual(TEXT("Fall recovery preserves A and C independently"),Scene.Core->RelayMask,static_cast<uint8>(5));
    TestTrue(TEXT("Fall recovery preserves the access gate latch"),Scene.Core->bGateLatched);
    for (int32 Index=0;Index<240&&!Scene.Core->bElectricalLive;++Index) Scene.Frame();
    Scene.Place(FVector(0,3710,62.5));Scene.Frames(2);
    TestEqual(TEXT("The unfinished B route can still cause electrical recovery"),Scene.Core->HazardResetCount,1);
    TestEqual(TEXT("Electrical recovery also preserves completed A and C"),Scene.Core->RelayMask,static_cast<uint8>(5));
    TestTrue(TEXT("Both restored return bridges remain available after recovery"),Scene.BridgeA->GetActorEnableCollision()&&Scene.BridgeC->GetActorEnableCollision());
    if (!Scene.Activate(1,*this)) return false;
    Scene.Place(FVector(-1100,2500,-180),MOVE_Falling);Scene.Frames(90);
    TestEqual(TEXT("A later fall preserves all three completed relays"),Scene.Core->RelayMask,static_cast<uint8>(7));
    TestEqual(TEXT("The console still counts all three relays"),Scene.Core->CompletedRelayCount,3);
    TestFalse(TEXT("Recovery never starts a reboot by itself"),Scene.Core->bRebootStarted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreConsoleHoldTest,"REBOOT.CentralCore.NativeConsoleRequiresEligibleContinuousKeyHold",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreConsoleHoldTest::RunTest(const FString& Parameters)
{
    FCoreTestWorld Scene;if (!Scene.Create(*this)) return false;
    Scene.Frames(2);Scene.Key(true);Scene.Frames(75);
    TestFalse(TEXT("The interaction binding cannot reboot an incomplete console"),Scene.Core->bRebootStarted);
    TestEqual(TEXT("An incomplete console accumulates no reboot hold"),Scene.Core->RebootHoldProgress,0.0f);
    Scene.Key(false);Scene.Frames(1);
    if (!Scene.ActivateAll(*this)) return false;
    Scene.Place(FVector(550,2580,62.5));Scene.Frames(2);
    TestTrue(TEXT("The completed console is eligible from its marked safe pad"),Scene.Core->CanReboot());
    Scene.Key(true);Scene.Frames(30);
    TestTrue(TEXT("A bound half-second hold accumulates native partial progress"),Scene.Core->RebootHoldProgress>.4f&&Scene.Core->RebootHoldProgress<.65f);
    TestFalse(TEXT("A short press does not start the ending"),Scene.Core->bRebootStarted);
    Scene.Key(false);Scene.Frames(1);
    TestEqual(TEXT("Releasing E cancels the partial native hold"),Scene.Core->RebootHoldProgress,0.0f);
    Scene.Key(true);Scene.Frames(30);Scene.Place(FVector(900,2580,62.5));Scene.Frames(3);
    TestFalse(TEXT("Leaving the marked pad removes console eligibility"),Scene.Core->CanReboot());
    TestEqual(TEXT("Leaving the pad cancels an unfinished held interaction"),Scene.Core->RebootHoldProgress,0.0f);
    Scene.Key(false);Scene.Frames(1);Scene.Place(FVector(550,2580,62.5));Scene.Frames(2);
    AStaticMeshActor* Wall=Scene.Box(FVector(550,2670,110),FVector(200,20,220));
    TestFalse(TEXT("The console cannot be reached through an obstruction"),Scene.Core->CanReboot());
    Scene.Key(true);Scene.Frames(75);
    TestFalse(TEXT("Holding E through a wall cannot start the ending"),Scene.Core->bRebootStarted);
    Scene.Key(false);Scene.Frames(1);Wall->SetActorEnableCollision(false);
    Scene.Place(FVector(550,2580,150),MOVE_Falling);
    TestFalse(TEXT("The final interaction requires the robot to be grounded"),Scene.Core->CanReboot());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreFinaleTest,"REBOOT.CentralCore.FifteenSecondFinaleLocksSafeRobotAndCompletes",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreFinaleTest::RunTest(const FString& Parameters)
{
    FCoreTestWorld Scene;if (!Scene.Create(*this)||!Scene.ActivateAll(*this)) return false;
    Scene.Place(FVector(550,2580,62.5));Scene.Frames(2);
    Scene.Key(true);Scene.Frames(61);
    TestTrue(TEXT("The continuous interaction binding starts the final reboot"),Scene.Core->bRebootStarted);
    TestTrue(TEXT("The finale locks the robot onto its safe pad"),Scene.Core->bRobotLocked);
    TestTrue(TEXT("The locked robot's movement component is disabled"),Scene.Pawn->GetCharacterMovement()->MovementMode==MOVE_None);
    TestTrue(TEXT("Movement and look input are ignored during the ending"),Scene.Player->IsMoveInputIgnored()&&Scene.Player->IsLookInputIgnored());
    TestTrue(TEXT("The locked capsule stands at the safe console position"),FVector::Dist(Scene.Pawn->GetActorLocation(),FVector(550,2580,62.5))<.1);
    TestFalse(TEXT("The ending is not complete immediately after the hold"),Scene.Core->bFacilityRebootComplete);
    const int32 Resets=Scene.Core->FallResetCount+Scene.Core->HazardResetCount;
    const FRotator RotorBefore=Scene.Rotor->GetActorRotation();
    Scene.Pawn->SetActorLocation(FVector(0,3710,-180),false,nullptr,ETeleportType::TeleportPhysics);
    Scene.Frame();
    TestTrue(TEXT("The ending keeps the robot on the safe pad even after displacement"),FVector::Dist(Scene.Pawn->GetActorLocation(),FVector(550,2580,62.5))<.1);
    for (int32 Index=0;Index<883;++Index) Scene.Frame(FVector::XAxisVector);
    TestFalse(TEXT("The ending remains in progress until its fifteen seconds elapse"),Scene.Core->bFacilityRebootComplete);
    TestTrue(TEXT("The guarded background machinery restarts during the ending"),!Scene.Rotor->GetActorRotation().Equals(RotorBefore,.01f));
    TestEqual(TEXT("Background machinery retains nonblocking collision"),Scene.Rotor->GetStaticMeshComponent()->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    Scene.Frames(20);
    TestTrue(TEXT("The ending reaches Facility Reboot Complete after fifteen seconds"),Scene.Core->bFacilityRebootComplete);
    TestTrue(TEXT("The finished ending reaches its full progress"),Scene.Core->FinaleProgress>.999f);
    TestEqual(TEXT("All three system indicators are active"),Scene.Core->ActiveSystemCount,3);
    TestTrue(TEXT("The completion message is displayed"),Scene.Core->CompletionText->Text.ToString().Contains(TEXT("FACILITY REBOOT COMPLETE")));
    TestFalse(TEXT("Finale cleanup leaves no live local electricity"),Scene.Core->bElectricalLive||Scene.Core->bElectricalWarning||Scene.Core->Arcs->IsVisible());
    TestEqual(TEXT("Fall and electrical recovery cannot interrupt the locked ending"),Scene.Core->FallResetCount+Scene.Core->HazardResetCount,Resets);
    TestTrue(TEXT("Normal lighting returns to the core"),Scene.EmergencyLight->GetLightComponent()->GetLightColor().G>.5f);
    TestTrue(TEXT("The stable ending camera remains selected"),Scene.Player->GetViewTarget()==Scene.Camera);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreEarnedProgressTest,"REBOOT.CentralCore.EarlierSystemsMustActuallyBeEarned",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoreEarnedProgressTest::RunTest(const FString& Parameters)
{
    FCoreTestWorld Scene;if (!Scene.Create(*this)) return false;
    Scene.Progress->ResetProgress();Scene.Frames(2);
    TestEqual(TEXT("Opening the core without progress presumes no earlier systems"),Scene.Core->ActiveSystemCount,0);
    if (!Scene.ActivateAll(*this)) return false;
    Scene.Place(FVector(550,2580,62.5));Scene.Frames(2);
    TestTrue(TEXT("Completing the local relays earns only the core system"),Scene.Progress->IsSystemComplete(2));
    TestEqual(TEXT("The system count includes only the one earned system"),Scene.Core->ActiveSystemCount,1);
    TestFalse(TEXT("All local relays cannot bypass missing earlier systems"),Scene.Core->CanReboot());
    Scene.Key(true);Scene.Frames(75);Scene.Key(false);Scene.Frames(1);
    TestFalse(TEXT("A full held interaction cannot start an unearned finale"),Scene.Core->bRebootStarted);
    TestEqual(TEXT("An ineligible finale accumulates no hold progress"),Scene.Core->RebootHoldProgress,0.0f);
    Scene.Progress->CompleteSystem(0);Scene.Frames(1);
    TestEqual(TEXT("Completing System 1 updates the real indicator count"),Scene.Core->ActiveSystemCount,2);
    TestFalse(TEXT("A missing System 2 still blocks the finale"),Scene.Core->CanReboot());
    Scene.Progress->ResetProgress();Scene.Progress->CompleteSystem(1);Scene.Frames(1);
    TestFalse(TEXT("A missing System 1 also blocks the finale"),Scene.Core->CanReboot());
    Scene.Progress->CompleteSystem(0);Scene.Frames(1);
    TestEqual(TEXT("All three earned systems are finally counted"),Scene.Core->ActiveSystemCount,3);
    TestTrue(TEXT("Earned earlier systems permit the normal console interaction"),Scene.Core->CanReboot());
    Scene.Key(true);Scene.Frames(61);Scene.Key(false);
    TestTrue(TEXT("The usual one-second hold starts the earned finale"),Scene.Core->bRebootStarted);
    return true;
}

#endif
