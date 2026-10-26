#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RebootCrusherController.h"
#include "RebootCrusherState.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrusherCycleTest,
    "REBOOT.CrusherApproach.CycleAndWarning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCrusherCycleTest::RunTest(const FString& Parameters)
{
    FRebootCrusherState State;
    TestTrue(TEXT("The initial first head is raised"),FMath::IsNearlyEqual(State.RaisedFraction(0),1.0f));
    TestTrue(TEXT("The initial second head is raised during its warning"),FMath::IsNearlyEqual(State.RaisedFraction(1),1.0f));
    State.Elapsed=0.35;
    TestTrue(TEXT("The first two-second phase is open"),State.Phase(0)==ERebootCrusherPhase::Open);
    State.Elapsed=1.999;
    TestTrue(TEXT("The full open interval precedes the warning"),State.Phase(0)==ERebootCrusherPhase::Open);
    State.Elapsed=2.0;
    TestTrue(TEXT("Warning begins at two seconds"),State.Phase(0)==ERebootCrusherPhase::Warning);
    TestTrue(TEXT("Warning begins while the visible head is raised"),FMath::IsNearlyEqual(State.RaisedFraction(0),1.0f));
    State.Elapsed=2.64;
    TestTrue(TEXT("The early warning gives the robot reaction time"),FMath::IsNearlyEqual(State.RaisedFraction(0),1.0f));
    State.Elapsed=2.825;
    TestTrue(TEXT("The late warning visibly lowers the head"),FMath::IsNearlyEqual(State.RaisedFraction(0),0.5f,0.001f));
    State.Elapsed=3.0;
    TestTrue(TEXT("The one-second closed interval begins after the warning"),State.Phase(0)==ERebootCrusherPhase::Closed);
    TestTrue(TEXT("The head reaches the floor for the closed interval"),FMath::IsNearlyZero(State.RaisedFraction(0)));
    State.Elapsed=3.999;
    TestTrue(TEXT("The head stays closed until the four-second cycle ends"),State.Phase(0)==ERebootCrusherPhase::Closed);
    State.Elapsed=4.175;
    TestTrue(TEXT("The next open interval raises the head"),State.Phase(0)==ERebootCrusherPhase::Open);
    TestTrue(TEXT("The rising head travels rather than jumping between poses"),FMath::IsNearlyEqual(State.RaisedFraction(0),0.5f,0.001f));
    State.Elapsed=4.35;
    TestTrue(TEXT("The new open window restores full clearance"),FMath::IsNearlyEqual(State.RaisedFraction(0),1.0f,0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrusherStaggerTest,
    "REBOOT.CrusherApproach.StaggeredCrossings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCrusherStaggerTest::RunTest(const FString& Parameters)
{
    FRebootCrusherState State;
    State.Elapsed=1.5;
    TestTrue(TEXT("The first crossing is open while the second is closed"),
        State.Phase(0)==ERebootCrusherPhase::Open && State.Phase(1)==ERebootCrusherPhase::Closed);
    State.Elapsed=2.5;
    TestTrue(TEXT("The next crossing opens while the first warns"),
        State.Phase(0)==ERebootCrusherPhase::Warning && State.Phase(1)==ERebootCrusherPhase::Open);
    State.Elapsed=3.5;
    TestTrue(TEXT("The second crossing stays open while the first is closed"),
        State.Phase(0)==ERebootCrusherPhase::Closed && State.Phase(1)==ERebootCrusherPhase::Open);
    const double Before=State.Elapsed;
    const float PoseBefore=State.RaisedFraction(0);
    State.Advance(-10.0f);
    TestEqual(TEXT("Negative time cannot rewind a crusher cycle"),State.Elapsed,Before);
    TestEqual(TEXT("Negative time cannot change the head pose"),State.RaisedFraction(0),PoseBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrusherStopTest,
    "REBOOT.CrusherApproach.PermanentShutdown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCrusherStopTest::RunTest(const FString& Parameters)
{
    FRebootCrusherState State;
    State.Elapsed=3.5;
    TestTrue(TEXT("Shutdown can begin during a closed phase"),State.Phase(0)==ERebootCrusherPhase::Closed);
    State.StopPermanently();
    for (int32 Step=0; Step<12; ++Step)
    {
        State.Advance(0.5f);
        for (int32 Index=0; Index<2; ++Index)
        {
            TestTrue(TEXT("Both shutdown crushers retain an open phase"),State.Phase(Index)==ERebootCrusherPhase::Open);
            TestTrue(TEXT("Both shutdown targets stay fully raised"),FMath::IsNearlyEqual(State.RaisedFraction(Index),1.0f));
        }
    }
    State.StopPermanently();
    State.Advance(1001.0f);
    TestTrue(TEXT("Repeated shutdown remains latched after many cycles"),State.bStopped);
    TestTrue(TEXT("The first target cannot restart"),FMath::IsNearlyEqual(State.RaisedFraction(0),1.0f));
    TestTrue(TEXT("The second target cannot restart"),FMath::IsNearlyEqual(State.RaisedFraction(1),1.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrusherRuntimeTest,
    "REBOOT.CrusherApproach.RuntimeController",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCrusherRuntimeTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Init=UWorld::InitializationValues()
        .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
        .EnableTraceCollision(true).CreateFXSystem(false).SetTransactional(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,
        ERHIFeatureLevel::Num,&Init);
    if (!TestNotNull(TEXT("Isolated crusher physics world is available"),World)) return false;
    struct FWorldCleanup { UWorld* World; ~FWorldCleanup() { World->DestroyWorld(false); } } Cleanup{World};
    UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* HeadMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_2/Meshes/SM_crusher_head_250x220x90.SM_crusher_head_250x220x90"));
    UStaticMesh* SwitchMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/REBOOT/Level_2/Meshes/SM_switch_pedestal.SM_switch_pedestal"));
    if (!TestNotNull(TEXT("Engine cube is available"),Cube)
        || !TestNotNull(TEXT("The imported crusher head is available"),HeadMesh)
        || !TestNotNull(TEXT("The imported pedestal is available"),SwitchMesh)) return false;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const auto MeshActor=[&](FVector Position,FVector Size,bool Collision)
    {
        AStaticMeshActor* Actor=World->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator,Spawn);
        UStaticMeshComponent* Component=Actor->GetStaticMeshComponent();
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetStaticMesh(Cube);
        Component->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
        Actor->SetActorScale3D(Size/100.0);
        return Actor;
    };
    MeshActor(FVector(0,0,-10),FVector(4000,4000,20),true);
    TArray<AStaticMeshActor*> Heads;
    TArray<AStaticMeshActor*> HeadColliders;
    for (const float X : {-300.0f,500.0f})
    {
        AStaticMeshActor* Head=World->SpawnActor<AStaticMeshActor>(FVector(X,0,269.8),FRotator(0,-90,0),Spawn);
        Head->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Head->GetStaticMeshComponent()->SetStaticMesh(HeadMesh);
        Head->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("NoCollision"));
        AStaticMeshActor* Collider=MeshActor(FVector(X,0,314.8),FVector(220,250,90),true);
        Collider->AttachToActor(Head,FAttachmentTransformRules::KeepWorldTransform);
        Heads.Add(Head);
        HeadColliders.Add(Collider);
    }
    AStaticMeshActor* Door=MeshActor(FVector(1180,0,2),FVector(24,238,306),false);
    AStaticMeshActor* DoorCollider=MeshActor(FVector(1180,0,155),FVector(24,238,306),true);
    DoorCollider->AttachToActor(Door,FAttachmentTransformRules::KeepWorldTransform);
    AStaticMeshActor* Switch=World->SpawnActor<AStaticMeshActor>(FVector(930,410,0),FRotator(0,-90,0),Spawn);
    Switch->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Switch->GetStaticMeshComponent()->SetStaticMesh(SwitchMesh);
    Switch->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("NoCollision"));
    MeshActor(FVector(930,410,57.5),FVector(50,50,85),true);
    MeshActor(FVector(930,410,110),FVector(70,70,20),true);
    AStaticMeshActor* Button=MeshActor(FVector(930,410,120),FVector(30,30,8),false);
    APlayerController* Player=World->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
    ACharacter* Pawn=World->SpawnActor<ACharacter>(FVector(-300,0,62.5),FRotator::ZeroRotator,Spawn);
    Pawn->GetCapsuleComponent()->SetCapsuleSize(33.373f,60.0f,true);
    Pawn->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
    Player->Possess(Pawn);
    ARebootCrusherController* Gallery=World->SpawnActor<ARebootCrusherController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
    Gallery->CrusherHeads={Heads[0],Heads[1]};
    Gallery->ExitDoor=Door;
    Gallery->SwitchActor=Switch;
    Gallery->ButtonActor=Button;
    World->InitializeActorsForPlay(FURL());
    Gallery->DispatchBeginPlay();
    World->UpdateWorldComponents(false,false);

    Gallery->Tick(1.75f);
    TestEqual(TEXT("The raised head does not hurt the robot during early warning"),Gallery->HazardResetCount,0);
    for (int32 Step=0; Step<40 && Gallery->HazardResetCount==0; ++Step) Gallery->Tick(0.05f);
    TestEqual(TEXT("The descending first head resets a robot beneath it"),Gallery->HazardResetCount,1);
    TestTrue(TEXT("The first crush returns to the room entrance"),Pawn->GetActorLocation().Equals(FVector(-750,0,62.5),0.5));
    for (int32 Index=0; Index<2; ++Index)
        TestTrue(TEXT("Each collision proxy follows its crusher head"),FMath::IsNearlyEqual(HeadColliders[Index]->GetActorLocation().Z,Heads[Index]->GetActorLocation().Z+45.0,0.1));

    Pawn->SetActorLocation(FVector(100,435,62.5));
    Gallery->Tick(1.0f);
    TestTrue(TEXT("Reaching the waiting pocket activates the middle reset point"),Gallery->bSafePocketReached);
    Pawn->SetActorLocation(FVector(500,0,62.5));
    for (int32 Step=0; Step<100 && Gallery->HazardResetCount<2; ++Step) Gallery->Tick(0.05f);
    TestEqual(TEXT("The second crusher also causes a hazard reset"),Gallery->HazardResetCount,2);
    TestTrue(TEXT("The later crush uses the reached safe pocket"),Pawn->GetActorLocation().Equals(FVector(100,435,62.5),0.5));
    TestFalse(TEXT("The safety switch cannot be reached remotely"),Gallery->CanInteractWithSwitch());
    Pawn->SetActorLocation(FVector(800,410,62.5));
    TestTrue(TEXT("The front display can be reached without its pedestal blocking it"),Gallery->CanInteractWithSwitch());
    AStaticMeshActor* Wall=MeshActor(FVector(845,410,115),FVector(20,120,200),true);
    TestFalse(TEXT("A wall blocks safety-switch interaction"),Gallery->CanInteractWithSwitch());
    Gallery->ActivateSafetySwitch();
    TestFalse(TEXT("Blocked activation leaves both crushers running"),Gallery->bCrushersStopped);
    Wall->SetActorEnableCollision(false);
    Gallery->ActivateSafetySwitch();
    TestTrue(TEXT("The accessible far switch permanently stops both crushers"),Gallery->bCrushersStopped);
    Gallery->Tick(1.2f);
    TestTrue(TEXT("The completed section opens its exit"),Gallery->ExitOpenProgress>0.99f);
    TestTrue(TEXT("The exit collision proxy rises with the door"),FMath::IsNearlyEqual(DoorCollider->GetActorLocation().Z,470.0,0.1));
    for (int32 Index=0; Index<2; ++Index)
        TestTrue(TEXT("Both stopped heads return to the raised pose"),FMath::IsNearlyEqual(Heads[Index]->GetActorLocation().Z,269.8,0.1));
    const int32 ResetsBefore=Gallery->HazardResetCount;
    Pawn->SetActorLocation(FVector(-300,0,62.5));
    Gallery->Tick(5.0f);
    TestEqual(TEXT("The permanent shutdown disables later crush resets"),Gallery->HazardResetCount,ResetsBefore);
    TestTrue(TEXT("The exit remains permanently open"),Gallery->ExitOpenProgress>0.99f);
    return true;
}

#endif
