// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 3E: Niagara Advanced VFX Handlers
// Implements actions for module creation, scripting, and advanced simulation (fluids/chaos).

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"

#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEditorModule.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraEmitterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraScriptFactoryNew.h"
#include "NiagaraDataInterface.h"
#endif

// Use consolidated JSON helpers
#define GetStringField GetJsonStringField
#define GetNumberField GetJsonNumberField
#define GetBoolField GetJsonBoolField

bool UMcpAutomationBridgeSubsystem::HandleManageNiagaraAdvancedAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_niagara_advanced"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    bool bSave = GetBoolField(Payload, TEXT("save"), true);

    // =========================================================================
    // Helpers
    // =========================================================================
    auto FindEmitterHandle = [&](UNiagaraSystem* System, const FString& TargetEmitter) -> FNiagaraEmitterHandle*
    {
        for (FNiagaraEmitterHandle& H : System->GetEmitterHandles())
        {
            if (H.GetName().ToString() == TargetEmitter)
            {
                return &H;
            }
        }
        return nullptr;
    };

    auto AddModuleToEmitterStack = [](FNiagaraEmitterHandle* Handle, const FString& ModuleScriptPath, ENiagaraScriptUsage TargetUsage, const FString& SuggestedName = FString()) -> UNiagaraNodeFunctionCall*
    {
        if (!Handle) return nullptr;
        FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
        if (!EmitterData) return nullptr;

        UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(EmitterData->GraphSource);
        if (!ScriptSource || !ScriptSource->NodeGraph) return nullptr;

        UNiagaraGraph* Graph = ScriptSource->NodeGraph;
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
        if (!TargetOutput) return nullptr;

        FSoftObjectPath AssetRef(ModuleScriptPath);
        UNiagaraScript* ModuleScript = Cast<UNiagaraScript>(AssetRef.TryLoad());
        if (!ModuleScript) return nullptr;

        return FNiagaraStackGraphUtilities::AddScriptModuleToStack(
            ModuleScript,
            *TargetOutput,
            INDEX_NONE,
            SuggestedName.IsEmpty() ? ModuleScript->GetName() : SuggestedName
        );
    };

    // =========================================================================
    // 3E.1 Module & Scripting Actions
    // =========================================================================

    if (SubAction == TEXT("create_niagara_module"))
    {
        FString Name = GetStringField(Payload, TEXT("name"));
        FString Path = GetStringField(Payload, TEXT("path"), TEXT("/Game/VFX/Modules"));
        
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (!Path.EndsWith(TEXT("/"))) Path += TEXT("/");
        FString PackageName = FPackageName::ObjectPathToPackageName(Path + Name);
        UPackage* Package = CreatePackage(*PackageName);

        UNiagaraScriptFactoryNew* Factory = NewObject<UNiagaraScriptFactoryNew>();
        // UE 5.7 removed ScriptUsage from factory - the script usage is set on the created asset instead
        // Factory->ScriptUsage = ENiagaraScriptUsage::Module; // Not available in UE 5.7
        
        UNiagaraScript* NewScript = Cast<UNiagaraScript>(Factory->FactoryCreateNew(
            UNiagaraScript::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));

        if (!NewScript)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create Niagara Script."), TEXT("CREATE_FAILED"));
            return true;
        }

        FAssetRegistryModule::AssetCreated(NewScript);
        if (bSave) McpSafeAssetSave(NewScript);

        Result->SetStringField(TEXT("assetPath"), NewScript->GetPathName());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Niagara Module: %s"), *Name));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Module created."), Result);
        return true;
    }

    if (SubAction == TEXT("add_niagara_script"))
    {
        FString SystemPath = GetStringField(Payload, TEXT("systemPath"));
        FString EmitterName = GetStringField(Payload, TEXT("emitterName"));
        FString ModulePath = GetStringField(Payload, TEXT("modulePath"));
        FString Stage = GetStringField(Payload, TEXT("stage"), TEXT("Update")); // Spawn, Update, Event, Simulation

        if (SystemPath.IsEmpty() || EmitterName.IsEmpty() || ModulePath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Emitter not found."), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        ENiagaraScriptUsage Usage = ENiagaraScriptUsage::ParticleUpdateScript;
        if (Stage.Equals(TEXT("Spawn"), ESearchCase::IgnoreCase)) Usage = ENiagaraScriptUsage::ParticleSpawnScript;
        else if (Stage.Equals(TEXT("EmitterSpawn"), ESearchCase::IgnoreCase)) Usage = ENiagaraScriptUsage::EmitterSpawnScript;
        else if (Stage.Equals(TEXT("EmitterUpdate"), ESearchCase::IgnoreCase)) Usage = ENiagaraScriptUsage::EmitterUpdateScript;
        else if (Stage.Equals(TEXT("SystemSpawn"), ESearchCase::IgnoreCase)) Usage = ENiagaraScriptUsage::SystemSpawnScript;
        else if (Stage.Equals(TEXT("SystemUpdate"), ESearchCase::IgnoreCase)) Usage = ENiagaraScriptUsage::SystemUpdateScript;

        UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(Handle, ModulePath, Usage);
        
        if (bSave) System->MarkPackageDirty();

        Result->SetBoolField(TEXT("moduleAdded"), NewModule != nullptr);
        Result->SetStringField(TEXT("message"), NewModule ? TEXT("Script added successfully.") : TEXT("Failed to add script. Check path and compatibility."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Script added."), Result);
        return true;
    }

    if (SubAction == TEXT("add_data_interface"))
    {
        // Generic add data interface by class name
        FString SystemPath = GetStringField(Payload, TEXT("systemPath"));
        FString ClassName = GetStringField(Payload, TEXT("className")); // e.g. "NiagaraDataInterfaceCurve"
        FString ParamName = GetStringField(Payload, TEXT("parameterName"));

        if (SystemPath.IsEmpty() || ClassName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Find the class
        UClass* DIClass = nullptr;
        // Try simple name first (nullptr replaces deprecated ANY_PACKAGE)
        DIClass = FindObject<UClass>(nullptr, *ClassName);
        if (!DIClass)
        {
            // Try full path or appending U
            DIClass = FindObject<UClass>(nullptr, *("U" + ClassName));
        }
        
        // If still not found and looks like a short name, iterate derived classes of UNiagaraDataInterface
        // Filter CDOs to avoid iterating over default objects
        if (!DIClass)
        {
            for (TObjectIterator<UClass> It; It; ++It)
            {
                // Skip CDOs and abstract classes
                if (It->HasAnyFlags(RF_ClassDefaultObject)) continue;
                if (It->IsChildOf(UNiagaraDataInterface::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
                {
                    if (It->GetName() == ClassName || It->GetName() == ("U" + ClassName))
                    {
                        DIClass = *It;
                        break;
                    }
                }
            }
        }

        if (!DIClass)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Data Interface class '%s' not found."), *ClassName), TEXT("CLASS_NOT_FOUND"));
            return true;
        }

        UNiagaraDataInterface* NewDI = NewObject<UNiagaraDataInterface>(System, DIClass, NAME_None, RF_Transactional);
        if (!NewDI)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to instantiate Data Interface."), TEXT("CREATE_FAILED"));
            return true;
        }

        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
        FNiagaraTypeDefinition TypeDef(DIClass);
        FNiagaraVariable DIParam(TypeDef, FName(*ParamName));
        UserStore.AddParameter(DIParam, true);
        UserStore.SetDataInterface(NewDI, DIParam);

        if (bSave) System->MarkPackageDirty();

        Result->SetStringField(TEXT("parameterName"), ParamName);
        Result->SetStringField(TEXT("className"), DIClass->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Data Interface added."), Result);
        return true;
    }

    // =========================================================================
    // 3E.2 Fluids & Chaos Integration
    // =========================================================================

    if (SubAction == TEXT("setup_niagara_fluids"))
    {
        // High level setup for 2D/3D fluids
        // This typically involves adding Grid2D/3D collections and solvers
        // For MCP, we'll implement this by adding the standard Fluid modules
        
        FString SystemPath = GetStringField(Payload, TEXT("systemPath"));
        FString EmitterName = GetStringField(Payload, TEXT("emitterName"));
        FString FluidType = GetStringField(Payload, TEXT("fluidType"), TEXT("2D")); // 2D or 3D

        if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing systemPath or emitterName."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Emitter not found."), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }

        // Add Grid Collection Module
        FString GridModulePath;
        if (FluidType == TEXT("3D"))
        {
            GridModulePath = TEXT("/Niagara/Modules/Fluids/Grid3D/Grid3D_Collection.Grid3D_Collection");
        }
        else
        {
            GridModulePath = TEXT("/Niagara/Modules/Fluids/Grid2D/Grid2D_Collection.Grid2D_Collection");
        }

        UNiagaraNodeFunctionCall* GridModule = AddModuleToEmitterStack(
            Handle, 
            GridModulePath, 
            ENiagaraScriptUsage::EmitterSpawnScript, // Grids usually initialized at spawn
            TEXT("GridCollection")
        );

        if (bSave) System->MarkPackageDirty();

        Result->SetBoolField(TEXT("fluidsSetup"), GridModule != nullptr);
        Result->SetStringField(TEXT("fluidType"), FluidType);
        Result->SetStringField(TEXT("message"), GridModule ? TEXT("Fluids setup successful.") : TEXT("Failed to add fluid modules. Check Niagara Fluids plugin."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Fluids setup."), Result);
        return true;
    }

    if (SubAction == TEXT("add_chaos_integration"))
    {
        // Adds Chaos Destruction Data Interface or Listeners
        FString SystemPath = GetStringField(Payload, TEXT("systemPath"));
        FString ParamName = GetStringField(Payload, TEXT("parameterName"), TEXT("ChaosDestruction"));

        if (SystemPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing systemPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Look for NiagaraDataInterfaceChaosDestruction
        // This might require the ChaosNiagara plugin (nullptr replaces deprecated ANY_PACKAGE)
        UClass* DIClass = FindObject<UClass>(nullptr, TEXT("NiagaraDataInterfaceChaosDestruction"));
        if (!DIClass)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Chaos Destruction Data Interface not found. Enable ChaosNiagara plugin."), TEXT("FEATURE_NOT_AVAILABLE"));
            return true;
        }

        UNiagaraDataInterface* NewDI = NewObject<UNiagaraDataInterface>(System, DIClass, NAME_None, RF_Transactional);
        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
        FNiagaraTypeDefinition TypeDef(DIClass);
        FNiagaraVariable DIParam(TypeDef, FName(*ParamName));
        UserStore.AddParameter(DIParam, true);
        UserStore.SetDataInterface(NewDI, DIParam);

        if (bSave) System->MarkPackageDirty();

        Result->SetStringField(TEXT("parameterName"), ParamName);
        Result->SetStringField(TEXT("message"), TEXT("Chaos integration added."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Chaos integration added."), Result);
        return true;
    }

#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara Advanced Actions only available in Editor builds."), TEXT("EDITOR_ONLY"));
#endif

    return true;
}
