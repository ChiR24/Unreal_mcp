// McpExecuteVerifyTests.cpp — a write's read-back judged against what it sent (Request/McpExecuteVerify.h): the
// comparison's rules on fixed values, and the receipt stamp (verified, mismatches, nothing when nothing compares).

#include "MCP/Execute/Request/McpExecuteVerify.h"
#include "MCP/Protocol/McpJsonRpcReplyCompaction.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpExecuteVerifyTest,
    "EngineWire.Gateway.ReadBackVerify",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpExecuteVerifyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using McpExecuteVerify::EMatch;
    auto Json = [](const TCHAR* Text)
    {
        TSharedPtr<FJsonValue> Value;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(TEXT("[")) + Text + TEXT("]")), Value);
        return Value.IsValid() && Value->AsArray().Num() > 0 ? Value->AsArray()[0] : nullptr;
    };
    auto Is = [this, &Json](const TCHAR* What, const TCHAR* Sent, const TCHAR* Back, EMatch Want)
    {
        TestTrue(What, McpExecuteVerify::Compare(Json(Sent), Json(Back)) == Want);
    };
    Is(TEXT("a float read back as stored"), TEXT("0.1"), TEXT("0.100000001"), EMatch::Same);
    Is(TEXT("a clamped number"), TEXT("5000"), TEXT("1000"), EMatch::Differs);
    Is(TEXT("a vector sent as a list"), TEXT("[1,2,3]"), TEXT("{\"X\":1,\"Y\":2,\"Z\":3}"), EMatch::Same);
    Is(TEXT("a rotator sent as a list"), TEXT("[0,90,0]"), TEXT("{\"Pitch\":0,\"Yaw\":90,\"Roll\":0}"), EMatch::Same);
    Is(TEXT("a moved vector"), TEXT("[1,2,3]"), TEXT("{\"X\":1,\"Y\":2,\"Z\":0}"), EMatch::Differs);
    Is(TEXT("extra fields read back do not count"), TEXT("{\"X\":1}"), TEXT("{\"X\":1,\"Y\":7}"), EMatch::Same);
    Is(TEXT("an enum with its scope"), TEXT("\"EComponentMobility::Movable\""), TEXT("\"Movable\""), EMatch::Same);
    Is(TEXT("an object path with its name"), TEXT("\"/Game/A/SM_Crate\""), TEXT("\"/Game/A/SM_Crate.SM_Crate\""), EMatch::Same);
    Is(TEXT("a wrapped object path"), TEXT("\"/Game/A/SM_Crate\""), TEXT("\"StaticMesh'/Game/A/SM_Crate.SM_Crate'\""), EMatch::Same);
    Is(TEXT("another asset"), TEXT("\"/Game/A/SM_Crate\""), TEXT("\"/Game/A/SM_Barrel.SM_Barrel\""), EMatch::Differs);
    Is(TEXT("a string against a number cannot be judged"), TEXT("\"1\""), TEXT("1"), EMatch::Unknown);

    const TSharedPtr<FJsonObject> Behavior = Json(TEXT("{\"readBack\":{\"compare\":{\"value\":\"$params.value\"}}}"))->AsObject();
    const TSharedPtr<FJsonObject> Params = Json(TEXT("{\"propertyName\":\"Intensity\",\"value\":5000}"))->AsObject();
    TSharedPtr<FJsonObject> Receipt = MakeShared<FJsonObject>();
    McpExecuteVerify::Stamp(Behavior, nullptr, Params, Json(TEXT("{\"value\":5000.0}"))->AsObject(), Receipt);
    TestTrue(TEXT("a matching read-back is verified"), Receipt->GetObjectField(TEXT("receipt"))->GetBoolField(TEXT("verified")));

    Receipt = MakeShared<FJsonObject>();
    McpExecuteVerify::Stamp(Behavior, nullptr, Params, Json(TEXT("{\"value\":1000}"))->AsObject(), Receipt);
    const TSharedPtr<FJsonObject> Block = Receipt->GetObjectField(TEXT("receipt"));
    TestFalse(TEXT("a clamped one is not"), Block->GetBoolField(TEXT("verified")));
    TestEqual(TEXT("naming the field"), Block->GetArrayField(TEXT("mismatches"))[0]->AsObject()->GetStringField(TEXT("field")), FString(TEXT("value")));

    Receipt = MakeShared<FJsonObject>();
    McpExecuteVerify::Stamp(Behavior, Json(TEXT("{\"verify\":false}"))->AsObject(), Params, Json(TEXT("{\"value\":1000}"))->AsObject(), Receipt);
    TestFalse(TEXT("verify false skips it"), Receipt->HasField(TEXT("receipt")));
    McpExecuteVerify::Stamp(Behavior, nullptr, Params, Json(TEXT("{\"applied\":2}"))->AsObject(), Receipt);
    TestFalse(TEXT("a reply without the field says nothing"), Receipt->HasField(TEXT("receipt")));

    // set_property's real reply: the value read back is undeclared output, so the projection put it under details.
    McpExecuteVerify::Stamp(Behavior, nullptr, Params, Json(TEXT("{\"saved\":true,\"details\":{\"value\":5000}}"))->AsObject(), Receipt);
    TestTrue(TEXT("a read-back under details is compared"), Receipt->HasField(TEXT("receipt")) &&
        Receipt->GetObjectField(TEXT("receipt"))->GetBoolField(TEXT("verified")));

    // On the wire: the reply's compaction keeps the verdict and the mismatches in the receipt it trims.
    const TSharedPtr<FJsonObject> Reply = Json(TEXT("{\"success\":true,\"operation\":\"execute\",\"data\":{\"value\":1000},"
        "\"receipt\":{\"verified\":false,\"mismatches\":[{\"field\":\"value\",\"sent\":5000,\"readBack\":1000}],\"handles\":[]}}"))->AsObject();
    const TSharedPtr<FJsonObject> Wire = McpJsonRpcReply::MakeCompactReply(Reply);
    const TSharedPtr<FJsonObject>* Kept = nullptr;
    TestTrue(TEXT("the compacted reply keeps verified and mismatches"), Wire.IsValid() && Wire->TryGetObjectField(TEXT("receipt"), Kept) &&
        Kept && (*Kept)->HasField(TEXT("verified")) && (*Kept)->HasField(TEXT("mismatches")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
