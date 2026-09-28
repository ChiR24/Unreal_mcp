#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

bool UMcpAutomationBridgeSubsystem::HandleManageWidgetAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_widget_authoring"))
    {
        return false;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }

    // Normalize parameter aliases once at the dispatch boundary.
    //
    // The add_content_widget family documents `componentName` ("Name for the SCS
    // component node"), but every widget-template/panel/visual handler reads
    // `slotName` and silently fell back to its generic default (TextBlock, Image,
    // Button, ...) when only componentName was supplied — so a caller asking for
    // "ScoreText" got a slot literally named "TextBlock". Aliasing the field here
    // keeps all 80+ downstream readers working without touching each one.
    if (Payload.IsValid() && !Payload->HasField(TEXT("slotName")))
    {
        FString ComponentNameAlias;
        if (Payload->TryGetStringField(TEXT("componentName"), ComponentNameAlias) &&
            !ComponentNameAlias.IsEmpty())
        {
            Payload->SetStringField(TEXT("slotName"), ComponentNameAlias);
        }
    }

    // Accept `name` + `path`/`folder` wherever a handler expects `widgetPath`.
    if (Payload.IsValid() && !Payload->HasField(TEXT("widgetPath")))
    {
        FString TemplateName;
        if (Payload->TryGetStringField(TEXT("name"), TemplateName) &&
            !TemplateName.IsEmpty())
        {
            FString TemplateFolder;
            if (!Payload->TryGetStringField(TEXT("folder"), TemplateFolder) ||
                TemplateFolder.IsEmpty())
            {
                Payload->TryGetStringField(TEXT("path"), TemplateFolder);
            }
            if (!TemplateFolder.IsEmpty() && !TemplateFolder.StartsWith(TEXT("/")))
            {
                TemplateFolder = TEXT("/Game/") + TemplateFolder;
            }
            if (!TemplateFolder.IsEmpty())
            {
                Payload->SetStringField(TEXT("widgetPath"),
                                        TemplateFolder / TemplateName);
            }
        }
    }

    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    using namespace WidgetAuthoringHandlers;
    static constexpr FWidgetAuthoringActionHandler Handlers[] = {
        HandleWidgetAuthoringCreation,
        HandleWidgetAuthoringTypedComponents,
        HandleWidgetAuthoringTypedPanels,
        HandleWidgetAuthoringInfo,
        HandleWidgetAuthoringCanvasSlotGeometry,
        HandleWidgetAuthoringSlotAppearance,
        HandleWidgetAuthoringStyleClipping,
        HandleWidgetAuthoringEventBindings,
        HandleWidgetAuthoringAnimationCore,
        HandleWidgetAuthoringPreview,
        HandleWidgetAuthoringGenericComponent,
        HandleWidgetAuthoringManipulation,
        HandleWidgetAuthoringAdvancedStyling,
        HandleWidgetAuthoringAnimationQueries,
        HandleWidgetAuthoringLocalization,
        HandleWidgetAuthoringPropertyBindings
    };

    for (FWidgetAuthoringActionHandler Handler : Handlers)
    {
        if (Handler(*this, RequestId, SubAction, Payload, RequestingSocket, ResultJson))
        {
            return true;
        }
    }

    return false;
}
