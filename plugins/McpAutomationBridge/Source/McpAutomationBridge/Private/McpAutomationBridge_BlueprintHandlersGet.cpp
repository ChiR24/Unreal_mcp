#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintGet(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if ((ActionMatchesPattern(TEXT("blueprint_get")) ||
       ActionMatchesPattern(TEXT("get_blueprint")) ||
       ActionMatchesPattern(TEXT("get")) ||
       AlphaNumLower.Contains(TEXT("blueprintget")) ||
       AlphaNumLower.Contains(TEXT("getblueprint"))) &&
      !Lower.Contains(TEXT("scs"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_get handler: RequestId=%s"), *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("blueprint_get requires a blueprint path."),
                             nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    bool bExists = false;
    TSharedPtr<FJsonObject> Entry = nullptr;

#if WITH_EDITOR
    FString Normalized;
    FString Err;
    UBlueprint *BP = LoadBlueprintAsset(Path, Normalized, Err);
    bExists = (BP != nullptr);
    if (bExists) {
      const FString Key =
          !Normalized.TrimStartAndEnd().IsEmpty() ? Normalized : Path;
      Entry = FMcpAutomationBridge_BuildBlueprintSnapshot(BP, Key);

      // Merge functions and events from registry
      TSharedPtr<FJsonObject> RegistryEntry =
          FMcpAutomationBridge_EnsureBlueprintEntry(Key);
      if (RegistryEntry.IsValid()) {
        // Build set of live variable names from the snapshot
        TSet<FString> LiveVariableNames;
        if (Entry->HasField(TEXT("variables"))) {
          TArray<TSharedPtr<FJsonValue>> LiveVariables =
              Entry->GetArrayField(TEXT("variables"));
          for (const TSharedPtr<FJsonValue> &VarVal : LiveVariables) {
            if (VarVal.IsValid() && VarVal->Type == EJson::Object) {
              FString VarName;
              if (VarVal->AsObject()->TryGetStringField(TEXT("name"), VarName)) {
                LiveVariableNames.Add(VarName);
              }
            }
          }
        }

        if (RegistryEntry->HasField(TEXT("defaults"))) {
          TSharedPtr<FJsonObject> EntryDefaults =
              Entry->HasField(TEXT("defaults"))
                  ? Entry->GetObjectField(TEXT("defaults"))
                  : MakeShared<FJsonObject>();
          const TSharedPtr<FJsonObject> RegistryDefaults =
              RegistryEntry->GetObjectField(TEXT("defaults"));
          if (RegistryDefaults.IsValid()) {
            for (const auto &Pair :
                 RegistryDefaults->Values) {
              const FString PairKey(*Pair.Key);
              // Only merge if this variable still exists in the live blueprint
              if (!LiveVariableNames.Contains(PairKey)) {
                continue;
              }
              if (EntryDefaults->HasField(PairKey)) {
                // Key exists - deep merge if both are JSON objects
                const TSharedPtr<FJsonObject>* ExistingObj = nullptr;
                if (Pair.Value->Type == EJson::Object &&
                    EntryDefaults->TryGetObjectField(PairKey, ExistingObj) &&
                    ExistingObj && (*ExistingObj).IsValid() &&
                    Pair.Value->AsObject().IsValid()) {
                  // Both are objects - deep merge sub-keys from registry
                  const TSharedPtr<FJsonObject> RegistryObj = Pair.Value->AsObject();
                  for (const auto &SubPair :
                       RegistryObj->Values) {
                    const FString SubKey(*SubPair.Key);
                    if (!(*ExistingObj)->HasField(SubKey)) {
                      (*ExistingObj)->SetField(SubKey, SubPair.Value);
                    }
                  }
                }
                // If not both objects, keep existing value (don't overwrite)
              }
              // Do NOT add missing keys - only merge into existing fields
            }
          }
          Entry->SetObjectField(TEXT("defaults"), EntryDefaults);
        }
        if (RegistryEntry->HasField(TEXT("metadata"))) {
          TSharedPtr<FJsonObject> EntryMetadata =
              Entry->HasField(TEXT("metadata"))
                  ? Entry->GetObjectField(TEXT("metadata"))
                  : MakeShared<FJsonObject>();
          const TSharedPtr<FJsonObject> RegistryMetadata =
              RegistryEntry->GetObjectField(TEXT("metadata"));
          if (RegistryMetadata.IsValid()) {
            for (const auto &Pair :
                 RegistryMetadata->Values) {
              const FString PairKey(*Pair.Key);
              // Only merge if this variable still exists in the live blueprint
              if (!LiveVariableNames.Contains(PairKey)) {
                continue;
              }
              if (EntryMetadata->HasField(PairKey)) {
                // Key exists - deep merge if both are JSON objects
                const TSharedPtr<FJsonObject>* ExistingObj = nullptr;
                if (Pair.Value->Type == EJson::Object &&
                    EntryMetadata->TryGetObjectField(PairKey, ExistingObj) &&
                    ExistingObj && (*ExistingObj).IsValid() &&
                    Pair.Value->AsObject().IsValid()) {
                  // Both are objects - deep merge sub-keys from registry
                  const TSharedPtr<FJsonObject> RegistryObj = Pair.Value->AsObject();
                  for (const auto &SubPair :
                       RegistryObj->Values) {
                    const FString SubKey(*SubPair.Key);
                    if (!(*ExistingObj)->HasField(SubKey)) {
                      (*ExistingObj)->SetField(SubKey, SubPair.Value);
                    }
                  }
                }
                // If not both objects, keep existing value (don't overwrite)
              }
            }
          }
          if (EntryMetadata->Values.Num() > 0) {
            Entry->SetObjectField(TEXT("metadata"), EntryMetadata);
          }
        }
        if (RegistryEntry->HasField(TEXT("functions"))) {
          TArray<TSharedPtr<FJsonValue>> RegFuncs =
              RegistryEntry->GetArrayField(TEXT("functions"));
          if (!Entry->HasField(TEXT("functions"))) {
            Entry->SetArrayField(TEXT("functions"), RegFuncs);
          } else {
            // Merge unique
            TArray<TSharedPtr<FJsonValue>> ExistingFuncs =
                Entry->GetArrayField(TEXT("functions"));
            TSet<FString> KnownNames;
            for (const auto &Val : ExistingFuncs) {
              const TSharedPtr<FJsonObject> Obj = Val->AsObject();
              FString N;
              if (Obj.IsValid() && Obj->TryGetStringField(TEXT("name"), N))
                KnownNames.Add(N);
            }
            for (const auto &Val : RegFuncs) {
              const TSharedPtr<FJsonObject> Obj = Val->AsObject();
              FString N;
              if (Obj.IsValid() && Obj->TryGetStringField(TEXT("name"), N) &&
                  !KnownNames.Contains(N))
                ExistingFuncs.Add(Val);
            }
            Entry->SetArrayField(TEXT("functions"), ExistingFuncs);
          }
        }

        if (RegistryEntry->HasField(TEXT("events"))) {
          TArray<TSharedPtr<FJsonValue>> RegEvents =
              RegistryEntry->GetArrayField(TEXT("events"));
          if (!Entry->HasField(TEXT("events"))) {
            Entry->SetArrayField(TEXT("events"), RegEvents);
          } else {
            // Merge unique
            TArray<TSharedPtr<FJsonValue>> ExistingEvents =
                Entry->GetArrayField(TEXT("events"));
            TSet<FString> KnownNames;
            for (const auto &Val : ExistingEvents) {
              const TSharedPtr<FJsonObject> Obj = Val->AsObject();
              FString N;
              if (Obj.IsValid() && Obj->TryGetStringField(TEXT("name"), N))
                KnownNames.Add(N);
            }
            for (const auto &Val : RegEvents) {
              const TSharedPtr<FJsonObject> Obj = Val->AsObject();
              FString N;
              if (Obj.IsValid() && Obj->TryGetStringField(TEXT("name"), N) &&
                  !KnownNames.Contains(N))
                ExistingEvents.Add(Val);
            }
            Entry->SetArrayField(TEXT("events"), ExistingEvents);
          }
        }
      }
    }
#else
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("blueprint_get requires editor build"), nullptr,
                           TEXT("NOT_AVAILABLE"));
    return true;
#endif

    if (!bExists) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint not found"), nullptr,
                             TEXT("NOT_FOUND"));
      return true;
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Blueprint fetched"), Entry, FString());
    return true;
  }

  // blueprint_add_node: Create a Blueprint graph node programmatically
  return false;
}
#endif
} // namespace McpBlueprintHandlers
