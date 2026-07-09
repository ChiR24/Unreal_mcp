#include "UI/Core/SUnrealAgentPanel.h"
#include "UI/Core/SUnrealAgentPanelPrivate.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
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


TSharedRef<SWidget> SUnrealAgentPanel::MakeComposerInputFrame(TSharedPtr<SMultiLineEditableTextBox>& OutPromptTextBox)
{
    TSharedPtr<SVerticalBox> AffordanceList;
    TSharedRef<SScrollBox> AffordanceScrollBox = SNew(SScrollBox)
        .ScrollBarThickness(FVector2D(4.0f, 4.0f));
    AffordanceScrollBox->AddSlot()
    [
        SAssignNew(AffordanceList, SVerticalBox)
        .Tag(FName(TEXT("UnrealAgent.Composer.AffordanceList")))
    ];
    TSharedRef<SWidget> InputFrame = SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBox)
            .HeightOverride(84.0f)
            [
                SNew(SOverlay)
                .Tag(FName(TEXT("UnrealAgent.Composer.InputFrame")))
                + SOverlay::Slot()
                [
                    SAssignNew(OutPromptTextBox, SMultiLineEditableTextBox)
                    .Tag(FName(TEXT("UnrealAgent.Composer.Input")))
                    .HintText(LOCTEXT("PromptHint", "Ask anything... type / for commands or @ to attach a file"))
                    .AutoWrapText(true)
                    .WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
                    .AllowMultiLine(true)
                    .ClearKeyboardFocusOnCommit(false)
                    .Padding(FMargin(7.0f, 6.0f, 38.0f, 8.0f))
                    .HScrollBar(MakeHiddenScrollBar(Orient_Horizontal))
                    .VScrollBar(MakeHiddenScrollBar(Orient_Vertical))
                    .HScrollBarPadding(FMargin(0.0f))
                    .VScrollBarPadding(FMargin(0.0f))
                    .OnTextChanged(this, &SUnrealAgentPanel::OnPromptTextChanged)
                    .OnKeyDownHandler(this, &SUnrealAgentPanel::OnPromptKeyDown)
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Bottom)
                .Padding(FMargin(0.0f, 0.0f, 6.0f, 6.0f))
                [
                    SNew(SBox)
                    .WidthOverride(36.0f)
                    .HeightOverride(28.0f)
                    [
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Composer.SendButton")))
                        .ToolTipText(this, &SUnrealAgentPanel::GetComposerHelperText)
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .IsEnabled_Lambda([this]()
                        {
                            return (AcpClient.IsValid() && AcpClient->IsPromptInFlight() && !AcpClient->IsCancelRequested()) || CanSendPrompt();
                        })
                        .OnClicked(this, &SUnrealAgentPanel::OnSendClicked)
                        [
                            SNew(SImage)
                            .Image(this, &SUnrealAgentPanel::GetSendButtonIconBrush)
                            .ColorAndOpacity(this, &SUnrealAgentPanel::GetSendButtonIconColor)
                            .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                        ]
                    ]
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .MaxHeight(180.0f)
        .Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
        [
            AffordanceScrollBox
        ];

    ComposerAffordanceLists.Add(AffordanceList);
    return InputFrame;
}

#undef LOCTEXT_NAMESPACE
