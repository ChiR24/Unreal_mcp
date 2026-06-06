#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/GridPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ListView.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
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
    // =========================================================================
    // 19.9 Generic Widget Actions (3 new actions)
    // =========================================================================

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

        // Find parent panel
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
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
            // Create a canvas panel as root if none exists
            Parent = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
            WidgetBP->WidgetTree->RootWidget = Parent;
            RegisterWidgetGuid(WidgetBP, Parent);
        }

        // Map component type to UWidget class
        UClass* WidgetClass = nullptr;

        // Common widget types
        if (ComponentType.Equals(TEXT("TextBlock"), ESearchCase::IgnoreCase) ||
            ComponentType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UTextBlock::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UButton::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UImage::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ProgressBar"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UProgressBar::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Slider"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USlider::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("CheckBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UCheckBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("EditableText"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UEditableText::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("EditableTextBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UEditableTextBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ComboBox"), ESearchCase::IgnoreCase) ||
                 ComponentType.Equals(TEXT("ComboBoxString"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UComboBoxString::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("SpinBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USpinBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("CanvasPanel"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UCanvasPanel::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("HorizontalBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UHorizontalBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("VerticalBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UVerticalBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("GridPanel"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UGridPanel::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("UniformGridPanel"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UUniformGridPanel::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Overlay"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UOverlay::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("SizeBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USizeBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ScaleBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UScaleBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Border"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UBorder::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Spacer"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USpacer::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ScrollBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UScrollBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("WidgetSwitcher"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UWidgetSwitcher::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ListView"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UListView::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("TileView"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UTileView::StaticClass();
        }
        else
        {
            // Try to find by class name
            FString ClassName = TEXT("U") + ComponentType;
            WidgetClass = FindObject<UClass>(nullptr, *ClassName);
            if (!WidgetClass)
            {
                // Try with Widget suffix
                ClassName = TEXT("U") + ComponentType + TEXT("Widget");
                WidgetClass = FindObject<UClass>(nullptr, *ClassName);
            }
        }

        if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unknown widget type: %s"), *ComponentType), TEXT("UNKNOWN_TYPE"));
            return true;
        }

        // Create the widget
        UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, *ComponentName);
        if (!NewWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct widget"), TEXT("CREATION_FAILED"));
            return true;
        }
        RegisterWidgetGuid(WidgetBP, NewWidget);

        // Add to parent
        Parent->AddChild(NewWidget);

        // Configure slot if canvas panel
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(NewWidget->Slot))
            {
                float PosX = static_cast<float>(GetJsonNumberField(Payload, TEXT("positionX"), 0.0));
                float PosY = static_cast<float>(GetJsonNumberField(Payload, TEXT("positionY"), 0.0));
                float SizeX = static_cast<float>(GetJsonNumberField(Payload, TEXT("sizeX"), 0.0));
                float SizeY = static_cast<float>(GetJsonNumberField(Payload, TEXT("sizeY"), 0.0));

                if (PosX != 0.0f || PosY != 0.0f)
                {
                    Slot->SetPosition(FVector2D(PosX, PosY));
                }
                if (SizeX > 0.0f && SizeY > 0.0f)
                {
                    Slot->SetSize(FVector2D(SizeX, SizeY));
                    Slot->SetAutoSize(false);
                }
            }
        }

        // Set initial text if TextBlock
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
        ResultJson->SetStringField(TEXT("parentName"), Parent->GetName());

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget component added"), ResultJson);
        return true;
    }

    return false;
}
}
