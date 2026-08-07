#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"

#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"

#include "Engine/Blueprint.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include MCP_USER_DEFINED_STRUCT_HEADER

#if WITH_EDITOR

namespace McpBlueprintHandlers
{

// Resolve structPath to a UScriptStruct (UUserDefinedStruct or native
// UScriptStruct), supporting both raw object paths and "Struct:" resolver
// tokens (which are handed to the shared type-string resolver).
static UStruct* McpResolveStructTarget(const FString& StructPath)
{
    if (StructPath.StartsWith(TEXT("Struct:")))
    {
        const McpBlueprintUtils::FTypeResolutionResult Res = McpBlueprintUtils::ResolvePinType(StructPath);
        if (Res.bSuccess && Res.PinType.PinSubCategoryObject.IsValid())
        {
            return Cast<UStruct>(Res.PinType.PinSubCategoryObject.Get());
        }
        return nullptr;
    }
    if (UUserDefinedStruct* UD = LoadObject<UUserDefinedStruct>(nullptr, *StructPath))
    {
        return UD;
    }
    if (UScriptStruct* NS = LoadObject<UScriptStruct>(nullptr, *StructPath))
    {
        return NS;
    }
    return nullptr;
}

// Locate a graph by name across the Blueprint's graph collections.
static UEdGraph* McpFindGraphByName(UBlueprint* BP, const FString& GraphName)
{
    auto Match = [&](const TArray<TObjectPtr<UEdGraph>>& Graphs) -> UEdGraph*
    {
        for (UEdGraph* G : Graphs)
        {
            if (G && G->GetName() == GraphName)
            {
                return G;
            }
        }
        return nullptr;
    };
    if (UEdGraph* G = Match(BP->UbergraphPages)) return G;
    if (UEdGraph* G = Match(BP->FunctionGraphs)) return G;
    if (UEdGraph* G = Match(BP->MacroGraphs)) return G;
    return nullptr;
}

bool HandleBlueprintStructMakeBreakNodes(const FBlueprintActionContext &Context)
{
    MCP_BLUEPRINT_ACTION_LOCALS(Context);

    if (Lower != TEXT("create_struct_make_break_nodes"))
    {
        return false;
    }

    const FString StructPath = GetJsonStringField(Payload, TEXT("structPath"));
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    const FString NodeType = GetJsonStringField(Payload, TEXT("nodeType"));
    const FString GraphName = GetJsonStringField(Payload, TEXT("graphName"));
    const double PosX = GetJsonNumberField(Payload, TEXT("posX"), TNumericLimits<double>::Lowest());
    const double PosY = GetJsonNumberField(Payload, TEXT("posY"), TNumericLimits<double>::Lowest());

    if (StructPath.IsEmpty() || BlueprintPath.IsEmpty() || NodeType.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing required parameter: structPath, blueprintPath or nodeType"), TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (NodeType != TEXT("make") && NodeType != TEXT("break"))
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("nodeType must be 'make' or 'break'"), TEXT("INVALID_OPERATION"));
        return true;
    }

    UStruct* Struct = McpResolveStructTarget(StructPath);
    if (!Struct)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UBlueprint *BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!BP)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UEdGraph *Graph = nullptr;
    if (!GraphName.IsEmpty())
    {
        Graph = McpFindGraphByName(BP, GraphName);
        if (!Graph)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Graph not found: %s"), *GraphName), TEXT("INVALID_OPERATION"));
            return true;
        }
    }
    else if (BP->UbergraphPages.Num() > 0)
    {
        Graph = BP->UbergraphPages[0];
    }
    else if (BP->FunctionGraphs.Num() > 0)
    {
        Graph = BP->FunctionGraphs[0];
    }
    else if (BP->MacroGraphs.Num() > 0)
    {
        Graph = BP->MacroGraphs[0];
    }

    if (!Graph)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Blueprint has no editable graph"), TEXT("INVALID_OPERATION"));
        return true;
    }

    // Position: honor explicit posX/posY; else staggered default so repeated
    // create_struct_make_break_nodes calls don't stack nodes at (0,0).
    FVector2f Pos;
    if (PosX > TNumericLimits<double>::Lowest() && PosY > TNumericLimits<double>::Lowest())
    {
        Pos = FVector2f(static_cast<float>(PosX), static_cast<float>(PosY));
    }
    else
    {
        const int32 Stagger = Graph->Nodes.Num();
        Pos = FVector2f(static_cast<float>((Stagger % 8) * 240),
                        static_cast<float>((Stagger / 8) * 240));
    }

    TSharedPtr<FJsonObject> Result;
    McpBlueprintUtils::McpBuildStructMakeBreakNodes(BP, Struct, Graph, Pos,
        NodeType == TEXT("make"), Result);

    if (Result->HasField(TEXT("error")))
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            Result->GetStringField(TEXT("error")), TEXT("INVALID_OPERATION"));
        return true;
    }

    // Backward-compatible field the build function does not set.
    Result->SetStringField(TEXT("nodeType"), NodeType);

    BP->MarkPackageDirty();
    McpSafeAssetSave(BP);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Make/Break node created"), Result);
    return true;
}

} // namespace McpBlueprintHandlers

#endif // WITH_EDITOR
