#include "Domains/ControlEditor/Toolsets/McpEditorToolsets.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpEditorToolsetsClassifyTest, "EngineWire.ControlEditor.ToolsetEffects",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A tool's effect: the verb rule where the registry states no access mode (5.8), the stated mode where it does (5.9),
// execute for an unstated one in a registry that states modes, and the blocked tools blocked whatever they state.
bool FMcpEditorToolsetsClassifyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpEditorToolsets;
    const auto Schema = [](const TCHAR* Mode)
    {
        TSharedPtr<FJsonObject> Tool = MakeShared<FJsonObject>();
        TSharedPtr<FJsonObject> Annotations = MakeShared<FJsonObject>();
        Annotations->SetStringField(TEXT("accessMode"), Mode);
        Tool->SetObjectField(TEXT("annotations"), Annotations);
        return Tool;
    };
    TestTrue(TEXT("5.8: a Get tool reads"), ClassifyTool(TEXT("LevelToolset.GetActors")) == EToolClass::Read);
    TestTrue(TEXT("5.8: a Delete tool is destructive"), ClassifyTool(TEXT("LevelToolset.DeleteActor")) == EToolClass::Destructive);
    TestTrue(TEXT("5.8: any other verb writes"), ClassifyTool(TEXT("LevelToolset.SpawnActor")) == EToolClass::Write);
    TestTrue(TEXT("the stated mode wins over the verb"), ClassifyTool(TEXT("AssetToolset.DeleteCache"), Schema(TEXT("read")), true) == EToolClass::Read);
    TestTrue(TEXT("write as stated"), ClassifyTool(TEXT("AssetToolset.GetOrMake"), Schema(TEXT("write")), true) == EToolClass::Write);
    TestTrue(TEXT("execute is destructive"), ClassifyTool(TEXT("AssetToolset.GetThing"), Schema(TEXT("execute")), true) == EToolClass::Destructive);
    TestTrue(TEXT("unstated in a registry that states modes is execute"), ClassifyTool(TEXT("AssetToolset.GetThing"), MakeShared<FJsonObject>(), true) == EToolClass::Destructive);
    TestTrue(TEXT("a Python tool stays blocked whatever it states"), ClassifyTool(TEXT("PythonToolset.RunCode"), Schema(TEXT("read")), true) == EToolClass::Blocked);
    TestTrue(TEXT("the settings toolset stays blocked"), ClassifyTool(TEXT("ConfigSettingsToolset.GetValue"), Schema(TEXT("read")), true) == EToolClass::Blocked);

    TArray<TSharedPtr<FJsonValue>> Plain, Stated;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[{\"name\":\"A\",\"tools\":[{\"name\":\"A.GetX\"}]}]")), Plain);
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[{\"name\":\"A\",\"tools\":[{\"name\":\"A.GetX\"},{\"name\":\"A.Y\",\"annotations\":{\"accessMode\":\"write\"}}]}]")), Stated);
    TestFalse(TEXT("a registry with no mode states none"), AnyAnnotated(Plain));
    TestTrue(TEXT("one tool's mode makes the registry one that states modes"), AnyAnnotated(Stated));
    return true;
}
#endif
