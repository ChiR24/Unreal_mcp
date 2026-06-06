#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Styling/SlateTypes.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringHudElements(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("add_health_bar"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        double X = GetJsonNumberField(Payload, TEXT("x"), 20.0);
        double Y = GetJsonNumberField(Payload, TEXT("y"), 20.0);
        double Width = GetJsonNumberField(Payload, TEXT("width"), 200.0);
        double Height = GetJsonNumberField(Payload, TEXT("height"), 20.0);

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find parent panel
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }

        if (!Parent)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("No valid parent panel found"), TEXT("PARENT_NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // Create horizontal box to hold health bar components
        UHorizontalBox* HealthBox = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("HealthBarContainer"));
        Parent->AddChild(HealthBox);

        // Add health icon/label
        UTextBlock* HealthLabel = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("HealthLabel"));
        HealthLabel->SetText(FText::FromString(TEXT("HP")));
        HealthBox->AddChild(HealthLabel);

        // Add progress bar for health
        UProgressBar* HealthProgress = CreateAndRegisterWidget<UProgressBar>(WidgetBP, WidgetBP->WidgetTree, TEXT("HealthBar"));
        HealthProgress->SetPercent(1.0f);
        HealthProgress->SetFillColorAndOpacity(FLinearColor(0.8f, 0.1f, 0.1f, 1.0f));
        HealthBox->AddChild(HealthProgress);

        // Set position if parent is canvas panel
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(HealthBox->Slot))
            {
                Slot->SetPosition(FVector2D(X, Y));
                Slot->SetSize(FVector2D(Width, Height));
            }
        }

        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), TEXT("HealthBarContainer"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Health bar added"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_crosshair"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        double Size = GetJsonNumberField(Payload, TEXT("size"), 32.0);

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find parent panel
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }

        if (!Parent)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("No valid parent panel found"), TEXT("PARENT_NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // Create crosshair image (uses a simple text-based crosshair, user can swap for image)
        UTextBlock* Crosshair = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("Crosshair"));
        Crosshair->SetText(FText::FromString(TEXT("+")));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = Crosshair->GetFont();
#else
        FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
        FontInfo.Size = static_cast<int32>(Size);
        Crosshair->SetFont(FontInfo);
        Crosshair->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Parent->AddChild(Crosshair);

        // Center the crosshair if parent is canvas panel
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Crosshair->Slot))
            {
                Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
                Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            }
        }

        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), TEXT("Crosshair"));
        ResultJson->SetStringField(TEXT("note"), TEXT("Simple crosshair added. Replace with Image widget and crosshair texture for custom appearance."));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Crosshair added"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_ammo_counter"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }

        if (!Parent)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("No valid parent panel found"), TEXT("PARENT_NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // Create ammo counter text
        UTextBlock* AmmoText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("AmmoCounter"));
        AmmoText->SetText(FText::FromString(TEXT("30 / 90")));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = AmmoText->GetFont();
#else
        // UE 5.0: Access Font property directly
        FSlateFontInfo FontInfo = AmmoText->Font;
#endif
        FontInfo.Size = 24;
        AmmoText->SetFont(FontInfo);
        Parent->AddChild(AmmoText);

        // Position at bottom right if canvas
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(AmmoText->Slot))
            {
                Slot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
                Slot->SetAlignment(FVector2D(1.0f, 1.0f));
                Slot->SetPosition(FVector2D(-20.0f, -20.0f));
            }
        }

        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), TEXT("AmmoCounter"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ammo counter added"), ResultJson);
        return true;
    }

    // Note: Detailed implementations for create_settings_menu, create_loading_screen,
    // add_minimap, add_compass, add_interaction_prompt, add_objective_tracker,
    // add_damage_indicator, create_inventory_ui, create_dialog_widget, create_radial_menu
    // are located in section 19.10 onwards.


    return false;
}
}
