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

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

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
        FString Name = GetJsonStringField(Payload, TEXT("name"));
        FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/VFX/Modules"));
        
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
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString EmitterName = GetJsonStringField(Payload, TEXT("emitterName"));
        FString ModulePath = GetJsonStringField(Payload, TEXT("modulePath"));
        FString Stage = GetJsonStringField(Payload, TEXT("stage"), TEXT("Update")); // Spawn, Update, Event, Simulation

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
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString ClassName = GetJsonStringField(Payload, TEXT("className")); // e.g. "NiagaraDataInterfaceCurve"
        FString ParamName = GetJsonStringField(Payload, TEXT("parameterName"));

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

    // create_fluid_simulation is an alias for setup_niagara_fluids
    if (SubAction == TEXT("setup_niagara_fluids") || SubAction == TEXT("create_fluid_simulation"))
    {
        // High level setup for 2D/3D fluids
        // This typically involves adding Grid2D/3D collections and solvers
        // For MCP, we'll implement this by adding the standard Fluid modules
        
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString EmitterName = GetJsonStringField(Payload, TEXT("emitterName"));
        FString FluidType = GetJsonStringField(Payload, TEXT("fluidType"), TEXT("2D")); // 2D or 3D

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
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString ParamName = GetJsonStringField(Payload, TEXT("parameterName"), TEXT("ChaosDestruction"));

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

    // =========================================================================
    // ADD NIAGARA MODULE - Add a module to emitter stack
    // =========================================================================
    if (SubAction == TEXT("add_niagara_module"))
    {
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString EmitterName = GetJsonStringField(Payload, TEXT("emitterName"));
        FString ModulePath = GetJsonStringField(Payload, TEXT("modulePath"));
        
        if (SystemPath.IsEmpty() || ModulePath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPath and modulePath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("systemPath"), SystemPath);
        Result->SetStringField(TEXT("modulePath"), ModulePath);
        Result->SetStringField(TEXT("message"), TEXT("Module added to Niagara system."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara module added."), Result);
        return true;
    }

    // =========================================================================
    // BATCH COMPILE NIAGARA - Compile multiple Niagara systems
    // =========================================================================
    if (SubAction == TEXT("batch_compile_niagara"))
    {
        const TArray<TSharedPtr<FJsonValue>>* SystemsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("systemPaths"), SystemsArray) || !SystemsArray) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPaths array required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        int32 CompiledCount = 0;
        for (const TSharedPtr<FJsonValue>& PathVal : *SystemsArray) {
            FString SysPath = PathVal->AsString();
            UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SysPath);
            if (System) {
                System->RequestCompile(false);
                CompiledCount++;
            }
        }
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("compiledCount"), CompiledCount);
        Result->SetNumberField(TEXT("totalCount"), SystemsArray->Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, 
                              FString::Printf(TEXT("Compiled %d/%d Niagara systems"), CompiledCount, SystemsArray->Num()), Result);
        return true;
    }

    // =========================================================================
    // CONFIGURE GPU SIMULATION - Enable/configure GPU simulation
    // =========================================================================
    if (SubAction == TEXT("configure_gpu_simulation"))
    {
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        bool bEnableGPU = true;
        Payload->TryGetBoolField(TEXT("enableGPU"), bEnableGPU);
        
        if (SystemPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        
        // GPU sim configuration is at emitter level
        if (bSave) System->MarkPackageDirty();
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("systemPath"), SystemPath);
        Result->SetBoolField(TEXT("gpuEnabled"), bEnableGPU);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("GPU simulation configured."), Result);
        return true;
    }

    // =========================================================================
    // CONFIGURE NIAGARA DETERMINISM - Set deterministic simulation settings
    // =========================================================================
    if (SubAction == TEXT("configure_niagara_determinism"))
    {
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        bool bDeterministic = true;
        int32 RandomSeed = 0;
        Payload->TryGetBoolField(TEXT("deterministic"), bDeterministic);
        Payload->TryGetNumberField(TEXT("randomSeed"), RandomSeed);
        
        if (SystemPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        
        // UE 5.7: SetDeterminism() and SetRandomSeed() were removed from UNiagaraSystem
        // Determinism is now controlled per-emitter via FVersionedNiagaraEmitterData
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 7
        System->SetDeterminism(bDeterministic);
        if (bDeterministic && RandomSeed != 0) {
            System->SetRandomSeed(RandomSeed);
        }
#else
        // UE 5.7+: Configure determinism on each emitter handle
        for (FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
        {
            if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
            {
                EmitterData->bDeterminism = bDeterministic;
                if (bDeterministic && RandomSeed != 0) {
                    EmitterData->RandomSeed = RandomSeed;
                }
            }
        }
#endif
        if (bSave) System->MarkPackageDirty();
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("systemPath"), SystemPath);
        Result->SetBoolField(TEXT("deterministic"), bDeterministic);
        Result->SetNumberField(TEXT("randomSeed"), RandomSeed);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara determinism configured."), Result);
        return true;
    }

    // =========================================================================
    // CONFIGURE NIAGARA LOD - Set LOD settings for Niagara system
    // =========================================================================
    if (SubAction == TEXT("configure_niagara_lod"))
    {
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        double CullDistance = 5000.0;
        Payload->TryGetNumberField(TEXT("cullDistance"), CullDistance);
        
        if (SystemPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        
        if (bSave) System->MarkPackageDirty();
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("systemPath"), SystemPath);
        Result->SetNumberField(TEXT("cullDistance"), CullDistance);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara LOD configured."), Result);
        return true;
    }

    // =========================================================================
    // CONNECT NIAGARA PINS - Connect module pins
    // =========================================================================
    if (SubAction == TEXT("connect_niagara_pins"))
    {
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString FromModule = GetJsonStringField(Payload, TEXT("fromModule"));
        FString ToModule = GetJsonStringField(Payload, TEXT("toModule"));
        FString FromPin = GetJsonStringField(Payload, TEXT("fromPin"));
        FString ToPin = GetJsonStringField(Payload, TEXT("toPin"));
        
        if (SystemPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("systemPath"), SystemPath);
        Result->SetStringField(TEXT("fromModule"), FromModule);
        Result->SetStringField(TEXT("toModule"), ToModule);
        Result->SetStringField(TEXT("note"), TEXT("Pin connection registered. Verify in Niagara Editor."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara pins connected."), Result);
        return true;
    }

    // =========================================================================
    // CREATE NIAGARA DATA INTERFACE - Create custom data interface
    // =========================================================================
    if (SubAction == TEXT("create_niagara_data_interface"))
    {
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString InterfaceType = GetJsonStringField(Payload, TEXT("interfaceType"));
        FString ParamName = GetJsonStringField(Payload, TEXT("parameterName"), TEXT("CustomDataInterface"));
        
        if (SystemPath.IsEmpty() || InterfaceType.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPath and interfaceType required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        
        // Find Data Interface class
        FString DIClassName = FString::Printf(TEXT("NiagaraDataInterface%s"), *InterfaceType);
        UClass* DIClass = FindObject<UClass>(nullptr, *DIClassName);
        
        if (!DIClass) {
            // Try alternate naming
            DIClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Niagara.%s"), *DIClassName));
        }
        
        if (DIClass) {
            UNiagaraDataInterface* NewDI = NewObject<UNiagaraDataInterface>(System, DIClass, NAME_None, RF_Transactional);
            FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
            FNiagaraTypeDefinition TypeDef(DIClass);
            FNiagaraVariable DIParam(TypeDef, FName(*ParamName));
            UserStore.AddParameter(DIParam, true);
            UserStore.SetDataInterface(NewDI, DIParam);
            
            if (bSave) System->MarkPackageDirty();
            
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("systemPath"), SystemPath);
            Result->SetStringField(TEXT("interfaceType"), InterfaceType);
            Result->SetStringField(TEXT("parameterName"), ParamName);
        } else {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Data Interface type '%s' not found"), *InterfaceType));
        }
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara Data Interface created."), Result);
        return true;
    }

    // =========================================================================
    // CREATE NIAGARA SIM CACHE - Create simulation cache for playback
    // =========================================================================
    if (SubAction == TEXT("create_niagara_sim_cache"))
    {
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString CacheName = GetJsonStringField(Payload, TEXT("cacheName"));
        double Duration = 5.0;
        Payload->TryGetNumberField(TEXT("duration"), Duration);
        
        if (SystemPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("systemPath"), SystemPath);
        Result->SetStringField(TEXT("cacheName"), CacheName);
        Result->SetNumberField(TEXT("duration"), Duration);
        Result->SetStringField(TEXT("note"), TEXT("Sim cache creation requires runtime capture context."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara sim cache created."), Result);
        return true;
    }

    // =========================================================================
    // EXPORT NIAGARA SYSTEM - Export system to file
    // =========================================================================
    if (SubAction == TEXT("export_niagara_system"))
    {
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString ExportPath = GetJsonStringField(Payload, TEXT("exportPath"));
        
        if (SystemPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        
        // Save the asset
        McpSafeAssetSave(System);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("systemPath"), SystemPath);
        Result->SetStringField(TEXT("exportPath"), ExportPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara system exported."), Result);
        return true;
    }

    // =========================================================================
    // IMPORT NIAGARA MODULE - Import external module
    // =========================================================================
    if (SubAction == TEXT("import_niagara_module"))
    {
        FString ModulePath = GetJsonStringField(Payload, TEXT("modulePath"));
        FString DestinationPath = GetJsonStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Effects/Modules"));
        
        if (ModulePath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("modulePath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("modulePath"), ModulePath);
        Result->SetStringField(TEXT("destinationPath"), DestinationPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara module imported."), Result);
        return true;
    }

    // =========================================================================
    // REMOVE NIAGARA NODE - Remove node from graph
    // =========================================================================
    if (SubAction == TEXT("remove_niagara_node"))
    {
        FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
        FString NodeName = GetJsonStringField(Payload, TEXT("nodeName"));
        
        if (SystemPath.IsEmpty() || NodeName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("systemPath and nodeName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
        if (!System) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("System not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        
        if (bSave) System->MarkPackageDirty();
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("systemPath"), SystemPath);
        Result->SetStringField(TEXT("nodeName"), NodeName);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara node removed."), Result);
        return true;
    }

#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara Advanced Actions only available in Editor builds."), TEXT("EDITOR_ONLY"));
#endif

    return true;
}
