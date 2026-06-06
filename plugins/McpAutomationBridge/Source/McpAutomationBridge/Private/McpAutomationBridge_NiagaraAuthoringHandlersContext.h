#pragma once

#include "McpVersionCompatibility.h"
#include "CoreMinimal.h"
#include "McpHandlerUtils.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
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
    bool bSave = true;
    TSharedPtr<FJsonObject> Result;

    void SendError(const FString& Message, const FString& ErrorCode) const;
    void SendSuccess(bool bSuccess, const FString& Message) const;
};

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

bool HandleSystemEmitterAction(FActionContext& Context, const FString& SubAction);
bool HandleSpawnModuleAction(FActionContext& Context, const FString& SubAction);
bool HandleDynamicsModuleAction(FActionContext& Context, const FString& SubAction);
bool HandleRendererAction(FActionContext& Context, const FString& SubAction);
bool HandleParameterAction(FActionContext& Context, const FString& SubAction);
bool HandleDataInterfaceAction(FActionContext& Context, const FString& SubAction);
bool HandleEventAction(FActionContext& Context, const FString& SubAction);
bool HandleSimulationAction(FActionContext& Context, const FString& SubAction);
bool HandleInfoValidationAction(FActionContext& Context, const FString& SubAction);
#endif
}
