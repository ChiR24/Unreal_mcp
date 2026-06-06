#pragma once

#include "../Acp/McpOpenCodeAcpClient.h"

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"

class SComboButton;
class SMultiLineEditableTextBox;
class SScrollBox;
class SSearchBox;
class STextBlock;
class SVerticalBox;
class SWidget;
struct FSlateBrush;

class SUnrealAgentPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SUnrealAgentPanel) {}
    SLATE_END_ARGS()

    virtual ~SUnrealAgentPanel() override;

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

#if WITH_DEV_AUTOMATION_TESTS
    static void SetChatHistoryStoragePathOverrideForAutomation(const FString& StoragePath);
    static void ClearChatHistoryStoragePathOverrideForAutomation();
    void AddTranscriptEntryForAutomation(const FString& Role, const FString& Text);
    void SetActiveContextWindowUsageForAutomation(int32 UsedTokens, int32 SizeTokens);
    FString GetContextWindowStatusTextForAutomation() const;
    void ToggleSidebarForAutomation();
    bool IsSidebarCollapsedForAutomation() const;
    int32 GetChatHistoryCountForAutomation() const;
    void ResetChatHistoryForAutomation();
#endif
    bool IsContextWindowVisibleForAutomation() const;

private:
    struct FToolActivityGroup
    {
        FString Key;
        FString Title;
        TArray<FString> Details;
        TSharedPtr<STextBlock> HeaderTextBlock;
        TSharedPtr<STextBlock> BodyTextBlock;
    };

    struct FChatTranscriptEntry
    {
        FString Role;
        FString Text;
    };

    struct FChatHistoryEntry
    {
        int32 Id = 0;
        FString Title;
        FString Preview;
        FString LastSummaryRole;
        TArray<FChatTranscriptEntry> TranscriptEntries;
        int32 EntryCount = 0;
        int32 ContextCharacters = 0;
        int32 ContextWindowUsedTokens = 0;
        int32 ContextWindowSizeTokens = 0;
        int32 LastSummaryCharacters = 0;
        bool bHasCustomTitle = false;
    };

    FReply OnConnectClicked();
    FReply OnSendClicked();
    FReply OnClearChatClicked();
    FReply OnNewChatClicked();
    FReply OnSidebarToggleClicked();
    FReply OnRetryLastPromptClicked();
    FReply OnApprovePermissionClicked();
    FReply OnApprovePermissionAlwaysClicked();
    FReply OnRejectPermissionClicked();
    FReply OnQuickPromptClicked(FString PromptText);
    FReply OnInspectContextClicked();
    FReply OnValidateProjectClicked();
    FReply OnChatHistoryEntryClicked(int32 EntryId);
    FReply OnChatHistoryRenameClicked(int32 EntryId);
    FReply OnChatHistoryRenameSaveClicked(int32 EntryId);
    FReply OnChatHistoryRenameCancelClicked();
    FReply OnChatHistoryDeleteClicked(int32 EntryId);
    FReply OnModelOptionClicked(TSharedPtr<FOpenCodeAcpModelOption> SelectedModel);
    FReply OnThinkingOptionClicked(TSharedPtr<FOpenCodeAcpThinkingOption> SelectedThinking);
    FReply OnAgentOptionClicked(TSharedPtr<FOpenCodeAcpAgentOption> SelectedAgent);
    FReply OnPromptKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent);
    void OnChatHistoryRenameTextChanged(const FText& NewText, int32 EntryId);
    void OnModelSelected(TSharedPtr<FOpenCodeAcpModelOption> SelectedModel);
    void OnThinkingSelected(TSharedPtr<FOpenCodeAcpThinkingOption> SelectedThinking);
    void OnAgentSelected(TSharedPtr<FOpenCodeAcpAgentOption> SelectedAgent);
    void OnModelMenuOpened();
    void OnModelSearchChanged(const FText& SearchText);
    void OnAttachContextCheckStateChanged(ECheckBoxState NewState);

    bool CanSendPrompt() const;
    bool CanRetryLastPrompt() const;
    bool CanSelectModel() const;
    bool CanSelectThinking() const;
    bool CanSelectAgent() const;
    bool CanApprovePermissionAlways() const;
    bool HasPermissionRequest() const;
    EVisibility GetEmptyStateVisibility() const;
    EVisibility GetConversationVisibility() const;
    EVisibility GetInitialComposerVisibility() const;
    EVisibility GetModelControlsVisibility() const;
    EVisibility GetThinkingSelectorVisibility() const;
    FSlateColor GetStatusBadgeColor() const;
    FSlateColor GetConnectionIndicatorColor() const;
    FSlateColor GetContextWindowIndicatorColor() const;
    FText GetStatusText() const;
    FText GetPermissionText() const;
    FText GetSelectedModelText() const;
    FText GetSelectedThinkingText() const;
    FText GetSelectedAgentText() const;
    FText GetConnectionButtonText() const;
    FText GetComposerHelperText() const;
    FText GetSidebarToggleText() const;
    FText GetChatHistoryEmptyText() const;
    FText GetContextWindowStatusText() const;
    FText GetContextWindowDetailText() const;
    FText GetStudioKitStatusText() const;
    FText GetEditorContextStatusText() const;
    FText GetValidationStatusText() const;
    ECheckBoxState GetAttachContextCheckState() const;
    EVisibility GetContextWindowVisibility() const;
    EVisibility GetExpandedSidebarVisibility() const;
    EVisibility GetCollapsedSidebarVisibility() const;
    int32 GetContextWindowTokenCapacity() const;
    int32 GetContextWindowUsedPercent() const;
    const FSlateBrush* GetSendButtonIconBrush() const;
    FSlateColor GetSendButtonIconColor() const;

    TSharedRef<SWidget> MakeSidebar();
    TSharedRef<SWidget> MakeCockpit();
    TSharedRef<SWidget> MakeMainLayout();
    TSharedRef<SWidget> MakeHeaderBar();
    TSharedRef<SWidget> MakeConversationArea();
    TSharedRef<SWidget> MakeEmptyPromptArea();
    TSharedRef<SWidget> MakePermissionBar();
    TSharedRef<SWidget> MakeExpandedSidebar();
    TSharedRef<SWidget> MakeCollapsedSidebar();
    TSharedRef<SWidget> MakeChatHistoryRow(const FChatHistoryEntry& Entry);
    TSharedRef<SWidget> MakeComposer(
        TSharedPtr<SMultiLineEditableTextBox>& OutPromptTextBox,
        TSharedPtr<SComboButton>& OutModelComboButton,
        TSharedPtr<SComboButton>& OutThinkingComboButton,
        TSharedPtr<SComboButton>& OutAgentComboButton,
        const FName& ComposerTag);
    TSharedRef<SWidget> MakeComposerInputFrame(TSharedPtr<SMultiLineEditableTextBox>& OutPromptTextBox);
    TSharedRef<SWidget> MakeComposerActionRow(
        TSharedPtr<SComboButton>& OutModelComboButton,
        TSharedPtr<SComboButton>& OutThinkingComboButton,
        TSharedPtr<SComboButton>& OutAgentComboButton);
    TSharedRef<SWidget> MakeComposerModelControls(
        TSharedPtr<SComboButton>& OutModelComboButton,
        TSharedPtr<SComboButton>& OutThinkingComboButton,
        TSharedPtr<SComboButton>& OutAgentComboButton);
    TSharedRef<SWidget> MakeComposerContextWindowStatus();
    TSharedRef<SWidget> MakeComposerHelperRow();
    TSharedRef<SWidget> MakeModelMenuContent();
    TSharedRef<SWidget> MakeModelMenuEntry(TSharedPtr<FOpenCodeAcpModelOption> ModelOption);
    TSharedRef<SWidget> MakeThinkingMenuContent();
    TSharedRef<SWidget> MakeThinkingMenuEntry(TSharedPtr<FOpenCodeAcpThinkingOption> ThinkingOption);
    TSharedRef<SWidget> MakeAgentMenuContent();
    TSharedRef<SWidget> MakeAgentMenuEntry(TSharedPtr<FOpenCodeAcpAgentOption> AgentOption);
    void PopulateModelMenuList();
    void PopulateThinkingMenuList(TSharedRef<SVerticalBox> ThinkingMenuList);
    void PopulateAgentMenuList(TSharedRef<SVerticalBox> AgentMenuList);
    TSharedPtr<SMultiLineEditableTextBox> GetActivePromptTextBox() const;
    void ClearPromptTextBoxes();
    void RefreshModelComboButtons();
    void RebuildFilteredModelOptions();
    void ResetTranscriptView();
    void EnsureActiveChatEntry(const FString& SeedText, bool bSeedUserTranscript);
    void UpdateActiveChatSummary(const FString& Role, const FString& Text, bool bCountAsTranscriptEntry);
    void LoadChatHistory();
    void SaveChatHistory() const;
    void RebuildChatHistoryList();
    void RestoreChatHistoryEntry(const FChatHistoryEntry& Entry);
    void StoreActiveContextWindowUsage();
    FString MakeChatTitleFromPrompt(const FString& Prompt) const;
    void SetStatus(const FString& NewStatus);
    void AddTranscriptEntry(const FString& Role, const FString& Text);
    void AddTranscriptEntryImmediately(const FString& Role, const FString& Text);
    void FlushPendingTranscript(bool bForce = false);
    bool ShouldAppendToLastTranscriptEntry(const FString& Role) const;
    FString ClampTranscriptText(const FString& Text) const;
    void TrimTranscriptHistory();
    void AppendActivityEntryToActive(const FString& Role, const FString& RawText);
    void AppendActivityTextRow(const FString& Role, const FString& RawText);
    void AppendToolActivityGroup(const FString& RawText);
    void ResetActiveActivityState();
    void FinalizeActiveReasoning();
    void HandlePermissionRequest(const FString& Description);
    void HandleClientStopped();
    void RefreshModelOptions();

private:
    TUniquePtr<FOpenCodeAcpClient> AcpClient;
    TSharedPtr<SScrollBox> TranscriptScrollBox;
    TSharedPtr<SMultiLineEditableTextBox> CenterPromptTextBox;
    TSharedPtr<SMultiLineEditableTextBox> BottomPromptTextBox;
    TSharedPtr<SComboButton> CenterModelComboButton;
    TSharedPtr<SComboButton> BottomModelComboButton;
    TSharedPtr<SComboButton> CenterThinkingComboButton;
    TSharedPtr<SComboButton> BottomThinkingComboButton;
    TSharedPtr<SComboButton> CenterAgentComboButton;
    TSharedPtr<SComboButton> BottomAgentComboButton;
    TSharedPtr<SSearchBox> ModelSearchBox;
    TSharedPtr<SVerticalBox> ModelMenuList;
    TSharedPtr<SVerticalBox> ChatHistoryList;
    TArray<TSharedPtr<FOpenCodeAcpModelOption>> ModelOptions;
    TArray<TSharedPtr<FOpenCodeAcpModelOption>> FilteredModelOptions;
    TArray<FChatHistoryEntry> ChatHistoryEntries;
    TSharedPtr<FOpenCodeAcpModelOption> SelectedModelOption;
    TSharedPtr<FOpenCodeAcpThinkingOption> SelectedThinkingOption;
    TSharedPtr<FOpenCodeAcpAgentOption> SelectedAgentOption;
    TArray<TSharedPtr<SWidget>> TranscriptEntryWidgets;
    TSharedPtr<STextBlock> LastTranscriptTextBlock;
    TSharedPtr<SVerticalBox> ActiveActivityBodyBox;
    TSharedPtr<STextBlock> LastActivityTextBlock;
    TSharedPtr<double> ActiveReasoningStartedSeconds;
    TSharedPtr<double> ActiveReasoningEndSeconds;
    TSharedPtr<bool> ActiveActivityHasReasoning;
    TSharedPtr<int32> ActiveActivityUpdateCount;
    TArray<FToolActivityGroup> ActiveToolActivityGroups;

    FString StatusText;
    FString LastTranscriptRole;
    FString LastActivityTranscriptRole;
    FString LastTranscriptText;
    FString PendingTranscriptRole;
    FString PendingTranscriptText;
    FString PendingPermissionDescription;
    FString LastUserPrompt;
    FString ModelSearchText;
    FString PendingRenameTitle;
    double LastTranscriptFlushTime = 0.0;
    int32 ActiveChatHistoryId = INDEX_NONE;
    int32 NextChatHistoryId = 1;
    int32 RenamingChatHistoryId = INDEX_NONE;
    bool bSidebarCollapsed = false;
    bool bHasConversationContent = false;
    bool bHasPendingPermission = false;
    bool bRestoringChatHistory = false;
    bool bAttachEditorContext = true;
};
