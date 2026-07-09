#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"

#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

#if WITH_EDITOR

namespace McpBlueprintHandlers
{

bool HandleBlueprintStructMakeBreakNodes(const FBlueprintActionContext &Context)
{
    MCP_BLUEPRINT_ACTION_LOCALS(Context);

    if (Lower != TEXT("create_struct_make_break_nodes"))
    {
        return false;
    }

    FString StructPath = GetJsonStringField(Payload, TEXT("structPath"));
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString NodeType = GetJsonStringField(Payload, TEXT("nodeType"));

    if (StructPath.IsEmpty() || BlueprintPath.IsEmpty() || NodeType.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing required parameter: structPath, blueprintPath or nodeType"), TEXT("MISSING_PARAMETER"));
        return true;
    }

    UUserDefinedStruct *S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
    if (!S)
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
    if (BP->UbergraphPages.Num() > 0)
    {
        Graph = BP->UbergraphPages[0];
    }
    else if (BP->FunctionGraphs.Num() > 0)
    {
        Graph = BP->FunctionGraphs[0];
    }

    if (!Graph)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Blueprint has no editable graph"), TEXT("INVALID_OPERATION"));
        return true;
    }

    UEdGraphNode *Node = nullptr;
    if (NodeType == TEXT("make"))
    {
        UK2Node_MakeStruct *MakeNode = NewObject<UK2Node_MakeStruct>(Graph);
        MakeNode->StructType = S;
        MakeNode->CreateNewGuid();
        Graph->AddNode(MakeNode);
        MakeNode->ReconstructNode();
        Node = MakeNode;
    }
    else if (NodeType == TEXT("break"))
    {
        UK2Node_BreakStruct *BreakNode = NewObject<UK2Node_BreakStruct>(Graph);
        BreakNode->StructType = S;
        BreakNode->CreateNewGuid();
        Graph->AddNode(BreakNode);
        BreakNode->ReconstructNode();
        Node = BreakNode;
    }
    if (Node)
    {
        // Offset new nodes so repeated create_struct_make_break_nodes calls
        // don't stack every node at (0,0) and overlap existing ones.
        const int32 Stagger = Graph->Nodes.Num();
        Node->NodePosX = (Stagger % 8) * 240;
        Node->NodePosY = (Stagger / 8) * 240;
    }
    else
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("nodeType must be 'make' or 'break'"), TEXT("INVALID_OPERATION"));
        return true;
    }

    BP->MarkPackageDirty();
    McpSafeAssetSave(BP);

    TArray<TSharedPtr<FJsonValue>> PinNames;
    if (Node)
    {
        for (UEdGraphPin *Pin : Node->Pins)
        {
            if (Pin && !Pin->PinName.IsNone())
            {
                PinNames.Add(MakeShared<FJsonValueString>(Pin->PinName.ToString()));
            }
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeGuid"), Node ? Node->NodeGuid.ToString() : FString());
    Result->SetStringField(TEXT("nodeType"), NodeType);
    Result->SetStringField(TEXT("structPath"), StructPath);
    Result->SetArrayField(TEXT("pinNames"), PinNames);
    McpHandlerUtils::AddVerification(Result, Node);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Make/Break node created"), Result);
    return true;
}

} // namespace McpBlueprintHandlers

#endif // WITH_EDITOR
