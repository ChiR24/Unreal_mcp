// =============================================================================
// McpAutomationBridge_NiagaraGraphHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Niagara Graph Manipulation Handlers
//
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
//
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_niagara_graph (Editor Only)
//   - add_module: Add Niagara module (function call) node to graph
//   - connect_pins: Connect two pins in Niagara graph
//   - remove_node: Remove node from Niagara graph
//   - set_parameter: Set exposed parameter value (Float/Bool only)
//
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: NiagaraSystem, NiagaraEmitter, NiagaraScript, NiagaraGraph
//   - Editor: Niagara nodes, EdGraph
//
// Version Compatibility Notes:
//   - UE 5.1+: GetInstance() returns FNiagaraEmitterHandleRef with .Emitter
//   - UE 5.0: GetInstance() returns UNiagaraEmitter* directly
//   - GetLatestEmitterData() can be null - must guard before dereferencing
//
// Architecture:
//   - System has multiple scripts (Spawn, Update, etc.)
//   - Emitter has multiple scripts per lifecycle stage
//   - Target graph resolved via scriptType parameter
// =============================================================================

#include "Core/Compatibility/McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/NiagaraGraph/McpAutomationBridge_NiagaraGraphHandlersPrivate.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"

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
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#endif

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleNiagaraGraphAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_niagara_graph"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *AssetPath);
    if (!System)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
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
    const bool bUpdateScript = GetJsonStringField(Payload, TEXT("scriptType")) == TEXT("Update");
    if (EmitterName.IsEmpty())
    {
        TargetScript = bUpdateScript ? System->GetSystemUpdateScript() : System->GetSystemSpawnScript();
    }
    else
    {
        // Emitter script - find by name
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            if (Handle.GetName() == FName(*EmitterName))
            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter;
                if (Emitter)
                {
                    const auto* EmitterData = Emitter->GetLatestEmitterData();
                    if (!EmitterData)
                    {
                        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Emitter data not available."), TEXT("EMITTER_DATA_MISSING"));
                        return true;
                    }

                    TargetScript = bUpdateScript ? EmitterData->UpdateScriptProps.Script : EmitterData->SpawnScriptProps.Script;
                }
#else
                // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
                UNiagaraEmitter* Emitter = Handle.GetInstance();
                if (Emitter)
                {
                    TargetScript = bUpdateScript ? Emitter->UpdateScriptProps.Script : Emitter->SpawnScriptProps.Script;
                }
#endif
                break;
            }
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
    // add_module: Add Niagara module (function call) node
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("add_module"))
    {
        FString ModulePath;
        Payload->TryGetStringField(TEXT("modulePath"), ModulePath);

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
            UNiagaraNodeOutput* Candidate = Cast<UNiagaraNodeOutput>(Node);
            if (Candidate && Candidate->GetUsage() == TargetScript->GetUsage())
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

        UNiagaraNodeFunctionCall* FuncNode =
            FNiagaraStackGraphUtilities::AddScriptModuleToStack(
                ModuleScript, *OutputNode);
        if (!FuncNode || !FuncNode->NodeGuid.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to add module to the Niagara stack."),
                TEXT("CREATE_FAILED"));
            return true;
        }
        System->MarkPackageDirty();

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Result, System);
        Result->SetStringField(TEXT("modulePath"), ModulePath);
        Result->SetStringField(TEXT("nodeId"), FuncNode->NodeGuid.ToString());

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Module node added."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // connect_pins: Connect two pins in Niagara graph
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("connect_pins"))
    {
        return McpNiagaraGraphHandlers::HandleConnectPins(
            this, RequestId, Payload, RequestingSocket, System, TargetGraph);
    }

    // -------------------------------------------------------------------------
    // remove_node: Remove node from Niagara graph
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("remove_node"))
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
            FString RemovalError;
            if (!McpNiagaraGraphHandlers::RemoveNiagaraGraphNodeSafely(
                    TargetGraph, TargetNode, RemovalError))
            {
                SendAutomationError(RequestingSocket, RequestId,
                    RemovalError,
                    TEXT("REMOVE_FAILED"));
                return true;
            }
            System->MarkPackageDirty();

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, System);
            Result->SetStringField(TEXT("nodeId"), NodeId);
            Result->SetBoolField(TEXT("removed"), true);

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

    // -------------------------------------------------------------------------
    // set_parameter: Set exposed parameter value (Float/Bool only)
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("set_parameter"))
    {
        FString ParamName;
        Payload->TryGetStringField(TEXT("parameterName"), ParamName);

        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();

        float FloatValue = 0.0f;
        bool BoolValue = false;

        double NumericValue = 0.0;
        if (Payload->TryGetNumberField(TEXT("value"), NumericValue))
        {
            FloatValue = static_cast<float>(NumericValue);
            BoolValue = (NumericValue != 0.0);
        }

        bool bBoolField = false;
        if (Payload->TryGetBoolField(TEXT("value"), bBoolField))
        {
            BoolValue = bBoolField;
            FloatValue = bBoolField ? 1.0f : 0.0f;
        }

        if (UserStore.FindParameterVariable(
            FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParamName))))
        {
            UserStore.SetParameterValue(FloatValue,
                FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParamName)));

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, System);
            Result->SetStringField(TEXT("parameterName"), ParamName);
            Result->SetNumberField(TEXT("value"), FloatValue);

            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Float parameter set."), Result);
            return true;
        }

        if (UserStore.FindParameterVariable(
            FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParamName))))
        {
            UserStore.SetParameterValue(BoolValue,
                FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParamName)));

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, System);
            Result->SetStringField(TEXT("parameterName"), ParamName);
            Result->SetBoolField(TEXT("value"), BoolValue);

            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Bool parameter set."), Result);
            return true;
        }

        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Parameter not found or type not supported (Float/Bool only)."),
            TEXT("PARAM_FAILED"));
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;

#else
    // Non-editor build
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
