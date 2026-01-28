#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

// Helper macros for JSON field access
#define GetStringFieldNiaG GetJsonStringField
#define GetNumberFieldNiaG GetJsonNumberField
#define GetBoolFieldNiaG GetJsonBoolField

#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScriptSource.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleNiagaraGraphAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_niagara_graph"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *AssetPath);
    if (!System)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FString SubAction = GetStringFieldNiaG(Payload, TEXT("subAction"));
    FString EmitterName;
    Payload->TryGetStringField(TEXT("emitterName"), EmitterName);

    // Find the target graph (System or Emitter)
    UNiagaraGraph* TargetGraph = nullptr;
    UNiagaraScript* TargetScript = nullptr;

    if (EmitterName.IsEmpty())
    {
        // System script
        TargetScript = System->GetSystemSpawnScript(); 
        // Note: System has multiple scripts (Spawn, Update). 
        // For simplicity, we might need to specify which script.
        // Let's assume SystemSpawn for now or let user specify 'scriptType'
        FString ScriptType;
        if (Payload->TryGetStringField(TEXT("scriptType"), ScriptType))
        {
            if (ScriptType == TEXT("Update")) TargetScript = System->GetSystemUpdateScript();
        }
    }
    else
    {
        // Emitter script
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            if (Handle.GetName() == EmitterName)
            {
                UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter;
                if (Emitter)
                {
                    // Again, Emitter has Spawn, Update, etc.
                    TargetScript = Emitter->GetLatestEmitterData()->SpawnScriptProps.Script; // Default
                    FString ScriptType;
                    if (Payload->TryGetStringField(TEXT("scriptType"), ScriptType))
                    {
                        if (ScriptType == TEXT("Update")) TargetScript = Emitter->GetLatestEmitterData()->UpdateScriptProps.Script;
                        // Add ParticleSpawn, ParticleUpdate etc.
                    }
                }
                break;
            }
        }
    }

    if (TargetScript)
    {
        // Need to cast to UNiagaraScriptSource to get the graph
        if (auto* Source = Cast<UNiagaraScriptSource>(TargetScript->GetLatestSource()))
        {
            TargetGraph = Source->NodeGraph;
        }
    }

    if (!TargetGraph)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Could not resolve target Niagara Graph."), TEXT("GRAPH_NOT_FOUND"));
        return true;
    }

    if (SubAction == TEXT("add_module"))
    {
        FString ModulePath; // Path to the module asset
        Payload->TryGetStringField(TEXT("modulePath"), ModulePath);
        
        // Logic to add a function call node for the module
        // This is complex in Niagara as it involves finding the script, creating a node, and wiring it into the stack.
        // Simplified version: just create the node.
        
        UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
        if (!ModuleScript)
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load module script."), TEXT("ASSET_NOT_FOUND"));
             return true;
        }

        UNiagaraNodeFunctionCall* FuncNode = NewObject<UNiagaraNodeFunctionCall>(TargetGraph);
        FuncNode->FunctionScript = ModuleScript;
        TargetGraph->AddNode(FuncNode, true, false);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Module node added."));
        return true;
    }
    // Implement other subactions: connect, remove, etc.

    else if (SubAction == TEXT("connect_pins"))
    {
        FString FromNodeId, FromPinName;
        FString ToNodeId, ToPinName;
        if (!Payload->TryGetStringField(TEXT("fromNode"), FromNodeId) || !Payload->TryGetStringField(TEXT("fromPin"), FromPinName) ||
            !Payload->TryGetStringField(TEXT("toNode"), ToNodeId) || !Payload->TryGetStringField(TEXT("toPin"), ToPinName))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("connect_pins requires fromNode, fromPin, toNode, toPin"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UEdGraphNode* FromNode = nullptr;
        UEdGraphNode* ToNode = nullptr;

        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == FromNodeId || Node->GetName() == FromNodeId || Node->GetNodeTitle(ENodeTitleType::ListView).ToString() == FromNodeId)
            {
                FromNode = Node;
            }
            if (Node->NodeGuid.ToString() == ToNodeId || Node->GetName() == ToNodeId || Node->GetNodeTitle(ENodeTitleType::ListView).ToString() == ToNodeId)
            {
                ToNode = Node;
            }
        }

        if (!FromNode || !ToNode)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not find source or destination node."), TEXT("NODE_NOT_FOUND"));
            return true;
        }

        UEdGraphPin* FromPin = FromNode->FindPin(FName(*FromPinName));
        UEdGraphPin* ToPin = ToNode->FindPin(FName(*ToPinName));

        if (!FromPin)
        {
            // Try lenient search
            for (UEdGraphPin* Pin : FromNode->Pins)
            {
                if (Pin->PinName.ToString() == FromPinName || Pin->GetDisplayName().ToString() == FromPinName) { FromPin = Pin; break; }
            }
        }
        if (!ToPin)
        {
             for (UEdGraphPin* Pin : ToNode->Pins)
            {
                if (Pin->PinName.ToString() == ToPinName || Pin->GetDisplayName().ToString() == ToPinName) { ToPin = Pin; break; }
            }
        }

        if (!FromPin || !ToPin)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not find source or destination pin."), TEXT("PIN_NOT_FOUND"));
            return true;
        }

        const bool bConnected = TargetGraph->GetSchema()->TryCreateConnection(FromPin, ToPin);
        if (bConnected)
        {
             SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Pins connected successfully."));
        }
        else
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to connect pins (schema blocked connection)."), TEXT("CONNECTION_FAILED"));
        }
        return true;
    }
    else if (SubAction == TEXT("remove_node"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == NodeId)
            {
                TargetNode = Node;
                break;
            }
        }

        if (TargetNode)
        {
            TargetGraph->RemoveNode(TargetNode);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node removed."));
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }
    else if (SubAction == TEXT("set_parameter"))
    {
        FString ParamName;
        Payload->TryGetStringField(TEXT("parameterName"), ParamName);
        
        // Try to find parameter in exposed user parameters
        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
        FNiagaraVariable Var;
        
        // Extract value from payload (supports numeric or boolean)
        float Val = 0.0f;
        bool bVal = false;

        double NumericValue = 0.0;
        if (Payload->TryGetNumberField(TEXT("value"), NumericValue))
        {
            Val = static_cast<float>(NumericValue);
            bVal = (NumericValue != 0.0);
        }

        bool BoolValue = false;
        if (Payload->TryGetBoolField(TEXT("value"), BoolValue))
        {
            bVal = BoolValue;
            Val = BoolValue ? 1.0f : 0.0f;
        }
        
        // Try float
        if (UserStore.FindParameterVariable(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParamName))))
        {
            UserStore.SetParameterValue(Val, FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParamName)));
            // ...
        }
        
        // Try bool
        if (UserStore.FindParameterVariable(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParamName))))
        {
            UserStore.SetParameterValue(bVal, FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParamName)));
            // ...
        }

        SendAutomationError(RequestingSocket, RequestId, TEXT("Parameter not found or type not supported (Float/Bool only)."), TEXT("PARAM_FAILED"));
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

#undef GetStringFieldNiaG
#undef GetNumberFieldNiaG
#undef GetBoolFieldNiaG

