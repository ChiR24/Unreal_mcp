// McpCsvProfileTests.cpp — the CSV profile summary (McpCsvProfile.h) on a small table shaped like the engine writer's:
// the EVENTS column first, a megabytes column that is no timing, and the header and metadata rows after the frames.

#include "Domains/Performance/Csv/McpCsvProfile.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpCsvProfileTest,
    "EngineWire.Performance.CsvProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpCsvProfileTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // As a 5.8 capture writes it: an EVENTS column first, text on the frame an event happened, the header again after
    // the frames, then the metadata row.
    const FString Csv = TEXT(
        "EVENTS,FrameTime,GameThreadTime,RenderThreadTime,GPUTime,GPUMem/LocalUsedMB\n"
        ",16.0,8.0,9.0,12.0,4000\n"
        ",16.0,8.5,9.0,12.5,4000\n"
        ",50.0,45.0,10.0,13.0,4100\n"
        "Cmd: csvprofile stop##3.99,17.0,9.0,9.5,30.0,4000\n"
        "EVENTS,FrameTime,GameThreadTime,RenderThreadTime,GPUTime,GPUMem/LocalUsedMB\n"
        "[HasHeaderRowAtEnd],1\n");
    FString Error;
    const TSharedPtr<FJsonObject> Summary = McpCsvProfile::Summarize(Csv, 2, 33.3, Error);
    if (!TestTrue(TEXT("a writer-shaped table reads"), Summary.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("the frames stop at the metadata"), static_cast<int32>(Summary->GetNumberField(TEXT("frames"))), 4);
    const TSharedPtr<FJsonObject> Frame = Summary->GetObjectField(TEXT("frameTime"));
    TestEqual(TEXT("mean frame time"), Frame->GetNumberField(TEXT("meanMs")), 24.75);
    TestEqual(TEXT("worst frame"), Frame->GetNumberField(TEXT("maxMs")), 50.0);
    TestEqual(TEXT("the heaviest stat by mean"), Summary->GetArrayField(TEXT("topByMean"))[0]->AsObject()->GetStringField(TEXT("stat")), FString(TEXT("GameThreadTime")));
    TestEqual(TEXT("the heaviest stat by peak"), Summary->GetArrayField(TEXT("topByPeak"))[0]->AsObject()->GetStringField(TEXT("stat")), FString(TEXT("GameThreadTime")));
    TestEqual(TEXT("one hitch over 33.3 ms"), static_cast<int32>(Summary->GetNumberField(TEXT("hitchCount"))), 1);
    const TSharedPtr<FJsonObject> Hitch = Summary->GetArrayField(TEXT("worstFrames"))[0]->AsObject();
    TestEqual(TEXT("it is frame 2"), static_cast<int32>(Hitch->GetNumberField(TEXT("frame"))), 2);
    TestEqual(TEXT("the game thread is its largest stat"), Hitch->GetArrayField(TEXT("largest"))[0]->AsObject()->GetStringField(TEXT("stat")), FString(TEXT("GameThreadTime")));
    TestFalse(TEXT("a table without frames is refused"), McpCsvProfile::Summarize(TEXT("FrameTime\n"), 5, 33.3, Error).IsValid());
    TestFalse(TEXT("one without a frame-time column too"), McpCsvProfile::Summarize(TEXT("A,B\n1,2\n"), 5, 33.3, Error).IsValid());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
