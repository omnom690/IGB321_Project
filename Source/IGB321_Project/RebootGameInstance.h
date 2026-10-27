#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "RebootGameInstance.generated.h"

// Earned systems belong to a play session and survive OpenLevel travel.
UCLASS()
class IGB321_PROJECT_API URebootGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void Shutdown() override;

    UFUNCTION(BlueprintCallable, Category="REBOOT|Progress")
    void CompleteSystem(int32 Index);
    UFUNCTION(BlueprintPure, Category="REBOOT|Progress")
    bool IsSystemComplete(int32 Index) const;
    UFUNCTION(BlueprintPure, Category="REBOOT|Progress")
    int32 GetCompletedSystemCount() const;
    UFUNCTION(BlueprintCallable, Category="REBOOT|Progress")
    void ResetProgress();

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="REBOOT|Progress")
    uint8 CompletedSystemMask=0;
private:
    void ObserveLevelProgress(UWorld* World,ELevelTick TickType,float DeltaSeconds);
    FDelegateHandle WorldTickHandle;
    UPROPERTY(Transient) TObjectPtr<UClass> LocalPowerControllerClass;
};
