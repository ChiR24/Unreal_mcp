#include "Domains/BlueprintGraph/Discovery/McpBPFinderNodes.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpBPFinderNodesTest, "EngineWire.BlueprintGraph.FinderNodes",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpBPFinderNodesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpBlueprintGraphHandlers;
    const auto Types = [](const TArray<FString>& Wanted)
    {
        FString Note;
        TArray<FString> Out;
        for (const TSharedPtr<FJsonValue>& Row : FinderNodeTypes(Wanted, Note))
        {
            Out.Add(Row->AsObject()->GetStringField(TEXT("nodeType")));
        }
        return FString::Join(Out, TEXT(","));
    };
    // The finder has already dropped "for", "from" and "to".
    TestEqual(TEXT("spawn actor from class"), Types({TEXT("spawn"), TEXT("actor"), TEXT("class")}), FString(TEXT("SpawnActorFromClass")));
    TestEqual(TEXT("for each loop is not also a for loop"), Types({TEXT("each"), TEXT("loop")}), FString(TEXT("ForEachLoop")));
    TestEqual(TEXT("for loop"), Types({TEXT("loop")}), FString(TEXT("ForLoop")));
    TestEqual(TEXT("branch if condition, once"), Types({TEXT("branch"), TEXT("if"), TEXT("condition")}), FString(TEXT("Branch")));
    TestEqual(TEXT("cast actor to character"), Types({TEXT("cast"), TEXT("actor"), TEXT("character")}), FString(TEXT("Cast")));
    TestEqual(TEXT("game time names no node"), Types({TEXT("game"), TEXT("time")}), FString());
    return true;
}
#endif
