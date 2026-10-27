#include "Domains/Environment/McpEnvShared.h"
#include "Domains/Log/McpLogHistory.h"
#include "Domains/Log/McpScriptErrors.h"
#include "Foundation/McpScreenshotResample.h"

#include "BlueprintEditor.h"
#include "BlueprintEditorModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Selection.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"

// inspect get_editor_focus: what the user is working on, so "this node", "the Blueprint I have open" or "that error"
// resolves to an asset, a graph and node ids instead of a guess.
namespace McpEnvironmentHandlers {
namespace {
// Rows paired with the time their editor was last brought forward, newest first.
using FMcpRecentRows = TArray<TPair<double, TSharedPtr<FJsonValue>>>;

TArray<TSharedPtr<FJsonValue>> McpNewestFirst(FMcpRecentRows &Rows)
{
    Rows.StableSort([](const TPair<double, TSharedPtr<FJsonValue>> &A, const TPair<double, TSharedPtr<FJsonValue>> &B)
    {
        return A.Key > B.Key;
    });
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const TPair<double, TSharedPtr<FJsonValue>> &Row : Rows)
    {
        Out.Add(Row.Value);
    }
    return Out;
}
} // namespace

bool HandleInspectEditorFocusAction(UEngineWireSubsystem &Bridge, const FString &RequestId,
                                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UAssetEditorSubsystem *Editors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    FMcpRecentRows Open;
    TMap<const UObject *, double> Activated;
    for (UObject *Asset : Editors ? Editors->GetAllEditedAssets() : TArray<UObject *>())
    {
        IAssetEditorInstance *Editor = Asset ? Editors->FindEditorForAsset(Asset, false) : nullptr;
        if (!Editor)
        {
            continue;
        }
        const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("asset"), Asset->GetOutermost()->GetName());
        Row->SetStringField(TEXT("editor"), Editor->GetEditorName().ToString());
        const double When = Editor->GetLastActivationTime();
        Activated.Add(Asset, When);
        Open.Emplace(When, TSharedPtr<FJsonValue>(MakeShared<FJsonValueObject>(Row)));
    }

    // The Kismet module creates every Blueprint editor it lists as an FBlueprintEditor (its list is
    // TArray<TWeakPtr<FBlueprintEditor>> in every 5.x), which owns the graph in front and its selection.
    FMcpRecentRows Blueprints;
    FString Headline;
    double HeadlineWhen = -1.0;
    FBlueprintEditorModule &Kismet = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>(TEXT("Kismet"));
    for (const TSharedRef<IBlueprintEditor> &Each : Kismet.GetBlueprintEditors())
    {
        FBlueprintEditor &Editor = static_cast<FBlueprintEditor &>(Each.Get());
        UBlueprint *Blueprint = Editor.GetBlueprintObj();
        if (!Blueprint)
        {
            continue;
        }
        const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("blueprint"), Blueprint->GetOutermost()->GetName());
        const UEdGraph *Graph = Editor.GetFocusedGraph();
        if (Graph)
        {
            Row->SetStringField(TEXT("graph"), Graph->GetName());
        }
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for (UObject *Selected : Editor.GetSelectedNodes())
        {
            const UEdGraphNode *Node = Cast<UEdGraphNode>(Selected);
            if (!Node || Nodes.Num() == 50)
            {
                continue;
            }
            const TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
            Item->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
            Item->SetStringField(TEXT("class"), Node->GetClass()->GetName());
            Nodes.Add(MakeShared<FJsonValueObject>(Item));
        }
        Row->SetArrayField(TEXT("selectedNodes"), Nodes);
        const double *Found = Activated.Find(Blueprint);
        const double When = Found ? *Found : 0.0;
        Blueprints.Emplace(When, TSharedPtr<FJsonValue>(MakeShared<FJsonValueObject>(Row)));
        // The message names the selection in the editor used last.
        if (Nodes.Num() > 0 && When >= HeadlineWhen)
        {
            HeadlineWhen = When;
            Headline = FString::Printf(TEXT("; %s has %d node(s) selected in %s"), *Blueprint->GetName(), Nodes.Num(),
                                       Graph ? *Graph->GetName() : TEXT("its graph"));
        }
    }

    TArray<TSharedPtr<FJsonValue>> Actors;
    if (GEditor && GEditor->GetSelectedActors())
    {
        TArray<AActor *> Selected;
        GEditor->GetSelectedActors()->GetSelectedObjects(Selected);
        for (const AActor *Actor : Selected)
        {
            if (Actor && Actors.Num() < 50)
            {
                Actors.Add(MakeShared<FJsonValueString>(Actor->GetActorLabel()));
            }
        }
    }
    TArray<TSharedPtr<FJsonValue>> Warnings;
    for (const FString &Line : FMcpLogHistory::Get().WarningsSince(FMcpLogHistory::Now() - 60.0, 10))
    {
        Warnings.Add(MakeShared<FJsonValueString>(Line));
    }

    const TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    const int32 OpenCount = Open.Num();
    Result->SetArrayField(TEXT("openEditors"), McpNewestFirst(Open));
    Result->SetArrayField(TEXT("blueprints"), McpNewestFirst(Blueprints));
    Result->SetArrayField(TEXT("selectedActors"), Actors);
    Result->SetArrayField(TEXT("recentWarnings"), Warnings);
    Result->SetArrayField(TEXT("scriptErrors"), FMcpScriptErrors::Get().ToJson());
    // Every editor-state read carries the shader count, as the other variants of the family do.
    McpAddShaderCompileState(Result, nullptr);
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%d asset editor(s) open%s."), OpenCount, *Headline), Result);
    return true;
}
} // namespace McpEnvironmentHandlers
