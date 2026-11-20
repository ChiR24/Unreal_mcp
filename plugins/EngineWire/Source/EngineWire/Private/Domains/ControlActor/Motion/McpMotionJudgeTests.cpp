// McpMotionJudgeTests.cpp — sample_motion's judge (McpMotionJudge.h) on recorded samples: a jump that rises to 250 and
// lands, with a Fuel property that drains while a boost is on.

#include "Domains/ControlActor/Motion/McpMotionJudge.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMotionJudgeTest,
    "EngineWire.ControlActor.MotionJudge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpMotionJudgeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    auto Parse = [](const FString& Text)
    {
        TSharedPtr<FJsonValue> Value;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Value);
        return Value.IsValid() ? Value->AsArray() : TArray<TSharedPtr<FJsonValue>>();
    };
    const TArray<TSharedPtr<FJsonValue>> Samples = Parse(TEXT("["
        "{\"t\":0,\"location\":[0,0,100],\"velocity\":[0,0,0],\"rotation\":[0,90,0],\"properties\":{\"Fuel\":100,\"bBoosting\":false}},"
        "{\"t\":0.2,\"location\":[0,0,180],\"velocity\":[0,0,400],\"properties\":{\"Fuel\":90,\"bBoosting\":true}},"
        "{\"t\":0.4,\"location\":[0,0,250],\"velocity\":[0,0,0],\"properties\":{\"Fuel\":70,\"bBoosting\":true}},"
        "{\"t\":0.6,\"location\":[0,0,180],\"velocity\":[0,0,-300],\"properties\":{\"Fuel\":50,\"bBoosting\":true}},"
        "{\"t\":0.8,\"location\":[0,0,100],\"velocity\":[0,0,0],\"properties\":{\"Fuel\":50,\"bBoosting\":false}}]"));
    auto Verdicts = [&](const FString& Clauses, const FString& Ended)
    {
        TArray<FString> Words;
        for (const TSharedPtr<FJsonValue>& Each : McpMotionJudge::Judge(Parse(Clauses), Samples, Ended))
        {
            Words.Add(Each->AsObject()->GetStringField(TEXT("verdict")));
        }
        return FString::Join(Words, TEXT(","));
    };
    TestEqual(TEXT("it rises to 200-300 cm, ever"), Verdicts(TEXT("[{\"expect\":\"location.z in 200..300\",\"when\":\"ever\"}]"), TEXT("duration")), FString(TEXT("ok")));
    TestEqual(TEXT("it never sinks below the floor"), Verdicts(TEXT("[{\"expect\":\"location.z >= 100\",\"when\":\"always\"}]"), TEXT("duration")), FString(TEXT("ok")));
    TestEqual(TEXT("not always above 150"), Verdicts(TEXT("[{\"expect\":\"location.z > 150\",\"when\":\"always\"}]"), TEXT("duration")), FString(TEXT("failed")));
    TestEqual(TEXT("at a time, and at the end"), Verdicts(TEXT("[{\"expect\":\"location.z > 200\",\"when\":\"at\",\"atSeconds\":0.4},{\"expect\":\"location.z == 100\",\"when\":\"atEnd\"}]"), TEXT("duration")), FString(TEXT("ok,ok")));
    TestEqual(TEXT("a time past the last sample is unknown"), Verdicts(TEXT("[{\"expect\":\"location.z > 0\",\"when\":\"at\",\"atSeconds\":5}]"), TEXT("duration")), FString(TEXT("unknown")));
    TestEqual(TEXT("after the boost starts, fuel falls within 0.5 s"),
        Verdicts(TEXT("[{\"after\":{\"expect\":\"bBoosting == true\"},\"expect\":\"Fuel < 80\",\"when\":\"ever\",\"withinSeconds\":0.5}]"), TEXT("duration")), FString(TEXT("ok")));
    TestEqual(TEXT("an after that never held fails a full run"),
        Verdicts(TEXT("[{\"after\":{\"expect\":\"location.z > 999\"},\"expect\":\"Fuel < 10\",\"when\":\"ever\"}]"), TEXT("duration")), FString(TEXT("failed")));
    TestEqual(TEXT("a value no sample holds is unknown"), Verdicts(TEXT("[{\"expect\":\"Heat > 0\",\"when\":\"always\"}]"), TEXT("duration")), FString(TEXT("unknown")));
    TestEqual(TEXT("a run cut short: an always already broken fails, an unmet ever is unknown"),
        Verdicts(TEXT("[{\"expect\":\"location.z > 150\",\"when\":\"always\"},{\"expect\":\"Fuel < 10\",\"when\":\"ever\"}]"), TEXT("realTimeCap")), FString(TEXT("failed,unknown")));
    TestEqual(TEXT("rotation carries over from the sample that had it"), Verdicts(TEXT("[{\"expect\":\"rotation.yaw == 90\",\"when\":\"atEnd\"}]"), TEXT("duration")), FString(TEXT("ok")));
    TestEqual(TEXT("not"), Verdicts(TEXT("[{\"expect\":\"not location.z > 300\",\"when\":\"always\"}]"), TEXT("duration")), FString(TEXT("ok")));

    FJsonObject Data;
    McpMotionJudge::AddTo(Parse(TEXT("[{\"expect\":\"location.z >= 100\"},{\"expect\":\"Heat > 0\",\"when\":\"ever\"}]")), Samples, TEXT("duration"), Data);
    TestEqual(TEXT("one unknown clause makes the run's verdict unknown"), Data.GetStringField(TEXT("judgeVerdict")), FString(TEXT("unknown")));
    FJsonObject None;
    McpMotionJudge::AddTo({}, Samples, TEXT("duration"), None);
    TestFalse(TEXT("no clauses, no judge"), None.HasField(TEXT("judge")));

    const TSharedPtr<FJsonObject> Bad = MakeShared<FJsonObject>();
    Bad->SetArrayField(TEXT("judge"), Parse(TEXT("[{\"expect\":\"location.z rises\",\"when\":\"ever\"}]")));
    FString Error;
    FString Pointer;
    TestFalse(TEXT("an expect that compares nothing is refused"), McpMotionJudge::Validate(Bad, Error, Pointer));
    TestEqual(TEXT("by its pointer"), Pointer, FString(TEXT("/judge/0")));
    Bad->SetArrayField(TEXT("judge"), Parse(TEXT("[{\"expect\":\"location.z > 1\",\"when\":\"sometimes\"}]")));
    TestFalse(TEXT("an unknown when is refused"), McpMotionJudge::Validate(Bad, Error, Pointer));
    TArray<TSharedPtr<FJsonValue>> Taken;
    TestFalse(TEXT("Take refuses it too"), McpMotionJudge::Take(Bad, Taken, Error));
    TestTrue(TEXT("naming the clause"), Error.StartsWith(TEXT("/judge/0: ")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
