#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringPreview(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.8 Utility (continued)
    // =========================================================================

    if (SubAction.Equals(TEXT("preview_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // A preview means the designer is actually shown (dogfood #192): open the asset editor.
        UAssetEditorSubsystem* AssetEditors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
        const bool bOpened = AssetEditors && AssetEditors->OpenEditorForAsset(WidgetBP);
        ResultJson->SetBoolField(TEXT("success"), bOpened);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetBoolField(TEXT("editorOpened"), bOpened);
        if (!bOpened)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("The Widget Blueprint Editor could not be opened for this asset"), TEXT("EDITOR_OPEN_FAILED"));
            return true;
        }
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget Blueprint opened in the Widget Blueprint Editor; the Designer tab shows the preview"), ResultJson);
        return true;
    }

    return false;
}
}
