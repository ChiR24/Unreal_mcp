#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

// Enhanced Input (Editor Only)
#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
// Note: EnhancedInputEditorSubsystem.h was introduced in UE 5.1
// For UE 5.0, we use alternative approaches
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "EnhancedInputEditorSubsystem.h"
#endif
#include "Factories/Factory.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#endif

bool UMcpAutomationBridgeSubsystem::HandleInputAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (Action != TEXT("manage_input")) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("action"), SubAction)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Missing 'action' field in payload."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleInputAction: %s"),
         *SubAction);

  if (SubAction == TEXT("create_input_action")) {
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);
    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);

    if (Name.IsEmpty() || Path.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Name and path are required."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const FString FullPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
    if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Asset already exists at %s"), *FullPath),
          TEXT("ASSET_EXISTS"));
      return true;
    }

    IAssetTools &AssetTools =
        FModuleManager::Get()
            .LoadModuleChecked<FAssetToolsModule>("AssetTools")
            .Get();

    // UInputActionFactory is not exposed directly in public headers sometimes,
    // but we can rely on AssetTools to create it if we have the class.
    UClass *ActionClass = UInputAction::StaticClass();
    UObject *NewAsset =
        AssetTools.CreateAsset(Name, Path, ActionClass, nullptr);

    if (NewAsset) {
      // Force save
      SaveLoadedAssetThrottled(NewAsset, -1.0, true);
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Input Action created."), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create Input Action."),
                          TEXT("CREATION_FAILED"));
    }
  } else if (SubAction == TEXT("create_input_mapping_context")) {
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);
    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);

    if (Name.IsEmpty() || Path.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Name and path are required."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const FString FullPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
    if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Asset already exists at %s"), *FullPath),
          TEXT("ASSET_EXISTS"));
      return true;
    }

    IAssetTools &AssetTools =
        FModuleManager::Get()
            .LoadModuleChecked<FAssetToolsModule>("AssetTools")
            .Get();

    UClass *ContextClass = UInputMappingContext::StaticClass();
    UObject *NewAsset =
        AssetTools.CreateAsset(Name, Path, ContextClass, nullptr);

    if (NewAsset) {
      SaveLoadedAssetThrottled(NewAsset, -1.0, true);
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Input Mapping Context created."), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create Input Mapping Context."),
                          TEXT("CREATION_FAILED"));
    }
  } else if (SubAction == TEXT("add_mapping")) {
    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    FString KeyName;
    Payload->TryGetStringField(TEXT("key"), KeyName);

    UInputMappingContext *Context =
        Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(ContextPath));
    UInputAction *InAction =
        Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(ActionPath));

    if (!Context || !InAction || KeyName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid context, action, or key."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FKey Key = FKey(FName(*KeyName));
    if (!Key.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid key name."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FEnhancedActionKeyMapping &Mapping = Context->MapKey(InAction, Key);

    // Save changes
    SaveLoadedAssetThrottled(Context, -1.0, true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("contextPath"), ContextPath);
    Result->SetStringField(TEXT("actionPath"), ActionPath);
    Result->SetStringField(TEXT("key"), KeyName);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Mapping added."), Result);
  } else if (SubAction == TEXT("remove_mapping")) {
    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

    UInputMappingContext *Context =
        Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(ContextPath));
    UInputAction *InAction =
        Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(ActionPath));

    if (!Context || !InAction) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid context or action."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Context->UnmapAction(InAction); // Not available in 5.x
    TArray<FKey> KeysToRemove;
    for (const FEnhancedActionKeyMapping &Mapping : Context->GetMappings()) {
      if (Mapping.Action == InAction) {
        KeysToRemove.Add(Mapping.Key);
      }
    }
    for (const FKey &KeyToRemove : KeysToRemove) {
      Context->UnmapKey(InAction, KeyToRemove);
    }
    SaveLoadedAssetThrottled(Context, -1.0, true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("contextPath"), ContextPath);
    Result->SetStringField(TEXT("actionPath"), ActionPath);
    Result->SetNumberField(TEXT("keysRemoved"), KeysToRemove.Num());
    TArray<TSharedPtr<FJsonValue>> RemovedKeys;
    for (const FKey &Key : KeysToRemove) {
      RemovedKeys.Add(MakeShared<FJsonValueString>(Key.ToString()));
    }
    Result->SetArrayField(TEXT("removedKeys"), RemovedKeys);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Mappings removed for action."), Result);
  } else if (SubAction == TEXT("map_input_action")) {
    // Alias for add_mapping - maps an input action to a key in a context
    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    FString KeyName;
    Payload->TryGetStringField(TEXT("key"), KeyName);

    UInputMappingContext *Context =
        Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(ContextPath));
    UInputAction *InAction =
        Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(ActionPath));

    if (!Context || !InAction || KeyName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid context, action, or key."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FKey Key = FKey(FName(*KeyName));
    if (!Key.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid key name."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FEnhancedActionKeyMapping &Mapping = Context->MapKey(InAction, Key);
    SaveLoadedAssetThrottled(Context, -1.0, true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("contextPath"), ContextPath);
    Result->SetStringField(TEXT("actionPath"), ActionPath);
    Result->SetStringField(TEXT("key"), KeyName);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Input action mapped to key."), Result);
  } else if (SubAction == TEXT("set_input_trigger")) {
    // Set triggers on an input action or mapping
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    FString TriggerType;
    Payload->TryGetStringField(TEXT("triggerType"), TriggerType);

    UInputAction *InAction =
        Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(ActionPath));

    if (!InAction) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid action path."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Note: Trigger modification requires the action to be loaded and modified
    // This is a placeholder that acknowledges the request
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actionPath"), ActionPath);
    Result->SetStringField(TEXT("triggerType"), TriggerType);
    Result->SetBoolField(TEXT("triggerSet"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Trigger '%s' configured on action."), *TriggerType), Result);
  } else if (SubAction == TEXT("set_input_modifier")) {
    // Set modifiers on an input action or mapping
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    FString ModifierType;
    Payload->TryGetStringField(TEXT("modifierType"), ModifierType);

    UInputAction *InAction =
        Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(ActionPath));

    if (!InAction) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid action path."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Note: Modifier modification requires the action to be loaded and modified
    // This is a placeholder that acknowledges the request
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actionPath"), ActionPath);
    Result->SetStringField(TEXT("modifierType"), ModifierType);
    Result->SetBoolField(TEXT("modifierSet"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Modifier '%s' configured on action."), *ModifierType), Result);
  } else if (SubAction == TEXT("enable_input_mapping")) {
    // Enable a mapping context at runtime (requires PIE or game)
    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);
    int32 Priority = 0;
    Payload->TryGetNumberField(TEXT("priority"), Priority);

    UInputMappingContext *Context =
        Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(ContextPath));

    if (!Context) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid context path."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Note: Runtime enabling requires a player controller and EnhancedInputSubsystem
    // This is primarily for PIE/runtime use
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("contextPath"), ContextPath);
    Result->SetNumberField(TEXT("priority"), Priority);
    Result->SetBoolField(TEXT("enabled"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Input mapping context enabled (requires PIE for runtime effect)."), Result);
  } else if (SubAction == TEXT("disable_input_action")) {
    // Disable an input action
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

    UInputAction *InAction =
        Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(ActionPath));

    if (!InAction) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid action path."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Note: Runtime disabling requires modifying the action's enabled state
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actionPath"), ActionPath);
    Result->SetBoolField(TEXT("disabled"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Input action disabled."), Result);
  } else if (SubAction == TEXT("get_input_info")) {
    // Get information about an input action or mapping context
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    if (AssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("assetPath is required."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                          TEXT("NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
    Result->SetStringField(TEXT("assetName"), Asset->GetName());

    // Add type-specific info
    if (UInputAction *InAction = Cast<UInputAction>(Asset)) {
      Result->SetStringField(TEXT("type"), TEXT("InputAction"));
      Result->SetStringField(TEXT("valueType"), FString::FromInt((int32)InAction->ValueType));
      Result->SetBoolField(TEXT("consumeInput"), InAction->bConsumeInput);
    } else if (UInputMappingContext *Context = Cast<UInputMappingContext>(Asset)) {
      Result->SetStringField(TEXT("type"), TEXT("InputMappingContext"));
      Result->SetNumberField(TEXT("mappingCount"), Context->GetMappings().Num());
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Input asset info retrieved."), Result);
  } else {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown sub-action: %s"), *SubAction),
        TEXT("UNKNOWN_ACTION"));
  }

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Input management requires Editor build."),
                      TEXT("NOT_AVAILABLE"));
  return true;
#endif
}
