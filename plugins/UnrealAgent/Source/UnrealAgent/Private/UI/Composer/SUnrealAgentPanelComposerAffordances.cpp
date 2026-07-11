#include "UI/Core/SUnrealAgentPanel.h"
#include "UI/Core/SUnrealAgentPanelPrivate.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"
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

    const FString ActorReferenceScheme = TEXT("actor:");

    bool IsActorReference(const FString& Token)
    {
        return Token.StartsWith(ActorReferenceScheme, ESearchCase::IgnoreCase);
    }

    FString MakeActorReference(const FString& ActorLabel)
    {
        return ActorReferenceScheme + ActorLabel;
    }

    TArray<FString> GetSelectedAssetPaths()
    {
        TArray<FString> AssetPaths;
        FContentBrowserModule* ContentBrowserModule =
            FModuleManager::LoadModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
        if (ContentBrowserModule == nullptr)
        {
            return AssetPaths;
        }
        TArray<FAssetData> SelectedAssets;
        ContentBrowserModule->Get().GetSelectedAssets(SelectedAssets);
        for (const FAssetData& AssetData : SelectedAssets)
        {
            const FString PackageName = AssetData.PackageName.ToString();
            if (!PackageName.IsEmpty())
            {
                AssetPaths.Add(PackageName);
            }
        }
        return AssetPaths;
    }

    // Bounded asset-registry search used as a fallback when there is no live
    // Content Browser selection. Returns editor-scoped PackageNames (e.g.
    // /Game/Characters/Hero) directly — no filesystem conversion, so project-root
    // path validation can never reject a valid asset. Enumerate is capped so a
    // per-keystroke call cannot scan the whole /Game tree.
    TArray<FString> GetAssetPathsByPrefix(const FString& Prefix)
    {
        TArray<FString> AssetPaths;
        FAssetRegistryModule* AssetRegistryModule =
            FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
        if (AssetRegistryModule == nullptr)
        {
            return AssetPaths;
        }
        // Strip a leading slash (e.g. "@/Foo") so the query filters by name/path,
        // not by a literal "/Game/..." prefix the user rarely types in full.
        FString SearchPrefix = Prefix;
        while (SearchPrefix.StartsWith(TEXT("/")))
        {
            SearchPrefix.RemoveFromStart(TEXT("/"));
        }
        const bool bBrowseAll = SearchPrefix.IsEmpty();
        IAssetRegistry& Registry = AssetRegistryModule->Get();
        FARFilter Filter;
        Filter.PackagePaths.Add(TEXT("/Game"));
        Filter.bRecursivePaths = true;
        int32 Inspected = 0;
        Registry.EnumerateAssets(Filter, [&AssetPaths, &Inspected, &SearchPrefix, bBrowseAll](const FAssetData& Asset)
        {
            if (++Inspected > 5000)
            {
                return false;
            }
            // Skip World Partition external-actor packages (auto-generated
            // _ExternalActors_ save artifacts under /Game). They are not
            // mentional assets and only pollute the @-mention list.
            if (Asset.PackageName.ToString().Contains(TEXT("_ExternalActors_")))
            {
                return true;
            }
            if (!bBrowseAll)
            {
                const FString Name = Asset.AssetName.ToString();
                const FString Package = Asset.PackageName.ToString();
                const bool bMatch = Name.Contains(SearchPrefix, ESearchCase::IgnoreCase)
                    || Package.Contains(SearchPrefix, ESearchCase::IgnoreCase);
                if (!bMatch)
                {
                    return true;
                }
            }
            AssetPaths.Add(Asset.PackageName.ToString());
            return AssetPaths.Num() < MaxComposerSuggestionRows;
        });
        return AssetPaths;
    }

    // Returns selected actors as (Label, ObjectPath) pairs. The object path is
    // globally unique and is what references are keyed on, so two actors that
    // share a display label (e.g. two "Cube" actors) remain distinguishable.
    TArray<TPair<FString, FString>> GetSelectedActors()
    {
        TArray<TPair<FString, FString>> Actors;
        if (GEditor == nullptr)
        {
            return Actors;
        }
        USelection* SelectedActors = GEditor->GetSelectedActors();
        if (SelectedActors == nullptr)
        {
            return Actors;
        }
        TArray<AActor*> Selected;
        SelectedActors->GetSelectedObjects<AActor>(Selected);
        for (AActor* Actor : Selected)
        {
            if (Actor != nullptr)
            {
                Actors.Emplace(Actor->GetActorNameOrLabel(), Actor->GetPathName());
            }
        }
        return Actors;
    }

    // Browses world actors (the items you see in the World Outliner: Landscape,
    // Lighting, DirectionalLight, etc.) as `@`-mention candidates. This is the
    // fallback used when there is no live Outliner selection, so typing "@" with
    // no selection still surfaces editor actors the way it surfaces Content Browser
    // assets. Filters by the actor's display label OR its class name (so "@light"
    // matches both a "Lighting" actor and a "DirectionalLight" class), and is
    // capped at the suggestion-row limit so the suggestion list stays bounded;
    // when the query matches few actors the iterator may still walk the whole
    // world per keystroke (same as GetActorLabelByPath). Mirrors
    // GetAssetPathsByPrefix but for placed actors rather
    // than /Game package names.
    TArray<TPair<FString, FString>> GetWorldActorRefsByPrefix(const FString& Prefix)
    {
        TArray<TPair<FString, FString>> Actors;
        if (GEditor == nullptr)
        {
            return Actors;
        }
        UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
        if (World == nullptr)
        {
            return Actors;
        }
        FString Query = Prefix;
        while (Query.StartsWith(TEXT("/")))
        {
            Query.RemoveFromStart(TEXT("/"));
        }
        const bool bBrowseAll = Query.IsEmpty();
        for (TActorIterator<AActor> It(World); It && Actors.Num() < MaxComposerSuggestionRows; ++It)
        {
            AActor* Actor = *It;
            if (!bBrowseAll)
            {
                const FString Label = Actor->GetActorNameOrLabel();
                const FString ClassName = Actor->GetClass()->GetName();
                const bool bMatch = Label.Contains(Query, ESearchCase::IgnoreCase)
                    || ClassName.Contains(Query, ESearchCase::IgnoreCase);
                if (!bMatch)
                {
                    continue;
                }
            }
            Actors.Emplace(Actor->GetActorNameOrLabel(), Actor->GetPathName());
        }
        return Actors;
    }

    // Resolves an actor object path back to its display label for UI text. Falls
    // back to the path itself when the actor is no longer in the world.
    FString GetActorLabelByPath(const FString& ActorPath)
    {
        if (GEditor == nullptr)
        {
            return ActorPath;
        }
        UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
        if (World == nullptr)
        {
            return ActorPath;
        }
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetPathName().Equals(ActorPath, ESearchCase::IgnoreCase))
            {
                return It->GetActorNameOrLabel();
            }
        }
        return ActorPath;
    }

    // Single source of truth for the file extensions offered as `@`-mentions.
    // `.ini`/`.uproject`/`.uplugin` are intentionally excluded: Config/*.ini
    // routinely holds capability tokens / API keys, and project/plugin descriptors
    // expose layout metadata. Both would be sent verbatim as resource_link text.
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

    // Project roots scanned for `@`-mention file candidates.
    TArray<FString> GetAttachableSearchRoots()
    {
        return {
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")),
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Config")),
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins"))
        };
    }

    // Bounded filesystem walk for `@`-mention file candidates. Restricted to the
    // attachable roots and extension allow-list, prunes build/output/source-control
    // directories, and stops after a sane number of files are visited so a
    // per-keystroke call cannot scan the whole project tree. Only runs when a
    // non-empty query is present, which bounds cost further.
    TArray<FString> GetFilePathsByPrefix(const FString& Prefix)
    {
        TArray<FString> FilePaths;
        FString Query = Prefix;
        while (Query.StartsWith(TEXT("/")))
        {
            Query.RemoveFromStart(TEXT("/"));
        }
        if (Query.IsEmpty())
        {
            return FilePaths;
        }
        const TArray<FString>& Extensions = GetAttachableExtensions();
        const TArray<FString> Roots = GetAttachableSearchRoots();

        int32 Found = 0;
        int32 Visited = 0;
        constexpr int32 MaxResults = MaxComposerSuggestionRows;
        constexpr int32 MaxVisited = 4000;

        TFunction<void(const FString&)> Walk;
        Walk = [&Walk, &FilePaths, &Query, &Found, &Visited](const FString& Dir)
        {
            if (Found >= MaxResults || Visited >= MaxVisited)
            {
                return;
            }
            TArray<FString> Entries;
            IFileManager::Get().FindFiles(Entries, *FPaths::Combine(Dir, TEXT("*")), true, true);
            for (const FString& Entry : Entries)
            {
                if (Found >= MaxResults || Visited >= MaxVisited)
                {
                    return;
                }
                const FString Full = FPaths::Combine(Dir, Entry);
                if (IFileManager::Get().DirectoryExists(*Full))
                {
                    // Skip build outputs, derived caches, and source-control dirs.
                    if (IsSkippedProjectPath(Full))
                    {
                        continue;
                    }
                    Walk(Full);
                }
                else
                {
                    ++Visited;
                    if (!IsAttachableProjectFile(Full))
                    {
                        continue;
                    }
                    const FString Name = FPaths::GetCleanFilename(Full);
                    if (Name.Contains(Query, ESearchCase::IgnoreCase))
                    {
                        FilePaths.Add(Full);
                        ++Found;
                    }
                }
            }
        };

        for (const FString& Root : Roots)
        {
            if (Found >= MaxResults)
            {
                break;
            }
            if (IFileManager::Get().DirectoryExists(*Root))
            {
                Walk(Root);
            }
        }
        return FilePaths;
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
        const FString ChipKey = Attachment.Kind == EComposerAttachmentKind::ActorRef
            ? MakeActorReference(Attachment.ReferenceText)
            : Attachment.AbsolutePath;
        const FString ChipTooltip = Attachment.Kind == EComposerAttachmentKind::ActorRef
            ? Attachment.ReferenceText
            : Attachment.AbsolutePath;

        AffordanceList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(2.0f, 1.0f))
        [
            SNew(SButton)
            .Tag(FName(TEXT("UnrealAgent.Composer.AttachmentChip")))
            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
            .ToolTipText(FText::FromString(ChipTooltip))
            .OnClicked(this, &SUnrealAgentPanel::OnComposerAttachmentRemoveClicked, ChipKey)
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
    const TArray<FString> SelectedAssetPaths = GetSelectedAssetPaths();
    TArray<TPair<FString, FString>> SelectedActors = GetSelectedActors();
    // A live Outliner selection is query-independent, so the display loop below
    // narrows it by label. With no selection we fall back to a prefix-filtered
    // browse of world actors (Landscape, Lighting, etc.) so "@" still surfaces
    // the editor items you see in the Outliner without selecting them first.
    // Those fallback results are already matched by label OR class name, so they
    // must NOT be re-filtered by label only (that would drop class matches).
    const bool bHadSelection = SelectedActors.Num() > 0;
    if (!bHadSelection)
    {
        SelectedActors = GetWorldActorRefsByPrefix(Prefix);
    }
    // Filesystem file mentions are only offered when the user has typed a query,
    // which bounds the per-keystroke walk to relevant candidates.
    const TArray<FString> FilePaths = GetFilePathsByPrefix(Prefix);

    // When there is no live editor selection, fall back to a bounded asset-registry
    // search. With an empty prefix this browses a few assets; with a prefix it
    // filters by name. PackageNames are editor-scoped, so no filesystem validation.
    TArray<FString> AssetPaths = SelectedAssetPaths;
    if (AssetPaths.Num() == 0)
    {
        AssetPaths = GetAssetPathsByPrefix(Prefix);
    }
    else
    {
        // A live Content Browser selection was used instead of the registry
        // search, so the typed prefix was not applied. Filter the selected
        // assets the same way GetAssetPathsByPrefix does (substring on the asset
        // name or package path) so selection and registry results behave
        // identically and the "no match" hint stays accurate.
        FString P = Prefix;
        while (P.StartsWith(TEXT("/")))
        {
            P.RemoveFromStart(TEXT("/"));
        }
        if (!P.IsEmpty())
        {
            AssetPaths.RemoveAll([&P](const FString& PackageName)
            {
                const FString AssetName = FPaths::GetCleanFilename(PackageName);
                return !AssetName.Contains(P, ESearchCase::IgnoreCase)
                    && !PackageName.Contains(P, ESearchCase::IgnoreCase);
            });
        }
    }

    const bool bNoResults = AssetPaths.Num() == 0 && SelectedActors.Num() == 0 && FilePaths.Num() == 0;
    if (bNoResults)
    {
        FText HintText = Prefix.IsEmpty()
            ? LOCTEXT("ComposerMentionEmptyHint",
                "No assets or actors yet. Select them in the Content Browser or Outliner, or type @ to search world actors (Landscape, Lighting, …).")
            : FText::Format(LOCTEXT("ComposerMentionNoMatchHint",
                "No assets or actors match \"{0}\". Select them in the editor, or type more to search."),
                FText::FromString(Prefix));
        AffordanceList->AddSlot()
            .AutoHeight()
            .Padding(FMargin(2.0f, 4.0f))
        [
            SNew(STextBlock)
            .Tag(FName(TEXT("UnrealAgent.Composer.MentionEmptyHint")))
            .Text(HintText)
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            .AutoWrapText(true)
        ];
        return;
    }

    int32 AddedRows = 0;
    const auto MatchesQuery = [&Prefix](const FString& Candidate)
    {
        FString P = Prefix;
        while (P.StartsWith(TEXT("/")))
        {
            P.RemoveFromStart(TEXT("/"));
        }
        return P.IsEmpty() || Candidate.Contains(P, ESearchCase::IgnoreCase);
    };

    // AssetPaths are already filtered (registry search, or the selection filter
    // above); no second gate needed.
    for (const FString& PackageName : AssetPaths)
    {
        if (AddedRows >= MaxComposerSuggestionRows)
        {
            break;
        }

        AffordanceList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(2.0f, 1.0f))
        [
            SNew(SButton)
            .Tag(FName(TEXT("UnrealAgent.Composer.FileSuggestion")))
            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
            .ToolTipText(FText::FromString(PackageName))
            .OnClicked(this, &SUnrealAgentPanel::OnComposerFileSuggestionClicked, PackageName)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("@ %s"), *PackageName)))
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
        ++AddedRows;
    }

    for (const TPair<FString, FString>& Actor : SelectedActors)
    {
        if (AddedRows >= MaxComposerSuggestionRows)
        {
            break;
        }
        const FString& ActorLabel = Actor.Key;
        const FString& ActorPath = Actor.Value;
        // Only the live-selection path needs a label re-filter; the fallback
        // path is already matched by label OR class name in GetWorldActorRefsByPrefix.
        if (bHadSelection && !MatchesQuery(ActorLabel))
        {
            continue;
        }

        // Key the reference on the unique object path so two actors with the same
        // display label stay distinguishable downstream.
        const FString ActorReference = MakeActorReference(ActorPath);
        AffordanceList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(2.0f, 1.0f))
        [
            SNew(SButton)
            .Tag(FName(TEXT("UnrealAgent.Composer.ActorSuggestion")))
            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
            .ToolTipText(FText::FromString(ActorLabel))
            .OnClicked(this, &SUnrealAgentPanel::OnComposerFileSuggestionClicked, ActorReference)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("@ actor %s"), *ActorLabel)))
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
        ++AddedRows;
    }

    for (const FString& FilePath : FilePaths)
    {
        if (AddedRows >= MaxComposerSuggestionRows)
        {
            break;
        }
        AffordanceList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(2.0f, 1.0f))
        [
            SNew(SButton)
            .Tag(FName(TEXT("UnrealAgent.Composer.FileSuggestion")))
            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
            .ToolTipText(FText::FromString(FilePath))
            .OnClicked(this, &SUnrealAgentPanel::OnComposerFileSuggestionClicked, FilePath)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("@ %s"), *FPaths::GetCleanFilename(FilePath))))
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

FReply SUnrealAgentPanel::OnComposerFileSuggestionClicked(FString Payload)
{
    const bool bIsActor = IsActorReference(Payload);

    if (bIsActor)
    {
        // Payload is the actor's unique object path ("actor:<Path>"). Key and dedup
        // on the path so two actors that share a display label stay distinct.
        const FString ActorPath = Payload.Mid(ActorReferenceScheme.Len());
        if (ComposerFileAttachments.ContainsByPredicate([&ActorPath](const FComposerFileAttachment& Attachment)
        {
            return Attachment.Kind == EComposerAttachmentKind::ActorRef
                && Attachment.ReferenceText == ActorPath;
        }))
        {
            return FReply::Handled();
        }
        FComposerFileAttachment Attachment;
        Attachment.Kind = EComposerAttachmentKind::ActorRef;
        Attachment.ReferenceText = ActorPath;
        Attachment.DisplayName = GetActorLabelByPath(ActorPath);
        ComposerFileAttachments.Add(MoveTemp(Attachment));
    }
    else
    {
        // Trust any valid registered package name as an asset reference. This
        // covers /Game plus custom content mounts (e.g. MCP_ADDITIONAL_PATH_PREFIXES,
        // registered plugin mounts), because IsValidLongPackageName consults the
        // actual mount table. A "/"-prefixed string that is not a real package
        // (e.g. /home/user/secret.cpp) is therefore not trusted. Anything else
        // goes through project-root validation as a filesystem path.
        const bool bIsValidPackage = FPackageName::IsValidLongPackageName(Payload);
        const FString AbsolutePath = bIsValidPackage ? Payload : FPaths::ConvertRelativePathToFull(Payload);
        if (!bIsValidPackage)
        {
            if (!IsPathInsideProject(AbsolutePath) || IsSkippedProjectPath(AbsolutePath))
            {
                return FReply::Handled();
            }
        }
        if (!ComposerFileAttachments.ContainsByPredicate([&AbsolutePath](const FComposerFileAttachment& Attachment)
        {
            return Attachment.Kind == EComposerAttachmentKind::File && Attachment.AbsolutePath == AbsolutePath;
        }))
        {
            FComposerFileAttachment Attachment;
            Attachment.AbsolutePath = AbsolutePath;
            Attachment.DisplayName = FPaths::GetCleanFilename(AbsolutePath);
            ComposerFileAttachments.Add(MoveTemp(Attachment));
        }
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

FReply SUnrealAgentPanel::OnComposerAttachmentRemoveClicked(FString Key)
{
    ComposerFileAttachments.RemoveAll([&Key](const FComposerFileAttachment& Attachment)
    {
        if (Attachment.Kind == EComposerAttachmentKind::ActorRef)
        {
            return MakeActorReference(Attachment.ReferenceText) == Key;
        }
        return Attachment.AbsolutePath == Key;
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
