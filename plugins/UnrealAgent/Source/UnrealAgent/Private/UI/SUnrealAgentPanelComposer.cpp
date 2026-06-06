#include "SUnrealAgentPanel.h"
#include "SUnrealAgentPanelPrivate.h"

#include "../Acp/McpOpenCodeAcpClient.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUnrealAgentPanel"

using namespace UnrealAgent::Panel;

TSharedRef<SWidget> SUnrealAgentPanel::MakeComposer(
    TSharedPtr<SMultiLineEditableTextBox>& OutPromptTextBox,
    TSharedPtr<SComboButton>& OutModelComboButton,
    TSharedPtr<SComboButton>& OutThinkingComboButton,
    TSharedPtr<SComboButton>& OutAgentComboButton,
    const FName& ComposerTag)
{
    TSharedRef<SVerticalBox> Composer = SNew(SVerticalBox)
        .Tag(ComposerTag)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 0.0f, 0.0f, 2.0f))
        [
            MakeComposerInputFrame(OutPromptTextBox)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 0.0f, 0.0f, 4.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                MakeComposerActionRow(OutModelComboButton, OutThinkingComboButton, OutAgentComboButton)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Fill)
            .Padding(FMargin(0.0f, 8.0f, 0.0f, 2.0f))
            [
                MakeComposerHelperRow()
            ]
        ];

    if (OutModelComboButton.IsValid())
    {
        OutModelComboButton->bShowMenuBackground = true;
    }
    if (OutThinkingComboButton.IsValid())
    {
        OutThinkingComboButton->bShowMenuBackground = true;
    }
    if (OutAgentComboButton.IsValid())
    {
        OutAgentComboButton->bShowMenuBackground = true;
    }

    return Composer;
}

#undef LOCTEXT_NAMESPACE
