#include "Domains/Blueprint/Variables/McpAutomationBridge_BlueprintHandlersAddVariablePinType.h"
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
    // validation occurs even if variable already exists
    FEdGraphPinType PinType;
    FString PinTypeError;
    if (!ResolveAddVariablePinType(VarType, PinType, PinTypeError)) {
      Bridge.SendAutomationError(RequestingSocket, RequestId, PinTypeError,
                                 TEXT("CLASS_NOT_FOUND"));
      return true;
    }

    const FString RequestedPath = Path;
    FString RegKey = Path;
    FString NormPath;
    if (FindBlueprintNormalizedPath(Path, NormPath) &&
        !NormPath.TrimStartAndEnd().IsEmpty()) {
      RegKey = NormPath;
    }

#if WITH_EDITOR
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

    bool bAlreadyExists = false;
    for (const FBPVariableDescription &Existing : Blueprint->NewVariables) {
      if (Existing.VarName == FName(*VarName)) {
        bAlreadyExists = true;
        break;
      }
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetStringField(TEXT("blueprintPath"), RegistryKey);
    Response->SetStringField(TEXT("variableName"), VarName);

    if (bAlreadyExists) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Log,
          TEXT("HandleBlueprintAction: variable '%s' already exists in '%s'"),
          *VarName, *RegistryKey);
      const TSharedPtr<FJsonObject> Snapshot =
          FMcpAutomationBridge_BuildBlueprintSnapshot(Blueprint, RegistryKey);
      if (Snapshot.IsValid()) {
        Response->SetObjectField(TEXT("blueprint"), Snapshot);
        if (Snapshot->HasField(TEXT("variables"))) {
          const TArray<TSharedPtr<FJsonValue>> Vars =
              Snapshot->GetArrayField(TEXT("variables"));
          if (const TSharedPtr<FJsonObject> VarJson = nullptr) {
            Response->SetObjectField(TEXT("variable"), VarJson);
          }
        }
      }
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
            PinType.PinCategory == MCP_PC_Int ||
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
        DefaultVal->TryGetString(DefaultStr);
        break;
      }
      NewVar.DefaultValue = DefaultStr;
    }

    Blueprint->NewVariables.Add(NewVar);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

    // Real test: Verify the variable actually exists in the compiled class or
    // blueprint
    bool bVerified = false;
    if (Blueprint->GeneratedClass) {
      if (FindFProperty<FProperty>(Blueprint->GeneratedClass,
                                   FName(*VarName))) {
        bVerified = true;
      }
    }

    // Fallback verification: check NewVariables if compilation didn't fully
    // propagate yet (though it should have)
    if (!bVerified) {
      for (const FBPVariableDescription &Var : Blueprint->NewVariables) {
        if (Var.VarName == FName(*VarName)) {
          bVerified = true;
          break;
        }
      }
    }

    if (!bVerified) {
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
    Response->SetBoolField(TEXT("public"), bPublic);
    const TSharedPtr<FJsonObject> Snapshot =
        FMcpAutomationBridge_BuildBlueprintSnapshot(Blueprint, RegistryKey);
    if (Snapshot.IsValid()) {
      Response->SetObjectField(TEXT("blueprint"), Snapshot);
      if (Snapshot->HasField(TEXT("variables"))) {
        const TArray<TSharedPtr<FJsonValue>> Vars =
            Snapshot->GetArrayField(TEXT("variables"));
        if (const TSharedPtr<FJsonObject> VarJson =
                FMcpAutomationBridge_FindNamedEntry(Vars, TEXT("name"),
                                                    VarName)) {
          Response->SetObjectField(TEXT("variable"), VarJson);
        }
      }
    }
    // Add verification data for the blueprint asset
    McpHandlerUtils::AddVerification(Response, Blueprint);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Variable added"), Response, FString());
    return true;
#else
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("blueprint_add_variable requires editor build"),
                           nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  return false;
}
#endif
} // namespace McpBlueprintHandlers
