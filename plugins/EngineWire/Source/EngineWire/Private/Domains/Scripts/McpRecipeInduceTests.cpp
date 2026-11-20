#include "Domains/Scripts/McpRecipeInduce.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpRecipeInduceTest, "EngineWire.Scripts.RecipeInduce",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace McpRecipeInduceTestDetail
{
McpRecipeInduce::FCall Call(const TCHAR* Capability, TFunctionRef<void(FJsonObject&)> Params, TFunctionRef<void(FJsonObject&)> Data)
{
    McpRecipeInduce::FCall Out;
    Out.Capability = Capability;
    Out.Params = MakeShared<FJsonObject>();
    Out.Data = MakeShared<FJsonObject>();
    Params(*Out.Params);
    Data(*Out.Data);
    return Out;
}
} // namespace McpRecipeInduceTestDetail

// Make a material, read a Blueprint nothing later uses, paint the material, make another of the same name elsewhere:
// the paint takes the made path from the first reply, the read is left out, the repeated name is one parameter, a new
// folder a second, and the property painted stays as written.
bool FMcpRecipeInduceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using McpRecipeInduceTestDetail::Call;
    const TArray<McpRecipeInduce::FCall> Calls = {
        Call(TEXT("test.make"), [](FJsonObject& P) { P.SetStringField(TEXT("name"), TEXT("M_Red")); P.SetStringField(TEXT("path"), TEXT("/Game/Mat")); },
             [](FJsonObject& D) { D.SetStringField(TEXT("assetPath"), TEXT("/Game/Mat/M_Red")); }),
        Call(TEXT("blueprint.lint_blueprint"), [](FJsonObject& P) { P.SetStringField(TEXT("blueprintPath"), TEXT("/Game/BP_Unused")); },
             [](FJsonObject& D) { D.SetNumberField(TEXT("findingCount"), 0); }),
        Call(TEXT("test.paint"), [](FJsonObject& P)
             {
                 P.SetStringField(TEXT("materialPath"), TEXT("/Game/Mat/M_Red"));
                 P.SetNumberField(TEXT("value"), 0.5);
                 P.SetStringField(TEXT("propertyName"), TEXT("Roughness"));
             },
             [](FJsonObject&) {}),
        Call(TEXT("test.make"), [](FJsonObject& P) { P.SetStringField(TEXT("name"), TEXT("M_Red")); P.SetStringField(TEXT("path"), TEXT("/Game/Mat2")); },
             [](FJsonObject&) {}),
    };
    const TSharedPtr<FJsonObject> Proposal = McpRecipeInduce::Induce(Calls);
    const FString Script = Proposal->GetStringField(TEXT("script"));
    TestEqual(TEXT("three steps: the unused read is left out"), int32(Proposal->GetNumberField(TEXT("stepCount"))), 3);
    TestFalse(FString::Printf(TEXT("the read is not in the script:\n%s"), *Script), Script.Contains(TEXT("lint_blueprint")));
    TestTrue(FString::Printf(TEXT("the paint takes the path the first step made:\n%s"), *Script), Script.Contains(TEXT(":materialPath s1.assetPath")));
    TestTrue(TEXT("the property painted stays as written"), Script.Contains(TEXT(":propertyName \"Roughness\"")));
    TestTrue(TEXT("the painted value is a parameter"), Script.Contains(TEXT(":value value")));
    TestTrue(TEXT("the repeated name is one parameter, the new folder another"), Script.Contains(TEXT("(let s4 (test.make :name name :path path2))")));
    TArray<FString> Names;
    for (const TSharedPtr<FJsonValue>& Name : Proposal->GetArrayField(TEXT("paramNames")))
    {
        Names.Add(Name->AsString());
    }
    TestEqual(TEXT("four parameters"), Names.Num(), 4);
    TestTrue(TEXT("each named after its field"), Names.Contains(TEXT("name")) && Names.Contains(TEXT("path")) && Names.Contains(TEXT("value")) && Names.Contains(TEXT("path2")));
    TestEqual(TEXT("args holds this run's value of each"), Proposal->GetObjectField(TEXT("args"))->GetStringField(TEXT("path2")), FString(TEXT("/Game/Mat2")));
    return true;
}
#endif
