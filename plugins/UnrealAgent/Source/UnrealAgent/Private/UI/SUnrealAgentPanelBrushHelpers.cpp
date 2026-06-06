#include "SUnrealAgentPanelPrivate.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SScrollBar.h"

#define LOCTEXT_NAMESPACE "SUnrealAgentPanel"

namespace UnrealAgent::Panel
{
const FSlateBrush* GetModelComboOutlineBrush()
    {
        static const FSlateRoundedBoxBrush OutlineBrush(
            FStyleColors::Input,
            4.0f,
            FStyleColors::InputOutline,
            1.0f);
        return &OutlineBrush;
    }

const FSlateBrush* GetModelProviderHeaderBrush()
    {
        static const FSlateRoundedBoxBrush HeaderBrush(
            FStyleColors::Header,
            2.0f,
            FStyleColors::Recessed,
            1.0f);
        return &HeaderBrush;
    }

const FSlateBrush* GetUserTranscriptBrush()
    {
        static const FSlateRoundedBoxBrush UserBrush(
            FStyleColors::Input,
            4.0f,
            FStyleColors::InputOutline,
            1.0f);
        return &UserBrush;
    }

const FSlateBrush* GetActivityTranscriptBrush()
    {
        static const FSlateRoundedBoxBrush ActivityBrush(
            FStyleColors::Recessed,
            3.0f,
            FStyleColors::Header,
            1.0f);
        return &ActivityBrush;
    }

const FSlateBrush* GetSidebarBrush()
    {
        static const FSlateRoundedBoxBrush SidebarBrush(
            FStyleColors::Panel,
            0.0f,
            FStyleColors::Recessed,
            1.0f);
        return &SidebarBrush;
    }

const FSlateBrush* GetSidebarActiveChatBrush()
    {
        static const FSlateRoundedBoxBrush ActiveChatBrush(
            FStyleColors::Select,
            3.0f,
            FStyleColors::SelectHover,
            1.0f);
        return &ActiveChatBrush;
    }

const FSlateBrush* GetSidebarInactiveChatBrush()
    {
        static const FSlateRoundedBoxBrush InactiveChatBrush(
            FStyleColors::Header,
            3.0f,
            FStyleColors::Recessed,
            1.0f);
        return &InactiveChatBrush;
    }

const FSlateBrush* GetHeaderBrush()
    {
        static const FSlateRoundedBoxBrush HeaderBrush(
            FStyleColors::Header,
            0.0f,
            FStyleColors::Recessed,
            1.0f);
        return &HeaderBrush;
    }

const FComboButtonStyle* GetTransparentModelComboButtonStyle()
    {
        static const FComboButtonStyle TransparentStyle = []()
        {
            FComboButtonStyle Style = FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton");
            return Style;
        }();
        return &TransparentStyle;
    }

const FSlateBrush* GetMenuSelectionBrush()
    {
        static const FSlateRoundedBoxBrush SelectionBrush(
            FStyleColors::Select,
            2.0f,
            FStyleColors::SelectHover,
            1.0f);
        return &SelectionBrush;
    }

TSharedRef<SScrollBar> MakeHiddenScrollBar(EOrientation Orientation)
    {
        return SNew(SScrollBar)
            .Orientation(Orientation)
            .Visibility(EVisibility::Collapsed)
            .Thickness(FVector2D(0.0f, 0.0f));
    }
}

#undef LOCTEXT_NAMESPACE
