#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Domains/WidgetAuthoring/Templates/McpWidgetSpec.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringCreation(
    UEngineWireSubsystem& Subsystem,
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
        // A full widgetPath names the asset by itself; it used to be refused with "Missing required parameter: name".
        TSharedPtr<FJsonObject> CreatePayload = Payload;
        const FString WidgetPathArg = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (GetJsonStringField(Payload, TEXT("name")).IsEmpty() && !WidgetPathArg.IsEmpty())
        {
            const FString PackageName = FPackageName::ObjectPathToPackageName(WidgetPathArg);
            CreatePayload = MakeShared<FJsonObject>(*Payload);
            CreatePayload->SetStringField(TEXT("name"), FPackageName::GetShortName(PackageName));
            CreatePayload->SetStringField(TEXT("path"), FPackageName::GetLongPackagePath(PackageName));
        }
        if (GetJsonStringField(CreatePayload, TEXT("name")).IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name (with path), or widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // An unknown or non-widget parentClass used to fall back to UserWidget and report success.
        const FString ParentClass = GetJsonStringField(Payload, TEXT("parentClass"), TEXT("UserWidget"));
        UClass* ParentUClass = UUserWidget::StaticClass();
        if (!ParentClass.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
        {
            // FindFirstObject (5.1+) found only a loaded class by name, not a Widget Blueprint path to parent on.
            ParentUClass = ResolveClassByName(ParentClass);
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

        UWidgetBlueprint* WidgetBlueprint = McpCreateTemplateWidgetBlueprint(Subsystem, RequestId, RequestingSocket, CreatePayload, TEXT(""), ParentUClass);
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

        // From 5.1 this found only a class already loaded, by name, so a Widget Blueprint path (the record's own example)
        // was INVALID_CLASS; ResolveClassByName loads a class or Blueprint path in every version.
        UClass* NewParentClass = ResolveClassByName(ParentClass);
        if (!NewParentClass || !NewParentClass->IsChildOf(UUserWidget::StaticClass()))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("parentClass %s is not a UserWidget class or Widget Blueprint that loads. Nothing was changed."), *ParentClass), TEXT("INVALID_CLASS"));
            return true;
        }
        // This widget, or one deriving from it, as its parent made the class its own ancestor.
        if (WidgetBP->GeneratedClass && NewParentClass->IsChildOf(WidgetBP->GeneratedClass))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s is %s itself or derives from it; a class cannot be its own parent. Nothing was changed."),
                *NewParentClass->GetName(), *WidgetBP->GetName()), TEXT("CYCLIC_PARENT"));
            return true;
        }

        // As the editor's Reparent does: nodes refreshed and the class compiled against its new parent, then saved.
        const FString PreviousParent = WidgetBP->ParentClass ? WidgetBP->ParentClass->GetPathName() : FString();
        WidgetBP->Modify();
        WidgetBP->ParentClass = NewParentClass;
        FBlueprintEditorUtils::RefreshAllNodes(WidgetBP);
        WidgetAuthoringHelpers::RefreshWidgetBlueprintClass(WidgetBP);
        ResultJson->SetBoolField(TEXT("saved"), WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP));

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("parentClass"), NewParentClass->GetPathName());
        ResultJson->SetStringField(TEXT("previousParentClass"), PreviousParent);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parent class to: %s"), *ParentClass));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Set parent class to: %s"), *ParentClass), ResultJson);
        return true;
    }

    return false;
}
}
