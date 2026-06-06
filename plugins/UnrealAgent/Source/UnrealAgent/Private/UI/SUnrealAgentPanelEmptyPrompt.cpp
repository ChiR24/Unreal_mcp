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


TSharedRef<SWidget> SUnrealAgentPanel::MakeEmptyPromptArea()
{
    return SNew(SVerticalBox)
        .Visibility(this, &SUnrealAgentPanel::GetInitialComposerVisibility)
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNullWidget::NullWidget
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        .Padding(FMargin(8.0f, 0.0f, 0.0f, 8.0f))
        [
            SNew(SBorder)
            .BorderImage(GetHeaderBrush())
            .Padding(FMargin(12.0f, 10.0f))
            [
                SNew(SVerticalBox)
                .Tag(FName(TEXT("UnrealAgent.EmptyState")))
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("EmptyStateTitle", "What should Unreal Agent help with?"))
                        .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 3.0f, 0.0f, 8.0f))
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("EmptyStateSubtitle", "Start with a focused editor task, or type your own prompt below."))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                        .AutoWrapText(true)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Fill)
                    [
                        SNew(SVerticalBox)
                        .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPromptGrid")))
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(FMargin(0.0f, 0.0f, 6.0f, 6.0f))
                            [
                                SNew(SButton)
                                .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.ArchitectureReview")))
                                .HAlign(HAlign_Center)
                                .ContentPadding(FMargin(6.0f, 3.0f))
                                .Text(LOCTEXT("QuickPromptArchitectureReview", "Architecture review"))
                                .OnClicked(this, &SUnrealAgentPanel::OnQuickPromptClicked, FString(TEXT("Act as Unreal Agent's production technical director. Inspect available project context and, when connected, use unreal-engine MCP tools such as manage_tools and inspect before making editor-state claims. Review architecture for a shippable game: modules, GameMode/GameState/Pawn/Controller/HUD/GameInstance ownership, C++ versus Blueprint boundaries, save/load, multiplayer, asset/content conventions, performance risks, and the smallest reversible next steps.")))
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(FMargin(6.0f, 0.0f, 0.0f, 6.0f))
                            [
                                SNew(SButton)
                                .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.GameplayPlan")))
                                .HAlign(HAlign_Center)
                                .ContentPadding(FMargin(6.0f, 3.0f))
                                .Text(LOCTEXT("QuickPromptGameplayPlan", "Gameplay plan"))
                                .OnClicked(this, &SUnrealAgentPanel::OnQuickPromptClicked, FString(TEXT("Create a complete Unreal game production plan for the concept I describe next. Start from the smallest playable prototype, then vertical slice, production, polish, and release readiness. Map each phase to concrete MCP tool domains for assets, Blueprints, levels, actors, UI, AI, audio, VFX, animation, combat, networking, inventory, interaction, tests, screenshots, profiling, and packaging. Include acceptance criteria, non-goals, risks, and what you need to inspect before implementation.")))
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(FMargin(0.0f, 6.0f, 6.0f, 0.0f))
                            [
                                SNew(SButton)
                                .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.QARiskPass")))
                                .HAlign(HAlign_Center)
                                .ContentPadding(FMargin(6.0f, 3.0f))
                                .Text(LOCTEXT("QuickPromptQARiskPass", "QA risk pass"))
                                .OnClicked(this, &SUnrealAgentPanel::OnQuickPromptClicked, FString(TEXT("Run a ship-readiness QA pass on the Unreal game, feature, or change I describe next. Define observable acceptance criteria, likely regressions, deterministic MCP/editor verification steps, PIE or automation coverage, screenshot/log evidence to collect, performance/accessibility/localization checks, and the release blockers that must be fixed before approval.")))
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(FMargin(6.0f, 6.0f, 0.0f, 0.0f))
                            [
                                SNew(SButton)
                                .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.EditorTooling")))
                                .HAlign(HAlign_Center)
                                .ContentPadding(FMargin(6.0f, 3.0f))
                                .Text(LOCTEXT("QuickPromptEditorTooling", "Editor tooling"))
                                .OnClicked(this, &SUnrealAgentPanel::OnQuickPromptClicked, FString(TEXT("Design or execute an MCP-backed Unreal editor automation workflow for the production task I describe next. Use manage_tools to confirm capabilities when connected, choose the safest tool domain, keep operations project-scoped and reversible, ask before destructive/bulk changes, and verify through deterministic editor inspection, asset compilation, screenshots, logs, tests, or build checks.")))
                            ]
                        ]
                    ]
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            MakeComposer(CenterPromptTextBox, CenterModelComboButton, CenterThinkingComboButton, CenterAgentComboButton, FName(TEXT("UnrealAgent.Composer.Center")))
        ];
}

#undef LOCTEXT_NAMESPACE
