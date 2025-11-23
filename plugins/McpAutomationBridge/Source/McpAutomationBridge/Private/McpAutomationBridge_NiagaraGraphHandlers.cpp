#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

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

    FString SubAction = Payload->GetStringField(TEXT("subAction"));
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
        // Niagara graphs are data-flow based but heavily rely on the "Stack" (System/Emitter/Module) structure.
        // Connecting pins arbitrarily requires knowing the exact Pin Graph representation (NiagaraNodeInput/Output).
        // This is significantly more complex than Blueprint graphs.
        SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara pin connection requires advanced stack context awareness not yet implemented."), TEXT("NOT_IMPLEMENTED"));
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
        // Setting a parameter in Niagara usually means setting a user parameter or a module input.
        // This requires traversing the UNiagaraScript or UNiagaraSystem exposed parameters.
        FString ParamName;
        Payload->TryGetStringField(TEXT("parameterName"), ParamName);
        FString ValueStr;
        Payload->TryGetStringField(TEXT("value"), ValueStr);
        
        // Basic implementation: Try to find a user parameter
        // NOTE: This requires accessing exposed parameters which might be on the System.
        if (System)
        {
            // UNiagaraSystem::SetParameterValue is not a direct API. 
            // We need to use FNiagaraUserRedirectionParameterStore or similar.
            // Due to API volatility between 5.0-5.4, we will defer this.
        }

        SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara parameter setting requires version-specific API (UserParameters vs VariableStore)."), TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
