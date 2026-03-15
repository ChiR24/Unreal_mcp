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

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

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
    // Validate action
    if (Action != TEXT("manage_niagara_graph"))
    {
        return false;
    }

#if WITH_EDITOR
    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract required asset path
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Load Niagara System
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *AssetPath);
    if (!System)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    // Extract subaction and optional emitter name
    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    FString EmitterName;
    Payload->TryGetStringField(TEXT("emitterName"), EmitterName);

    // -------------------------------------------------------------------------
    // Resolve target graph (System or Emitter)
    // -------------------------------------------------------------------------
    UNiagaraGraph* TargetGraph = nullptr;
    UNiagaraScript* TargetScript = nullptr;

    if (EmitterName.IsEmpty())
    {
        // System script (default to Spawn, can override via scriptType)
        TargetScript = System->GetSystemSpawnScript();
        
        FString ScriptType;
        if (Payload->TryGetStringField(TEXT("scriptType"), ScriptType))
        {
            if (ScriptType == TEXT("Update"))
            {
                TargetScript = System->GetSystemUpdateScript();
            }
        }
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
                    // Guard against null emitter data
                    const auto* EmitterData = Emitter->GetLatestEmitterData();
                    if (!EmitterData)
                    {
                        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Emitter data not available."), TEXT("EMITTER_DATA_MISSING"));
                        return true;
                    }

                    // Default to Spawn script
                    TargetScript = EmitterData->SpawnScriptProps.Script;
                    
                    FString ScriptType;
                    if (Payload->TryGetStringField(TEXT("scriptType"), ScriptType))
                    {
                        if (ScriptType == TEXT("Update"))
                        {
                            TargetScript = EmitterData->UpdateScriptProps.Script;
                        }
                    }
                }
#else
                // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
                UNiagaraEmitter* Emitter = Handle.GetInstance();
                if (Emitter)
                {
                    TargetScript = Emitter->SpawnScriptProps.Script;
                    
                    FString ScriptType;
                    if (Payload->TryGetStringField(TEXT("scriptType"), ScriptType))
                    {
                        if (ScriptType == TEXT("Update"))
                        {
                            TargetScript = Emitter->UpdateScriptProps.Script;
                        }
                    }
                }
#endif
                break;
            }
        }
    }

    // Get graph from script source
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

        UNiagaraNodeFunctionCall* FuncNode = NewObject<UNiagaraNodeFunctionCall>(TargetGraph);
        FuncNode->FunctionScript = ModuleScript;
        TargetGraph->AddNode(FuncNode, true, false);

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
        FString FromNodeId, FromPinName;
        FString ToNodeId, ToPinName;

        if (!Payload->TryGetStringField(TEXT("fromNode"), FromNodeId) ||
            !Payload->TryGetStringField(TEXT("fromPin"), FromPinName) ||
            !Payload->TryGetStringField(TEXT("toNode"), ToNodeId) ||
            !Payload->TryGetStringField(TEXT("toPin"), ToPinName))
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("connect_pins requires fromNode, fromPin, toNode, toPin"), 
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Find nodes by ID, name, or title
        UEdGraphNode* FromNode = nullptr;
        UEdGraphNode* ToNode = nullptr;

        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            const FString NodeId = Node->NodeGuid.ToString();
            const FString NodeName = Node->GetName();
            const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();

            if (NodeId == FromNodeId || NodeName == FromNodeId || NodeTitle == FromNodeId)
            {
                FromNode = Node;
            }
            if (NodeId == ToNodeId || NodeName == ToNodeId || NodeTitle == ToNodeId)
            {
                ToNode = Node;
            }
        }

        if (!FromNode || !ToNode)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Could not find source or destination node."), TEXT("NODE_NOT_FOUND"));
            return true;
        }

        // Find pins (try exact match first, then lenient search)
        UEdGraphPin* FromPin = FromNode->FindPin(FName(*FromPinName));
        UEdGraphPin* ToPin = ToNode->FindPin(FName(*ToPinName));

        // Lenient pin search
        if (!FromPin)
        {
            for (UEdGraphPin* Pin : FromNode->Pins)
            {
                if (Pin->PinName.ToString() == FromPinName || 
                    Pin->GetDisplayName().ToString() == FromPinName)
                {
                    FromPin = Pin;
                    break;
                }
            }
        }

        if (!ToPin)
        {
            for (UEdGraphPin* Pin : ToNode->Pins)
            {
                if (Pin->PinName.ToString() == ToPinName || 
                    Pin->GetDisplayName().ToString() == ToPinName)
                {
                    ToPin = Pin;
                    break;
                }
            }
        }

        if (!FromPin || !ToPin)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Could not find source or destination pin."), TEXT("PIN_NOT_FOUND"));
            return true;
        }

        // Attempt connection
        const bool bConnected = TargetGraph->GetSchema()->TryCreateConnection(FromPin, ToPin);
        if (bConnected)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, System);
            Result->SetStringField(TEXT("fromNode"), FromNodeId);
            Result->SetStringField(TEXT("fromPin"), FromPinName);
            Result->SetStringField(TEXT("toNode"), ToNodeId);
            Result->SetStringField(TEXT("toPin"), ToPinName);
            Result->SetBoolField(TEXT("connected"), true);

            SendAutomationResponse(RequestingSocket, RequestId, true, 
                TEXT("Pins connected successfully."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Failed to connect pins (schema blocked connection)."), 
                TEXT("CONNECTION_FAILED"));
        }
        return true;
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
            TargetGraph->RemoveNode(TargetNode);

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

        // Get exposed parameters store
        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();

        // Extract value (numeric or boolean)
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

        // Try float parameter
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

        // Try bool parameter
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

    // Unknown subaction
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
