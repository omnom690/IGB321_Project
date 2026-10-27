#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RebootGameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRebootProgressTest,"REBOOT.Progress.EarnedSystemsAreExplicitAndIdempotent",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRebootProgressTest::RunTest(const FString& Parameters)
{
    URebootGameInstance* Progress=NewObject<URebootGameInstance>();
    if (!TestNotNull(TEXT("A new session progress instance is available"),Progress)) return false;
    TestEqual(TEXT("A new session does not presume any earlier systems"),Progress->GetCompletedSystemCount(),0);
    for (int32 Index=0;Index<3;++Index) TestFalse(TEXT("Every system begins unearned"),Progress->IsSystemComplete(Index));
    Progress->CompleteSystem(-1);Progress->CompleteSystem(3);Progress->CompleteSystem(MAX_int32);
    TestEqual(TEXT("Invalid system identifiers cannot award progress"),Progress->GetCompletedSystemCount(),0);
    TestFalse(TEXT("Invalid queries are false"),Progress->IsSystemComplete(-1)||Progress->IsSystemComplete(3));
    Progress->CompleteSystem(1);
    TestTrue(TEXT("The processing terminal records its own earned system"),Progress->IsSystemComplete(1));
    TestFalse(TEXT("Completing processing never grants System 1 or the core"),Progress->IsSystemComplete(0)||Progress->IsSystemComplete(2));
    Progress->CompleteSystem(1);
    TestEqual(TEXT("Repeated completion cannot increase the count twice"),Progress->GetCompletedSystemCount(),1);
    Progress->CompleteSystem(0);Progress->CompleteSystem(2);
    TestEqual(TEXT("Three distinct earned systems produce the complete count"),Progress->GetCompletedSystemCount(),3);
    Progress->ResetProgress();
    TestEqual(TEXT("Explicitly starting a new session clears all earned systems"),Progress->GetCompletedSystemCount(),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRebootSessionIsolationTest,"REBOOT.Progress.SessionsDoNotShareCompletionFlags",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRebootSessionIsolationTest::RunTest(const FString& Parameters)
{
    URebootGameInstance* First=NewObject<URebootGameInstance>();
    URebootGameInstance* Second=NewObject<URebootGameInstance>();
    if (!TestNotNull(TEXT("First session exists"),First)||!TestNotNull(TEXT("Second session exists"),Second)) return false;
    First->CompleteSystem(0);First->CompleteSystem(1);
    TestEqual(TEXT("The first session retains both earned earlier systems"),First->GetCompletedSystemCount(),2);
    TestEqual(TEXT("Another new game starts with no progress"),Second->GetCompletedSystemCount(),0);
    Second->CompleteSystem(2);
    TestFalse(TEXT("A different session cannot unlock this session's core flag"),First->IsSystemComplete(2));
    First->ResetProgress();
    TestTrue(TEXT("Resetting one session leaves another session untouched"),Second->IsSystemComplete(2));
    return true;
}

#endif
