#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Subsystems/AssetEditorSubsystem.h"
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
    // Accept both 'create_widget_blueprint' and 'create_widget' for flexibility
    if (SubAction.Equals(TEXT("create_widget_blueprint"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("create_widget"), ESearchCase::IgnoreCase))
    {
        if (GetJsonStringField(Payload, TEXT("name")).IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // An unknown or non-widget parentClass used to fall back to UserWidget and report success.
        const FString ParentClass = GetJsonStringField(Payload, TEXT("parentClass"), TEXT("UserWidget"));
        UClass* ParentUClass = UUserWidget::StaticClass();
        if (!ParentClass.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            ParentUClass = FindFirstObject<UClass>(*ParentClass, EFindFirstObjectOptions::None);
#else
            ParentUClass = ResolveClassByName(ParentClass);
#endif
            if (!ParentUClass)
            {
                ParentUClass = LoadClass<UUserWidget>(nullptr, *ParentClass);
            }
            if (!ParentUClass || !ParentUClass->IsChildOf(UUserWidget::StaticClass()))
            {
                Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
                    TEXT("parentClass '%s' is not a UserWidget class (a native class name or a Widget Blueprint class path)."), *ParentClass),
                    TEXT("INVALID_PARENT_CLASS"));
                return true;
            }
        }

        UWidgetBlueprint* WidgetBlueprint = McpCreateTemplateWidgetBlueprint(Subsystem, RequestId, RequestingSocket, Payload, TEXT(""), ParentUClass);
        if (!WidgetBlueprint)
        {
            return true;
        }
        const FString Name = WidgetBlueprint->GetName();

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
        const bool bCompiled = McpSafeCompileBlueprint(WidgetBlueprint);
        const bool bSaved = McpSafeAssetSave(WidgetBlueprint);
        const bool bPostCreateSucceeded = bCompiled && bSaved;

        ResultJson->SetBoolField(TEXT("success"), bPostCreateSucceeded);
        ResultJson->SetStringField(TEXT("message"), bPostCreateSucceeded
            ? FString::Printf(TEXT("Created widget blueprint: %s"), *Name)
            : FString::Printf(TEXT("Widget blueprint created but post-create steps failed: %s"), *Name));
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBlueprint));
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

    if (SubAction.Equals(TEXT("show_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetId = GetJsonStringField(Payload, TEXT("widgetId"));
        FString Message = GetJsonStringField(Payload, TEXT("message"));

        if (WidgetId.Equals(TEXT("notification"), ESearchCase::IgnoreCase))
        {
            FString NotificationText = Message.IsEmpty() ? TEXT("Notification") : Message;

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
                McpPathRefusalMessage(TEXT("widgetPath"), EffectivePath),
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        EffectivePath = SanitizedPath;

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(EffectivePath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget blueprint not found: %s"), *EffectivePath),
                TEXT("NOT_FOUND"));
            return true;
        }

        // Note: Actually showing the widget in viewport requires PIE (Play In Editor)
        if (GEditor)
        {
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
                McpPathRefusalMessage(TEXT("widgetPath"), WidgetPath),
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
        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parent class to: %s"), *ParentClass));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Set parent class to: %s"), *ParentClass), ResultJson);
        return true;
    }

    return false;
}
}
