#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringGenericComponent(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // add_widget_component - Generic action to add any UWidget-derived component
    if (SubAction.Equals(TEXT("add_widget_component"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString ComponentType = GetJsonStringField(Payload, TEXT("componentType"));
        if (ComponentType.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: componentType"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
        if (ComponentName.IsEmpty())
        {
            ComponentName = ComponentType + TEXT("_") + FGuid::NewGuid().ToString().Left(8);
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // parentName must name a panel; without it the root takes the widget (SafeAddWidgetToTree
        // makes a canvas root when there is none). A name that missed used to seat it at the root.
        FString ParentSlot;
        const FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        if (!ParentName.IsEmpty())
        {
            const UPanelWidget* Parent = Cast<UPanelWidget>(FindWidgetByName(WidgetBP->WidgetTree, ParentName));
            if (!Parent)
            {
                Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
                    TEXT("parentName '%s' is not a panel in '%s' (get_widget_info lists the tree)."), *ParentName, *WidgetPath),
                    TEXT("PARENT_NOT_FOUND"));
                return true;
            }
            ParentSlot = Parent->GetName();
        }

        // "Text" and "ComboBox" are shorthands; every other name resolves as a class:
        // a bare UMG/engine name ("TextBlock", "BackgroundBlur", any case), a /Script
        // path, or a Widget Blueprint path. Reflected names carry no "U" prefix.
        FString TypeName = ComponentType;
        if (TypeName.Equals(TEXT("Text"), ESearchCase::IgnoreCase)) TypeName = TEXT("TextBlock");
        else if (TypeName.Equals(TEXT("ComboBox"), ESearchCase::IgnoreCase)) TypeName = TEXT("ComboBoxString");
        UClass* WidgetClass = ResolveClassByName(TypeName);
        if (!WidgetClass && TypeName.StartsWith(TEXT("U")))
        {
            WidgetClass = ResolveClassByName(TypeName.Mid(1));
        }

        if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unknown widget type: %s"), *ComponentType), TEXT("UNKNOWN_TYPE"));
            return true;
        }

        const bool bExisted = WidgetBP->WidgetTree->FindWidget(FName(*ComponentName)) != nullptr;
        UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, *ComponentName);
        if (!NewWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct widget"), TEXT("CREATION_FAILED"));
            return true;
        }
        if (!SafeAddWidgetToTree(WidgetBP, NewWidget, ParentSlot, Payload))
        {
            if (!bExisted)
            {
                UnregisterWidgetGuid(WidgetBP, NewWidget);
                WidgetBP->WidgetTree->RemoveWidget(NewWidget);
            }
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add widget component to widget tree"), TEXT("TREE_ERROR"));
            return true;
        }

        if (UTextBlock* TextWidget = Cast<UTextBlock>(NewWidget))
        {
            FString InitialText = GetJsonStringField(Payload, TEXT("text"));
            if (!InitialText.IsEmpty())
            {
                TextWidget->SetText(FText::FromString(InitialText));
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("componentName"), ComponentName);
        ResultJson->SetStringField(TEXT("componentType"), WidgetClass->GetName());
        ResultJson->SetStringField(TEXT("parentName"), NewWidget->GetParent() ? NewWidget->GetParent()->GetName() : FString());

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget component added"), ResultJson);
        return true;
    }

    return false;
}
}
