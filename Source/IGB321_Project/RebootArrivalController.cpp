#include "RebootArrivalController.h"

#include "CollisionQueryParams.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

ARebootArrivalController::ARebootArrivalController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ARebootArrivalController::BeginPlay()
{
    Super::BeginPlay();
    ResolveActors();
    MakeMovable(DoorActor);
    MakeMovable(ButtonActor);

    if (IsValid(DoorActor))
    {
        DoorClosedLocation = DoorActor->GetActorLocation();
        bDoorLocationReady = true;
    }

    PrepareStatusMaterials();
    RefreshPlayerController();
}

void ARebootArrivalController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshPlayerController();

    if (!bSwitchActivated && IsValid(PlayerController)
        && PlayerController->WasInputKeyJustPressed(EKeys::E) && CanInteract())
    {
        ActivateSwitch();
    }

    if (bSwitchActivated && !bDoorOpened && IsValid(DoorActor) && bDoorLocationReady)
    {
        const float Height = FMath::Max(DoorLiftHeight, 1.0f);
        const FVector Target = DoorClosedLocation + FVector(0.0, 0.0, Height);
        const FVector Position = FMath::VInterpConstantTo(
            DoorActor->GetActorLocation(), Target, DeltaSeconds, FMath::Max(DoorSpeed, 1.0f));
        DoorActor->SetActorLocation(Position, false, nullptr, ETeleportType::None);
        DoorOpenProgress = FMath::Clamp(
            static_cast<float>((Position.Z - DoorClosedLocation.Z) / Height), 0.0f, 1.0f);

        if (Position.Equals(Target, 0.1))
        {
            DoorActor->SetActorLocation(Target, false, nullptr, ETeleportType::None);
            DoorOpenProgress = 1.0f;
            bDoorOpened = true;
        }
    }
}

void ARebootArrivalController::ActivateSwitch()
{
    if (bSwitchActivated || !IsValid(DoorActor) || !bDoorLocationReady)
    {
        return;
    }

    bSwitchActivated = true;

    for (UMaterialInstanceDynamic* Material : StatusMaterials)
    {
        if (IsValid(Material))
        {
            Material->SetScalarParameterValue(TEXT("Active"), 1.0f);
            Material->SetScalarParameterValue(TEXT("Glow"), 2.0f);
        }
    }

    if (IsValid(ButtonActor))
    {
        ButtonActor->AddActorLocalOffset(
            FVector(0.0, 0.0, -FMath::Max(ButtonTravel, 0.0f)), false, nullptr, ETeleportType::None);
    }

    if (IsValid(ActivationSound))
    {
        UGameplayStatics::PlaySoundAtLocation(this, ActivationSound, GetSwitchTarget());
    }
}

bool ARebootArrivalController::CanInteract() const
{
    if (bSwitchActivated || !IsValid(PlayerController) || !IsValid(SwitchActor)
        || !IsValid(DoorActor) || !bDoorLocationReady)
    {
        return false;
    }

    const APawn* Pawn = PlayerController->GetPawn();
    if (!IsValid(Pawn))
    {
        return false;
    }

    const FVector Target = GetSwitchTarget();
    const float Range = FMath::Max(InteractionRange, 1.0f);
    return FVector::DistSquared(Pawn->GetActorLocation(), Target) <= FMath::Square(Range)
        && HasSwitchLineOfSight(Pawn, Target);
}

void ARebootArrivalController::ResolveActors()
{
    const auto FirstWithTag = [this](const FName Tag) -> AActor*
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsWithTag(this, Tag, Found);
        return Found.IsEmpty() ? nullptr : Found[0];
    };

    if (!IsValid(DoorActor))
    {
        DoorActor = FirstWithTag(TEXT("LS1_Door"));
    }
    if (!IsValid(SwitchActor))
    {
        SwitchActor = FirstWithTag(TEXT("LS1_Switch"));
    }
    if (!IsValid(ButtonActor))
    {
        ButtonActor = FirstWithTag(TEXT("LS1_Button"));
    }

    for (const FName Tag : {FName(TEXT("LS1_DoorIndicator")), FName(TEXT("LS1_Cable"))})
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsWithTag(this, Tag, Found);
        for (AActor* Actor : Found)
        {
            StatusActors.AddUnique(Actor);
        }
    }
}

void ARebootArrivalController::PrepareStatusMaterials()
{
    StatusMaterials.Reset();

    for (AActor* Actor : StatusActors)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        TArray<UMeshComponent*> Components;
        Actor->GetComponents<UMeshComponent>(Components);
        for (UMeshComponent* Component : Components)
        {
            for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
            {
                if (UMaterialInstanceDynamic* Material = Component->CreateDynamicMaterialInstance(Slot))
                {
                    StatusMaterials.Add(Material);
                }
            }
        }
    }
}

void ARebootArrivalController::RefreshPlayerController()
{
    APlayerController* Current = UGameplayStatics::GetPlayerController(this, 0);
    if (Current != PlayerController)
    {
        if (IsValid(PlayerController))
        {
            RemoveTickPrerequisiteActor(PlayerController);
        }
        PlayerController = Current;
        if (IsValid(PlayerController))
        {
            AddTickPrerequisiteActor(PlayerController);
        }
    }
}

void ARebootArrivalController::MakeMovable(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    TArray<USceneComponent*> Components;
    Actor->GetComponents<USceneComponent>(Components);
    for (USceneComponent* Component : Components)
    {
        Component->SetMobility(EComponentMobility::Movable);
    }
}

bool ARebootArrivalController::HasSwitchLineOfSight(const APawn* Pawn, const FVector& Target) const
{
    if (!GetWorld())
    {
        return false;
    }

    FVector EyeLocation;
    FRotator EyeRotation;
    Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
    FCollisionQueryParams Parameters(SCENE_QUERY_STAT(RebootArrivalInteraction), false, Pawn);
    Parameters.AddIgnoredActor(this);
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, Target, ECC_Visibility, Parameters))
    {
        return true;
    }

    const AActor* HitActor = Hit.GetActor();
    return HitActor == SwitchActor || HitActor == ButtonActor
        || (IsValid(HitActor) && IsValid(SwitchActor) && HitActor->IsAttachedTo(SwitchActor));
}

FVector ARebootArrivalController::GetSwitchTarget() const
{
    AActor* TargetActor = IsValid(ButtonActor) ? ButtonActor.Get() : SwitchActor.Get();
    if (!IsValid(TargetActor))
    {
        return GetActorLocation();
    }

    FVector Center;
    FVector Extent;
    TargetActor->GetActorBounds(false, Center, Extent);
    return Center;
}
