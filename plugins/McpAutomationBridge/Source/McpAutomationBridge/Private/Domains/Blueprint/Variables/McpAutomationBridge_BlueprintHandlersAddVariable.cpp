#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"
// Supplies MCP_BLUEPRINT_ACTION_LOCALS, which declares RequestId /
// RequestingSocket / LocalPayload / Bridge for every handler in this file. Every
// other Blueprint handler that uses the macro includes this; omitting it here
// compiled only by transitive luck and fails outright under an installed engine.
#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/Blueprint/Variables/McpAutomationBridge_BlueprintVariableObjectDefault.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/ScopeExit.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintAddVariable(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("add_variable"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_add_variable handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_add_variable requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    FString VarName;
    LocalPayload->TryGetStringField(TEXT("variableName"), VarName);
    if (VarName.TrimStartAndEnd().IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("variableName required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString VarType;
    LocalPayload->TryGetStringField(TEXT("variableType"), VarType);
    const TSharedPtr<FJsonValue> DefaultVal =
        LocalPayload->TryGetField(TEXT("defaultValue"));
    FString Category;
    LocalPayload->TryGetStringField(TEXT("category"), Category);
    const bool bReplicated =
        LocalPayload->HasField(TEXT("isReplicated"))
            ? GetJsonBoolField(LocalPayload, TEXT("isReplicated"))
            : false;
    const bool bPublic = LocalPayload->HasField(TEXT("isPublic"))
                             ? GetJsonBoolField(LocalPayload, TEXT("isPublic"))
                             : false;

    // Validate variableType BEFORE checking existence to ensure parameter
    // validation occurs even if variable already exists. Route through the
    // shared resolver so struct/enum/container/soft-ref type specs all work
    // and unknown or malformed specs fail with a structured error instead of
    // silently falling back to another type.
    const McpBlueprintUtils::FTypeResolutionResult VarTypeResolved =
        McpBlueprintUtils::ResolvePinType(VarType);
    if (!VarTypeResolved.bSuccess) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Type '%s' rejected: %s"), *VarType,
                          *VarTypeResolved.OutError),
          TEXT("TYPE_RESOLUTION_FAILED"));
      return true;
    }
    const FEdGraphPinType PinType = VarTypeResolved.PinType;

    const FString RequestedPath = Path;
    FString RegKey = Path;
    FString NormPath;
    if (FindBlueprintNormalizedPath(Path, NormPath) &&
        !NormPath.TrimStartAndEnd().IsEmpty()) {
      RegKey = NormPath;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: blueprint_add_variable start "
                "RequestId=%s Path=%s VarName=%s"),
           *RequestId, *RequestedPath, *VarName);


    FString LocalNormalized;
    FString LocalLoadError;
    UBlueprint *Blueprint =
        LoadBlueprintAsset(RequestedPath, LocalNormalized, LocalLoadError);
    if (!Blueprint) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleBlueprintAction: failed to load "
                  "blueprint_add_variable target %s (%s)"),
             *RegKey, *LocalLoadError);
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          LocalLoadError.IsEmpty()
                              ? TEXT("Failed to load blueprint")
                              : LocalLoadError,
                          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    const FString RegistryKey =
        !LocalNormalized.IsEmpty() ? LocalNormalized : RequestedPath;

    // PinType was already validated before loading the blueprint

    const FBPVariableDescription *ExistingVar = nullptr;
    for (const FBPVariableDescription &Existing : Blueprint->NewVariables) {
      if (Existing.VarName == FName(*VarName)) {
        ExistingVar = &Existing;
        break;
      }
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    // The asset is named once, by the verification fields (assetPath); variable{} is the variable as the Blueprint
    // holds it, so the request is not echoed beside it.
    Response->SetStringField(TEXT("variableName"), VarName);

    if (ExistingVar) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Log,
          TEXT("HandleBlueprintAction: variable '%s' already exists in '%s'"),
          *VarName, *RegistryKey);
      // Only this variable's entry: the whole snapshot (every component) made
      // each add_variable reply several KB.
      Response->SetObjectField(
          TEXT("variable"), FMcpAutomationBridge_BuildVariableJson(Blueprint, *ExistingVar));
      Response->SetBoolField(TEXT("success"), true);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Variable already exists"), Response,
                             FString());
      return true;
    }

    // A name the parent class already uses cannot be a new variable: the
    // compiler renames the new one, and the reply used to say only "Variable
    // add verification failed". Name the inherited property and the way out.
    const FProperty *Inherited = Blueprint->ParentClass
        ? FindFProperty<FProperty>(Blueprint->ParentClass.Get(), FName(*VarName)) : nullptr;
    if (Inherited) {
      const UClass *Owner = Inherited->GetOwnerClass();
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("'%s' is already a property of the parent class %s (declared on %s, type %s), "
                               "so it cannot be added as a new variable. Use the inherited property "
                               "(set its default with edit_variable set_default, propertyName '%s'), "
                               "or pick another variableName."),
                          *VarName, *Blueprint->ParentClass->GetName(),
                          Owner ? *Owner->GetName() : *Blueprint->ParentClass->GetName(),
                          *Inherited->GetCPPType(), *Inherited->GetName()),
          TEXT("VARIABLE_NAME_CONFLICT"));
      return true;
    }

    Blueprint->Modify();

    FBPVariableDescription NewVar;
    NewVar.VarName = FName(*VarName);
    NewVar.VarGuid = FGuid::NewGuid();
    NewVar.FriendlyName = VarName;
    if (!Category.IsEmpty()) {
      NewVar.Category = FText::FromString(Category);
    } else {
      NewVar.Category = FText::GetEmpty();
    }
    NewVar.VarType = PinType;
    NewVar.PropertyFlags |= CPF_Edit;
    NewVar.PropertyFlags |= CPF_BlueprintVisible;
    NewVar.PropertyFlags &= ~CPF_BlueprintReadOnly;
    if (bReplicated) {
      NewVar.PropertyFlags |= CPF_Net;
    }
    // isPublic is the editor's eye toggle (Instance Editable). It was read and
    // then dropped, so isPublic:false still made every variable public. Only an
    // explicit false turns it off; callers that omit it keep the old default.
    if (LocalPayload->HasField(TEXT("isPublic")) && !bPublic) {
      NewVar.PropertyFlags |= CPF_DisableEditOnInstance;
    }
    // Expose on Spawn makes the variable a pin on SpawnActor and Create Widget nodes for this class. A batch step
    // that asked for it was silently ignored, so the later SpawnActor node had no pin to wire. The editor only
    // exposes an instance-editable variable, so the flag implies that too.
    const bool bExposeOnSpawn = GetJsonBoolField(LocalPayload, TEXT("exposeOnSpawn"));
    if (bExposeOnSpawn) {
      NewVar.PropertyFlags &= ~CPF_DisableEditOnInstance;
      NewVar.SetMetaData(FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
    }
    // Every default is written, and checked, after the first compile: an object
    // or array ({x,y,z}, a color, a list) has no string form until the property
    // exists, and the compiler only warns on scalar text it cannot parse.
    const bool bHasDefault = DefaultVal.IsValid() && DefaultVal->Type != EJson::Null;
    McpJsonScalarToString(DefaultVal, NewVar.DefaultValue);

    Blueprint->NewVariables.Add(NewVar);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);

    // Matched by guid: a name that collides with another member (a component, a
    // function) is renamed by the compiler. All or nothing: a renamed variable,
    // or one whose default does not fit, is removed again before the reply.
    auto FindAdded = [&Blueprint, &NewVar]() {
      return Blueprint->NewVariables.FindByPredicate(
          [&NewVar](const FBPVariableDescription &Var) { return Var.VarGuid == NewVar.VarGuid; });
    };
    const FBPVariableDescription *Compiled = FindAdded();
    const bool bRenamed = !Compiled || Compiled->VarName != NewVar.VarName;
    FString DefaultError;
    if (bRenamed || (bHasDefault && !McpApplyVariableDefault(Blueprint, NewVar.VarName, DefaultVal, DefaultError))) {
      if (const FBPVariableDescription *Added = FindAdded()) {
        const FName AddedName = Added->VarName;
        FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, AddedName);
        McpSafeCompileBlueprint(Blueprint);
      }
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          bRenamed ? FString::Printf(TEXT("'%s' collides with an existing member (a component, function or event), so "
                                          "it was not added. Pick another variableName."), *VarName)
                   : FString::Printf(TEXT("Variable '%s' was not added: %s. Give a defaultValue its type accepts, or "
                                          "omit it."), *VarName, *DefaultError),
          bRenamed ? TEXT("VARIABLE_NAME_CONFLICT") : TEXT("DEFAULT_NOT_APPLIED"));
      return true;
    }
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);
    const FBPVariableDescription *AddedVar = FindAdded();

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: variable '%s' added to '%s' (saved=%s "
                "verified=true)"),
           *VarName, *RegistryKey, bSaved ? TEXT("true") : TEXT("false"));

    Response->SetBoolField(TEXT("success"), true);
    Response->SetBoolField(TEXT("saved"), bSaved);
    Response->SetObjectField(TEXT("variable"),
                             FMcpAutomationBridge_BuildVariableJson(Blueprint, *AddedVar));
    // Add verification data for the blueprint asset
    McpHandlerUtils::AddVerification(Response, Blueprint);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Variable added"), Response, FString());
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
