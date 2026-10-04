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

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_NIAGARA_EMITTER_DATA_TYPE FVersionedNiagaraEmitterData
#define MCP_GET_EMITTER_DATA(Handle) (Handle).GetEmitterData()
#define MCP_GET_LATEST_EMITTER_DATA(Emitter) (Emitter)->GetLatestEmitterData()
#else
#define MCP_NIAGARA_EMITTER_DATA_TYPE UNiagaraEmitter
#define MCP_GET_EMITTER_DATA(Handle) (&(Handle))->GetInstance()
#define MCP_GET_LATEST_EMITTER_DATA(Emitter) (Emitter)
#endif

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
// Giving a module input a value through its override pin (a dynamic input, a linked parameter) needs functions the
// engine exports only from 5.3: on 5.1 and 5.2 GetOrCreateStackFunctionInputOverridePin does not link.
#define MCP_HAS_NIAGARA_INPUT_OVERRIDES (MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES && ENGINE_MINOR_VERSION >= 3)

#include "NiagaraDataInterfaceSkeletalMesh.h"

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
bool AddDataInterfaceUserParameter(UNiagaraSystem* System, const FString& ParamName, UClass* DataInterfaceClass);
FNiagaraTypeDefinition ResolveNiagaraTypeByName(const FString& ParamType);
// Every script whose rapid-iteration store holds module inputs: the system spawn/update scripts
// (emitter-stage modules are mirrored there) plus each emitter's own scripts.
inline TArray<UNiagaraScript*> GatherModuleInputScripts(UNiagaraSystem* System)
{
    TArray<UNiagaraScript*> Scripts{System->GetSystemSpawnScript(), System->GetSystemUpdateScript()};
    for (FNiagaraEmitterHandle& Handle : MCP_MUTABLE_EMITTER_HANDLES(System))
    {
        if (MCP_NIAGARA_EMITTER_DATA_TYPE* Data = MCP_GET_EMITTER_DATA(Handle))
        {
            Data->GetScripts(Scripts, false);
        }
    }
    return Scripts;
}
// A module's static switch ("Ribbon Width Mode", "Write Color") is no rapid-iteration parameter: it is a
// not-connectable input pin of the module's call node, named after the switch, on every engine 5.0-5.8.
inline bool IsStaticSwitchPin(const UEdGraphPin* Pin)
{
    return Pin && Pin->Direction == EGPD_Input && Pin->bNotConnectable && !Pin->bOrphanedPin;
}
// "NiagaraBool" or "NiagaraInt32" for those switch types (an enum switch carries its UEnum instead).
inline FName PinStructName(const UEdGraphPin& Pin)
{
    const UObject* Struct = Pin.PinType.PinSubCategoryObject.Get();
    return Struct ? Struct->GetFName() : NAME_None;
}
// In ...HandlersStackGraph.cpp: writes Value (an enum entry by display name, name or value; a bool; an int) to the
// switch ParamName ("InitializeParticle.Ribbon Width Mode") on every match in the named emitter, or in the system
// and every emitter, and returns the count written; OutError names what the switch takes when Value fits none.
int32 SetModuleStaticSwitch(UNiagaraSystem* System, const FString& EmitterName, const FString& ParamName, const TSharedPtr<FJsonValue>& Value, FString& OutError);
void CollectNiagaraSystemStackIssues(
    UNiagaraSystem* System,
    TArray<TSharedPtr<FJsonValue>>& OutErrors,
    TArray<TSharedPtr<FJsonValue>>& OutWarnings);
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
// In ...HandlersModuleInfo.cpp: EmitterObj's modules and moduleCount, and in its moduleInputs each input wired to a
// parameter or a dynamic input as {linkedTo}, which the literal stored for the input does not show.
void AddEmitterModuleInfo(TSharedPtr<FJsonObject>& EmitterObj, const FNiagaraEmitterHandle& Handle);
#endif
#if MCP_HAS_NIAGARA_INPUT_OVERRIDES
// The last "."-separated part of an input name ("Module.Lifetime" -> "Lifetime").
FString BareInputName(const FString& InputName);
// The input of Module that InputName names: the bare name, the stored "Module.<Name>" form or the stack's aliased
// "<ModuleName>.<Name>" form, case ignored. False when the module has no such input (or no graph).
bool FindModuleInput(UNiagaraNodeFunctionCall* Module, const FString& InputName, FNiagaraVariable& OutInput);
// Every input FindModuleInput can find on Module, comma-separated, for an error that names the right spellings.
FString ListModuleInputs(UNiagaraNodeFunctionCall* Module);
UNiagaraNodeFunctionCall* ResolveDynamicInputTargetNode(
    FActionContext& Context,
    UNiagaraGraph* Graph,
    const FString& TargetNodeId,
    const FString& InputName,
    FString& OutError,
    FString& OutErrorCode);
#endif

bool HandleSystemEmitterAction(FActionContext& Context, const FString& SubAction);
bool HandleFixedModuleAction(FActionContext& Context, const FString& SubAction);
bool HandleRendererAction(FActionContext& Context, const FString& SubAction);
bool HandleParameterAction(FActionContext& Context, const FString& SubAction);
// set_parameter_value: one parameter, or every entry of a parameters list. In ...HandlersParameterValues.cpp.
bool SetParameterValue(FActionContext& Context);
// Writes Payload's parameterValue to every rapid-iteration copy of the module input ParamName
// ("SpawnRate.SpawnRate"), in EmitterName's scripts or, empty, every script's; returns the copies written.
int32 SetModuleInputValue(UNiagaraSystem* System, const FString& EmitterName, const FString& ParamName, const TSharedPtr<FJsonObject>& Payload, TArray<FString>& Candidates, FString& MatchedType);
bool HandleDynamicInputAction(FActionContext& Context, const FString& SubAction);
bool HandleDataInterfaceAction(FActionContext& Context, const FString& SubAction);
bool HandleEventAction(FActionContext& Context, const FString& SubAction);
bool HandleSimulationAction(FActionContext& Context, const FString& SubAction);
bool HandleInfoValidationAction(FActionContext& Context, const FString& SubAction);
}
