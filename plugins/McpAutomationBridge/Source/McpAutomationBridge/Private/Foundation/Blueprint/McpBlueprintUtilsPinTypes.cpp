#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Safety/McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Math/Color.h"
#endif

#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2

namespace McpBlueprintUtils
{

UK2Node_VariableGet* CreateVariableGetter(UEdGraph* Graph, const FMemberReference& VarRef, float NodePosX, float NodePosY)
{
    if (!Graph)
    {
        return nullptr;
    }

    UK2Node_VariableGet* NewGet = NewObject<UK2Node_VariableGet>(Graph);
    if (!NewGet)
    {
        return nullptr;
    }

    Graph->Modify();
    NewGet->SetFlags(RF_Transactional);
    NewGet->VariableReference = VarRef;
    Graph->AddNode(NewGet, true, false);
    NewGet->CreateNewGuid();
    NewGet->NodePosX = NodePosX;
    NewGet->NodePosY = NodePosY;
    NewGet->AllocateDefaultPins();
    NewGet->Modify();

    return NewGet;
}

void LogConnectionFailure(const TCHAR* Context, UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, const FPinConnectionResponse& Response)
{
    if (!SourcePin || !TargetPin)
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s: connection skipped due to null pins (source=%p target=%p)"),
            Context, SourcePin, TargetPin);
        return;
    }

    FString SourceNodeName = SourcePin->GetOwningNode() ? SourcePin->GetOwningNode()->GetName() : TEXT("<null>");
    FString TargetNodeName = TargetPin->GetOwningNode() ? TargetPin->GetOwningNode()->GetName() : TEXT("<null>");

    UE_LOG(LogTemp, Verbose, TEXT("%s: schema rejected connection %s (%s) -> %s (%s) reason=%d"),
        Context, *SourceNodeName, *SourcePin->PinName.ToString(),
        *TargetNodeName, *TargetPin->PinName.ToString(),
        static_cast<int32>(Response.Response));
}

}

#endif
