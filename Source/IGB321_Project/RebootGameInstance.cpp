#include "RebootGameInstance.h"
#include "RebootBotVisualComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace
{
void InstallPlayerVisual(UWorld* World)
{
    ACharacter* Character=UGameplayStatics::GetPlayerCharacter(World,0);
    if (!IsValid(Character) || !Character->GetClass()->GetPathName().Contains(TEXT("/Game/StackOBot/Blueprints/Character/BP_Bot."))
        || Character->FindComponentByClass<URebootBotVisualComponent>()) return;
    const URebootBotVisualComponent* Settings=GetDefault<URebootBotVisualComponent>();
    if (Settings->RobotMesh.IsNull() || !FPackageName::DoesPackageExist(Settings->RobotMesh.ToSoftObjectPath().GetLongPackageName())) return;
    USkeletalMesh* Mesh=Settings->RobotMesh.LoadSynchronous();
    if (!IsValid(Mesh)) return;
    URebootBotVisualComponent* Visual=NewObject<URebootBotVisualComponent>(Character,TEXT("REBOOT_Robot2_Visual"));
    Visual->SetupAttachment(Character->GetRootComponent());Character->AddInstanceComponent(Visual);
    Visual->RegisterComponent();
    if (!Visual->InitializeForCharacter(Character,Mesh)) Visual->DestroyComponent();
}
}

void URebootGameInstance::Init()
{
    Super::Init();ResetProgress();
    WorldTickHandle=FWorldDelegates::OnWorldPostActorTick.AddUObject(this,&URebootGameInstance::ObserveLevelProgress);
}

void URebootGameInstance::Shutdown()
{
    FWorldDelegates::OnWorldPostActorTick.Remove(WorldTickHandle);
    LocalPowerControllerClass=nullptr;
    Super::Shutdown();
}

void URebootGameInstance::CompleteSystem(int32 Index)
{
    if (Index>=0 && Index<3) CompletedSystemMask|=static_cast<uint8>(1u<<Index);
}

bool URebootGameInstance::IsSystemComplete(int32 Index) const
{
    return Index>=0 && Index<3 && (CompletedSystemMask&(1u<<Index))!=0;
}

int32 URebootGameInstance::GetCompletedSystemCount() const
{
    return static_cast<int32>(IsSystemComplete(0))+IsSystemComplete(1)+IsSystemComplete(2);
}

void URebootGameInstance::ResetProgress() { CompletedSystemMask=0; }

void URebootGameInstance::ObserveLevelProgress(UWorld* World,ELevelTick TickType,float DeltaSeconds)
{
    if (!IsValid(World) || !World->IsGameWorld() || World->GetGameInstance()!=this) return;
    InstallPlayerVisual(World);
    const FString LevelName=UGameplayStatics::GetCurrentLevelName(World,true);
    if (IsSystemComplete(0) || (LevelName!=TEXT("Level_01_ReceivingSorting") && LevelName!=TEXT("Level_01_ContinuousFactory"))) return;
    if (!LocalPowerControllerClass)
        LocalPowerControllerClass=LoadClass<AActor>(nullptr,TEXT("/Game/REBOOT/Level_1/Section_4/Blueprints/BP_LocalPowerController.BP_LocalPowerController_C"));
    if (!LocalPowerControllerClass) return;
    // Observe the original terminal's earned latch; entering the next map never grants it.
    const FBoolProperty* Enabled=FindFProperty<FBoolProperty>(LocalPowerControllerClass,TEXT("Enabled"));
    const FBoolProperty* LocalPower=FindFProperty<FBoolProperty>(LocalPowerControllerClass,TEXT("LocalPower"));
    if (!Enabled || !LocalPower) return;
    for (TActorIterator<AActor> It(World,LocalPowerControllerClass);It;++It)
        if (Enabled->GetPropertyValue_InContainer(*It) && LocalPower->GetPropertyValue_InContainer(*It))
        {
            CompleteSystem(0);break;
        }
}
