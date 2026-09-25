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

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintAddVariable(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_add_variable")) ||
      ActionMatchesPattern(TEXT("add_variable")) ||
      AlphaNumLower.Contains(TEXT("blueprintaddvariable")) ||
      AlphaNumLower.Contains(TEXT("addvariable"))) {
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

    if (GBlueprintBusySet.Contains(RegKey)) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Blueprint %s is busy"), *RegKey),
          TEXT("BLUEPRINT_BUSY"));
      return true;
    }

    GBlueprintBusySet.Add(RegKey);
    ON_SCOPE_EXIT {
      if (GBlueprintBusySet.Contains(RegKey)) {
        GBlueprintBusySet.Remove(RegKey);
      }
    };

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
    Response->SetStringField(TEXT("blueprintPath"), RegistryKey);
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
      Response->SetStringField(
          TEXT("note"), TEXT("Variable already exists; no changes applied."));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Variable already exists"), Response,
                             FString());
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
    TSharedPtr<FJsonValue> ObjectDefault;

    // Apply the requested default value. FBPVariableDescription stores the
    // default as a string (the same form the editor's "Default Value" field
    // serializes to); UE parses it back into the typed default on compile.
    // Previously the payload's defaultValue was read but never applied, so
    // every variable was created with a zero/empty default regardless of the
    // value supplied (e.g. a float HealPct requested as 0.35 stayed 0).
    if (DefaultVal.IsValid() && DefaultVal->Type != EJson::Null) {
      FString DefaultStr;
      switch (DefaultVal->Type) {
      case EJson::Boolean:
        DefaultStr = DefaultVal->AsBool() ? TEXT("true") : TEXT("false");
        break;
      case EJson::Number: {
        const double Num = DefaultVal->AsNumber();
        const bool bIsIntLike =
            PinType.PinCategory == UEdGraphSchema_K2::PC_Int ||
            PinType.PinCategory == UEdGraphSchema_K2::PC_Byte;
        if (bIsIntLike && FMath::Frac(Num) == 0.0) {
          DefaultStr = FString::Printf(TEXT("%lld"), static_cast<int64>(Num));
        } else {
          DefaultStr = FString::SanitizeFloat(Num);
        }
        break;
      }
      case EJson::String:
        DefaultStr = DefaultVal->AsString();
        break;
      default:
        // An object or array ({x,y,z}, a color, a list) has no string form
        // until the property exists, so it is written after the first compile.
        ObjectDefault = DefaultVal;
        break;
      }
      NewVar.DefaultValue = DefaultStr;
    }

    Blueprint->NewVariables.Add(NewVar);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    FString ObjectDefaultError;
    if (ObjectDefault.IsValid() &&
        !McpApplyVariableObjectDefault(Blueprint, NewVar.VarName, ObjectDefault,
                                       ObjectDefaultError)) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Variable '%s' was added without its defaultValue: %s. Set it "
                               "with edit_variable set_default."), *VarName, *ObjectDefaultError),
          TEXT("DEFAULT_NOT_APPLIED"));
      return true;
    }
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

    // Verify against the variable list (the compiled class may lag a compile);
    // the entry found is also what the reply reports.
    const FBPVariableDescription *AddedVar = nullptr;
    for (const FBPVariableDescription &Var : Blueprint->NewVariables) {
      if (Var.VarName == NewVar.VarName) {
        AddedVar = &Var;
      }
    }

    if (!AddedVar) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
             TEXT("HandleBlueprintAction: variable '%s' added but verification "
                  "failed in '%s'"),
             *VarName, *RegistryKey);
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(
          TEXT("error"),
          TEXT("Verification failed: variable not found after add"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Variable add verification failed"), Err,
                             TEXT("VERIFICATION_FAILED"));
      return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: variable '%s' added to '%s' (saved=%s "
                "verified=true)"),
           *VarName, *RegistryKey, bSaved ? TEXT("true") : TEXT("false"));

    Response->SetBoolField(TEXT("success"), true);
    Response->SetBoolField(TEXT("saved"), bSaved);
    if (!VarType.IsEmpty()) {
      Response->SetStringField(TEXT("variableType"), VarType);
    }
    if (!Category.IsEmpty()) {
      Response->SetStringField(TEXT("category"), Category);
    }
    Response->SetBoolField(TEXT("replicated"), bReplicated);
    Response->SetBoolField(TEXT("public"),
                           (AddedVar->PropertyFlags & CPF_DisableEditOnInstance) == 0);
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
#endif
} // namespace McpBlueprintHandlers
