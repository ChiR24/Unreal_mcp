#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "CoreMinimal.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_NIAGARA_VERSIONING_APIS 1
#define MCP_NIAGARA_EMITTER_DATA_TYPE FVersionedNiagaraEmitterData
#define MCP_GET_EMITTER_DATA(Handle) (Handle).GetEmitterData()
#define MCP_GET_LATEST_EMITTER_DATA(Emitter) (Emitter)->GetLatestEmitterData()
#define MCP_GET_EMITTER_VERSION_GUID(Emitter) (Emitter)->GetExposedVersion().VersionGuid
#else
#define MCP_HAS_NIAGARA_VERSIONING_APIS 0
#define MCP_NIAGARA_EMITTER_DATA_TYPE UNiagaraEmitter
#define MCP_GET_EMITTER_DATA(Handle) (&(Handle))->GetInstance()
#define MCP_GET_LATEST_EMITTER_DATA(Emitter) (Emitter)
#define MCP_GET_EMITTER_VERSION_GUID(Emitter) FGuid()
#endif

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraConstants.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceAudioSpectrum.h"
#include "NiagaraDataInterfaceCollisionQuery.h"
#include "NiagaraDataInterfaceSpline.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraParameterStore.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"

#if __has_include("NiagaraEmitterFactoryNew.h") && !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
#include "NiagaraEmitterFactoryNew.h"
#define MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW 1
#else
#define MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW 0
#endif

#if __has_include("NiagaraSystemFactoryNew.h") && !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
#include "NiagaraSystemFactoryNew.h"
#define MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW 1
#else
#define MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#define MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES 1
#else
#define MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES 0
#endif

#if __has_include("NiagaraDataInterfaceSkeletalMesh.h")
#include "NiagaraDataInterfaceSkeletalMesh.h"
#define MCP_HAS_NIAGARA_SKELETAL_MESH_DI 1
#elif __has_include("DataInterface/NiagaraDataInterfaceSkeletalMesh.h")
#include "DataInterface/NiagaraDataInterfaceSkeletalMesh.h"
#define MCP_HAS_NIAGARA_SKELETAL_MESH_DI 1
#else
#define MCP_HAS_NIAGARA_SKELETAL_MESH_DI 0
#endif

#if __has_include("NiagaraDataInterfaceStaticMesh.h")
#include "NiagaraDataInterfaceStaticMesh.h"
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 1
#elif __has_include("DataInterface/NiagaraDataInterfaceStaticMesh.h")
#include "DataInterface/NiagaraDataInterfaceStaticMesh.h"
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 1
#elif __has_include("Internal/DataInterface/NiagaraDataInterfaceStaticMesh.h")
#include "Internal/DataInterface/NiagaraDataInterfaceStaticMesh.h"
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 1
#else
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 0
#endif
#endif

namespace McpNiagaraAuthoringHandlers
{
// UE 5.7 deprecates two of the module scripts this domain inserts:
// Spawn/Initialization/InitializeParticle (superseded by .../V2/InitializeParticle)
// and Update/Forces/DragForce (superseded by Update/Forces/Drag). Inserting a
// deprecated module is what left the emitter stack reporting "The module has
// unmet dependencies". The plugin supports UE 5.0-5.8 and the successors do not
// exist on the older engines, so prefer the current path and fall back to the
// legacy one only when the current package is genuinely absent.
inline FString McpPreferredModulePath(const TCHAR* Current, const TCHAR* Legacy)
{
    const FString Package = FSoftObjectPath(Current).GetLongPackageName();
    return FPackageName::DoesPackageExist(Package) ? FString(Current) : FString(Legacy);
}

// "The module has unmet dependencies" named neither the dependency nor what to do
// about it. The module script declares both, so report them: the required id, which
// side of this module the provider has to sit on, and the engine's own description.
// Reads the path the handler recorded on the result; a no-op when it is absent.
inline void McpAnnotateUnmetDependencies(const TSharedPtr<FJsonObject>& Result)
{
    FString ScriptPath;
    if (!Result.IsValid() || !Result->TryGetStringField(TEXT("moduleScriptPath"), ScriptPath))
    {
        return;
    }
    UNiagaraScript* Script = Cast<UNiagaraScript>(FSoftObjectPath(ScriptPath).TryLoad());
    const FVersionedNiagaraScriptData* Data = Script ? Script->GetLatestScriptData() : nullptr;
    if (!Data)
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Entries;
    for (const FNiagaraModuleDependency& Dep : Data->RequiredDependencies)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("id"), Dep.Id.ToString());
        Entry->SetStringField(TEXT("mustSit"),
            Dep.Type == ENiagaraModuleDependencyType::PreDependency ? TEXT("before this module")
                                                                    : TEXT("after this module"));
        Entry->SetStringField(TEXT("description"), Dep.Description.ToString());
        Entries.Add(MakeShared<FJsonValueObject>(Entry));
    }
    if (Entries.Num() > 0)
    {
        Result->SetArrayField(TEXT("requiredDependencies"), Entries);
    }
    // Actionable, not just descriptive: a module required AFTER this one can be
    // satisfied without leaving MCP, because add_niagara_module appends. Only a
    // required-before dependency needs the editor, since nothing here reorders a
    // stack (Niagara's own reorder lives in the unexported stack view model).
    Result->SetStringField(TEXT("dependencyHint"),
        TEXT("The module was appended to the end of its stack section. For a dependency that must sit AFTER it, add that module next with add_niagara_module {systemPath, emitterName, modulePath, scriptType:\"Update\"} - appending puts it in the right place. A dependency that must sit BEFORE it has to be reordered in the Niagara editor; no MCP capability moves stack modules."));
}

struct FActionContext
{
    UMcpAutomationBridgeSubsystem* Subsystem = nullptr;
    FString RequestId;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    FString Name;
    FString Path;
    FString AssetPath;
    FString SystemPath;
    FString EmitterPath;
    FString EmitterName;
    FString SubAction;
    bool bSave = true;
    TSharedPtr<FJsonObject> Result;

    void SendError(const FString& Message, const FString& ErrorCode) const;
    void SendSuccess(bool bSuccess, const FString& Message) const;
};

bool IsStackModuleAuthoringSubAction(const FString& SubAction);

FActionContext MakeActionContext(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool ValidateCommonFields(FActionContext& Context);
bool ValidateNiagaraIdentifier(FActionContext& Context, const FString& Value, const FString& ParamName, bool bAllowDot);
FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& Obj);
FLinearColor GetColorFromJson(const TSharedPtr<FJsonObject>& Obj);

#if WITH_EDITOR
UNiagaraSystem* LoadSystemOrError(FActionContext& Context);
FNiagaraEmitterHandle* FindEmitterHandle(UNiagaraSystem* System, const FString& TargetEmitter);
bool LoadSystemAndEmitter(FActionContext& Context, UNiagaraSystem*& System, FNiagaraEmitterHandle*& Handle);
void MarkDirtyAndVerify(FActionContext& Context, UObject* Object);
UNiagaraNodeFunctionCall* AddModuleToEmitterStack(
    FNiagaraEmitterHandle* Handle,
    const FString& ModuleScriptPath,
    ENiagaraScriptUsage TargetUsage,
    const FString& SuggestedName = FString());
UNiagaraScriptSource* GetEmitterScriptSource(FNiagaraEmitterHandle* Handle);
bool EnsureScriptOutputGraph(UNiagaraScriptSource* ScriptSource, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId);
bool AddOrSetFloatUserParameter(UNiagaraSystem* System, const FString& ParamName, float Value);
bool AddOrSetBoolUserParameter(UNiagaraSystem* System, const FString& ParamName, bool Value);
bool AddOrSetVectorUserParameter(UNiagaraSystem* System, const FString& ParamName, const FVector& Value);
bool AddOrSetColorUserParameter(UNiagaraSystem* System, const FString& ParamName, const FLinearColor& Value);
bool AddDataInterfaceUserParameter(UNiagaraSystem* System, const FString& ParamName, UClass* DataInterfaceClass);
FNiagaraTypeDefinition ResolveNiagaraTypeByName(const FString& ParamType);
void CollectNiagaraSystemStackIssues(
    UNiagaraSystem* System,
    TArray<TSharedPtr<FJsonValue>>& OutErrors,
    TArray<TSharedPtr<FJsonValue>>& OutWarnings);
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
UNiagaraNodeFunctionCall* ResolveDynamicInputTargetNode(
    FActionContext& Context,
    UNiagaraGraph* Graph,
    const FString& TargetNodeId,
    const FString& InputName,
    FString& OutError,
    FString& OutErrorCode);
#endif

bool HandleSystemEmitterAction(FActionContext& Context, const FString& SubAction);
bool HandleSpawnModuleAction(FActionContext& Context, const FString& SubAction);
bool HandleDynamicsModuleAction(FActionContext& Context, const FString& SubAction);
bool HandleRendererAction(FActionContext& Context, const FString& SubAction);
bool HandleParameterAction(FActionContext& Context, const FString& SubAction);
bool HandleDynamicInputAction(FActionContext& Context, const FString& SubAction);
bool HandleDataInterfaceAction(FActionContext& Context, const FString& SubAction);
bool HandleEventAction(FActionContext& Context, const FString& SubAction);
bool HandleSimulationAction(FActionContext& Context, const FString& SubAction);
bool HandleInfoValidationAction(FActionContext& Context, const FString& SubAction);
#endif
}
