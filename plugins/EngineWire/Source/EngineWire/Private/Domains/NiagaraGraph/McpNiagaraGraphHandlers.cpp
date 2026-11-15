#include "Core/Compatibility/McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/NiagaraGraph/McpNiagaraGraphPrivate.h"
#include "Domains/NiagaraGraph/McpNiagaraModuleOrder.h"
#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

#include "Dom/JsonObject.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "EdGraph/EdGraph.h"
#include "ScopedTransaction.h"

bool UEngineWireSubsystem::HandleNiagaraGraphAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // The authoring folder's own context: systemPath (else system, else assetPath) and every path through the shared
    // canonicalizer, systemPath and assetPath naming two systems refused, the system loaded and made transactional.
    McpNiagaraAuthoringHandlers::FActionContext Context =
        McpNiagaraAuthoringHandlers::MakeActionContext(this, RequestId, Payload, RequestingSocket);
    if (!McpNiagaraAuthoringHandlers::ValidateCommonFields(Context))
    {
        return true;
    }
    UNiagaraSystem* System = McpNiagaraAuthoringHandlers::LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }

    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    FString EmitterName; Payload->TryGetStringField(TEXT("emitterName"), EmitterName);

    // -------------------------------------------------------------------------
    // Resolve target graph (System or Emitter)
    // -------------------------------------------------------------------------
    UNiagaraGraph* TargetGraph = nullptr;
    UNiagaraScript* TargetScript = nullptr;

    // Spawn script by default; scriptType "Update" selects the update script.
    const FString ScriptType = GetJsonStringField(Payload, TEXT("scriptType"));
    const bool bUpdateScript = ScriptType.Equals(TEXT("Update"), ESearchCase::IgnoreCase);
    if (!bUpdateScript && !ScriptType.IsEmpty() && !ScriptType.Equals(TEXT("Spawn"), ESearchCase::IgnoreCase))
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("scriptType '%s' is neither Spawn nor Update. Nothing was changed."),
            *ScriptType), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (EmitterName.IsEmpty())
    {
        TargetScript = bUpdateScript ? System->GetSystemUpdateScript() : System->GetSystemSpawnScript();
    }
    else
    {
        FNiagaraEmitterHandle* Handle = McpNiagaraAuthoringHandlers::FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found in Niagara system '%s' "
                "(its emitters: [%s]); pass one of them as 'emitterName'. Nothing was changed."), *EmitterName, *System->GetPathName(),
                *McpNiagaraAuthoringHandlers::ListEmitterNames(System)), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }
        // The scripts of the version the handle uses.
        if (MCP_NIAGARA_EMITTER_DATA_TYPE* Data = MCP_GET_EMITTER_DATA(*Handle))
        {
            TargetScript = bUpdateScript ? Data->UpdateScriptProps.Script : Data->SpawnScriptProps.Script;
        }
    }

    if (TargetScript)
    {
        if (auto* Source = Cast<UNiagaraScriptSource>(TargetScript->GetLatestSource()))
        {
            TargetGraph = Source->NodeGraph;
        }
    }

    if (!TargetGraph)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Could not resolve target Niagara Graph."), TEXT("GRAPH_NOT_FOUND"));
        return true;
    }

    // -------------------------------------------------------------------------
    // add_niagara_module: Add Niagara module (function call) node
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("add_niagara_module"))
    {
        FString ModulePath;
        Payload->TryGetStringField(TEXT("modulePath"), ModulePath);
        if (!McpNiagaraAuthoringHandlers::ValidateAndSanitizePath(Context, ModulePath, TEXT("modulePath")))
        {
            return true;
        }

        UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
        if (!ModuleScript)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Could not load module script."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        UNiagaraNodeOutput* OutputNode = nullptr;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            // An emitter with interpolated spawning runs its particle spawn script as
            // ParticleSpawnScriptInterpolated while the stack's output node says
            // ParticleSpawnScript, so an exact match found no stack to add to.
            UNiagaraNodeOutput* Candidate = Cast<UNiagaraNodeOutput>(Node);
            if (Candidate && UNiagaraScript::IsEquivalentUsage(Candidate->GetUsage(), TargetScript->GetUsage()))
            {
                OutputNode = Candidate;
                break;
            }
        }
        if (!OutputNode)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Could not resolve the target Niagara stack output."),
                TEXT("GRAPH_NOT_FOUND"));
            return true;
        }

        const int32 StackIndex = McpNiagaraModuleOrder::DependencyInsertIndex(*ModuleScript, *OutputNode);
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
        UNiagaraNodeFunctionCall* FuncNode =
            FNiagaraStackGraphUtilities::AddScriptModuleToStack(
                ModuleScript, *OutputNode, StackIndex);
        const TCHAR* FailMessage = TEXT("Failed to add module to the Niagara stack.");
        const TCHAR* FailCode = TEXT("CREATE_FAILED");
#else
        // UE 5.0 declares AddScriptModuleToStack without exporting it, so a plugin cannot call it there.
        UNiagaraNodeFunctionCall* FuncNode = nullptr;
        const TCHAR* FailMessage = TEXT("Adding a module to a Niagara stack needs UE 5.1 or later; this editor is UE 5.0.");
        const TCHAR* FailCode = TEXT("NOT_SUPPORTED");
#endif
        if (!FuncNode || !FuncNode->NodeGuid.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, FailMessage, FailCode);
            return true;
        }
        const TSharedPtr<FJsonObject>& Result = Context.Result;
        McpNiagaraAuthoringHandlers::MarkDirtyAndVerify(Context, System);
        Result->SetStringField(TEXT("modulePath"), ModulePath);
        Result->SetStringField(TEXT("nodeId"), FuncNode->NodeGuid.ToString());
        if (StackIndex != INDEX_NONE)
        {
            Result->SetNumberField(TEXT("stackIndex"), StackIndex);
        }
        // What the editor's stack shows for the system now (stackErrors, and its warnings), as the typed module adds
        // report it: the context's send adds it for an add_*_module sub-action.
        Context.SubAction = SubAction;
        Context.SendSuccess(true, TEXT("Module node added."));
        return true;
    }

    // -------------------------------------------------------------------------
    // connect_niagara_pins: Connect two pins in Niagara graph
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("connect_niagara_pins"))
    {
        return McpNiagaraGraphHandlers::HandleConnectPins(
            this, RequestId, Payload, RequestingSocket, System, TargetGraph);
    }

    // -------------------------------------------------------------------------
    // remove_niagara_node: Remove node from Niagara graph
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("remove_niagara_node"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        UEdGraphNode* TargetNode = McpNiagaraGraphHandlers::FindNodeByIdNameOrTitle(TargetGraph, NodeId);
        if (TargetNode)
        {
            const FString RemovedGuid = TargetNode->NodeGuid.ToString();
            FString RemovalError;
            int32 RemovedCount = 0;
            bool bRemoved = false;
            {
                // One undo step for the whole removal, opened before the first Modify and the first cut and closed before the
                // save: the schema's own step took back only the re-link, and undo listed the module again with its links cut.
                FScopedTransaction Transaction(FText::FromString(TEXT("Remove Niagara Node")));
                // The module's inputs are keys of the scripts' rapid-iteration stores: the scripts are recorded in the step.
                for (UNiagaraScript* Script : McpNiagaraAuthoringHandlers::GatherModuleInputScripts(System))
                {
                    if (Script)
                    {
                        Script->Modify(false);
                    }
                }
                bRemoved = McpNiagaraGraphHandlers::RemoveNiagaraGraphNodeSafely(TargetGraph, TargetNode, RemovalError, RemovedCount);
                if (!bRemoved)
                {
                    Transaction.Cancel();
                }
            }
            if (!bRemoved)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    RemovalError,
                    TEXT("REMOVE_FAILED"));
                return true;
            }
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpNiagaraGraphHandlers::SaveNiagaraGraphEdit(System, Payload, Result);
            McpHandlerUtils::AddVerification(Result, System);
            Result->SetStringField(TEXT("nodeId"), RemovedGuid);
            Result->SetBoolField(TEXT("removed"), true);
            Result->SetNumberField(TEXT("removedCount"), RemovedCount);

            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Node removed."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;

}
