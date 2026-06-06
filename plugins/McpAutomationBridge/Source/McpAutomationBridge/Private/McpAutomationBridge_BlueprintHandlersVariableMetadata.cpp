#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpConnectionManager.h"
#include "McpHandlerUtils.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintSetVariableMetadata(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_set_variable_metadata")) ||
      ActionMatchesPattern(TEXT("set_variable_metadata")) ||
      AlphaNumLower.Contains(TEXT("blueprintsetvariablemetadata")) ||
      AlphaNumLower.Contains(TEXT("setvariablemetadata"))) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Verbose,
        TEXT("Entered blueprint_set_variable_metadata handler: RequestId=%s"),
        *RequestId);
#if WITH_EDITOR
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_set_variable_metadata requires a blueprint path."),
          nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    FString VarName;
    LocalPayload->TryGetStringField(TEXT("variableName"), VarName);
    if (VarName.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("variableName required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const TSharedPtr<FJsonValue> MetaVal =
        LocalPayload->TryGetField(TEXT("metadata"));
    const TSharedPtr<FJsonObject> MetaObjPtr =
        MetaVal.IsValid() && MetaVal->Type == EJson::Object
            ? MetaVal->AsObject()
            : nullptr;
    if (!MetaObjPtr.IsValid()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("metadata object required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (GBlueprintBusySet.Contains(Path)) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint is busy"), nullptr,
                             TEXT("BLUEPRINT_BUSY"));
      return true;
    }

    GBlueprintBusySet.Add(Path);
    ON_SCOPE_EXIT {
      if (GBlueprintBusySet.Contains(Path)) {
        GBlueprintBusySet.Remove(Path);
      }
    };

    FString Normalized;
    FString LoadErr;
    UBlueprint *Blueprint = LoadBlueprintAsset(Path, Normalized, LoadErr);
    if (!Blueprint) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      if (!LoadErr.IsEmpty()) {
        Err->SetStringField(TEXT("error"), LoadErr);
      }
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to load blueprint"), Err,
                             TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    const FString RegistryKey = Normalized.IsEmpty() ? Path : Normalized;

    // Find the variable description (case-insensitive)
    FBPVariableDescription *VariableDesc = nullptr;
    for (FBPVariableDescription &Desc : Blueprint->NewVariables) {
      if (Desc.VarName == FName(*VarName)) {
        VariableDesc = &Desc;
        break;
      }
      if (Desc.VarName.ToString().Equals(VarName, ESearchCase::IgnoreCase)) {
        VariableDesc = &Desc;
        VarName = Desc.VarName.ToString();
        break;
      }
    }

    if (!VariableDesc) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), TEXT("Variable not found"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Variable not found"), Err,
                             TEXT("VARIABLE_NOT_FOUND"));
      return true;
    }

    Blueprint->Modify();

    TArray<FString> AppliedKeys;
    for (const auto &Pair : MetaObjPtr->Values) {
      if (!Pair.Value.IsValid()) {
        continue;
      }

      const FString KeyStr(*Pair.Key);
      const FString ValueStr =
          FMcpAutomationBridge_JsonValueToString(Pair.Value);
      const FName MetaKey = FMcpAutomationBridge_ResolveMetadataKey(KeyStr);

      if (ValueStr.IsEmpty()) {
        FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(
            Blueprint, VariableDesc->VarName, nullptr, MetaKey);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Removed metadata '%s' from variable '%s'"),
               *MetaKey.ToString(), *VarName);
      } else {
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(
            Blueprint, VariableDesc->VarName, nullptr, MetaKey, ValueStr);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Set metadata '%s'='%s' on variable '%s'"),
               *MetaKey.ToString(), *ValueStr, *VarName);
      }

      AppliedKeys.Add(MetaKey.ToString());
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

    const TSharedPtr<FJsonObject> Snapshot =
        FMcpAutomationBridge_BuildBlueprintSnapshot(Blueprint, RegistryKey);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
    Resp->SetStringField(TEXT("variableName"), VarName);
    Resp->SetBoolField(TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> AppliedKeysJson;
    for (const FString &Key : AppliedKeys) {
      AppliedKeysJson.Add(MakeShared<FJsonValueString>(Key));
    }
    Resp->SetArrayField(TEXT("appliedKeys"), AppliedKeysJson);
    if (Snapshot.IsValid() && Snapshot->HasField(TEXT("metadata"))) {
      Resp->SetObjectField(TEXT("metadata"),
                           Snapshot->GetObjectField(TEXT("metadata")));
    }
    if (Snapshot.IsValid()) {
      Resp->SetObjectField(TEXT("blueprint"), Snapshot);
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Variable metadata applied"), Resp, FString());

    // Notify waiters
    TSharedPtr<FJsonObject> Notify = McpHandlerUtils::CreateResultObject();
    Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
    Notify->SetStringField(TEXT("event"),
                           TEXT("set_variable_metadata_completed"));
    Notify->SetStringField(TEXT("requestId"), RequestId);
    Notify->SetObjectField(TEXT("result"), Resp);
    if (Bridge.ConnectionManager.IsValid()) {
      Bridge.ConnectionManager->SendControlMessage(Notify);
    }
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_set_variable_metadata requires editor build"), nullptr,
        TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  return false;
}
#endif
} // namespace McpBlueprintHandlers
