// McpEngineRolesTests.cpp — on the running engine every role resolves to what the static role check found in this
// engine's header tree (Resources/EngineRoles/resolved.generated.json), and a role that check found absent or
// ambiguous does not resolve. A role whose plugin this project leaves off is skipped.
#include "Foundation/Reflect/McpEngineRoles.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Foundation/Reflect/McpEngineRolesMatch.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpEngineRolesTest,
    "EngineWire.Foundation.EngineRoles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpEngineRolesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const TSharedPtr<IPlugin> Self = IPluginManager::Get().FindPlugin(TEXT("EngineWire"));
    FString Text;
    TSharedPtr<FJsonObject> Table;
    const bool bRead = Self.IsValid() &&
        FFileHelper::LoadFileToString(Text, *FPaths::Combine(Self->GetBaseDir(), TEXT("Resources"), TEXT("EngineRoles"), TEXT("resolved.generated.json"))) &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Table) && Table.IsValid();
    if (!TestTrue(TEXT("the static role table is readable"), bRead))
    {
        return false;
    }
    const FString Engine = FEngineVersion::Current().ToString(EVersionComponent::Minor);
    int32 Checked = 0;
    // Held: a range-for over a member of the temporary Describe returns would read it after it is freed.
    const TSharedRef<FJsonObject> Described = McpEngineRoles::Describe(FString());
    for (const TSharedPtr<FJsonValue>& Value : Described->GetArrayField(TEXT("roles")))
    {
        const TSharedPtr<FJsonObject> Row = Value->AsObject();
        const FString Role = Row->GetStringField(TEXT("role"));
        const FString PluginName = Row->GetStringField(TEXT("plugin"));
        const TSharedPtr<IPlugin> Plugin = PluginName.IsEmpty() ? nullptr : IPluginManager::Get().FindPlugin(PluginName);
        if (!PluginName.IsEmpty() && !(Plugin.IsValid() && Plugin->IsEnabled()))
        {
            continue;
        }
        const TSharedPtr<FJsonObject>* Columns = nullptr;
        FString Expected;
        if (!Table->TryGetObjectField(Role, Columns) || !Columns)
        {
            AddError(FString::Printf(TEXT("%s is not in resolved.generated.json: run .compat/check-roles.mjs --write"), *Role));
            continue;
        }
        const bool bExpected = (*Columns)->TryGetStringField(Engine, Expected) && !Expected.StartsWith(TEXT("?"));
        const bool bResolved = Row->GetBoolField(TEXT("resolved"));
        ++Checked;
        if (bExpected != bResolved)
        {
            AddError(FString::Printf(TEXT("%s: the header check says %s on %s, the running engine says %s (%s)"), *Role,
                                     bExpected ? *Expected : TEXT("absent"), *Engine, bResolved ? TEXT("resolved") : TEXT("absent"),
                                     *Row->GetStringField(TEXT("evidence"))));
            continue;
        }
        if (bResolved)
        {
            // The table holds the C++ name (UMVVMBlueprintView, AnimationAssets); a type role is found by its path.
            const FString Name = Expected.Mid(Expected.Find(TEXT(":")) + 1);
            const FString Found = Row->GetStringField(TEXT("found"));
            const bool bSame = Found.StartsWith(TEXT("/")) ? Found.EndsWith(TEXT(".") + Name.Mid(1)) : Found == Name;
            TestTrue(FString::Printf(TEXT("%s is %s here, as in the header tree"), *Role, *Name), bSame);
        }
    }
    AddInfo(FString::Printf(TEXT("%d roles checked on %s"), Checked, *Engine));
    TestTrue(TEXT("at least one role is checked"), Checked > 0);

    // Words and type names, the matching rules shared with the header check.
    TestTrue(TEXT("a bool's b prefix is not a word"),
             McpEngineRolesMatch::Words(TEXT("bDeprecatedSource")) == TArray<FString>{TEXT("deprecated"), TEXT("source")});
    TestTrue(TEXT("camel case splits"),
             McpEngineRolesMatch::Words(TEXT("ViewModelName")) == TArray<FString>{TEXT("view"), TEXT("model"), TEXT("name")});
    const FProperty* Tags = FindFProperty<FProperty>(AActor::StaticClass(), TEXT("Tags"));
    TestEqual(TEXT("an array's C++ name"), McpEngineRolesMatch::CppName(Tags), FString(TEXT("TArray<FName>")));
    const TSharedPtr<FJsonObject> NameArray = MakeShared<FJsonObject>();
    const TSharedPtr<FJsonObject> Inner = MakeShared<FJsonObject>();
    Inner->SetStringField(TEXT("cpp"), TEXT("FName"));
    NameArray->SetObjectField(TEXT("array"), Inner);
    auto NoRoles = [](FName) -> UStruct* { return nullptr; };
    TestTrue(TEXT("an array of FName fits {array: {cpp: FName}}"), McpEngineRolesMatch::Fits(NameArray, Tags, NoRoles));
    TestFalse(TEXT("and not {cpp: FName}"), McpEngineRolesMatch::Fits(Inner, Tags, NoRoles));
    TestFalse(TEXT("an unknown role does not resolve"), McpEngineRoles::Resolve(TEXT("No.Such.Role")).IsResolved());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
