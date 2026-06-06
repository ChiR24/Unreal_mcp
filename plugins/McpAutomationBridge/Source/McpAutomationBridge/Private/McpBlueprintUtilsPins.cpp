#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpSafeOperations.h"
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
#endif

#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2

namespace McpBlueprintUtils
{

UEdGraphPin* FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == Direction)
        {
            return Pin;
        }
    }

    return nullptr;
}

UEdGraphPin* FindOutputPin(UEdGraphNode* Node, const FName& PinName)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output)
        {
            if (PinName.IsNone())
            {
                return Pin;
            }
            if (Pin->PinName == PinName)
            {
                return Pin;
            }
        }
    }

    return nullptr;
}

UEdGraphPin* FindInputPin(UEdGraphNode* Node, const FName& PinName)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == PinName)
        {
            return Pin;
        }
    }

    return nullptr;
}

UEdGraphPin* FindDataPin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const FName& PreferredName)
{
    if (!Node)
    {
        return nullptr;
    }

    UEdGraphPin* Fallback = nullptr;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->Direction != Direction)
        {
            continue;
        }
        if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
        {
            continue;
        }
        if (!PreferredName.IsNone() && Pin->PinName == PreferredName)
        {
            return Pin;
        }
        if (!Fallback)
        {
            Fallback = Pin;
        }
    }

    return Fallback;
}

UEdGraphPin* FindPreferredEventExec(UEdGraph* Graph)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Prefer custom events, fall back to first available event node
    UEdGraphPin* Fallback = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node)
        {
            continue;
        }

        if (UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
        {
            UEdGraphPin* ExecPin = FindExecPin(Custom, EGPD_Output);
            if (ExecPin && ExecPin->LinkedTo.Num() == 0)
            {
                return ExecPin;
            }
            if (!Fallback && ExecPin)
            {
                Fallback = ExecPin;
            }
        }
        else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
        {
            UEdGraphPin* ExecPin = FindExecPin(EventNode, EGPD_Output);
            if (ExecPin && ExecPin->LinkedTo.Num() == 0 && !Fallback)
            {
                Fallback = ExecPin;
            }
        }
    }

    return Fallback;
}
}

#endif
