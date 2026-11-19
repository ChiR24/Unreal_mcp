#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "CoreMinimal.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EngineWireSubsystem.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterface.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraParameterStore.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"

// MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW and MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW come from FeatureDetection.json.
#if MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW
#include "NiagaraEmitterFactoryNew.h"
#endif
#if MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW
#include "NiagaraSystemFactoryNew.h"
#endif

#if MCP_HAS_NIAGARA_VERSIONED_EMITTER
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#define MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES 1
#else
#define MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES 0
#endif
// Giving a module input a value through its override pin (a dynamic input, a linked parameter) needs functions the
// engine exports only from 5.3: on 5.1 and 5.2 GetOrCreateStackFunctionInputOverridePin does not link.
#define MCP_HAS_NIAGARA_INPUT_OVERRIDES (MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES && MCP_HAS_NIAGARA_INPUT_OVERRIDE_PIN_EXPORTED)


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

struct FActionContext
{
    UEngineWireSubsystem* Subsystem = nullptr;
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
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool ValidateCommonFields(FActionContext& Context);
// PathToCheck through the shared canonicalizer (an empty one passes); a refusal names ParamName and is sent.
bool ValidateAndSanitizePath(FActionContext& Context, FString& PathToCheck, const FString& ParamName);
bool ValidateNiagaraIdentifier(FActionContext& Context, const FString& Value, const FString& ParamName, bool bAllowDot);

UNiagaraSystem* LoadSystemOrError(FActionContext& Context);
FNiagaraEmitterHandle* FindEmitterHandle(UNiagaraSystem* System, const FString& TargetEmitter);
// Context.EmitterName in System; an omitted name takes the sole emitter (emitterResolvedBy). On success names the system
// and emitter in the result and sets Context.EmitterName to the handle; on a miss sends EMITTER_NOT_FOUND listing the emitters.
FNiagaraEmitterHandle* ResolveEmitterHandle(FActionContext& Context, UNiagaraSystem* System);
// The same resolution without a reply: a miss returns nullptr with the refusal in OutError (a parameters list fails
// only the entries that need the emitter).
FNiagaraEmitterHandle* FindTargetEmitter(FActionContext& Context, UNiagaraSystem* System, FString& OutError);
bool LoadSystemAndEmitter(FActionContext& Context, UNiagaraSystem*& System, FNiagaraEmitterHandle*& Handle);
// The emitter a stack edit changes: the system, the handle, the handle's emitter object and data and, from 5.1, the
// version. Filled by LoadEmitterTarget, false after its refusal: LoadSystemAndEmitter's, or UNSUPPORTED_EMITTER for
// a handle without them (a lightweight emitter).
struct FEmitterTarget
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    UNiagaraEmitter* Emitter = nullptr;
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = nullptr;
#if MCP_HAS_NIAGARA_VERSIONED_EMITTER
    FVersionedNiagaraEmitter VersionedEmitter;
#endif
};
bool LoadEmitterTarget(FActionContext& Context, FEmitterTarget& Target);
// A graph or stack edit persists: dirty the system, then save it unless the caller sent save:false.
// Dirtying alone let every module add, removal and wire vanish at the next editor start.
inline void SaveNiagaraGraphEdit(UNiagaraSystem* System, const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& Result)
{
    // Compiled too: until then the system ran its old scripts and a module just added had no inputs to list or set.
    System->RequestCompile(false);
    System->MarkPackageDirty();
    if (GetJsonBoolField(Payload, TEXT("save"), true))
    {
        Result->SetBoolField(TEXT("saved"), McpSafeOperations::McpSafeAssetSave(System));
    }
}
// SaveNiagaraGraphEdit for the action's system, then the verification block.
void MarkDirtyAndVerify(FActionContext& Context, UNiagaraSystem* System);
UNiagaraNodeFunctionCall* AddModuleToEmitterStack(
    FNiagaraEmitterHandle* Handle,
    const FString& ModuleScriptPath,
    ENiagaraScriptUsage TargetUsage,
    const FString& SuggestedName = FString());
UNiagaraScriptSource* GetEmitterScriptSource(FNiagaraEmitterHandle* Handle);
// False after refusing UNSUPPORTED_VERSION on UE 5.0, which has no stack utilities to add a module with (every add
// returned null there and was answered as that one module failing).
inline bool StackEditsAvailableOrRefuse(FActionContext& Context)
{
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
    return true;
#else
    Context.SendError(TEXT("Adding a module to an emitter stack needs UE 5.1 or later; 5.0 has no Niagara stack utilities. "
        "Nothing was changed."), TEXT("UNSUPPORTED_VERSION"));
    return false;
#endif
}
bool EnsureScriptOutputGraph(UNiagaraScriptSource* ScriptSource, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId);
bool AddDataInterfaceUserParameter(UNiagaraSystem* System, const FString& ParamName, UClass* DataInterfaceClass);
// The system's user parameter named ParamName, of any type ("User." is added to a bare name); null when it has none.
const FNiagaraVariableWithOffset* FindUserParameter(UNiagaraSystem* System, const FString& ParamName);
// A parameter's value in its own type: a number, a bool, or an array for a vector or colour; null for other types.
TSharedPtr<FJsonValue> ReadNiagaraValue(const FNiagaraParameterStore& Store, const FNiagaraVariable& Variable);
// With actorName, puts the placed effect's own user parameter values (and the ones it overrides) under placed; false
// after replying ACTOR_NOT_FOUND or COMPONENT_NOT_FOUND. Without actorName it does nothing and returns true.
bool AddPlacedUserParameters(FActionContext& Context, UNiagaraSystem* System);
// ParamType (Float, Int, Bool, Vector or Color) as a Niagara type in OutType; false after refusing any other name.
bool ResolveNiagaraTypeOrRefuse(FActionContext& Context, const FString& ParamType, FNiagaraTypeDefinition& OutType);
// False after refusing NAME_TAKEN when the system already has a user parameter of that name, of any type.
bool UserParameterNameFree(FActionContext& Context, UNiagaraSystem* System, const FString& ParamName);
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
// In McpNiagaraStackGraph.cpp: writes Value (an enum entry by display name, name or value; a bool; an int) to the
// switch ParamName ("InitializeParticle.Ribbon Width Mode") on every match in the named emitter, or in the system
// and every emitter, and returns the count written; OutError names what the switch takes when Value fits none.
int32 SetModuleStaticSwitch(UNiagaraSystem* System, const FString& EmitterName, const FString& ParamName, const TSharedPtr<FJsonValue>& Value, FString& OutError);
void CollectNiagaraSystemStackIssues(
    UNiagaraSystem* System,
    TArray<TSharedPtr<FJsonValue>>& OutErrors,
    TArray<TSharedPtr<FJsonValue>>& OutWarnings);
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
// In McpNiagaraModuleInfo.cpp: EmitterObj's modules and moduleCount, and in its moduleInputs each input wired to a
// parameter or a dynamic input as {linkedTo}, which the literal stored for the input does not show.
void AddEmitterModuleInfo(TSharedPtr<FJsonObject>& EmitterObj, const FNiagaraEmitterHandle& Handle);
#endif
// In McpNiagaraEmitterCopy.cpp: add_emitter with sourceSystemPath + sourceEmitterName copies that system's emitter
// (modules, values, renderers, bindings) and the user parameters it reads, as the editor's paste does.
bool CopyEmitterFromSystem(FActionContext& Context);
// In McpNiagaraModuleInfo.cpp: drops EmitterObj's moduleInputs and staticSwitches of modules not named (any case);
// no names keeps everything.
void KeepModuleEntries(const TSharedPtr<FJsonObject>& EmitterObj, const TArray<FString>& ModuleNames);
#if MCP_HAS_NIAGARA_INPUT_OVERRIDES
// The last "."-separated part of an input name ("Module.Lifetime" -> "Lifetime").
FString BareInputName(const FString& InputName);
// The input of Module that InputName names: the bare name, the stored "Module.<Name>" form or the stack's aliased
// "<ModuleName>.<Name>" form, case ignored. False when the module has no such input (or no graph).
bool FindModuleInput(UNiagaraNodeFunctionCall* Module, const FString& InputName, FNiagaraVariable& OutInput);
// Every input FindModuleInput can find on Module, comma-separated, for an error that names the right spellings.
FString ListModuleInputs(UNiagaraNodeFunctionCall* Module);
// The dynamic-input call that "<FunctionName>.<Input>" names (RandomRangeFloat.Minimum), with the input; null if none.
UNiagaraNodeFunctionCall* FindDynamicInputCallInput(UNiagaraGraph* Graph, const FString& InputName, FNiagaraVariable& OutInput);
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
// set_parameter_value: one parameter, or every entry of a parameters list. In McpNiagaraParamValues.cpp.
bool SetParameterValue(FActionContext& Context);
// In McpNiagaraInputOwner.cpp. The emitter a module input or static switch ParamName is written in when the caller
// names none: the one emitter it matches in (a full "Constants.<Emitter>.<Module>.<Input>" names its own), or empty
// for the system stage or no match at all. Only a name that matches in more than one place needs emitterName: then
// false, with OutError a short "needs emitterName: one of [...]" (one parameters-list entry's reason).
bool FindInputEmitter(UNiagaraSystem* System, const FString& ParamName, FString& OutEmitter, FString& OutError);
// The same for one set_parameter_value call: sets Context.EmitterName and names it in the result, or sends
// EMITTER_NOT_FOUND ("Nothing was changed.") and returns false.
bool ResolveInputEmitter(FActionContext& Context, UNiagaraSystem* System, const FString& ParamName);
// The system's emitter handle names, comma-separated.
FString ListEmitterNames(UNiagaraSystem* System);
// Writes Payload's parameterValue to every rapid-iteration copy of the module input ParamName
// ("SpawnRate.SpawnRate"), in EmitterName's scripts or, empty, every script's; returns the copies written.
int32 SetModuleInputValue(UNiagaraSystem* System, const FString& EmitterName, const FString& ParamName, const TSharedPtr<FJsonObject>& Payload, TArray<FString>& Candidates, FString& MatchedType);
bool HandleDynamicInputAction(FActionContext& Context, const FString& SubAction);
bool HandleDataInterfaceAction(FActionContext& Context, const FString& SubAction);
bool HandleEventAction(FActionContext& Context, const FString& SubAction);
bool HandleSimulationAction(FActionContext& Context, const FString& SubAction);
bool HandleInfoValidationAction(FActionContext& Context, const FString& SubAction);
}
