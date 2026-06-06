#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintSetMetadata(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_set_metadata")) ||
      ActionMatchesPattern(TEXT("set_metadata")) ||
      AlphaNumLower.Contains(TEXT("blueprintsetmetadata")) ||
      AlphaNumLower.Contains(TEXT("setmetadata"))) {
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

#if WITH_EDITOR
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
      if (Pair.Value->Type == EJson::String) {
        MetaValue = Pair.Value->AsString();
      } else if (Pair.Value->Type == EJson::Boolean) {
        MetaValue = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
      } else if (Pair.Value->Type == EJson::Number) {
        MetaValue = FString::Printf(TEXT("%g"), Pair.Value->AsNumber());
      } else {
        continue;
      }

      // Set metadata on the blueprint class
      if (BP->GeneratedClass) {
        BP->GeneratedClass->SetMetaData(MetaKey, *MetaValue);
      }
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
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_set_metadata requires editor build"), nullptr,
        TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }
  return false;
}
#endif
} // namespace McpBlueprintHandlers
