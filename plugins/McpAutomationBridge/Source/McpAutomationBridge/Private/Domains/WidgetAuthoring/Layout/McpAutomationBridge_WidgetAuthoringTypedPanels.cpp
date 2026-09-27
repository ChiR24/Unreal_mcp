#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/GridPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Types/SlateEnums.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

namespace
{
FMargin ReadMargin(const TSharedPtr<FJsonObject>& Object)
{
    return FMargin(GetJsonNumberField(Object, TEXT("left"), 0.0), GetJsonNumberField(Object, TEXT("top"), 0.0),
                   GetJsonNumberField(Object, TEXT("right"), 0.0), GetJsonNumberField(Object, TEXT("bottom"), 0.0));
}

// Case-insensitive lookup of a payload string in a {name, value} table; the
// value is left untouched when the field is absent or names no entry.
template <typename TEnum, int32 N>
bool ReadNamed(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, const TPair<const TCHAR*, TEnum> (&Table)[N], TEnum& Out)
{
    const FString Value = GetJsonStringField(Payload, Field, TEXT(""));
    for (const TPair<const TCHAR*, TEnum>& Entry : Table)
    {
        if (Value.Equals(Entry.Key, ESearchCase::IgnoreCase))
        {
            Out = Entry.Value;
            return true;
        }
    }
    return false;
}
}

// The typed panel widgets: each is AddConfiguredWidget plus its own setters.
bool HandleWidgetAuthoringTypedPanels(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    const auto Add = [&](UClass* Class, const TCHAR* DefaultSlot, const TCHAR* Label, TFunctionRef<void(UWidget*)> Configure)
    {
        return AddConfiguredWidget(Subsystem, RequestId, Payload, RequestingSocket, ResultJson, Class, DefaultSlot, Label, Configure);
    };
    const auto ReadFloat = [&Payload](const TCHAR* Field, double Default, TFunctionRef<void(float)> Apply)
    {
        if (Payload->HasField(Field))
        {
            Apply(static_cast<float>(GetJsonNumberField(Payload, Field, Default)));
        }
    };
    const auto None = [](UWidget*) {};

    if (SubAction.Equals(TEXT("add_canvas_panel"), ESearchCase::IgnoreCase)) return Add(UCanvasPanel::StaticClass(), TEXT("CanvasPanel"), TEXT("canvas panel"), None);
    if (SubAction.Equals(TEXT("add_horizontal_box"), ESearchCase::IgnoreCase)) return Add(UHorizontalBox::StaticClass(), TEXT("HorizontalBox"), TEXT("horizontal box"), None);
    if (SubAction.Equals(TEXT("add_vertical_box"), ESearchCase::IgnoreCase)) return Add(UVerticalBox::StaticClass(), TEXT("VerticalBox"), TEXT("vertical box"), None);
    if (SubAction.Equals(TEXT("add_overlay"), ESearchCase::IgnoreCase)) return Add(UOverlay::StaticClass(), TEXT("Overlay"), TEXT("overlay"), None);

    if (SubAction.Equals(TEXT("add_safe_zone"), ESearchCase::IgnoreCase))
    {
        return Add(USafeZone::StaticClass(), TEXT("SafeZone"), TEXT("safe zone"), [](UWidget* Widget) { Widget->bIsVariable = true; });
    }
    if (SubAction.Equals(TEXT("add_spacer"), ESearchCase::IgnoreCase))
    {
        const float SizeX = GetJsonNumberField(Payload, TEXT("sizeX"), 100.0f);
        const float SizeY = GetJsonNumberField(Payload, TEXT("sizeY"), 100.0f);
        return Add(USpacer::StaticClass(), TEXT("Spacer"), TEXT("spacer"), [&](UWidget* Widget)
        {
            Widget->bIsVariable = true;
            CastChecked<USpacer>(Widget)->SetSize(FVector2D(SizeX, SizeY));
            ResultJson->SetNumberField(TEXT("sizeX"), SizeX);
            ResultJson->SetNumberField(TEXT("sizeY"), SizeY);
        });
    }
    if (SubAction.Equals(TEXT("add_widget_switcher"), ESearchCase::IgnoreCase))
    {
        const int32 ActiveIndex = GetJsonIntField(Payload, TEXT("activeIndex"), 0);
        return Add(UWidgetSwitcher::StaticClass(), TEXT("WidgetSwitcher"), TEXT("widget switcher"), [&](UWidget* Widget)
        {
            Widget->bIsVariable = true;
            CastChecked<UWidgetSwitcher>(Widget)->SetActiveWidgetIndex(ActiveIndex);
            ResultJson->SetNumberField(TEXT("activeIndex"), ActiveIndex);
        });
    }
    if (SubAction.Equals(TEXT("add_border"), ESearchCase::IgnoreCase))
    {
        return Add(UBorder::StaticClass(), TEXT("Border"), TEXT("border"), [&](UWidget* Widget)
        {
            UBorder* Border = CastChecked<UBorder>(Widget);
            if (Payload->HasTypedField<EJson::Object>(TEXT("brushColor")))
            {
                Border->SetBrushColor(ExtractLinearColorField(Payload, TEXT("brushColor"), FLinearColor::White));
            }
            if (Payload->HasTypedField<EJson::Object>(TEXT("contentColorAndOpacity")))
            {
                Border->SetContentColorAndOpacity(ExtractLinearColorField(Payload, TEXT("contentColorAndOpacity"), FLinearColor::White));
            }
            if (Payload->HasTypedField<EJson::Object>(TEXT("padding")))
            {
                Border->SetPadding(ReadMargin(Payload->GetObjectField(TEXT("padding"))));
            }
        });
    }
    if (SubAction.Equals(TEXT("add_grid_panel"), ESearchCase::IgnoreCase))
    {
        return Add(UGridPanel::StaticClass(), TEXT("GridPanel"), TEXT("grid panel"), [&](UWidget*)
        {
            // A UGridPanel has no column/row count: its extent comes from the
            // Row/Column each child slot claims, so say the fields were ignored.
            if (Payload->HasField(TEXT("columnCount")) || Payload->HasField(TEXT("rowCount")))
            {
                ResultJson->SetBoolField(TEXT("gridSizeApplied"), false);
                ResultJson->SetStringField(TEXT("gridSizeNote"), TEXT("columnCount/rowCount were ignored: a GridPanel sizes itself from the Row and Column its children claim. Set those on each child's slot."));
            }
        });
    }
    if (SubAction.Equals(TEXT("add_uniform_grid"), ESearchCase::IgnoreCase))
    {
        return Add(UUniformGridPanel::StaticClass(), TEXT("UniformGridPanel"), TEXT("uniform grid panel"), [&](UWidget* Widget)
        {
            UUniformGridPanel* Grid = CastChecked<UUniformGridPanel>(Widget);
            const TSharedPtr<FJsonObject> Padding = GetObjectField(Payload, TEXT("slotPadding"));
            if (Padding.IsValid())
            {
                Grid->SetSlotPadding(ReadMargin(Padding));
            }
            ReadFloat(TEXT("minDesiredSlotWidth"), 0.0, [Grid](float V) { Grid->SetMinDesiredSlotWidth(V); });
            ReadFloat(TEXT("minDesiredSlotHeight"), 0.0, [Grid](float V) { Grid->SetMinDesiredSlotHeight(V); });
        });
    }
    if (SubAction.Equals(TEXT("add_wrap_box"), ESearchCase::IgnoreCase))
    {
        return Add(UWrapBox::StaticClass(), TEXT("WrapBox"), TEXT("wrap box"), [&](UWidget* Widget)
        {
            UWrapBox* WrapBox = CastChecked<UWrapBox>(Widget);
            const TSharedPtr<FJsonObject> Padding = GetObjectField(Payload, TEXT("innerSlotPadding"));
            if (Padding.IsValid())
            {
                WrapBox->SetInnerSlotPadding(FVector2D(GetJsonNumberField(Padding, TEXT("x"), 0.0), GetJsonNumberField(Padding, TEXT("y"), 0.0)));
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            ReadFloat(TEXT("wrapSize"), 0.0, [WrapBox](float V) { WrapBox->SetWrapSize(V); });
#endif
        });
    }
    if (SubAction.Equals(TEXT("add_scroll_box"), ESearchCase::IgnoreCase))
    {
        return Add(UScrollBox::StaticClass(), TEXT("ScrollBox"), TEXT("scroll box"), [&](UWidget* Widget)
        {
            UScrollBox* ScrollBox = CastChecked<UScrollBox>(Widget);
            const bool bHorizontal = GetJsonStringField(Payload, TEXT("orientation"), TEXT("Vertical")).Equals(TEXT("Horizontal"), ESearchCase::IgnoreCase);
            ScrollBox->SetOrientation(bHorizontal ? EOrientation::Orient_Horizontal : EOrientation::Orient_Vertical);
            static const TPair<const TCHAR*, ESlateVisibility> Visibilities[] = {
                {TEXT("Visible"), ESlateVisibility::Visible}, {TEXT("Collapsed"), ESlateVisibility::Collapsed}, {TEXT("Hidden"), ESlateVisibility::Hidden}};
            ESlateVisibility Visibility;
            if (ReadNamed(Payload, TEXT("scrollBarVisibility"), Visibilities, Visibility))
            {
                ScrollBox->SetScrollBarVisibility(Visibility);
            }
            if (Payload->HasField(TEXT("alwaysShowScrollbar")))
            {
                ScrollBox->SetAlwaysShowScrollbar(GetJsonBoolField(Payload, TEXT("alwaysShowScrollbar")));
            }
        });
    }
    if (SubAction.Equals(TEXT("add_size_box"), ESearchCase::IgnoreCase))
    {
        return Add(USizeBox::StaticClass(), TEXT("SizeBox"), TEXT("size box"), [&](UWidget* Widget)
        {
            USizeBox* SizeBox = CastChecked<USizeBox>(Widget);
            ReadFloat(TEXT("widthOverride"), 100.0, [SizeBox](float V) { SizeBox->SetWidthOverride(V); });
            ReadFloat(TEXT("heightOverride"), 100.0, [SizeBox](float V) { SizeBox->SetHeightOverride(V); });
            ReadFloat(TEXT("minDesiredWidth"), 0.0, [SizeBox](float V) { SizeBox->SetMinDesiredWidth(V); });
            ReadFloat(TEXT("minDesiredHeight"), 0.0, [SizeBox](float V) { SizeBox->SetMinDesiredHeight(V); });
            ReadFloat(TEXT("maxDesiredWidth"), 0.0, [SizeBox](float V) { SizeBox->SetMaxDesiredWidth(V); });
            ReadFloat(TEXT("maxDesiredHeight"), 0.0, [SizeBox](float V) { SizeBox->SetMaxDesiredHeight(V); });
        });
    }
    if (SubAction.Equals(TEXT("add_scale_box"), ESearchCase::IgnoreCase))
    {
        return Add(UScaleBox::StaticClass(), TEXT("ScaleBox"), TEXT("scale box"), [&](UWidget* Widget)
        {
            UScaleBox* ScaleBox = CastChecked<UScaleBox>(Widget);
            static const TPair<const TCHAR*, EStretch::Type> Stretches[] = {
                {TEXT("None"), EStretch::None}, {TEXT("Fill"), EStretch::Fill}, {TEXT("ScaleToFit"), EStretch::ScaleToFit},
                {TEXT("ScaleToFitX"), EStretch::ScaleToFitX}, {TEXT("ScaleToFitY"), EStretch::ScaleToFitY},
                {TEXT("ScaleToFill"), EStretch::ScaleToFill}, {TEXT("UserSpecified"), EStretch::UserSpecified}};
            EStretch::Type Stretch;
            if (ReadNamed(Payload, TEXT("stretch"), Stretches, Stretch))
            {
                ScaleBox->SetStretch(Stretch);
                if (Stretch == EStretch::UserSpecified)
                {
                    ReadFloat(TEXT("userSpecifiedScale"), 1.0, [ScaleBox](float V) { ScaleBox->SetUserSpecifiedScale(V); });
                }
            }
            static const TPair<const TCHAR*, EStretchDirection::Type> Directions[] = {
                {TEXT("Both"), EStretchDirection::Both}, {TEXT("DownOnly"), EStretchDirection::DownOnly}, {TEXT("UpOnly"), EStretchDirection::UpOnly}};
            EStretchDirection::Type Direction;
            if (ReadNamed(Payload, TEXT("stretchDirection"), Directions, Direction))
            {
                ScaleBox->SetStretchDirection(Direction);
            }
        });
    }
    return false;
}
}
