#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

bool UEngineWireSubsystem::HandleManageWidgetAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // The handler table routes manage_widget_authoring here with the sub-action in its payload.
    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    // Accept `name` + `path`/`folder` wherever a handler expects `widgetPath`.
    if (!Payload->HasField(TEXT("widgetPath")))
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
            if (!TemplateFolder.IsEmpty())
            {
                TemplateFolder = McpRootedContentPath(TemplateFolder);
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
        HandleWidgetAuthoringAudit,
        HandleWidgetAuthoringArrange,
        HandleWidgetAuthoringGenericComponent,
        HandleWidgetAuthoringManipulation,
        HandleWidgetAuthoringDuplicate,
        HandleWidgetAuthoringAdvancedStyling,
        HandleWidgetAuthoringAnimationQueries,
        HandleWidgetAuthoringLocalization,
        HandleWidgetAuthoringHudElements,
        HandleWidgetAuthoringTreeBuild,
        HandleWidgetAuthoringScreens,
        HandleWidgetAuthoringPropertyBindings,
        HandleWidgetAuthoringViewModels
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
