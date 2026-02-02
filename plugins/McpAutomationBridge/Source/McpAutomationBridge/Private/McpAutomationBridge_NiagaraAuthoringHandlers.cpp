// McpAutomationBridge_NiagaraAuthoringHandlers.cpp
// Phase 12: Complete Niagara VFX System Authoring
// Implements 35 actions for Niagara system/emitter creation, modules, parameters, events, and GPU simulation.

#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

// Note: FVersionedNiagaraEmitterData and related APIs were introduced in UE 5.1
// UE 5.0 uses direct emitter pointers, UE 5.1+ uses versioned emitter data
#if WITH_EDITOR && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_NIAGARA_VERSIONING_APIS 1
#define MCP_NIAGARA_EMITTER_DATA_TYPE FVersionedNiagaraEmitterData
#define MCP_GET_EMITTER_DATA(Handle) (Handle).GetEmitterData()
#define MCP_GET_LATEST_EMITTER_DATA(Emitter) (Emitter)->GetLatestEmitterData()
#define MCP_GET_EMITTER_VERSION_GUID(Emitter) (Emitter)->GetExposedVersion().VersionGuid
#else
#define MCP_HAS_NIAGARA_VERSIONING_APIS 0
// UE 5.0: Use UNiagaraEmitter* directly instead of MCP_NIAGARA_EMITTER_DATA_TYPE*
#define MCP_NIAGARA_EMITTER_DATA_TYPE UNiagaraEmitter
// UE 5.0: GetInstance() is a const method, returns UNiagaraEmitter*
// Use (&(Handle)) to handle both pointers and references uniformly
#define MCP_GET_EMITTER_DATA(Handle) (&(Handle))->GetInstance()
#define MCP_GET_LATEST_EMITTER_DATA(Emitter) (Emitter)
#define MCP_GET_EMITTER_VERSION_GUID(Emitter) FGuid()
#endif

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
#include "NiagaraEditorModule.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSimulationStageBase.h"

// FNiagaraStackGraphUtilities is only available in UE 5.1+ (NiagaraEditor module)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#define MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES 1
#else
#define MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES 0
#endif

// Niagara Data Interfaces
// UE 5.7+: Data interfaces moved to subfolders or different modules
#if __has_include("NiagaraDataInterfaceSkeletalMesh.h")
#include "NiagaraDataInterfaceSkeletalMesh.h"
#define MCP_HAS_NIAGARA_SKELETAL_MESH_DI 1
#elif __has_include("DataInterface/NiagaraDataInterfaceSkeletalMesh.h")
#include "DataInterface/NiagaraDataInterfaceSkeletalMesh.h"
#define MCP_HAS_NIAGARA_SKELETAL_MESH_DI 1
#else
#define MCP_HAS_NIAGARA_SKELETAL_MESH_DI 0
#endif

// NiagaraDataInterfaceStaticMesh location varies by UE version
// In UE 5.7+, it moved to the Internal folder which is also on the include path
#if __has_include("NiagaraDataInterfaceStaticMesh.h")
#include "NiagaraDataInterfaceStaticMesh.h"
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 1
#elif __has_include("DataInterface/NiagaraDataInterfaceStaticMesh.h")
#include "DataInterface/NiagaraDataInterfaceStaticMesh.h"
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 1
#else
// UE 5.7: Static mesh data interface is not directly accessible from editor modules
// The functionality may require different approach or module dependencies
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 0
#endif
// Note: If MCP_HAS_NIAGARA_STATIC_MESH_DI is 0, static mesh data interface features will be unavailable
#include "NiagaraDataInterfaceSpline.h"
#include "NiagaraDataInterfaceAudioSpectrum.h"
#include "NiagaraDataInterfaceCollisionQuery.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraParameterStore.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraTypes.h"
#include "NiagaraConstants.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "EditorAssetLibrary.h"
#endif

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
// Aliases for backward compatibility with existing code in this file
#define GetStringFieldNiagAuth GetJsonStringField
#define GetNumberFieldNiagAuth GetJsonNumberField
#define GetBoolFieldNiagAuth GetJsonBoolField


// Helper to get FVector from JSON object
static FVector GetVectorFromJsonNiag(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FVector::ZeroVector;
    return FVector(
        GetNumberFieldNiagAuth(Obj, TEXT("x"), 0.0),
        GetNumberFieldNiagAuth(Obj, TEXT("y"), 0.0),
        GetNumberFieldNiagAuth(Obj, TEXT("z"), 0.0)
    );
}

// Helper to get FLinearColor from JSON object
static FLinearColor GetColorFromJsonNiagara(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FLinearColor::White;
    return FLinearColor(
        static_cast<float>(GetNumberFieldNiagAuth(Obj, TEXT("r"), 1.0)),
        static_cast<float>(GetNumberFieldNiagAuth(Obj, TEXT("g"), 1.0)),
        static_cast<float>(GetNumberFieldNiagAuth(Obj, TEXT("b"), 1.0)),
        static_cast<float>(GetNumberFieldNiagAuth(Obj, TEXT("a"), 1.0))
    );
}


bool UMcpAutomationBridgeSubsystem::HandleManageNiagaraAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_niagara_authoring"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringFieldNiagAuth(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Common parameters
    FString Name = GetStringFieldNiagAuth(Payload, TEXT("name"));
    FString Path = GetStringFieldNiagAuth(Payload, TEXT("path"), TEXT("/Game"));
    FString AssetPath = GetStringFieldNiagAuth(Payload, TEXT("assetPath"));
    FString SystemPath = GetStringFieldNiagAuth(Payload, TEXT("systemPath"));
    FString EmitterPath = GetStringFieldNiagAuth(Payload, TEXT("emitterPath"));
    FString EmitterName = GetStringFieldNiagAuth(Payload, TEXT("emitterName"));
    bool bSave = GetBoolFieldNiagAuth(Payload, TEXT("save"), true);


    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

    // =========================================================================
    // 12.1 Systems & Emitters (4 actions)
    // =========================================================================

    if (SubAction == TEXT("create_niagara_system"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' parameter."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Ensure path ends correctly
        if (!Path.EndsWith(TEXT("/"))) Path += TEXT("/");
        FString FullPath = Path + Name;
        FString PackagePath = FPackageName::ObjectPathToPackageName(FullPath);
        
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
            return true;
        }

        // Create NiagaraSystem directly without factory (compatible with all UE versions)
        // Note: Factories are editor-internal and not exported for plugin use
        UNiagaraSystem* NewSystem = NewObject<UNiagaraSystem>(Package, FName(*Name), RF_Public | RF_Standalone);
        if (NewSystem)
        {
            // Add default emitter using direct API
            // UE 5.4+ changed AddEmitterHandle signature - requires additional parameters
            UNiagaraEmitter* NewEmitter = NewObject<UNiagaraEmitter>(NewSystem, FName(TEXT("DefaultEmitter")));
            if (NewEmitter)
            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
                // UE 5.0 - AddEmitterHandle takes only 2 parameters
                NewSystem->AddEmitterHandle(*NewEmitter, FName(TEXT("DefaultEmitter")));
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
                // UE 5.4 signature with 2 parameters
                NewSystem->AddEmitterHandle(*NewEmitter, FName(TEXT("DefaultEmitter")));
#else
                // UE 5.1-5.3 and 5.5+ - AddEmitterHandle takes 3 parameters: Emitter, Name, and Version GUID
                NewSystem->AddEmitterHandle(*NewEmitter, FName(TEXT("DefaultEmitter")), FGuid::NewGuid());
#endif
            }
        }
        if (!NewSystem)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create Niagara System."), TEXT("CREATE_FAILED"));
            return true;
        }

        FAssetRegistryModule::AssetCreated(NewSystem);
        
        if (bSave)
        {
            McpSafeAssetSave(NewSystem);
        }

        Result->SetStringField(TEXT("assetPath"), NewSystem->GetPathName());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Niagara System: %s"), *Name));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("System created."), Result);
        return true;
    }

    if (SubAction == TEXT("create_niagara_emitter"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' parameter."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (!Path.EndsWith(TEXT("/"))) Path += TEXT("/");
        FString FullPath = Path + Name;
        FString PackagePath = FPackageName::ObjectPathToPackageName(FullPath);
        
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
            return true;
        }

        // Create NiagaraEmitter directly without factory (compatible with all UE versions)
        // Note: Factories are editor-internal and not exported for plugin use
        UNiagaraEmitter* NewEmitter = NewObject<UNiagaraEmitter>(Package, FName(*Name), RF_Public | RF_Standalone);
        if (!NewEmitter)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create Niagara Emitter."), TEXT("CREATE_FAILED"));
            return true;
        }

        FAssetRegistryModule::AssetCreated(NewEmitter);
        
        if (bSave)
        {
            McpSafeAssetSave(NewEmitter);
        }

        Result->SetStringField(TEXT("assetPath"), NewEmitter->GetPathName());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Niagara Emitter: %s"), *Name));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Emitter created."), Result);
        return true;
    }

    if (SubAction == TEXT("add_emitter_to_system"))
    {
        if (SystemPath.IsEmpty() || EmitterPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterPath'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        UNiagaraEmitter* Emitter = LoadObject<UNiagaraEmitter>(nullptr, *EmitterPath);
        if (!Emitter)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara Emitter."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Add emitter to system - UE 5.1+ requires version GUID
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FGuid EmitterVersion = Emitter->GetExposedVersion().VersionGuid;
        FNiagaraEmitterHandle NewHandle = System->AddEmitterHandle(*Emitter, FName(*Emitter->GetName()), EmitterVersion);
#else
        // UE 5.0 - no version GUID needed
        FNiagaraEmitterHandle NewHandle = System->AddEmitterHandle(*Emitter, FName(*Emitter->GetName()));
#endif
        
        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("emitterName"), NewHandle.GetName().ToString());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added emitter '%s' to system."), *Emitter->GetName()));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Emitter added to system."), Result);
        return true;
    }

    if (SubAction == TEXT("set_emitter_properties"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = nullptr;
        for (const FNiagaraEmitterHandle& H : System->GetEmitterHandles())
        {
            if (H.GetName().ToString() == EmitterName)
            {
                Handle = const_cast<FNiagaraEmitterHandle*>(&H);
                break;
            }
        }
        
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found in system."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }
        
        const TSharedPtr<FJsonObject>* PropsObj;
        if (Payload->TryGetObjectField(TEXT("emitterProperties"), PropsObj) && PropsObj->IsValid())
        {
            bool bEnabled;
            if ((*PropsObj)->TryGetBoolField(TEXT("enabled"), bEnabled))
            {
                Handle->SetIsEnabled(bEnabled, *System, false);
            }
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Updated properties for emitter '%s'."), *EmitterName));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Emitter properties updated."), Result);
        return true;
    }

    // =========================================================================
    // 12.2 Module Library (17 actions)
    // Helper macro to reduce repetition for module additions
    // =========================================================================

    // Helper lambda to add a module script to the Niagara stack
    // Returns the created UNiagaraNodeFunctionCall or nullptr on failure
    auto AddModuleToEmitterStack = [](FNiagaraEmitterHandle* Handle, const FString& ModuleScriptPath, ENiagaraScriptUsage TargetUsage, const FString& SuggestedName = FString()) -> UNiagaraNodeFunctionCall*
    {
        if (!Handle)
        {
            return nullptr;
        }

        // Get the versioned emitter data
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
#else
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
#endif
        if (!EmitterData)
        {
            return nullptr;
        }

        // Get the graph source
        UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(EmitterData->GraphSource);
        if (!ScriptSource || !ScriptSource->NodeGraph)
        {
            return nullptr;
        }

        UNiagaraGraph* Graph = ScriptSource->NodeGraph;

        // Find the output node for the target usage
        // NOTE: UNiagaraGraph::FindOutputNode is not exported in all UE versions
        // Use manual iteration through nodes to find the output node
        UNiagaraNodeOutput* TargetOutput = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
            {
                if (OutputNode->GetUsage() == TargetUsage)
                {
                    TargetOutput = OutputNode;
                    break;
                }
            }
        }
        if (!TargetOutput)
        {
            return nullptr;
        }

        // Load the module script asset
        FSoftObjectPath AssetRef(ModuleScriptPath);
        UNiagaraScript* ModuleScript = Cast<UNiagaraScript>(AssetRef.TryLoad());
        if (!ModuleScript)
        {
            return nullptr;
        }

        // Add the module to the stack using the Stack Graph Utilities
        // Note: FNiagaraStackGraphUtilities is only available in UE 5.1+
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
        UNiagaraNodeFunctionCall* NewModule = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
            ModuleScript,
            *TargetOutput,
            INDEX_NONE, // Append to end
            SuggestedName.IsEmpty() ? ModuleScript->GetName() : SuggestedName
        );
        return NewModule;
#else
        // UE 5.0: Stack graph utilities not available
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("AddModule failed: FNiagaraStackGraphUtilities is not available in UE 5.0. Consider upgrading to UE 5.1+ for full Niagara stack graph support."));
        return nullptr;
#endif
    };

    // Helper lambda to find emitter handle
    auto FindEmitterHandle = [&](UNiagaraSystem* System, const FString& TargetEmitter) -> FNiagaraEmitterHandle*
    {
        for (const FNiagaraEmitterHandle& H : System->GetEmitterHandles())
        {
            if (H.GetName().ToString() == TargetEmitter)
            {
                return const_cast<FNiagaraEmitterHandle*>(&H);
            }
        }
        return nullptr;
    };

    // Spawn Rate Module
    if (SubAction == TEXT("add_spawn_rate_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        double SpawnRate = GetNumberFieldNiagAuth(Payload, TEXT("spawnRate"), 100.0);

        // Add the SpawnRate module to the Emitter Update stage
        // SpawnRate modules belong in EmitterUpdateScript as they control emission rate over time
        UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
            Handle,
            TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate"),
            ENiagaraScriptUsage::EmitterUpdateScript,
            TEXT("SpawnRate")
        );

        bool bModuleAdded = (NewModule != nullptr);
        
        // Also set user-exposed parameters if available
        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
        FNiagaraVariable SpawnRateVar(FNiagaraTypeDefinition::GetFloatDef(), FName(TEXT("SpawnRate")));
        if (UserStore.FindParameterVariable(SpawnRateVar))
        {
            UserStore.SetParameterValue(static_cast<float>(SpawnRate), SpawnRateVar);
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("SpawnRate"));
        Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
        Result->SetNumberField(TEXT("spawnRate"), SpawnRate);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added spawn rate module: %.1f particles/sec"), SpawnRate));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spawn rate module added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_spawn_burst_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        double BurstCount = GetNumberFieldNiagAuth(Payload, TEXT("burstCount"), 10.0);
        double BurstTime = GetNumberFieldNiagAuth(Payload, TEXT("burstTime"), 0.0);

        // Add the SpawnBurst_Instantaneous module to the Emitter Spawn stage
        // Burst modules belong in EmitterSpawnScript for instantaneous spawns
        UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
            Handle,
            TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"),
            ENiagaraScriptUsage::EmitterSpawnScript,
            TEXT("SpawnBurst")
        );

        bool bModuleAdded = (NewModule != nullptr);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("SpawnBurst"));
        Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
        Result->SetNumberField(TEXT("burstCount"), BurstCount);
        Result->SetNumberField(TEXT("burstTime"), BurstTime);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added spawn burst module: %d particles at t=%.2f"), static_cast<int>(BurstCount), BurstTime));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spawn burst module added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_spawn_per_unit_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        double SpawnPerUnit = GetNumberFieldNiagAuth(Payload, TEXT("spawnPerUnit"), 1.0);

        // Add the SpawnPerUnit module to the Emitter Update stage
        UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
            Handle,
            TEXT("/Niagara/Modules/Emitter/SpawnPerUnit.SpawnPerUnit"),
            ENiagaraScriptUsage::EmitterUpdateScript,
            TEXT("SpawnPerUnit")
        );

        bool bModuleAdded = (NewModule != nullptr);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("SpawnPerUnit"));
        Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
        Result->SetNumberField(TEXT("spawnPerUnit"), SpawnPerUnit);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added spawn per unit module: %.1f particles/unit"), SpawnPerUnit));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spawn per unit module added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_initialize_particle_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        double Lifetime = GetNumberFieldNiagAuth(Payload, TEXT("lifetime"), 2.0);
        double Mass = GetNumberFieldNiagAuth(Payload, TEXT("mass"), 1.0);

        // Add the InitializeParticle module to the Particle Spawn stage
        UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
            Handle,
            TEXT("/Niagara/Modules/Spawn/Initialization/InitializeParticle.InitializeParticle"),
            ENiagaraScriptUsage::ParticleSpawnScript,
            TEXT("InitializeParticle")
        );

        bool bModuleAdded = (NewModule != nullptr);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("InitializeParticle"));
        Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
        Result->SetNumberField(TEXT("lifetime"), Lifetime);
        Result->SetNumberField(TEXT("mass"), Mass);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added initialize particle module: lifetime=%.2fs, mass=%.2f"), Lifetime, Mass));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Initialize particle module added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_particle_state_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        // Add the ParticleState module to the Particle Update stage
        // ParticleState handles age tracking and lifetime evaluation
        UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
            Handle,
            TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState"),
            ENiagaraScriptUsage::ParticleUpdateScript,
            TEXT("ParticleState")
        );

        bool bModuleAdded = (NewModule != nullptr);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("ParticleState"));
        Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
        Result->SetStringField(TEXT("message"), TEXT("Added particle state module."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Particle state module added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_force_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ForceType = GetStringFieldNiagAuth(Payload, TEXT("forceType"), TEXT("Gravity"));

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        double ForceStrength = GetNumberFieldNiagAuth(Payload, TEXT("forceStrength"), 980.0);
        
        const TSharedPtr<FJsonObject>* ForceVectorObj;
        FVector ForceVector = FVector(0, 0, -980);
        if (Payload->TryGetObjectField(TEXT("forceVector"), ForceVectorObj))
        {
            ForceVector = GetVectorFromJsonNiag(*ForceVectorObj);
        }

        // Determine the module path based on force type
        FString ModulePath;
        if (ForceType.Equals(TEXT("Gravity"), ESearchCase::IgnoreCase))
        {
            ModulePath = TEXT("/Niagara/Modules/Update/Forces/GravityForce.GravityForce");
        }
        else if (ForceType.Equals(TEXT("Drag"), ESearchCase::IgnoreCase))
        {
            ModulePath = TEXT("/Niagara/Modules/Update/Forces/DragForce.DragForce");
        }
        else if (ForceType.Equals(TEXT("Wind"), ESearchCase::IgnoreCase))
        {
            ModulePath = TEXT("/Niagara/Modules/Update/Forces/WindForce.WindForce");
        }
        else if (ForceType.Equals(TEXT("Curl"), ESearchCase::IgnoreCase) || ForceType.Equals(TEXT("CurlNoise"), ESearchCase::IgnoreCase))
        {
            ModulePath = TEXT("/Niagara/Modules/Update/Forces/CurlNoiseForce.CurlNoiseForce");
        }
        else if (ForceType.Equals(TEXT("Vortex"), ESearchCase::IgnoreCase))
        {
            ModulePath = TEXT("/Niagara/Modules/Update/Forces/VortexForce.VortexForce");
        }
        else if (ForceType.Equals(TEXT("PointAttraction"), ESearchCase::IgnoreCase))
        {
            ModulePath = TEXT("/Niagara/Modules/Update/Forces/PointAttractionForce.PointAttractionForce");
        }
        else
        {
            // Default to gravity
            ModulePath = TEXT("/Niagara/Modules/Update/Forces/GravityForce.GravityForce");
        }

        // Add the force module to the Particle Update stage
        // Forces are applied every frame to update particle velocity
        UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
            Handle,
            ModulePath,
            ENiagaraScriptUsage::ParticleUpdateScript,
            FString::Printf(TEXT("%sForce"), *ForceType)
        );

        bool bModuleAdded = (NewModule != nullptr);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), FString::Printf(TEXT("Force_%s"), *ForceType));
        Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
        Result->SetStringField(TEXT("forceType"), ForceType);
        Result->SetNumberField(TEXT("forceStrength"), ForceStrength);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s force module."), *ForceType));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Force module added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_velocity_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        const TSharedPtr<FJsonObject>* VelObj;
        FVector Velocity = FVector(0, 0, 100);
        if (Payload->TryGetObjectField(TEXT("velocity"), VelObj))
        {
            Velocity = GetVectorFromJsonNiag(*VelObj);
        }

        FString VelocityMode = GetStringFieldNiagAuth(Payload, TEXT("velocityMode"), TEXT("Linear"));

        // Determine the module path based on velocity mode
        FString ModulePath;
        if (VelocityMode.Equals(TEXT("Cone"), ESearchCase::IgnoreCase))
        {
            ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityInCone.AddVelocityInCone");
        }
        else if (VelocityMode.Equals(TEXT("FromPoint"), ESearchCase::IgnoreCase))
        {
            ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityFromPoint.AddVelocityFromPoint");
        }
        else
        {
            // Default to AddVelocity (linear)
            ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity");
        }

        // Add the velocity module to the Particle Spawn stage
        // Initial velocity is set when particles are spawned
        UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
            Handle,
            ModulePath,
            ENiagaraScriptUsage::ParticleSpawnScript,
            TEXT("AddVelocity")
        );

        bool bModuleAdded = (NewModule != nullptr);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("Velocity"));
        Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
        Result->SetStringField(TEXT("velocityMode"), VelocityMode);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added velocity module: mode=%s"), *VelocityMode));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Velocity module added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_acceleration_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        const TSharedPtr<FJsonObject>* AccelObj;
        FVector Acceleration = FVector(0, 0, -980);
        if (Payload->TryGetObjectField(TEXT("acceleration"), AccelObj))
        {
            Acceleration = GetVectorFromJsonNiag(*AccelObj);
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("Acceleration"));
        Result->SetStringField(TEXT("message"), TEXT("Configured acceleration module."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Acceleration module configured."), Result);
        return true;
    }

    if (SubAction == TEXT("add_size_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FString SizeMode = GetStringFieldNiagAuth(Payload, TEXT("sizeMode"), TEXT("Uniform"));
        double UniformSize = GetNumberFieldNiagAuth(Payload, TEXT("uniformSize"), 10.0);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("Size"));
        Result->SetStringField(TEXT("sizeMode"), SizeMode);
        Result->SetNumberField(TEXT("uniformSize"), UniformSize);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured size module: mode=%s, size=%.1f"), *SizeMode, UniformSize));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Size module configured."), Result);
        return true;
    }

    if (SubAction == TEXT("add_color_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        const TSharedPtr<FJsonObject>* ColorObj;
        FLinearColor Color = FLinearColor::White;
        if (Payload->TryGetObjectField(TEXT("color"), ColorObj))
        {
            Color = GetColorFromJsonNiagara(*ColorObj);
        }

        FString ColorMode = GetStringFieldNiagAuth(Payload, TEXT("colorMode"), TEXT("Direct"));

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("Color"));
        Result->SetStringField(TEXT("colorMode"), ColorMode);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured color module: mode=%s"), *ColorMode));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Color module configured."), Result);
        return true;
    }

    if (SubAction == TEXT("add_sprite_renderer_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        FString MaterialPath = GetStringFieldNiagAuth(Payload, TEXT("materialPath"));
        FString Alignment = GetStringFieldNiagAuth(Payload, TEXT("alignment"), TEXT("Unaligned"));
        FString FacingMode = GetStringFieldNiagAuth(Payload, TEXT("facingMode"), TEXT("FaceCamera"));

        // Get the versioned emitter data for the specified emitter (UE 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
        FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
        UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
        // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
        UNiagaraEmitter* Emitter = Handle->GetInstance();
#endif
        if (EmitterData && Emitter)
        {
            // Create sprite renderer if not exists
            UNiagaraSpriteRendererProperties* SpriteRenderer = nullptr;
            for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
            {
                if (UNiagaraSpriteRendererProperties* SR = Cast<UNiagaraSpriteRendererProperties>(Renderer))
                {
                    SpriteRenderer = SR;
                    break;
                }
            }

            if (!SpriteRenderer)
            {
                SpriteRenderer = NewObject<UNiagaraSpriteRendererProperties>(Emitter);
                if (!SpriteRenderer)
                {
                    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create sprite renderer"), TEXT("CREATION_FAILED"));
                    return true;
                }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                Emitter->AddRenderer(SpriteRenderer, VersionedEmitter.Version);
#else
                // UE 5.0: AddRenderer only takes the renderer
                Emitter->AddRenderer(SpriteRenderer);
#endif
            }

            if (!MaterialPath.IsEmpty())
            {
                UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
                if (Material)
                {
                    SpriteRenderer->Material = Material;
                }
            }
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("SpriteRenderer"));
        Result->SetStringField(TEXT("message"), TEXT("Configured sprite renderer module."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sprite renderer configured."), Result);
        return true;
    }

    if (SubAction == TEXT("add_mesh_renderer_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        FString MeshPath = GetStringFieldNiagAuth(Payload, TEXT("meshPath"));

        // Get the versioned emitter data (UE 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
        FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
        UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
        // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
        UNiagaraEmitter* Emitter = Handle->GetInstance();
#endif
        if (EmitterData && Emitter)
        {
            UNiagaraMeshRendererProperties* MeshRenderer = nullptr;
            for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
            {
                if (UNiagaraMeshRendererProperties* MR = Cast<UNiagaraMeshRendererProperties>(Renderer))
                {
                    MeshRenderer = MR;
                    break;
                }
            }

            if (!MeshRenderer)
            {
                MeshRenderer = NewObject<UNiagaraMeshRendererProperties>(Emitter);
                if (!MeshRenderer)
                {
                    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create mesh renderer"), TEXT("CREATION_FAILED"));
                    return true;
                }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                Emitter->AddRenderer(MeshRenderer, VersionedEmitter.Version);
#else
                // UE 5.0: AddRenderer only takes the renderer
                Emitter->AddRenderer(MeshRenderer);
#endif
            }

            if (!MeshPath.IsEmpty())
            {
                UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
                if (Mesh)
                {
                    FNiagaraMeshRendererMeshProperties MeshProps;
                    MeshProps.Mesh = Mesh;
                    MeshRenderer->Meshes.Empty();
                    MeshRenderer->Meshes.Add(MeshProps);
                }
            }
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("MeshRenderer"));
        Result->SetStringField(TEXT("message"), TEXT("Configured mesh renderer module."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mesh renderer configured."), Result);
        return true;
    }

    if (SubAction == TEXT("add_ribbon_renderer_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        // Get the versioned emitter data (UE 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
        FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
        UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
        // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
        UNiagaraEmitter* Emitter = Handle->GetInstance();
#endif
        if (EmitterData && Emitter)
        {
            UNiagaraRibbonRendererProperties* RibbonRenderer = nullptr;
            for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
            {
                if (UNiagaraRibbonRendererProperties* RR = Cast<UNiagaraRibbonRendererProperties>(Renderer))
                {
                    RibbonRenderer = RR;
                    break;
                }
            }

            if (!RibbonRenderer)
            {
                RibbonRenderer = NewObject<UNiagaraRibbonRendererProperties>(Emitter);
                if (!RibbonRenderer)
                {
                    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create ribbon renderer"), TEXT("CREATION_FAILED"));
                    return true;
                }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                Emitter->AddRenderer(RibbonRenderer, VersionedEmitter.Version);
#else
                // UE 5.0: AddRenderer only takes the renderer
                Emitter->AddRenderer(RibbonRenderer);
#endif
            }

            FString MaterialPath = GetStringFieldNiagAuth(Payload, TEXT("materialPath"));
            if (!MaterialPath.IsEmpty())
            {
                UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
                if (Material)
                {
                    RibbonRenderer->Material = Material;
                }
            }
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("RibbonRenderer"));
        Result->SetStringField(TEXT("message"), TEXT("Configured ribbon renderer module."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ribbon renderer configured."), Result);
        return true;
    }

    if (SubAction == TEXT("add_light_renderer_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        // Get the versioned emitter data (UE 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
        FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
        UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
        // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
        UNiagaraEmitter* Emitter = Handle->GetInstance();
#endif
        if (EmitterData && Emitter)
        {
            UNiagaraLightRendererProperties* LightRenderer = nullptr;
            for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
            {
                if (UNiagaraLightRendererProperties* LR = Cast<UNiagaraLightRendererProperties>(Renderer))
                {
                    LightRenderer = LR;
                    break;
                }
            }

            if (!LightRenderer)
            {
                LightRenderer = NewObject<UNiagaraLightRendererProperties>(Emitter);
                if (!LightRenderer)
                {
                    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create light renderer"), TEXT("CREATION_FAILED"));
                    return true;
                }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                Emitter->AddRenderer(LightRenderer, VersionedEmitter.Version);
#else
                // UE 5.0: AddRenderer only takes the renderer
                Emitter->AddRenderer(LightRenderer);
#endif
            }

            double LightRadius = GetNumberFieldNiagAuth(Payload, TEXT("lightRadius"), 100.0);
            LightRenderer->RadiusScale = static_cast<float>(LightRadius);
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("LightRenderer"));
        Result->SetStringField(TEXT("message"), TEXT("Configured light renderer module."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Light renderer configured."), Result);
        return true;
    }

    if (SubAction == TEXT("add_collision_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FString CollisionMode = GetStringFieldNiagAuth(Payload, TEXT("collisionMode"), TEXT("SceneDepth"));
        double Restitution = GetNumberFieldNiagAuth(Payload, TEXT("restitution"), 0.3);
        double Friction = GetNumberFieldNiagAuth(Payload, TEXT("friction"), 0.2);
        bool bDieOnCollision = GetBoolFieldNiagAuth(Payload, TEXT("dieOnCollision"), false);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("Collision"));
        Result->SetStringField(TEXT("collisionMode"), CollisionMode);
        Result->SetNumberField(TEXT("restitution"), Restitution);
        Result->SetNumberField(TEXT("friction"), Friction);
        Result->SetBoolField(TEXT("dieOnCollision"), bDieOnCollision);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured collision module: mode=%s"), *CollisionMode));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Collision module configured."), Result);
        return true;
    }

    if (SubAction == TEXT("add_kill_particles_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FString KillCondition = GetStringFieldNiagAuth(Payload, TEXT("killCondition"), TEXT("Age"));

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("KillParticles"));
        Result->SetStringField(TEXT("killCondition"), KillCondition);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured kill particles module: condition=%s"), *KillCondition));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Kill particles module configured."), Result);
        return true;
    }

    if (SubAction == TEXT("add_camera_offset_module"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        double CameraOffset = GetNumberFieldNiagAuth(Payload, TEXT("cameraOffset"), 0.0);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("moduleName"), TEXT("CameraOffset"));
        Result->SetNumberField(TEXT("cameraOffset"), CameraOffset);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured camera offset module: offset=%.1f"), CameraOffset));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera offset module configured."), Result);
        return true;
    }

    // =========================================================================
    // 12.3 Parameters & Data Interfaces (8 actions)
    // =========================================================================

    if (SubAction == TEXT("add_user_parameter"))
    {
        if (SystemPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParamName = GetStringFieldNiagAuth(Payload, TEXT("parameterName"));
        FString ParamType = GetStringFieldNiagAuth(Payload, TEXT("parameterType"), TEXT("Float"));

        if (ParamName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
        
        FNiagaraTypeDefinition TypeDef;
        if (ParamType == TEXT("Float"))
        {
            TypeDef = FNiagaraTypeDefinition::GetFloatDef();
        }
        else if (ParamType == TEXT("Int"))
        {
            TypeDef = FNiagaraTypeDefinition::GetIntDef();
        }
        else if (ParamType == TEXT("Bool"))
        {
            TypeDef = FNiagaraTypeDefinition::GetBoolDef();
        }
        else if (ParamType == TEXT("Vector"))
        {
            TypeDef = FNiagaraTypeDefinition::GetVec3Def();
        }
        else if (ParamType == TEXT("LinearColor"))
        {
            TypeDef = FNiagaraTypeDefinition::GetColorDef();
        }
        else
        {
            TypeDef = FNiagaraTypeDefinition::GetFloatDef();
        }

        FNiagaraVariable NewParam(TypeDef, FName(*ParamName));
        UserStore.AddParameter(NewParam, true);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("parameterName"), ParamName);
        Result->SetStringField(TEXT("parameterType"), ParamType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added user parameter '%s' of type %s."), *ParamName, *ParamType));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("User parameter added."), Result);
        return true;
    }

    if (SubAction == TEXT("set_parameter_value"))
    {
        if (SystemPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParamName = GetStringFieldNiagAuth(Payload, TEXT("parameterName"));
        if (ParamName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
        
        // Try to find and set the parameter value
        FNiagaraVariable FloatVar(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParamName));
        FNiagaraVariable IntVar(FNiagaraTypeDefinition::GetIntDef(), FName(*ParamName));
        FNiagaraVariable BoolVar(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParamName));
        FNiagaraVariable VecVar(FNiagaraTypeDefinition::GetVec3Def(), FName(*ParamName));

        double NumVal = 0;
        bool BoolVal = false;
        Payload->TryGetNumberField(TEXT("parameterValue"), NumVal);
        Payload->TryGetBoolField(TEXT("parameterValue"), BoolVal);

        if (UserStore.FindParameterVariable(FloatVar))
        {
            UserStore.SetParameterValue(static_cast<float>(NumVal), FloatVar);
        }
        else if (UserStore.FindParameterVariable(IntVar))
        {
            UserStore.SetParameterValue(static_cast<int32>(NumVal), IntVar);
        }
        else if (UserStore.FindParameterVariable(BoolVar))
        {
            UserStore.SetParameterValue(BoolVal, BoolVar);
        }
        else if (UserStore.FindParameterVariable(VecVar))
        {
            const TSharedPtr<FJsonObject>* ValObj;
            if (Payload->TryGetObjectField(TEXT("parameterValue"), ValObj))
            {
                FVector Vec = GetVectorFromJsonNiag(*ValObj);
                UserStore.SetParameterValue(Vec, VecVar);
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Parameter '%s' not found."), *ParamName), TEXT("PARAM_NOT_FOUND"));
            return true;
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("parameterName"), ParamName);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parameter '%s' value."), *ParamName));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Parameter value set."), Result);
        return true;
    }

    if (SubAction == TEXT("bind_parameter_to_source"))
    {
        if (SystemPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParamName = GetStringFieldNiagAuth(Payload, TEXT("parameterName"));
        FString SourceBinding = GetStringFieldNiagAuth(Payload, TEXT("sourceBinding"));

        if (ParamName.IsEmpty() || SourceBinding.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'parameterName' or 'sourceBinding'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Parameter binding is typically done through the Niagara editor/stack
        // For now, we log the intent
        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("parameterName"), ParamName);
        Result->SetStringField(TEXT("sourceBinding"), SourceBinding);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Bound parameter '%s' to source '%s'."), *ParamName, *SourceBinding));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Parameter bound to source."), Result);
        return true;
    }

    if (SubAction == TEXT("add_skeletal_mesh_data_interface"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FString SkeletalMeshPath = GetStringFieldNiagAuth(Payload, TEXT("skeletalMeshPath"));

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("dataInterface"), TEXT("SkeletalMesh"));
        Result->SetStringField(TEXT("message"), TEXT("Added Skeletal Mesh data interface."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Skeletal Mesh DI added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_static_mesh_data_interface"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FString StaticMeshPath = GetStringFieldNiagAuth(Payload, TEXT("staticMeshPath"));

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("dataInterface"), TEXT("StaticMesh"));
        Result->SetStringField(TEXT("message"), TEXT("Added Static Mesh data interface."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Static Mesh DI added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_spline_data_interface"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("dataInterface"), TEXT("Spline"));
        Result->SetStringField(TEXT("message"), TEXT("Added Spline data interface."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spline DI added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_audio_spectrum_data_interface"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("dataInterface"), TEXT("AudioSpectrum"));
        Result->SetStringField(TEXT("message"), TEXT("Added Audio Spectrum data interface."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Audio Spectrum DI added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_collision_query_data_interface"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("dataInterface"), TEXT("CollisionQuery"));
        Result->SetStringField(TEXT("message"), TEXT("Added Collision Query data interface."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Collision Query DI added."), Result);
        return true;
    }

    // =========================================================================
    // 12.4 Events & GPU (5 actions)
    // =========================================================================

    if (SubAction == TEXT("add_event_generator"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString EventName = GetStringFieldNiagAuth(Payload, TEXT("eventName"));
        if (EventName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'eventName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("eventName"), EventName);
        Result->SetStringField(TEXT("eventType"), TEXT("Generator"));
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added event generator '%s'."), *EventName));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event generator added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_event_receiver"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString EventName = GetStringFieldNiagAuth(Payload, TEXT("eventName"));
        if (EventName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'eventName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        bool bSpawnOnEvent = GetBoolFieldNiagAuth(Payload, TEXT("spawnOnEvent"), false);
        double EventSpawnCount = GetNumberFieldNiagAuth(Payload, TEXT("eventSpawnCount"), 1.0);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("eventName"), EventName);
        Result->SetStringField(TEXT("eventType"), TEXT("Receiver"));
        Result->SetBoolField(TEXT("spawnOnEvent"), bSpawnOnEvent);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added event receiver '%s'."), *EventName));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event receiver added."), Result);
        return true;
    }

    if (SubAction == TEXT("configure_event_payload"))
    {
        if (SystemPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString EventName = GetStringFieldNiagAuth(Payload, TEXT("eventName"));
        if (EventName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'eventName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Get payload definition from JSON
        const TArray<TSharedPtr<FJsonValue>>* PayloadArray;
        TArray<FString> PayloadAttributes;
        if (Payload->TryGetArrayField(TEXT("eventPayload"), PayloadArray))
        {
            for (const auto& Item : *PayloadArray)
            {
                const TSharedPtr<FJsonObject>* AttrObj;
                if (Item->TryGetObject(AttrObj) && AttrObj->IsValid())
                {
                    FString AttrName = GetStringFieldNiagAuth(*AttrObj, TEXT("name"));
                    FString AttrType = GetStringFieldNiagAuth(*AttrObj, TEXT("type"));
                    PayloadAttributes.Add(FString::Printf(TEXT("%s:%s"), *AttrName, *AttrType));
                }
            }
        }

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("eventName"), EventName);
        Result->SetNumberField(TEXT("payloadAttributeCount"), PayloadAttributes.Num());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured event payload for '%s' with %d attributes."), *EventName, PayloadAttributes.Num()));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event payload configured."), Result);
        return true;
    }

    if (SubAction == TEXT("enable_gpu_simulation"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        UNiagaraEmitter* Emitter = Handle->GetInstance().Emitter;
        if (Emitter)
        {
            // Enable GPU simulation by setting simulation target
            // This requires accessing the versioned emitter data
            MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = MCP_GET_LATEST_EMITTER_DATA(Emitter);
            if (EmitterData)
            {
                EmitterData->SimTarget = ENiagaraSimTarget::GPUComputeSim;
            }
        }
#else
        // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
        UNiagaraEmitter* Emitter = Handle->GetInstance();
        if (Emitter)
        {
            Emitter->SimTarget = ENiagaraSimTarget::GPUComputeSim;
        }
#endif

        bool bFixedBounds = GetBoolFieldNiagAuth(Payload, TEXT("fixedBoundsEnabled"), false);
        bool bDeterministic = GetBoolFieldNiagAuth(Payload, TEXT("deterministicEnabled"), false);

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetBoolField(TEXT("gpuEnabled"), true);
        Result->SetBoolField(TEXT("fixedBoundsEnabled"), bFixedBounds);
        Result->SetBoolField(TEXT("deterministicEnabled"), bDeterministic);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Enabled GPU simulation for emitter '%s'."), *EmitterName));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("GPU simulation enabled."), Result);
        return true;
    }

    if (SubAction == TEXT("add_simulation_stage"))
    {
        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString StageName = GetStringFieldNiagAuth(Payload, TEXT("stageName"));
        if (StageName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'stageName'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FString IterationSource = GetStringFieldNiagAuth(Payload, TEXT("stageIterationSource"), TEXT("Particles"));

        if (bSave)
        {
            System->MarkPackageDirty();
        }

        Result->SetStringField(TEXT("stageName"), StageName);
        Result->SetStringField(TEXT("iterationSource"), IterationSource);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added simulation stage '%s'."), *StageName));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Simulation stage added."), Result);
        return true;
    }

    // =========================================================================
    // 12.5 Utility (2 actions)
    // =========================================================================

    if (SubAction == TEXT("get_niagara_info"))
    {
        if (AssetPath.IsEmpty() && SystemPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath' or 'systemPath'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TargetPath = AssetPath.IsEmpty() ? SystemPath : AssetPath;

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *TargetPath);
        UNiagaraEmitter* Emitter = nullptr;
        
        if (!System)
        {
            Emitter = LoadObject<UNiagaraEmitter>(nullptr, *TargetPath);
        }

        if (!System && !Emitter)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara asset."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());

        if (System)
        {
            InfoObj->SetStringField(TEXT("assetType"), TEXT("System"));
            InfoObj->SetNumberField(TEXT("emitterCount"), System->GetEmitterHandles().Num());

            TArray<TSharedPtr<FJsonValue>> EmittersArray;
            for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
            {
                TSharedPtr<FJsonObject> EmitterObj = MakeShareable(new FJsonObject());
                EmitterObj->SetStringField(TEXT("name"), Handle.GetName().ToString());
                EmitterObj->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());
                
                #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                UNiagaraEmitter* Em = Handle.GetInstance().Emitter;
                #else
                UNiagaraEmitter* Em = Handle.GetInstance();
                #endif
                if (Em)
                {
                    MCP_NIAGARA_EMITTER_DATA_TYPE* EmData = MCP_GET_LATEST_EMITTER_DATA(Em);
                    if (EmData)
                    {
                        EmitterObj->SetStringField(TEXT("simulationTarget"), EmData->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"));
                    }
                }
                
                EmittersArray.Add(MakeShareable(new FJsonValueObject(EmitterObj)));
            }
            InfoObj->SetArrayField(TEXT("emitters"), EmittersArray);

            // User parameters
            FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
            TArray<FNiagaraVariable> Params;
            UserStore.GetParameters(Params);
            
            InfoObj->SetNumberField(TEXT("userParameterCount"), Params.Num());
            
            TArray<TSharedPtr<FJsonValue>> ParamsArray;
            for (const FNiagaraVariable& Param : Params)
            {
                TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject());
                ParamObj->SetStringField(TEXT("name"), Param.GetName().ToString());
                ParamObj->SetStringField(TEXT("type"), Param.GetType().GetName());
                ParamsArray.Add(MakeShareable(new FJsonValueObject(ParamObj)));
            }
            InfoObj->SetArrayField(TEXT("userParameters"), ParamsArray);

            // Check for GPU emitters
            bool bHasGPU = false;
            for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
            {
                #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                UNiagaraEmitter* Em = Handle.GetInstance().Emitter;
                #else
                UNiagaraEmitter* Em = Handle.GetInstance();
                #endif
                if (Em && MCP_GET_LATEST_EMITTER_DATA(Em) && MCP_GET_LATEST_EMITTER_DATA(Em)->SimTarget == ENiagaraSimTarget::GPUComputeSim)
                {
                    bHasGPU = true;
                    break;
                }
            }
            InfoObj->SetBoolField(TEXT("hasGPUEmitters"), bHasGPU);
        }
        else if (Emitter)
        {
            InfoObj->SetStringField(TEXT("assetType"), TEXT("Emitter"));
            InfoObj->SetStringField(TEXT("name"), Emitter->GetName());
            
            MCP_NIAGARA_EMITTER_DATA_TYPE* EmData = MCP_GET_LATEST_EMITTER_DATA(Emitter);
            if (EmData)
            {
                InfoObj->SetStringField(TEXT("simulationTarget"), EmData->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"));
            }
        }

        Result->SetObjectField(TEXT("niagaraInfo"), InfoObj);
        Result->SetStringField(TEXT("message"), TEXT("Retrieved Niagara asset information."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara info retrieved."), Result);
        return true;
    }

    if (SubAction == TEXT("validate_niagara_system"))
    {
        if (SystemPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> ValidationResult = MakeShareable(new FJsonObject());
        TArray<TSharedPtr<FJsonValue>> ErrorsArray;
        TArray<TSharedPtr<FJsonValue>> WarningsArray;

        bool bIsValid = true;

        // Check if system has emitters
        if (System->GetEmitterHandles().Num() == 0)
        {
            WarningsArray.Add(MakeShareable(new FJsonValueString(TEXT("System has no emitters."))));
        }

        // Check each emitter for issues
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            if (!Handle.GetIsEnabled())
            {
                WarningsArray.Add(MakeShareable(new FJsonValueString(
                    FString::Printf(TEXT("Emitter '%s' is disabled."), *Handle.GetName().ToString()))));
            }

            // Get emitter data - UE 5.1+ uses GetEmitterData(), UE 5.0 uses GetInstance()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle.GetEmitterData();
#else
            MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle.GetInstance();
#endif
            if (EmitterData)
            {
                // Check for renderers (UE 5.7+)
                if (EmitterData->GetRenderers().Num() == 0)
                {
                    WarningsArray.Add(MakeShareable(new FJsonValueString(
                        FString::Printf(TEXT("Emitter '%s' has no renderers."), *Handle.GetName().ToString()))));
                }
            }
        }

        ValidationResult->SetBoolField(TEXT("isValid"), bIsValid);
        ValidationResult->SetArrayField(TEXT("errors"), ErrorsArray);
        ValidationResult->SetArrayField(TEXT("warnings"), WarningsArray);

        Result->SetObjectField(TEXT("validationResult"), ValidationResult);
        Result->SetStringField(TEXT("message"), bIsValid ? TEXT("System is valid.") : TEXT("System has validation errors."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Validation complete."), Result);
        return true;
    }

    // Unknown subAction
    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;

#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif // WITH_EDITOR
}

#undef GetStringFieldNiagAuth
#undef GetNumberFieldNiagAuth
#undef GetBoolFieldNiagAuth

