#include "UI/Core/SUnrealAgentPanel.h"
#include "UI/Core/SUnrealAgentPanelPrivate.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUnrealAgentPanel"

using namespace UnrealAgent::Panel;

namespace
{
    constexpr int32 MaxComposerSuggestionRows = 6;

    // Single source of truth for the file extensions the composer will offer as
    // `@`-mentions. `.ini`/`.uproject`/`.uplugin` are intentionally excluded:
    // `Config/*.ini` routinely holds capability tokens / API keys, and project
    // / plugin descriptors expose layout metadata. Both would be sent verbatim
    // as `resource_link` text blocks. If a maintainer adds an entry here, both
    // the per-keystroke walk and the click-time accept check pick it up.
    const TArray<FString>& GetAttachableExtensions()
    {
        static const TArray<FString> Extensions = {
            TEXT("cpp"), TEXT("h"), TEXT("hpp"), TEXT("cs"), TEXT("ts"), TEXT("js"),
            TEXT("json"), TEXT("jsonc"), TEXT("md"), TEXT("txt"), TEXT("py"), TEXT("sh")
        };
        return Extensions;
    }

    bool IsAttachableProjectFile(const FString& FilePath)
    {
        const FString Extension = FPaths::GetExtension(FilePath).ToLower();
        for (const FString& Allowed : GetAttachableExtensions())
        {
            if (Allowed == Extension)
            {
                return true;
            }
        }
        return false;
    }

    FString GetComposerText(const TSharedPtr<SMultiLineEditableTextBox>& PromptTextBox)
    {
        return PromptTextBox.IsValid() ? PromptTextBox->GetText().ToString() : FString();
    }

    bool TryGetSlashPrefix(const FString& Text, FString& OutPrefix)
    {
        const FString Trimmed = Text.TrimStart();
        if (!Trimmed.StartsWith(TEXT("/")))
        {
            return false;
        }
        FString Token = Trimmed.Mid(1);
        int32 BreakIndex = INDEX_NONE;
        if (Token.FindChar(TEXT(' '), BreakIndex) || Token.FindChar(TEXT('\n'), BreakIndex) || Token.FindChar(TEXT('\r'), BreakIndex))
        {
            Token.LeftInline(BreakIndex);
        }
        OutPrefix = Token.TrimStartAndEnd();
        return true;
    }

    bool TryGetMentionPrefix(const FString& Text, FString& OutPrefix)
    {
        int32 AtIndex = INDEX_NONE;
        if (!Text.FindLastChar(TEXT('@'), AtIndex))
        {
            return false;
        }
        const FString Tail = Text.Mid(AtIndex + 1);
        if (Tail.Contains(TEXT(" ")) || Tail.Contains(TEXT("\n")) || Tail.Contains(TEXT("\r")))
        {
            return false;
        }
        OutPrefix = Tail.TrimStartAndEnd();
        return true;
    }

    bool IsPathInsideProject(const FString& AbsolutePath)
    {
        const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
        const FString Candidate = FPaths::ConvertRelativePathToFull(AbsolutePath);
        if (ProjectRoot.IsEmpty() || Candidate.IsEmpty() || ProjectRoot.Len() > Candidate.Len())
        {
            return false;
        }
        // Require a path-separator boundary so a sibling directory whose name starts
        // with the project root (e.g. /home/user/myproject-evil) cannot pass.
        return Candidate.Equals(ProjectRoot, ESearchCase::IgnoreCase)
            || Candidate.StartsWith(ProjectRoot + TEXT("/"), ESearchCase::IgnoreCase);
    }

    bool IsSkippedProjectPath(const FString& FilePath)
    {
        FString Normalized = FilePath;
        FPaths::NormalizeFilename(Normalized);
        return Normalized.Contains(TEXT("/Binaries/"))
            || Normalized.Contains(TEXT("/Intermediate/"))
            || Normalized.Contains(TEXT("/Saved/"))
            || Normalized.Contains(TEXT("/.git/"))
            || Normalized.Contains(TEXT("/.opencode/"));
    }

    // Project roots scanned for `@`-mention candidates. Restricted to text/code paths
    // to keep the per-keystroke walk bounded and to avoid suggesting binary assets or
    // build outputs the model cannot usefully read as text.
    TArray<FString> GetAttachableSearchRoots()
    {
        return {
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")),
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Config")),
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins"))
        };
    }
}

void SUnrealAgentPanel::RebuildComposerAffordanceLists()
{
    for (int32 Index = ComposerAffordanceLists.Num() - 1; Index >= 0; --Index)
    {
        TSharedPtr<SVerticalBox> AffordanceList = ComposerAffordanceLists[Index].Pin();
        if (!AffordanceList.IsValid())
        {
            ComposerAffordanceLists.RemoveAt(Index);
            continue;
        }
        PopulateComposerAffordanceList(AffordanceList.ToSharedRef());
    }
}

void SUnrealAgentPanel::PopulateComposerAffordanceList(TSharedRef<SVerticalBox> AffordanceList)
{
    AffordanceList->ClearChildren();
    PopulateComposerAttachmentRows(AffordanceList);

    const FString Text = GetComposerText(GetActivePromptTextBox());
    FString Prefix;
    if (TryGetSlashPrefix(Text, Prefix))
    {
        PopulateComposerCommandRows(AffordanceList, Prefix);
    }
    else if (TryGetMentionPrefix(Text, Prefix))
    {
        PopulateComposerFileRows(AffordanceList, Prefix);
    }
}

void SUnrealAgentPanel::PopulateComposerAttachmentRows(TSharedRef<SVerticalBox> AffordanceList)
{
    if (ComposerFileAttachments.Num() == 0)
    {
        return;
    }

    AffordanceList->AddSlot()
    .AutoHeight()
    .Padding(FMargin(2.0f, 0.0f, 2.0f, 4.0f))
    [
        SNew(STextBlock)
        .Tag(FName(TEXT("UnrealAgent.Composer.AttachmentHeader")))
        .Text(LOCTEXT("ComposerAttachedFiles", "Attached files for next prompt"))
        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
    ];

    for (const FComposerFileAttachment& Attachment : ComposerFileAttachments)
    {
        AffordanceList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(2.0f, 1.0f))
        [
            SNew(SButton)
            .Tag(FName(TEXT("UnrealAgent.Composer.AttachmentChip")))
            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
            .ToolTipText(FText::FromString(Attachment.AbsolutePath))
            .OnClicked(this, &SUnrealAgentPanel::OnComposerAttachmentRemoveClicked, Attachment.AbsolutePath)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("@ %s  ×"), *Attachment.DisplayName)))
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
    }
}

void SUnrealAgentPanel::PopulateComposerCommandRows(TSharedRef<SVerticalBox> AffordanceList, const FString& Prefix)
{
    const TArray<FOpenCodeAcpCommandOption>* Commands = AcpClient.IsValid() ? &AcpClient->GetAvailableCommands() : nullptr;
    if (Commands == nullptr || Commands->Num() == 0)
    {
        AffordanceList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(8.0f, 4.0f))
        [
            SNew(STextBlock)
            .Tag(FName(TEXT("UnrealAgent.Composer.CommandHint")))
            .Text(LOCTEXT("ComposerCommandsPending", "Connect first so OpenCode can advertise commands and skills."))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    int32 AddedRows = 0;
    for (const FOpenCodeAcpCommandOption& Command : *Commands)
    {
        if (!Prefix.IsEmpty() && !Command.Name.StartsWith(Prefix, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (AddedRows++ >= MaxComposerSuggestionRows)
        {
            break;
        }

        AffordanceList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(2.0f, 1.0f))
        [
            SNew(SButton)
            .Tag(FName(TEXT("UnrealAgent.Composer.CommandSuggestion")))
            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
            .OnClicked(this, &SUnrealAgentPanel::OnComposerCommandSuggestionClicked, Command.Name)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("/%s — %s"), *Command.Name, *Command.Description)))
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
    }
}

void SUnrealAgentPanel::PopulateComposerFileRows(TSharedRef<SVerticalBox> AffordanceList, const FString& Prefix)
{
    // Walk the roots (excluded by design: Content/, Intermediate/, Binaries/,
    // DerivedDataCache/) and filter in-memory by the allow-list. Walking
    // once per root instead of once per extension avoids 36 recursive scans
    // (12 extensions × 3 roots) on a large project.
    if (!bComposerMentionFileCacheValid)
    {
        ComposerMentionFileCache.Reset();
        TSet<FString> AllowSet;
        for (const FString& Extension : GetAttachableExtensions())
        {
            AllowSet.Add(Extension);
        }
        for (const FString& Root : GetAttachableSearchRoots())
        {
            if (!IFileManager::Get().DirectoryExists(*Root))
            {
                continue;
            }
            TArray<FString> RootFiles;
            IFileManager::Get().FindFilesRecursive(RootFiles, *Root, TEXT("*.*"), true, false);
            for (const FString& FilePath : RootFiles)
            {
                if (AllowSet.Contains(FPaths::GetExtension(FilePath).ToLower()))
                {
                    ComposerMentionFileCache.Add(MoveTemp(const_cast<FString&>(FilePath)));
                }
            }
        }
        ComposerMentionFileCache.Sort();
        bComposerMentionFileCacheValid = true;
    }

    const TArray<FString>& Files = ComposerMentionFileCache;
    const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    int32 AddedRows = 0;
    for (const FString& FilePath : Files)
    {
        if (AddedRows >= MaxComposerSuggestionRows)
        {
            break;
        }
        if (!IsPathInsideProject(FilePath) || !IsAttachableProjectFile(FilePath) || IsSkippedProjectPath(FilePath))
        {
            continue;
        }

        const FString CleanName = FPaths::GetCleanFilename(FilePath);
        const FString AbsolutePath = FPaths::ConvertRelativePathToFull(FilePath);
        const FString RelativePath = AbsolutePath.Replace(*ProjectRoot, TEXT(""), ESearchCase::IgnoreCase);
        if (!Prefix.IsEmpty() && !CleanName.StartsWith(Prefix, ESearchCase::IgnoreCase) && !RelativePath.Contains(Prefix, ESearchCase::IgnoreCase))
        {
            continue;
        }

        AffordanceList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(2.0f, 1.0f))
        [
            SNew(SButton)
            .Tag(FName(TEXT("UnrealAgent.Composer.FileSuggestion")))
            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
            .ToolTipText(FText::FromString(AbsolutePath))
            .OnClicked(this, &SUnrealAgentPanel::OnComposerFileSuggestionClicked, AbsolutePath)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("@ %s"), *RelativePath)))
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
        ++AddedRows;
    }
}

FReply SUnrealAgentPanel::OnComposerCommandSuggestionClicked(FString CommandName)
{
    const TSharedPtr<SMultiLineEditableTextBox> PromptTextBox = GetActivePromptTextBox();
    if (PromptTextBox.IsValid())
    {
        const FString NewText = FString::Printf(TEXT("/%s "), *CommandName);
        PromptTextBox->SetText(FText::FromString(NewText));
        // Place the caret at the end and keep focus so the user can keep typing
        // the slash command's arguments without a second click into the textbox.
        PromptTextBox->GoTo(FTextLocation(NewText.Len()));
        FSlateApplication::Get().SetKeyboardFocus(PromptTextBox.ToSharedRef());
    }
    // SetText fires OnTextChanged -> ScheduleComposerAffordanceRebuild, so the
    // list refreshes without an explicit call here.
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnComposerFileSuggestionClicked(FString FilePath)
{
    const FString AbsolutePath = FPaths::ConvertRelativePathToFull(FilePath);
    // Defense in depth: even if the affordance list was bypassed, refuse to attach a
    // path that escapes the project root or points inside an excluded directory.
    if (!IsPathInsideProject(AbsolutePath)
        || !IsAttachableProjectFile(AbsolutePath)
        || IsSkippedProjectPath(AbsolutePath))
    {
        return FReply::Handled();
    }
    if (!ComposerFileAttachments.ContainsByPredicate([&AbsolutePath](const FComposerFileAttachment& Attachment)
    {
        return Attachment.AbsolutePath == AbsolutePath;
    }))
    {
        FComposerFileAttachment Attachment;
        Attachment.AbsolutePath = AbsolutePath;
        Attachment.DisplayName = FPaths::GetCleanFilename(AbsolutePath);
        ComposerFileAttachments.Add(Attachment);
    }

    const TSharedPtr<SMultiLineEditableTextBox> PromptTextBox = GetActivePromptTextBox();
    if (PromptTextBox.IsValid())
    {
        FString Text = PromptTextBox->GetText().ToString();
        int32 AtIndex = INDEX_NONE;
        if (Text.FindLastChar(TEXT('@'), AtIndex))
        {
            Text.LeftInline(AtIndex);
            const FString NewText = Text.TrimEnd() + TEXT(" ");
            PromptTextBox->SetText(FText::FromString(NewText));
            PromptTextBox->GoTo(FTextLocation(NewText.Len()));
        }
        FSlateApplication::Get().SetKeyboardFocus(PromptTextBox.ToSharedRef());
    }
    // The SetText above and the attachment-list mutation both need to refresh the
    // suggestion list. The debounced scheduler handles both via OnTextChanged.
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnComposerAttachmentRemoveClicked(FString FilePath)
{
    ComposerFileAttachments.RemoveAll([&FilePath](const FComposerFileAttachment& Attachment)
    {
        return Attachment.AbsolutePath == FilePath;
    });
    ScheduleComposerAffordanceRebuild();
    return FReply::Handled();
}

namespace
{
    constexpr float ComposerAffordanceDebounceSeconds = 0.15f;
}

void SUnrealAgentPanel::ScheduleComposerAffordanceRebuild()
{
    // Coalesce rapid text-change events into a single delayed rebuild so the panel
    // does not block the Slate tick on a per-keystroke file walk. FTSTicker works
    // outside of a UWorld (Slate widgets have no GetWorld), unlike FTimerManager.
    if (ComposerAffordanceDebounceHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ComposerAffordanceDebounceHandle);
    }
    ComposerAffordanceDebounceHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([WeakSelf = TWeakPtr<SUnrealAgentPanel>(SharedThis(this))](float)
        {
            if (TSharedPtr<SUnrealAgentPanel> Self = WeakSelf.Pin())
            {
                Self->RebuildComposerAffordanceLists();
            }
            return false; // one-shot
        }),
        ComposerAffordanceDebounceSeconds);
}

#undef LOCTEXT_NAMESPACE
