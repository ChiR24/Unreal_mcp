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

#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "SUnrealAgentPanel"

using namespace UnrealAgent::Panel;

namespace
{
	// The default Slate font (DroidSansFallback) cannot render the colored-circle
	// emoji (U+1F7E0/1F7E1/1F7E2) and other Supplemental-Plane glyphs that
	// assistant messages sometimes contain, spamming "Could not find Glyph Index"
	// warnings when a chat history is opened. We clone the editor's normal text
	// composite font and add a bundled monochrome symbol/emoji font as a per-range
	// fallback so those codepoints resolve instead of falling through to last-resort.

	FString ResolveBundledEmojiFontPath()
	{
		FString FontPath;
		IPluginManager& PluginMgr = IPluginManager::Get();
		if (const TSharedPtr<IPlugin> Plugin = PluginMgr.FindPlugin(TEXT("UnrealAgent")))
		{
			FontPath = Plugin->GetBaseDir() / TEXT("Resources/Fonts/NotoSansSymbols2-Regular.ttf");
		}
		if (!FPaths::FileExists(FontPath))
		{
			// Editor-only safety net if the bundled asset is missing for any reason.
			FontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/NotoSansSymbols2-Regular.ttf");
		}
		return FontPath;
	}

	FSlateFontInfo MakeEmojiCapableBodyFont()
	{
		static FSlateFontInfo CachedFont;
		static bool bInitialized = false;
		if (bInitialized)
		{
			return CachedFont;
		}
		bInitialized = true;

		// Preserve the exact size/appearance of the default transcript text font.
		FSlateFontInfo BaseInfo = FAppStyle::Get().GetFontStyle(TEXT("NormalFont"));
		const FCompositeFont* BaseComposite = BaseInfo.GetCompositeFont();
		if (!BaseComposite)
		{
			BaseInfo = FAppStyle::Get().GetFontStyle(TEXT("SmallFont"));
			BaseComposite = BaseInfo.GetCompositeFont();
		}

		TSharedPtr<FCompositeFont> EmojiComposite = MakeShared<FStandaloneCompositeFont>();
		if (BaseComposite)
		{
			EmojiComposite->DefaultTypeface = BaseComposite->DefaultTypeface;
			EmojiComposite->FallbackTypeface = BaseComposite->FallbackTypeface;
		}
		else
		{
			const FString Roboto = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
			EmojiComposite->DefaultTypeface.AppendFont(FName(TEXT("Regular")), Roboto, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		}

		const FString EmojiFontPath = ResolveBundledEmojiFontPath();
		if (!EmojiFontPath.IsEmpty() && FPaths::FileExists(EmojiFontPath))
		{
			FCompositeSubFont EmojiSubFont;
			EmojiSubFont.Typeface.AppendFont(FName(TEXT("Regular")), EmojiFontPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			// Ranges DroidSansFallback lacks: emoji plane, misc symbols/dingbats, misc symbols and arrows.
			EmojiSubFont.CharacterRanges.Add(FInt32Range(0x1F000, 0x1FAFF));
			EmojiSubFont.CharacterRanges.Add(FInt32Range(0x2600, 0x27BF));
			EmojiSubFont.CharacterRanges.Add(FInt32Range(0x2B00, 0x2BFF));
			EmojiSubFont.ScalingFactor = 1.0f;
			EmojiComposite->SubTypefaces.Add(MoveTemp(EmojiSubFont));
		}

		CachedFont = BaseInfo;
		CachedFont.FontObject = nullptr;
		CachedFont.CompositeFont = EmojiComposite;
		return CachedFont;
	}
}

void SUnrealAgentPanel::AddTranscriptEntryImmediately(const FString& Role, const FString& Text)
{
    if (Role == TEXT("System") || !IsConversationRole(Role) || !TranscriptScrollBox.IsValid() || Text.IsEmpty())
    {
        return;
    }

    TSharedPtr<SHorizontalBox> EntryWidget;
    TSharedPtr<STextBlock> EntryTextBlock;
    const FLinearColor AccentColor = RoleAccentColor(Role);
    const bool bUserEntry = IsUserTranscriptRole(Role);
    const bool bActivityEntry = IsActivityTranscriptRole(Role);
    const bool bReasoningEntry = Role == TEXT("Thought");
    const EHorizontalAlignment EntryAlignment = bUserEntry ? HAlign_Right : HAlign_Fill;
    const ETextJustify::Type TextJustification = bUserEntry ? ETextJustify::Right : ETextJustify::Left;
    const FText RoleLabel = RoleLabelText(Role);
    const bool bShowRoleLabel = !RoleLabel.IsEmpty();
    const FString RawText = ClampTranscriptText(Text);
    const FString DisplayText = RenderTranscriptText(Role, RawText);

    if (bActivityEntry)
    {
        if (Role == TEXT("Tool") && !ParseToolActivityDisplay(RawText).bShouldShow)
        {
            return;
        }

        bHasConversationContent = true;
        if (!bRestoringChatHistory)
        {
            UpdateActiveChatSummary(Role, RawText, true);
        }

        if (LastTranscriptRole == TEXT("Activity") && ActiveActivityBodyBox.IsValid())
        {
            if (!ActiveReasoningStartedSeconds.IsValid() || *ActiveReasoningStartedSeconds <= 0.0)
            {
                ActiveReasoningStartedSeconds = MakeShared<double>(FPlatformTime::Seconds());
                ActiveReasoningEndSeconds = MakeShared<double>(0.0);
            }
            if (bReasoningEntry)
            {
                if (ActiveActivityHasReasoning.IsValid())
                {
                    *ActiveActivityHasReasoning = true;
                }
            }
            if (ActiveActivityUpdateCount.IsValid())
            {
                ++(*ActiveActivityUpdateCount);
            }

            AppendActivityEntryToActive(Role, RawText);
            TranscriptScrollBox->ScrollToEnd();
            return;
        }

        ResetActiveActivityState();
        ActiveReasoningStartedSeconds = MakeShared<double>(FPlatformTime::Seconds());
        ActiveReasoningEndSeconds = MakeShared<double>(0.0);
        ActiveActivityHasReasoning = MakeShared<bool>(bReasoningEntry);
        ActiveActivityUpdateCount = MakeShared<int32>(1);

        TranscriptScrollBox->AddSlot()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
        [
            SAssignNew(EntryWidget, SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .HAlign(HAlign_Fill)
            [
                SNew(SBox)
                .MaxDesiredWidth(TranscriptEntryWidth)
                [
                    SNew(SExpandableArea)
                    .Tag(FName(TEXT("UnrealAgent.Transcript.Working")))
                    .InitiallyCollapsed(true)
                    .AllowAnimatedTransition(false)
                    .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                    .BodyBorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                    .BorderBackgroundColor(FStyleColors::Transparent)
                    .BodyBorderBackgroundColor(FStyleColors::Transparent)
                    .HeaderPadding(FMargin(0.0f))
                    .Padding(FMargin(18.0f, 6.0f, 0.0f, 0.0f))
                    .HeaderContent()
                    [
                        SNew(STextBlock)
                        .Tag(FName(TEXT("UnrealAgent.Transcript.Working.Header")))
                        .Text_Lambda([StartedAtSeconds = ActiveReasoningStartedSeconds, EndSeconds = ActiveReasoningEndSeconds, bHasReasoning = ActiveActivityHasReasoning, UpdateCount = ActiveActivityUpdateCount]()
                        {
                            return ActivityTitleText(StartedAtSeconds, EndSeconds, bHasReasoning, UpdateCount);
                        })
                        .Font(FAppStyle::Get().GetFontStyle("SmallFont"))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    .BodyContent()
                    [
                        SAssignNew(ActiveActivityBodyBox, SVerticalBox)
                        .Tag(FName(TEXT("UnrealAgent.Transcript.Working.Body")))
                    ]
                ]
            ]
        ];

        TranscriptEntryWidgets.Add(EntryWidget);
        LastTranscriptRole = TEXT("Activity");
        AppendActivityEntryToActive(Role, RawText);
        TrimTranscriptHistory();
        TranscriptScrollBox->ScrollToEnd();
        return;
    }

    bHasConversationContent = true;

    if (LastTranscriptRole == TEXT("Activity"))
    {
        FinalizeActiveReasoning();
    }

    if (ShouldAppendToLastTranscriptEntry(Role))
    {
        LastTranscriptText = ClampTranscriptText(LastTranscriptText + Text);
        LastTranscriptTextBlock->SetText(FText::FromString(RenderTranscriptText(Role, LastTranscriptText)));
        if (!bRestoringChatHistory)
        {
            UpdateActiveChatSummary(Role, LastTranscriptText, false);
        }
        TranscriptScrollBox->ScrollToEnd();
        return;
    }

    if (!bRestoringChatHistory)
    {
        UpdateActiveChatSummary(Role, RawText, true);
    }

    const FName EntryBorderTag = bUserEntry ? FName(TEXT("UnrealAgent.Transcript.UserBubble")) : FName();
    const FName EntryTextTag = bUserEntry
        ? FName(TEXT("UnrealAgent.Transcript.UserText"))
        : Role == TEXT("OpenCode")
            ? FName(TEXT("UnrealAgent.Transcript.AssistantText"))
            : FName(TEXT("UnrealAgent.Transcript.Text"));

    TranscriptScrollBox->AddSlot()
    .Padding(FMargin(0.0f, 0.0f, 0.0f, 14.0f))
    [
        SAssignNew(EntryWidget, SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .HAlign(EntryAlignment)
        [
            SNew(SBox)
            .MaxDesiredWidth(bUserEntry ? UserTranscriptMaxWidth : TranscriptEntryWidth)
            [
                SNew(SBorder)
                .Tag(EntryBorderTag)
                .BorderImage(bUserEntry ? GetUserTranscriptBrush() : FCoreStyle::Get().GetBrush("NoBrush"))
                .Padding(bUserEntry ? FMargin(12.0f, 8.0f) : FMargin(0.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBox)
                        .Visibility(bShowRoleLabel ? EVisibility::Visible : EVisibility::Collapsed)
                        [
                            SNew(STextBlock)
                            .Text(RoleLabel)
                            .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                            .ColorAndOpacity(AccentColor)
                            .Justification(TextJustification)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(bShowRoleLabel ? FMargin(0.0f, 4.0f, 0.0f, 0.0f) : FMargin(0.0f))
                    [
                        SAssignNew(EntryTextBlock, STextBlock)
                        .Tag(EntryTextTag)
                        .Text(FText::FromString(DisplayText))
                        .Font(MakeEmojiCapableBodyFont())
                        .ColorAndOpacity(FSlateColor::UseForeground())
                        .AutoWrapText(true)
                        .WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
                        .Justification(TextJustification)
                    ]
                ]
            ]
        ]
    ];

    TranscriptEntryWidgets.Add(EntryWidget);
    LastTranscriptTextBlock = EntryTextBlock;
    LastTranscriptRole = Role;
    LastTranscriptText = RawText;
    TrimTranscriptHistory();
    TranscriptScrollBox->ScrollToEnd();
}

#undef LOCTEXT_NAMESPACE
