#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintSetMetadata(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("set_metadata"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_set_metadata handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_set_metadata requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    const TSharedPtr<FJsonObject>* MetadataObj = nullptr;
    if (!LocalPayload->TryGetObjectField(TEXT("metadata"), MetadataObj) ||
        !MetadataObj || !(*MetadataObj).IsValid()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("metadata object required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // propertyName names a member: its metadata is variable metadata, which
    // set_variable_metadata already writes (and verifies). It used to be
    // ignored, putting the keys on the class instead.
    FString MemberName;
    if (LocalPayload->TryGetStringField(TEXT("propertyName"), MemberName) && !MemberName.IsEmpty()) {
      TSharedPtr<FJsonObject> MemberPayload = MakeShared<FJsonObject>(*LocalPayload);
      MemberPayload->SetStringField(TEXT("variableName"), MemberName);
      return HandleBlueprintSetVariableMetadata(BuildBlueprintActionContext(
          Bridge, RequestId, TEXT("set_variable_metadata"), MemberPayload, RequestingSocket));
    }

    FString Normalized;
    FString LoadErr;
    UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
    if (!BP) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), LoadErr);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to load blueprint"), Err,
                             TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    const FString RegistryKey = Normalized.IsEmpty() ? Path : Normalized;
    // Class metadata lives on the generated class; with none there is nowhere
    // to write it (every key used to be reported as set anyway).
    if (!BP->GeneratedClass) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("The Blueprint has no generated class yet, so no metadata was written; compile it (manage_blueprint compile) and retry"),
                             nullptr, TEXT("BLUEPRINT_NOT_COMPILED"));
      return true;
    }

    // Set metadata on the blueprint package or asset
    TArray<FString> MetadataSet;
    for (const auto& Pair :
         (*MetadataObj)->Values) {
      const FString MetadataKey(*Pair.Key);
      if (!Pair.Value.IsValid()) {
        continue;
      }
      const FName MetaKey = FMcpAutomationBridge_ResolveMetadataKey(MetadataKey);
      FString MetaValue;
      if (!McpJsonScalarToString(Pair.Value, MetaValue)) {
        continue;
      }

      BP->GeneratedClass->SetMetaData(MetaKey, *MetaValue);
      // Note: UBlueprint itself doesn't have SetMetaData in UE 5.7+
      // Metadata is stored on the GeneratedClass
      MetadataSet.Add(MetadataKey);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    const bool bSaved = SaveLoadedAssetThrottled(BP);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
    TArray<TSharedPtr<FJsonValue>> MetaArray;
    for (const FString& Key : MetadataSet) {
      MetaArray.Add(MakeShared<FJsonValueString>(Key));
    }
    Resp->SetArrayField(TEXT("metadataSet"), MetaArray);
    Resp->SetBoolField(TEXT("saved"), bSaved);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Metadata set"), Resp, FString());
    return true;
  }
  return false;
}
} // namespace McpBlueprintHandlers
