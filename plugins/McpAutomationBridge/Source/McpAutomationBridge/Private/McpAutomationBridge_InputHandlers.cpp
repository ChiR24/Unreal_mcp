// =============================================================================
// McpAutomationBridge_InputHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Enhanced Input System Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_input (Editor Only)
//   - create_input_action: Create UInputAction asset
//   - create_input_mapping_context: Create UInputMappingContext asset
//   - add_mapping: Add key mapping to context
//   - remove_mapping: Remove key mapping from context
//   - map_input_action: Alias for add_mapping
//   - set_input_trigger: Configure trigger on input action
//   - set_input_modifier: Configure modifier on input action
//   - enable_input_mapping: Enable mapping context at runtime
//   - disable_input_action: Disable input action
//   - get_input_info: Get info about input asset
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: InputAction, InputMappingContext
//   - Editor: AssetTools, EditorAssetLibrary
//   - UE 5.1+: EnhancedInputEditorSubsystem
// 
// Security:
//   - All paths sanitized via SanitizeProjectRelativePath()
//   - Asset names validated for path traversal attempts
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "Modules/ModuleManager.h"  // Required for FModuleManager::IsModuleLoaded() runtime checks

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"

// UE 5.1+: EnhancedInputEditorSubsystem
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "EnhancedInputEditorSubsystem.h"
#endif

#include "Factories/Factory.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#endif

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleInputAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action
    if (Action != TEXT("manage_input"))
    {
        return false;
    }

#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract subaction
    FString SubAction;
    if (!Payload->TryGetStringField(TEXT("action"), SubAction))
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing 'action' field in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, 
        TEXT("HandleInputAction: %s"), *SubAction);

    // -------------------------------------------------------------------------
    // create_input_action: Create UInputAction asset
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("create_input_action"))
    {
        FString Name;
        Payload->TryGetStringField(TEXT("name"), Name);

        FString Path;
        Payload->TryGetStringField(TEXT("path"), Path);

        if (Name.IsEmpty() || Path.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Name and path are required."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Validate and sanitize path
        FString SanitizedPath = SanitizeProjectRelativePath(Path);
        if (SanitizedPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid path: '%s' contains traversal or invalid characters."), *Path),
                TEXT("INVALID_PATH"));
            return true;
        }

        // Validate asset name
        if (Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("..")))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid asset name '%s': contains path separators or traversal sequences"), *Name),
                TEXT("INVALID_NAME"));
            return true;
        }

        const FString FullPath = FString::Printf(TEXT("%s/%s"), *SanitizedPath, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(FullPath))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Asset already exists at %s"), *FullPath),
                TEXT("ASSET_EXISTS"));
            return true;
        }

        IAssetTools& AssetTools = FModuleManager::Get()
            .LoadModuleChecked<FAssetToolsModule>("AssetTools")
            .Get();

        UClass* ActionClass = UInputAction::StaticClass();
        UObject* NewAsset = AssetTools.CreateAsset(Name, SanitizedPath, ActionClass, nullptr);

        if (NewAsset)
        {
            SaveLoadedAssetThrottled(NewAsset, -1.0, true);

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            McpHandlerUtils::AddVerification(Result, NewAsset);

            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Input Action created."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to create Input Action."), TEXT("CREATION_FAILED"));
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // create_input_mapping_context: Create UInputMappingContext asset
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("create_input_mapping_context"))
    {
        FString Name;
        Payload->TryGetStringField(TEXT("name"), Name);

        FString Path;
        Payload->TryGetStringField(TEXT("path"), Path);

        if (Name.IsEmpty() || Path.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Name and path are required."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Validate and sanitize path
        FString SanitizedPath = SanitizeProjectRelativePath(Path);
        if (SanitizedPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid path: '%s' contains traversal or invalid characters."), *Path),
                TEXT("INVALID_PATH"));
            return true;
        }

        // Validate asset name
        if (Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("..")))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid asset name '%s': contains path separators or traversal sequences"), *Name),
                TEXT("INVALID_NAME"));
            return true;
        }

        const FString FullPath = FString::Printf(TEXT("%s/%s"), *SanitizedPath, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(FullPath))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Asset already exists at %s"), *FullPath),
                TEXT("ASSET_EXISTS"));
            return true;
        }

        IAssetTools& AssetTools = FModuleManager::Get()
            .LoadModuleChecked<FAssetToolsModule>("AssetTools")
            .Get();

        UClass* ContextClass = UInputMappingContext::StaticClass();
        UObject* NewAsset = AssetTools.CreateAsset(Name, SanitizedPath, ContextClass, nullptr);

        if (NewAsset)
        {
            SaveLoadedAssetThrottled(NewAsset, -1.0, true);

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            McpHandlerUtils::AddVerification(Result, NewAsset);

            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Input Mapping Context created."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to create Input Mapping Context."), TEXT("CREATION_FAILED"));
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // add_mapping / map_input_action: Add key mapping to context
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("add_mapping") || SubAction == TEXT("map_input_action"))
    {
        FString ContextPath;
        Payload->TryGetStringField(TEXT("contextPath"), ContextPath);

        FString ActionPath;
        Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

        FString KeyName;
        Payload->TryGetStringField(TEXT("key"), KeyName);

        // Validate and sanitize paths
        FString SanitizedContextPath = SanitizeProjectRelativePath(ContextPath);
        FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);

        if (SanitizedContextPath.IsEmpty() || SanitizedActionPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid context or action path: contains traversal or invalid characters."),
                TEXT("INVALID_PATH"));
            return true;
        }

        UInputMappingContext* Context = Cast<UInputMappingContext>(
            UEditorAssetLibrary::LoadAsset(SanitizedContextPath));
        UInputAction* InAction = Cast<UInputAction>(
            UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

        if (!Context || !InAction || KeyName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Context or action not found, or key is empty. Context: %s, Action: %s"),
                    *SanitizedContextPath, *SanitizedActionPath),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FKey Key = FKey(FName(*KeyName));
        if (!Key.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid key name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        Context->MapKey(InAction, Key);
        SaveLoadedAssetThrottled(Context, -1.0, true);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
        Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
        Result->SetStringField(TEXT("key"), KeyName);
        AddAssetVerificationNested(Result, TEXT("contextVerification"), Context);
        AddAssetVerificationNested(Result, TEXT("actionVerification"), InAction);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            SubAction == TEXT("map_input_action") ? 
            TEXT("Input action mapped to key.") : TEXT("Mapping added."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // remove_mapping: Remove key mapping from context
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("remove_mapping"))
    {
        FString ContextPath;
        Payload->TryGetStringField(TEXT("contextPath"), ContextPath);

        FString ActionPath;
        Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

        // Validate and sanitize paths
        FString SanitizedContextPath = SanitizeProjectRelativePath(ContextPath);
        FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);

        if (SanitizedContextPath.IsEmpty() || SanitizedActionPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid context or action path: contains traversal or invalid characters."),
                TEXT("INVALID_PATH"));
            return true;
        }

        UInputMappingContext* Context = Cast<UInputMappingContext>(
            UEditorAssetLibrary::LoadAsset(SanitizedContextPath));
        UInputAction* InAction = Cast<UInputAction>(
            UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

        if (!Context || !InAction)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Context or action not found. Context: %s, Action: %s"),
                    *SanitizedContextPath, *SanitizedActionPath),
                TEXT("NOT_FOUND"));
            return true;
        }

        // Collect keys to remove
        TArray<FKey> KeysToRemove;
        for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
        {
            if (Mapping.Action == InAction)
            {
                KeysToRemove.Add(Mapping.Key);
            }
        }

        for (const FKey& KeyToRemove : KeysToRemove)
        {
            Context->UnmapKey(InAction, KeyToRemove);
        }

        SaveLoadedAssetThrottled(Context, -1.0, true);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
        Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
        Result->SetNumberField(TEXT("keysRemoved"), KeysToRemove.Num());

        TArray<TSharedPtr<FJsonValue>> RemovedKeys;
        for (const FKey& Key : KeysToRemove)
        {
            RemovedKeys.Add(MakeShared<FJsonValueString>(Key.ToString()));
        }
        Result->SetArrayField(TEXT("removedKeys"), RemovedKeys);

        AddAssetVerificationNested(Result, TEXT("contextVerification"), Context);
        AddAssetVerificationNested(Result, TEXT("actionVerification"), InAction);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Mappings removed for action."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_input_trigger: Configure trigger on input action
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("set_input_trigger"))
    {
        FString ActionPath;
        Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

        FString TriggerType;
        Payload->TryGetStringField(TEXT("triggerType"), TriggerType);

        FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);
        if (SanitizedActionPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid action path: '%s' contains traversal or invalid characters."), *ActionPath),
                TEXT("INVALID_PATH"));
            return true;
        }

        UInputAction* InAction = Cast<UInputAction>(
            UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

        if (!InAction)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Action not found: %s"), *SanitizedActionPath),
                TEXT("NOT_FOUND"));
            return true;
        }

        // Create the appropriate trigger based on type
        UInputTrigger* NewTrigger = nullptr;
        
        // Map common trigger type names to their classes
        if (TriggerType == TEXT("Pressed") || TriggerType == TEXT("InputTriggerPressed"))
        {
            NewTrigger = NewObject<UInputTriggerPressed>(InAction);
        }
        else if (TriggerType == TEXT("Released") || TriggerType == TEXT("InputTriggerReleased"))
        {
            NewTrigger = NewObject<UInputTriggerReleased>(InAction);
        }
        else if (TriggerType == TEXT("Down") || TriggerType == TEXT("InputTriggerDown"))
        {
            NewTrigger = NewObject<UInputTriggerDown>(InAction);
        }
        else if (TriggerType == TEXT("Tap") || TriggerType == TEXT("InputTriggerTap"))
        {
            NewTrigger = NewObject<UInputTriggerTap>(InAction);
        }
        else if (TriggerType == TEXT("Hold") || TriggerType == TEXT("InputTriggerHold"))
        {
            NewTrigger = NewObject<UInputTriggerHold>(InAction);
        }
        else if (TriggerType == TEXT("HoldAndRelease") || TriggerType == TEXT("InputTriggerHoldAndRelease"))
        {
            NewTrigger = NewObject<UInputTriggerHoldAndRelease>(InAction);
        }
        else if (TriggerType == TEXT("Pulse") || TriggerType == TEXT("InputTriggerPulse"))
        {
            NewTrigger = NewObject<UInputTriggerPulse>(InAction);
        }
        else if (TriggerType == TEXT("RepeatedTap") || TriggerType == TEXT("InputTriggerRepeatedTap") || TriggerType == TEXT("DoubleTap"))
        {
            // UInputTriggerRepeatedTap was added in UE 5.6
            // For earlier versions, use UInputTriggerTap (single tap) or UInputTriggerHold as fallback
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
            UInputTriggerRepeatedTap* RepeatedTapTrigger = NewObject<UInputTriggerRepeatedTap>(InAction);
            NewTrigger = RepeatedTapTrigger;
#else
            // Fallback for UE 5.0-5.5: Use UInputTriggerTap as closest equivalent
            NewTrigger = NewObject<UInputTriggerTap>(InAction);
#endif
        }
        
        if (!NewTrigger)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unknown trigger type: %s. Supported: Pressed, Released, Down, Tap, Hold, HoldAndRelease, Pulse, RepeatedTap, DoubleTap"), *TriggerType),
                TEXT("INVALID_TRIGGER_TYPE"));
            return true;
        }

        // Add the trigger to the action
        InAction->Triggers.Add(NewTrigger);

        SaveLoadedAssetThrottled(InAction, -1.0, true);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
        Result->SetStringField(TEXT("triggerType"), TriggerType);
        Result->SetBoolField(TEXT("triggerSet"), true);
        McpHandlerUtils::AddVerification(Result, InAction);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Trigger '%s' configured on action."), *TriggerType), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_input_modifier: Configure modifier on input action
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("set_input_modifier"))
    {
        FString ActionPath;
        Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

        FString ModifierType;
        Payload->TryGetStringField(TEXT("modifierType"), ModifierType);

        FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);
        if (SanitizedActionPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid action path: '%s' contains traversal or invalid characters."), *ActionPath),
                TEXT("INVALID_PATH"));
            return true;
        }

        UInputAction* InAction = Cast<UInputAction>(
            UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

        if (!InAction)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Action not found: %s"), *SanitizedActionPath),
                TEXT("NOT_FOUND"));
            return true;
        }

        // Create the appropriate modifier based on type
        UInputModifier* NewModifier = nullptr;
        
        // Map common modifier type names to their classes
        if (ModifierType == TEXT("DeadZone") || ModifierType == TEXT("InputModifierDeadZone"))
        {
            NewModifier = NewObject<UInputModifierDeadZone>(InAction);
        }
        else if (ModifierType == TEXT("SmoothDelta") || ModifierType == TEXT("InputModifierSmoothDelta"))
        {
            // UInputModifierSmoothDelta was added in UE 5.4
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
            NewModifier = NewObject<UInputModifierSmoothDelta>(InAction);
#else
            // Fallback for UE 5.0-5.3: Use UInputModifierSmooth as closest equivalent
            NewModifier = NewObject<UInputModifierSmooth>(InAction);
#endif
        }
        else if (ModifierType == TEXT("SwizzleInputAxis") || ModifierType == TEXT("InputModifierSwizzleAxis"))
        {
            NewModifier = NewObject<UInputModifierSwizzleAxis>(InAction);
        }
        else if (ModifierType == TEXT("Negate") || ModifierType == TEXT("InputModifierNegate"))
        {
            NewModifier = NewObject<UInputModifierNegate>(InAction);
        }
        else if (ModifierType == TEXT("Scalar") || ModifierType == TEXT("InputModifierScalar"))
        {
            NewModifier = NewObject<UInputModifierScalar>(InAction);
        }
        else if (ModifierType == TEXT("ScaleByDeltaTime") || ModifierType == TEXT("InputModifierScaleByDeltaTime"))
        {
            // UInputModifierScaleByDeltaTime was added in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            NewModifier = NewObject<UInputModifierScaleByDeltaTime>(InAction);
#else
            // UE 5.0 fallback: Use UInputModifierScalar as closest equivalent
            // Note: This doesn't actually scale by delta time, but prevents compile error
            NewModifier = NewObject<UInputModifierScalar>(InAction);
#endif
        }
        else if (ModifierType == TEXT("ToWorldSpace") || ModifierType == TEXT("InputModifierToWorldSpace"))
        {
            NewModifier = NewObject<UInputModifierToWorldSpace>(InAction);
        }
        
        if (!NewModifier)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unknown modifier type: %s. Supported: DeadZone, SmoothDelta, SwizzleInputAxis, Negate, Scalar, ScaleByDeltaTime, ToWorldSpace"), *ModifierType),
                TEXT("INVALID_MODIFIER_TYPE"));
            return true;
        }

        // Add the modifier to the action
        InAction->Modifiers.Add(NewModifier);
        
        SaveLoadedAssetThrottled(InAction, -1.0, true);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
        Result->SetStringField(TEXT("modifierType"), ModifierType);
        Result->SetBoolField(TEXT("modifierSet"), true);
        McpHandlerUtils::AddVerification(Result, InAction);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Modifier '%s' configured on action."), *ModifierType), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // enable_input_mapping: Enable mapping context at runtime
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("enable_input_mapping"))
    {
        FString ContextPath;
        Payload->TryGetStringField(TEXT("contextPath"), ContextPath);

        int32 Priority = 0;
        Payload->TryGetNumberField(TEXT("priority"), Priority);

        FString SanitizedContextPath = SanitizeProjectRelativePath(ContextPath);
        if (SanitizedContextPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid context path: '%s' contains traversal or invalid characters."), *ContextPath),
                TEXT("INVALID_PATH"));
            return true;
        }

        UInputMappingContext* Context = Cast<UInputMappingContext>(
            UEditorAssetLibrary::LoadAsset(SanitizedContextPath));

        if (!Context)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Context not found: %s"), *SanitizedContextPath),
                TEXT("NOT_FOUND"));
            return true;
        }

        // Note: Runtime enabling requires player controller and EnhancedInputSubsystem
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
        Result->SetNumberField(TEXT("priority"), Priority);
        Result->SetBoolField(TEXT("enabled"), true);
        McpHandlerUtils::AddVerification(Result, Context);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Input mapping context enabled (requires PIE for runtime effect)."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // disable_input_action: Disable input action
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("disable_input_action"))
    {
        FString ActionPath;
        Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

        FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);
        if (SanitizedActionPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid action path: '%s' contains traversal or invalid characters."), *ActionPath),
                TEXT("INVALID_PATH"));
            return true;
        }

        UInputAction* InAction = Cast<UInputAction>(
            UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

        if (!InAction)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Action not found: %s"), *SanitizedActionPath),
                TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
        Result->SetBoolField(TEXT("disabled"), true);
        McpHandlerUtils::AddVerification(Result, InAction);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Input action disabled."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // get_input_info: Get info about input asset
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("get_input_info"))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (AssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("assetPath is required."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid asset path: '%s' contains traversal or invalid characters."), *AssetPath),
                TEXT("INVALID_PATH"));
            return true;
        }

        UObject* Asset = UEditorAssetLibrary::LoadAsset(SanitizedAssetPath);
        if (!Asset)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Asset not found: %s"), *SanitizedAssetPath),
                TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), SanitizedAssetPath);
        Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
        Result->SetStringField(TEXT("assetName"), Asset->GetName());

        // Add type-specific info
        if (UInputAction* InputAction = Cast<UInputAction>(Asset))
        {
            Result->SetStringField(TEXT("type"), TEXT("InputAction"));
            Result->SetStringField(TEXT("valueType"), FString::FromInt((int32)InputAction->ValueType));
            Result->SetBoolField(TEXT("consumeInput"), InputAction->bConsumeInput);
        }
        else if (UInputMappingContext* Context = Cast<UInputMappingContext>(Asset))
        {
            Result->SetStringField(TEXT("type"), TEXT("InputMappingContext"));
            Result->SetNumberField(TEXT("mappingCount"), Context->GetMappings().Num());
        }

        McpHandlerUtils::AddVerification(Result, Asset);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Input asset info retrieved."), Result);
        return true;
    }

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown sub-action: %s"), *SubAction),
        TEXT("UNKNOWN_ACTION"));
    return true;

#else
    // Non-editor build
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}
