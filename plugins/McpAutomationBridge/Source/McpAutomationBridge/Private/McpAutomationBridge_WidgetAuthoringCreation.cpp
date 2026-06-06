#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#endif
#include "UObject/Package.h"
#include "WidgetBlueprint.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringCreation(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.1 Widget Creation
    // =========================================================================

    // Accept both 'create_widget_blueprint' and 'create_widget' for flexibility
    if (SubAction.Equals(TEXT("create_widget_blueprint"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("create_widget"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"));
        if (Name.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // Accept both 'path' (preferred) and 'folder' for the destination directory
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty())
        {
            Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));
        }

        // SECURITY: Validate folder path for traversal attacks
        FString SanitizedFolder = SanitizeProjectRelativePath(Folder);
        if (SanitizedFolder.IsEmpty() && !Folder.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid folder path: path traversal or invalid characters detected"),
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        Folder = SanitizedFolder;

        FString ParentClass = GetJsonStringField(Payload, TEXT("parentClass"), TEXT("UserWidget"));

        // Build full path
        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // CRITICAL: Check if a widget blueprint with this name already exists
        // This prevents the engine assertion crash in FKismetEditorUtilities::CreateBlueprint()
        // which has: check(FindObject<UBlueprint>(Outer, *NewBPName.ToString()) == NULL)
        FString NewBPObjectPath = FullPath + TEXT(".") + Name;
        if (FindObject<UWidgetBlueprint>(nullptr, *NewBPObjectPath) != nullptr)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name),
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        // Also check the package path
        if (UPackage* ExistingPackage = FindPackage(nullptr, *FullPath))
        {
            if (FindObject<UWidgetBlueprint>(ExistingPackage, *Name) != nullptr)
            {
                Subsystem.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name),
                    TEXT("ALREADY_EXISTS"));
                return true;
            }
        }

        // Create package
        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        // Find parent class
        UClass* ParentUClass = UUserWidget::StaticClass();
        if (!ParentClass.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
        {
            // Try to find custom parent class
            // Note: FindFirstObject was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            UClass* FoundClass = FindFirstObject<UClass>(*ParentClass, EFindFirstObjectOptions::None);
#else
            UClass* FoundClass = ResolveClassByName(ParentClass);
#endif
            if (FoundClass && FoundClass->IsChildOf(UUserWidget::StaticClass()))
            {
                ParentUClass = FoundClass;
            }
        }

        // Create widget blueprint
        UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            ParentUClass,
            Package,
            FName(*Name),
            BPTYPE_Normal,
            UWidgetBlueprint::StaticClass(),
            UWidgetBlueprintGeneratedClass::StaticClass()
        ));

        if (!WidgetBlueprint)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create widget blueprint"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Mark package dirty and notify asset registry
        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBlueprint);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
        const bool bCompiled = McpSafeCompileBlueprint(WidgetBlueprint);
        const bool bSaved = McpSafeAssetSave(WidgetBlueprint);
        const bool bPostCreateSucceeded = bCompiled && bSaved;

        // Return the full object path (Package.ObjectName format) for proper loading
        FString ObjectPath = WidgetBlueprint->GetPathName();

        ResultJson->SetBoolField(TEXT("success"), bPostCreateSucceeded);
        ResultJson->SetStringField(TEXT("message"), bPostCreateSucceeded
            ? FString::Printf(TEXT("Created widget blueprint: %s"), *Name)
            : FString::Printf(TEXT("Widget blueprint created but post-create steps failed: %s"), *Name));
        ResultJson->SetStringField(TEXT("widgetPath"), ObjectPath);
        ResultJson->SetBoolField(TEXT("compileSucceeded"), bCompiled);
        ResultJson->SetBoolField(TEXT("saveSucceeded"), bSaved);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBlueprint);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, bPostCreateSucceeded,
            bPostCreateSucceeded
                ? FString::Printf(TEXT("Created widget blueprint: %s"), *Name)
                : FString::Printf(TEXT("Widget blueprint created but post-create steps failed (compile=%s, save=%s)"),
                    bCompiled ? TEXT("true") : TEXT("false"),
                    bSaved ? TEXT("true") : TEXT("false")),
            ResultJson,
            bPostCreateSucceeded ? TEXT("") : TEXT("POST_CREATE_FAILED"));
        return true;
    }

    // =========================================================================
    // show_widget: Show a widget in viewport or display notification
    // =========================================================================
    if (SubAction.Equals(TEXT("show_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetId = GetJsonStringField(Payload, TEXT("widgetId"));
        FString Message = GetJsonStringField(Payload, TEXT("message"));

        // Handle notification widget specially
        if (WidgetId.Equals(TEXT("notification"), ESearchCase::IgnoreCase))
        {
            FString NotificationText = Message.IsEmpty() ? TEXT("Notification") : Message;

            // Use notification system
            FNotificationInfo Info(FText::FromString(NotificationText));
            Info.ExpireDuration = 3.0f;
            Info.bUseLargeFont = true;

            FSlateNotificationManager::Get().AddNotification(Info);

            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("message"), TEXT("Notification shown"));
            ResultJson->SetStringField(TEXT("widgetId"), WidgetId);

            Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Notification shown"), ResultJson);
            return true;
        }

        // For regular widgets, we need a path
        FString EffectivePath = WidgetPath.IsEmpty() ? GetJsonStringField(Payload, TEXT("name")) : WidgetPath;
        if (EffectivePath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: widgetPath or name"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // SECURITY: Validate widget path
        FString SanitizedPath = SanitizeProjectRelativePath(EffectivePath);
        if (SanitizedPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid widgetPath: path traversal or invalid characters detected"),
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        EffectivePath = SanitizedPath;

        // Load the widget blueprint
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(EffectivePath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget blueprint not found: %s"), *EffectivePath),
                TEXT("NOT_FOUND"));
            return true;
        }

        // Note: Actually showing the widget in viewport requires PIE (Play In Editor)
        // In editor mode, we can open the widget designer instead
        if (GEditor)
        {
            // Open the widget blueprint in the editor
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(WidgetBP);
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Widget opened: %s"), *EffectivePath));
        ResultJson->SetStringField(TEXT("widgetPath"), EffectivePath);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Widget opened: %s"), *EffectivePath), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_widget_parent_class"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentClass = GetJsonStringField(Payload, TEXT("parentClass"));

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // SECURITY: Validate widget path
        FString SanitizedWidgetPath = SanitizeProjectRelativePath(WidgetPath);
        if (SanitizedWidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid widgetPath: path traversal or invalid characters detected"),
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        WidgetPath = SanitizedWidgetPath;

        if (ParentClass.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: parentClass"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find parent class
        // Note: FindFirstObject was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        UClass* NewParentClass = FindFirstObject<UClass>(*ParentClass, EFindFirstObjectOptions::None);
#else
        UClass* NewParentClass = ResolveClassByName(ParentClass);
#endif
        if (!NewParentClass || !NewParentClass->IsChildOf(UUserWidget::StaticClass()))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Parent class not found or invalid"), TEXT("INVALID_CLASS"));
            return true;
        }

        // Set parent class
        WidgetBP->ParentClass = NewParentClass;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parent class to: %s"), *ParentClass));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Set parent class to: %s"), *ParentClass), ResultJson);
        return true;
    }

    return false;
}
}
