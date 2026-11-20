// McpInflightLedgerTests.cpp — the in-flight ledger's file, on a ledger of its own in a temporary folder.

#include "Core/Requests/McpInflightLedger.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/Subsystem/McpEndpointFile.h"
#include "EngineWireSubsystem.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace McpInflightLedgerTestData
{
TSharedPtr<FJsonObject> ReadJson(const FString& Path)
{
    FString Text;
    TSharedPtr<FJsonObject> Json;
    if (FFileHelper::LoadFileToString(Text, *Path))
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        FJsonSerializer::Deserialize(Reader, Json);
    }
    return Json;
}

TArray<TSharedPtr<FJsonValue>> Rows(const FString& Dir)
{
    const TSharedPtr<FJsonObject> Json = ReadJson(FPaths::Combine(Dir, TEXT("inflight.json")));
    return Json.IsValid() ? Json->GetArrayField(TEXT("entries")) : TArray<TSharedPtr<FJsonValue>>();
}
} // namespace McpInflightLedgerTestData

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpInflightLedgerTest,
    "EngineWire.Core.InflightLedger",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpInflightLedgerTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpInflightLedgerTestData;
    const FString Dir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("McpInflightLedgerTest"));
    IFileManager::Get().DeleteDirectory(*Dir, false, true);
    IFileManager::Get().MakeDirectory(*Dir, true);
    FMcpInflightLedger Ledger(Dir);
    const double Self = static_cast<double>(FPlatformProcess::GetCurrentProcessId());

    Ledger.Dispatch(TEXT("req-a"), TEXT("manage_asset.create_material"));
    Ledger.Dispatch(TEXT("req-b"), TEXT("control_actor.spawn"));
    TArray<TSharedPtr<FJsonValue>> Listed = Rows(Dir);
    TestEqual(TEXT("both calls are on disk once dispatched"), Listed.Num(), 2);
    if (Listed.Num() == 2)
    {
        const TSharedPtr<FJsonObject> First = Listed[0]->AsObject();
        TestEqual(TEXT("in dispatch order"), First->GetStringField(TEXT("requestId")), FString(TEXT("req-a")));
        TestEqual(TEXT("named by tool and action"), First->GetStringField(TEXT("capability")), FString(TEXT("manage_asset.create_material")));
        TestEqual(TEXT("running"), First->GetStringField(TEXT("phase")), FString(TEXT("dispatch")));
        TestEqual(TEXT("with this editor's pid"), First->GetNumberField(TEXT("pid")), Self);
        TestFalse(TEXT("and no parameters"), First->HasField(TEXT("params")));
    }
    Ledger.Wait(TEXT("req-a"));
    Listed = Rows(Dir);
    TestTrue(TEXT("a call that returned without a reply waits"), Listed.Num() == 2 &&
        Listed[0]->AsObject()->GetStringField(TEXT("phase")) == TEXT("latent"));
    Ledger.Done(TEXT("req-a"));
    Ledger.Done(TEXT("req-unknown"));
    Listed = Rows(Dir);
    TestTrue(TEXT("an answered call leaves; an unknown one changes nothing"), Listed.Num() == 1 &&
        Listed[0]->AsObject()->GetStringField(TEXT("requestId")) == TEXT("req-b"));
    Ledger.Stop();
    TestEqual(TEXT("a clean stop lists nothing"), Rows(Dir).Num(), 0);

    // Another process's entries (pid 1 is never this editor) move into last-crash.json at the next start.
    const TSharedRef<FJsonObject> Old = MakeShared<FJsonObject>();
    const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("requestId"), TEXT("req-old"));
    Entry->SetStringField(TEXT("capability"), TEXT("manage_asset.import"));
    Entry->SetStringField(TEXT("phase"), TEXT("dispatch"));
    Entry->SetNumberField(TEXT("pid"), 1.0);
    Old->SetArrayField(TEXT("entries"), {MakeShared<FJsonValueObject>(Entry)});
    McpEndpointFile::WriteWhole(FPaths::Combine(Dir, TEXT("inflight.json")), Old);
    TestEqual(TEXT("start moves the earlier editor's call"), Ledger.Start(), 1);
    const TSharedPtr<FJsonObject> Crash = ReadJson(FPaths::Combine(Dir, TEXT("last-crash.json")));
    TestTrue(TEXT("into last-crash.json, under that process"), Crash.IsValid() &&
        Crash->GetNumberField(TEXT("processId")) == 1.0 && Crash->GetArrayField(TEXT("inflight")).Num() == 1);
    TestEqual(TEXT("and inflight.json starts empty"), Rows(Dir).Num(), 0);
    // That capability, called again in this editor run, is warned about once; another one is not.
    AddExpectedError(TEXT("ended while running manage_asset.import"), EAutomationExpectedErrorFlags::Contains, 1);
    Ledger.WarnIfEndedIn(TEXT("manage_asset.import"));
    Ledger.WarnIfEndedIn(TEXT("manage_asset.import"));
    Ledger.WarnIfEndedIn(TEXT("control_actor.spawn"));

    // Two writes per call on the game thread: what they cost here.
    const double Begin = FPlatformTime::Seconds();
    for (int32 Index = 0; Index < 100; ++Index)
    {
        const FString Id = FString::Printf(TEXT("req-%d"), Index);
        Ledger.Dispatch(Id, TEXT("inspect.get_property"));
        Ledger.Done(Id);
    }
    const double PerCallMs = (FPlatformTime::Seconds() - Begin) * 1000.0 / 100.0;
    AddInfo(FString::Printf(TEXT("inflight ledger: %.2f ms per call (dispatch + done)"), PerCallMs));
    // Only a pathological cost fails: the figure itself is the measurement (a build beside it slows the disk).
    TestTrue(TEXT("a call's two writes stay under 50 ms"), PerCallMs < 50.0);

    IFileManager::Get().DeleteDirectory(*Dir, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpInflightLedgerTestSubsystemTest,
    "EngineWire.Core.InflightLedger.TestSubsystemsStayOut",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpInflightLedgerTestSubsystemTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // A test's own subsystem (no transport) runs a request whose handler never replies, as the queue tests' fixtures
    // do: the editor's inflight.json must not list it, or the next start reports it as a call the editor died in.
    UEngineWireSubsystem* Subsystem = NewObject<UEngineWireSubsystem>();
    Subsystem->RegisterHandler(TEXT("ledger_probe_fixture"),
        [](const FString&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>) { return true; });
    const FString Id = TEXT("ledger-probe-never-replies");
    TestEqual(TEXT("queued"), Subsystem->QueueAutomationRequest(Id, TEXT("ledger_probe_fixture"), MakeShared<FJsonObject>(), nullptr),
              EAutomationQueueRejection::None);
    Subsystem->ProcessPendingAutomationRequests();
    FString Text;
    FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP"), TEXT("inflight.json")));
    TestFalse(TEXT("the editor's ledger does not list a test subsystem's call"), Text.Contains(Id));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpInflightHeldWarningsTest,
    "EngineWire.Core.InflightLedger.HeldWarnings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpInflightHeldWarningsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // A call that answers later (a test run, a playtest) replies from a fresh capture: what its dispatch logged, such as
    // the notice that the last editor died in this capability, is held for it and handed back once.
    UEngineWireSubsystem* Subsystem = NewObject<UEngineWireSubsystem>();
    auto Has = [](const TArray<FString>& Lines) { return Lines.ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("held-warning-probe")); }); };
    Subsystem->BeginErrorCapture();
    UE_LOG(LogTemp, Warning, TEXT("held-warning-probe: logged before the call answers"));
    Subsystem->HoldCapturedWarnings(TEXT("held-probe"));
    TestTrue(TEXT("the dispatch bracket captured it"), Has(Subsystem->EndErrorCapture()));
    Subsystem->BeginErrorCapture();
    Subsystem->ReturnHeldWarnings(TEXT("held-probe"));
    TestTrue(TEXT("the bracket that answers has it back"), Has(Subsystem->EndErrorCapture()));
    Subsystem->BeginErrorCapture();
    Subsystem->ReturnHeldWarnings(TEXT("held-probe"));
    TestFalse(TEXT("once"), Has(Subsystem->EndErrorCapture()));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
