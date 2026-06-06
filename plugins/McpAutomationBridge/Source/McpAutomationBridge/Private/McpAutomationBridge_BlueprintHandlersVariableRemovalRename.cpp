#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintRemoveRenameVariable(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_remove_variable")) ||
      AlphaNumLower.Contains(TEXT("blueprintremovevariable"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_remove_variable handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_remove_variable requires a blueprint path."), nullptr,
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

#if WITH_EDITOR
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: blueprint_remove_variable start "
                "RequestId=%s Path=%s VarName=%s"),
           *RequestId, *Path, *VarName);

    FString LocalNormalized;
    FString LocalLoadError;
    UBlueprint *Blueprint =
        LoadBlueprintAsset(Path, LocalNormalized, LocalLoadError);
    if (!Blueprint) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             LocalLoadError.IsEmpty()
                                 ? TEXT("Failed to load blueprint")
                                 : LocalLoadError,
                             nullptr, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    const FName TargetVarName(*VarName);
    int32 VarIndex = -1;
    for (int32 i = 0; i < Blueprint->NewVariables.Num(); ++i) {
      if (Blueprint->NewVariables[i].VarName == TargetVarName) {
        VarIndex = i;
        break;
      }
    }

    if (VarIndex == INDEX_NONE) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Variable '%s' not found in blueprint."),
                          *VarName),
          nullptr, TEXT("NOT_FOUND"));
      return true;
    }

    FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, TargetVarName);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: variable '%s' removed from '%s' "
                "(saved=%s)"),
           *VarName, *Path, bSaved ? TEXT("true") : TEXT("false"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("variableName"), VarName);
    Result->SetStringField(TEXT("blueprintPath"), LocalNormalized);
    // Add verification data for the blueprint asset
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Variable removed successfully"), Result);
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_remove_variable requires editor build"), nullptr,
        TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (ActionMatchesPattern(TEXT("blueprint_rename_variable")) ||
      AlphaNumLower.Contains(TEXT("blueprintrenamevariable"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_rename_variable handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_rename_variable requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    FString OldName;
    LocalPayload->TryGetStringField(TEXT("oldName"), OldName);
    FString NewName;
    LocalPayload->TryGetStringField(TEXT("newName"), NewName);

    if (OldName.IsEmpty() || NewName.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Missing 'oldName' or 'newName' in payload."),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

#if WITH_EDITOR
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: blueprint_rename_variable start "
                "RequestId=%s Path=%s OldName=%s NewName=%s"),
           *RequestId, *Path, *OldName, *NewName);

    FString LocalNormalized;
    FString LocalLoadError;
    UBlueprint *Blueprint =
        LoadBlueprintAsset(Path, LocalNormalized, LocalLoadError);
    if (!Blueprint) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             LocalLoadError.IsEmpty()
                                 ? TEXT("Failed to load blueprint")
                                 : LocalLoadError,
                             nullptr, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    const FName OldVarName(*OldName);
    bool bFound = false;
    for (const FBPVariableDescription &Var : Blueprint->NewVariables) {
      if (Var.VarName == OldVarName) {
        bFound = true;
        break;
      }
    }

    if (!bFound) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Variable '%s' not found in blueprint."),
                          *OldName),
          nullptr, TEXT("NOT_FOUND"));
      return true;
    }

    FBlueprintEditorUtils::RenameMemberVariable(Blueprint, OldVarName,
                                                FName(*NewName));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: variable renamed from '%s' to '%s' in "
                "'%s' (saved=%s)"),
           *OldName, *NewName, *Path, bSaved ? TEXT("true") : TEXT("false"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("oldName"), OldName);
    Result->SetStringField(TEXT("newName"), NewName);
    Result->SetStringField(TEXT("blueprintPath"), LocalNormalized);
    // Add verification data for the blueprint asset
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Variable renamed successfully"), Result);
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_rename_variable requires editor build"), nullptr,
        TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  // Add an event to the blueprint (synchronous editor implementation)
  return false;
}
#endif
} // namespace McpBlueprintHandlers
