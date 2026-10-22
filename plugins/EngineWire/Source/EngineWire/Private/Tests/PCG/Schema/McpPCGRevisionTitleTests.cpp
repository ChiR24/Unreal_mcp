#include "Misc/AutomationTest.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/App.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRevisionTitleTest, "EngineWire.PCG.Revision.LocalizedTitlesAreNotAuthoredEdits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRevisionTitleTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World || World->WorldType != EWorldType::Editor || FString(FApp::GetProjectName()) != TEXT("PCGValidation"))
    {
        AddError(TEXT("The presentation-revision probe requires the owned PCGValidation editor; no culture was changed."));
        return false;
    }
    // Process-local runtime culture only; never write editor settings or configuration.
    const FString OriginalCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
    ON_SCOPE_EXIT { FInternationalization::Get().SetCurrentCulture(OriginalCulture); };
    if (!TestTrue(TEXT("English runtime culture is available"), FInternationalization::Get().SetCurrentCulture(TEXT("en")))) return false;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGCreatePointsSettings"));
    if (!TestNotNull(TEXT("Native localized fixture class is already loaded"), Class)) return false;
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(Class, Settings);
    if (!TestTrue(TEXT("A real non-boundary fixture node exists"), Node && Settings)) return false;
    const FString EnglishTitle = ReadPCGNode(Graph.Get(), Node, false)->GetStringField(TEXT("title"));
    const FString Before = GetPCGGraphRevision(Graph.Get());
    const bool bDirty = Graph->GetOutermost()->IsDirty();
    if (!TestFalse(TEXT("A complete authored token exists before presentation changes"), Before.IsEmpty())) return false;
    if (!TestTrue(TEXT("Chinese runtime culture is available"), FInternationalization::Get().SetCurrentCulture(TEXT("zh-Hans")))) return false;
    const FString ChineseTitle = ReadPCGNode(Graph.Get(), Node, false)->GetStringField(TEXT("title"));
    TestTrue(TEXT("The fixture actually observes different localized presentation strings"), EnglishTitle != ChineseTitle);
    const FString AfterPresentation = GetPCGGraphRevision(Graph.Get());
    TestEqual(TEXT("Localized display titles are not authored PCG edits"), AfterPresentation, Before);
    TestEqual(TEXT("Presentation change leaves the package dirty state unchanged"), Graph->GetOutermost()->IsDirty(), bDirty);
    Node->NodeTitle = FName(TEXT("MCP_AuthoredNodeTitle"));
    const FString Authored = GetPCGGraphRevision(Graph.Get());
    TestFalse(TEXT("Explicit authored NodeTitle still invalidates the token"), Authored.IsEmpty() || Authored == AfterPresentation);
    if (!TestTrue(TEXT("Returning to English runtime culture succeeds"), FInternationalization::Get().SetCurrentCulture(TEXT("en")))) return false;
    TestEqual(TEXT("Explicit title state remains language-independent"), GetPCGGraphRevision(Graph.Get()), Authored);
    Node->NodeTitle = NAME_None;
    TestEqual(TEXT("Restoring the authored title restores the original token"), GetPCGGraphRevision(Graph.Get()), Before);
    Settings->Seed += 1;
    const FString ChangedSettings = GetPCGGraphRevision(Graph.Get());
    TestFalse(TEXT("Actual settings still invalidate the token"), ChangedSettings.IsEmpty() || ChangedSettings == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPinTextIdentityTest, "EngineWire.PCG.Revision.PinTextIdentityIsNotAuthoredEdit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPinTextIdentityTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass(), Settings);
    UPCGPin* Pin = Node ? Node->GetInputPin(TEXT("Seed")) : nullptr;
    if (!TestNotNull(TEXT("An actual spawner override pin exists"), Pin)) return false;
    const FText Format = NSLOCTEXT("McpPCGRevisionTests", "PinTooltip", "Type {0}, attribute {1}");
    const FText Type = FText::FromString(TEXT("int32")), Name = FText::FromString(TEXT("Seed"));
    Pin->Properties.Tooltip = FText::Format(Format, Type, Name);
    FString RawBefore;
    FPCGPinProperties::StaticStruct()->ExportText(RawBefore, &Pin->Properties, nullptr, Pin, PPF_None, nullptr);
    const FString Before = GetPCGGraphRevision(Graph.Get());
    const FString Source = Pin->Properties.Tooltip.BuildSourceString();
    if (!TestFalse(TEXT("The initial authored revision is complete"), Before.IsEmpty())) return false;
    Pin->Properties.Tooltip = FText::Format(Format,
        FText::ChangeKey(TEXT(""), TEXT("7B5A848F4DD7407EBAF111904B6679A1"), Type),
        FText::ChangeKey(TEXT(""), TEXT("1994976A4D72484CB3EAACDDA4690A94"), Name));
    FString RawAfter;
    FPCGPinProperties::StaticStruct()->ExportText(RawAfter, &Pin->Properties, nullptr, Pin, PPF_None, nullptr);
    TestNotEqual(TEXT("Persistence-style text identity promotion changes raw pin export"), RawAfter, RawBefore);
    TestEqual(TEXT("The authored tooltip source did not change"), Pin->Properties.Tooltip.BuildSourceString(), Source);
    TestEqual(TEXT("Text history identity alone does not invalidate the graph token"), GetPCGGraphRevision(Graph.Get()), Before);
    Pin->Properties.bAllowMultipleData = !Pin->Properties.bAllowMultipleData;
    TestNotEqual(TEXT("Real pin restrictions remain authored inputs"), GetPCGGraphRevision(Graph.Get()), Before);
    Pin->Properties.bAllowMultipleData = !Pin->Properties.bAllowMultipleData;
    Pin->Properties.Tooltip = FText::Format(Format, Type, FText::FromString(TEXT("ChangedSeed")));
    TestNotEqual(TEXT("Actual tooltip source edits remain visible"), GetPCGGraphRevision(Graph.Get()), Before);
    Pin->Properties.Tooltip = FText::Format(Format, Type, Name);
    TestEqual(TEXT("Restoring authored pin state restores the token"), GetPCGGraphRevision(Graph.Get()), Before);
    ++Settings->Seed;
    TestNotEqual(TEXT("Settings edits remain authored inputs"), GetPCGGraphRevision(Graph.Get()), Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPinTextSemanticsTest, "EngineWire.PCG.Revision.PinTextCultureAndSemanticKinds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPinTextSemanticsTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World || World->WorldType != EWorldType::Editor || FString(FApp::GetProjectName()) != TEXT("PCGValidation"))
    {
        AddError(TEXT("Pin text culture coverage requires the owned PCGValidation editor."));
        return false;
    }
    const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
    ON_SCOPE_EXIT { FInternationalization::Get().SetCurrentCulture(Culture); };
    if (!TestTrue(TEXT("English culture is available"), FInternationalization::Get().SetCurrentCulture(TEXT("en")))) return false;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass(), Settings);
    UPCGPin* Pin = Node ? Node->GetInputPin(TEXT("Seed")) : nullptr;
    if (!TestNotNull(TEXT("Native spawner generated an override tooltip"), Pin)) return false;
    const FString Source = Pin->Properties.Tooltip.BuildSourceString();
    const FString English = GetPCGGraphRevision(Graph.Get());
    if (!TestFalse(TEXT("Complete pin revision"), English.IsEmpty())) return false;
    if (!TestTrue(TEXT("Chinese culture is available"), FInternationalization::Get().SetCurrentCulture(TEXT("zh-Hans")))) return false;
    TestEqual(TEXT("Native tooltip retains its untranslated source"), Pin->Properties.Tooltip.BuildSourceString(), Source);
    TestEqual(TEXT("Derived pin localization is not authored state"), GetPCGGraphRevision(Graph.Get()), English);
    Pin->Properties.Tooltip = FText::FromString(TEXT("Shared tooltip"));
    const FString Ordinary = GetPCGGraphRevision(Graph.Get());
    Pin->Properties.Tooltip = FText::AsCultureInvariant(TEXT("Shared tooltip"));
    TestNotEqual(TEXT("Invariant and localizable tooltip kinds remain distinct"), GetPCGGraphRevision(Graph.Get()), Ordinary);
    const FName TableId(*(TEXT("McpPinText_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const FStringTableRef Table = FStringTable::NewStringTable();
    for (const TCHAR* Key : {TEXT("First"), TEXT("Second")})
    {
#if WITH_EDITORONLY_DATA && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8))
        Table->SetSourceString(Key, TEXT("Shared tooltip"), FString());
#else
        Table->SetSourceString(Key, TEXT("Shared tooltip"));
#endif
    }
    FStringTableRegistry::Get().RegisterStringTable(TableId, Table);
    ON_SCOPE_EXIT { FStringTableRegistry::Get().UnregisterStringTable(TableId); };
    Pin->Properties.Tooltip = FText::FromStringTable(TableId, TEXT("First"), EStringTableLoadingPolicy::Find);
    TestTrue(TEXT("The fixture uses a real in-memory string-table entry"), Pin->Properties.Tooltip.IsFromStringTable());
    const FString First = GetPCGGraphRevision(Graph.Get());
    Pin->Properties.Tooltip = FText::FromStringTable(TableId, TEXT("Second"), EStringTableLoadingPolicy::Find);
    TestEqual(TEXT("The two table entries display the same source"), Pin->Properties.Tooltip.BuildSourceString(), FString(TEXT("Shared tooltip")));
    TestNotEqual(TEXT("String-table entry identity is still authored state"), GetPCGGraphRevision(Graph.Get()), First);
    return true;
}
#endif
