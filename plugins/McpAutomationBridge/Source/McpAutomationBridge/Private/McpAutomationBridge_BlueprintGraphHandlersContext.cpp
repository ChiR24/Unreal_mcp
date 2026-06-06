#include "McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

namespace McpBlueprintGraphHandlers
{
void FActionContext::SendError(
    const FString& Message,
    const FString& ErrorCode) const
{
    Subsystem->SendAutomationError(
        RequestingSocket,
        RequestId,
        Message,
        ErrorCode);
}

void FActionContext::SendResponse(
    const FString& Message,
    const TSharedPtr<FJsonObject>& Result) const
{
    Subsystem->SendAutomationResponse(
        RequestingSocket,
        RequestId,
        true,
        Message,
        Result);
}

bool ValidateProvidedPaths(const FActionContext& Context)
{
    FString AssetPath;
    if (Context.Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
        !AssetPath.IsEmpty() &&
        SanitizeProjectRelativePath(AssetPath).IsEmpty())
    {
        Context.SendError(
            TEXT("Invalid assetPath: contains traversal sequences or invalid characters."),
            TEXT("INVALID_PATH"));
        return false;
    }

    FString BlueprintPath;
    if (Context.Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) &&
        !BlueprintPath.IsEmpty() &&
        SanitizeProjectRelativePath(BlueprintPath).IsEmpty())
    {
        Context.SendError(
            TEXT("Invalid blueprintPath: contains traversal sequences or invalid characters."),
            TEXT("INVALID_PATH"));
        return false;
    }

    return true;
}

#if WITH_EDITOR
UEdGraphNode* FActionContext::FindNode(const FString& Id) const
{
    if (Id.IsEmpty())
    {
        return nullptr;
    }

    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        if (Node &&
            (Node->NodeGuid.ToString().Equals(Id, ESearchCase::IgnoreCase) ||
             Node->GetName().Equals(Id, ESearchCase::IgnoreCase)))
        {
            return Node;
        }
    }
    return nullptr;
}

UEdGraphPin* FActionContext::FindPin(
    UEdGraphNode* Node,
    const FString& PinName) const
{
    if (!Node || PinName.IsEmpty())
    {
        return nullptr;
    }

    FString CleanPinName;
    if (!PinName.Split(TEXT("."), nullptr, &CleanPinName))
    {
        CleanPinName = PinName;
    }

    auto MatchPin = [Node](const FString& Candidate) -> UEdGraphPin*
    {
        if (UEdGraphPin* Pin = Node->FindPin(*Candidate))
        {
            return Pin;
        }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin &&
                (Pin->PinName.ToString().Equals(
                     Candidate,
                     ESearchCase::IgnoreCase) ||
                 Pin->GetDisplayName().ToString().Equals(
                     Candidate,
                     ESearchCase::IgnoreCase)))
            {
                return Pin;
            }
        }
        return nullptr;
    };

    if (UEdGraphPin* Pin = MatchPin(CleanPinName))
    {
        return Pin;
    }

    FString UnderscorePinName = CleanPinName;
    UnderscorePinName.ReplaceCharInline(TEXT(' '), TEXT('_'));
    if (!UnderscorePinName.Equals(
            CleanPinName,
            ESearchCase::CaseSensitive))
    {
        return MatchPin(UnderscorePinName);
    }
    return nullptr;
}

static UEdGraph* FindTargetGraph(
    UBlueprint* Blueprint,
    const FString& GraphName)
{
    UEdGraph* TargetGraph = nullptr;
    if (GraphName.IsEmpty() ||
        GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
    {
        if (Blueprint->UbergraphPages.Num() > 0)
        {
            TargetGraph = Blueprint->UbergraphPages[0];
        }
    }
    else
    {
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph->GetName() == GraphName)
            {
                TargetGraph = Graph;
                break;
            }
        }
        if (!TargetGraph)
        {
            for (UEdGraph* Graph : Blueprint->UbergraphPages)
            {
                if (Graph->GetName() == GraphName)
                {
                    TargetGraph = Graph;
                    break;
                }
            }
        }
    }

    if (!TargetGraph)
    {
        TArray<UEdGraph*> AllGraphs;
        Blueprint->GetAllGraphs(AllGraphs);
        for (UEdGraph* Graph : AllGraphs)
        {
            if (Graph->GetName() == GraphName)
            {
                TargetGraph = Graph;
                break;
            }
        }
    }
    return TargetGraph;
}

bool PrepareBlueprintAndGraph(FActionContext& Context)
{
    FString AssetPath;
    if (!Context.Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("blueprintPath"), AssetPath);
    }

    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty())
    {
        Context.SendError(
            TEXT("Invalid asset path: contains traversal sequences or invalid characters."),
            TEXT("INVALID_PATH"));
        return false;
    }

    FString NormalizedPath;
    FString LoadError;
    Context.Blueprint =
        LoadBlueprintAsset(AssetPath, NormalizedPath, LoadError);
    if (!Context.Blueprint)
    {
        Context.SendError(
            LoadError.IsEmpty()
                ? FString::Printf(
                      TEXT("Could not load blueprint at path: %s"),
                      *AssetPath)
                : LoadError,
            TEXT("ASSET_NOT_FOUND"));
        return false;
    }

    FString GraphName;
    Context.Payload->TryGetStringField(TEXT("graphName"), GraphName);
    Context.TargetGraph = FindTargetGraph(Context.Blueprint, GraphName);
    if (!Context.TargetGraph)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Could not find graph '%s' in blueprint."),
                *GraphName),
            TEXT("GRAPH_NOT_FOUND"));
        return false;
    }
    return true;
}

bool HandleListNodeTypes(FActionContext& Context)
{
    if (Context.SubAction != TEXT("list_node_types"))
    {
        return false;
    }

    TArray<TSharedPtr<FJsonValue>> NodeTypes;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(UK2Node::StaticClass()) ||
            It->HasAnyClassFlags(CLASS_Abstract))
        {
            continue;
        }
        TSharedPtr<FJsonObject> TypeObj =
            McpHandlerUtils::CreateResultObject();
        TypeObj->SetStringField(TEXT("className"), It->GetName());
        TypeObj->SetStringField(
            TEXT("displayName"),
            It->GetDisplayNameText().ToString());
        NodeTypes.Add(MakeShared<FJsonValueObject>(TypeObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("nodeTypes"), NodeTypes);
    Result->SetNumberField(TEXT("count"), NodeTypes.Num());
    Context.SendResponse(TEXT("Node types listed."), Result);
    return true;
}
#else
bool PrepareBlueprintAndGraph(FActionContext&)
{
    return false;
}

bool HandleListNodeTypes(FActionContext&)
{
    return false;
}
#endif
}
