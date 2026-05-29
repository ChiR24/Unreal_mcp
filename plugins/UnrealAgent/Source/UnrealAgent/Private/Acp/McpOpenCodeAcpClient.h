#pragma once

#include "Containers/Queue.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformProcess.h"
#include "Templates/SharedPointer.h"

struct FOpenCodeAcpModelOption
{
    FString Id;
    FString Name;
    FString Provider;
    int32 ContextWindowTokens = 0;

    FString GetDisplayName() const
    {
        const FString Source = Name.IsEmpty() ? Id : Name;
        FString ParsedProvider;
        FString ParsedModel;
        if (Source.Split(TEXT("/"), &ParsedProvider, &ParsedModel) && !ParsedProvider.TrimStartAndEnd().IsEmpty() && !ParsedModel.TrimStartAndEnd().IsEmpty())
        {
            return ParsedModel.TrimStartAndEnd();
        }
        return Source;
    }

    FString GetProviderName() const
    {
        if (!Provider.IsEmpty())
        {
            return Provider;
        }

        const FString Source = Name.IsEmpty() ? Id : Name;
        FString ParsedProvider;
        FString ParsedModel;
        if (Source.Split(TEXT("/"), &ParsedProvider, &ParsedModel) && !ParsedProvider.TrimStartAndEnd().IsEmpty() && !ParsedModel.TrimStartAndEnd().IsEmpty())
        {
            return ParsedProvider.TrimStartAndEnd();
        }
        return TEXT("Other");
    }
};

struct FOpenCodeAcpAgentOption
{
    FString Id;
    FString Name;
    FString Description;

    FString GetDisplayName() const
    {
        if (Id == TEXT("unreal-agent"))
        {
            return TEXT("Unreal - Creator");
        }
        return Name.IsEmpty() ? Id : Name;
    }
};

struct FOpenCodeAcpThinkingOption
{
    FString Id;
    FString Name;
    FString Description;

    FString GetDisplayName() const
    {
        return Name.IsEmpty() ? Id : Name;
    }
};

struct FOpenCodeAcpPermissionOption
{
    FString Id;
    FString Kind;
};

DECLARE_DELEGATE_OneParam(FOnOpenCodeAcpStatus, const FString&);
DECLARE_DELEGATE_TwoParams(FOnOpenCodeAcpTranscript, const FString&, const FString&);
DECLARE_DELEGATE_OneParam(FOnOpenCodeAcpPermission, const FString&);
DECLARE_DELEGATE(FOnOpenCodeAcpModelsChanged);
DECLARE_DELEGATE(FOnOpenCodeAcpStopped);

class FOpenCodeAcpClient
{
public:
    FOpenCodeAcpClient();
    ~FOpenCodeAcpClient();

    bool Start(const FString& InWorkingDirectory);
    void Stop();
    void Tick();

    bool IsReady() const { return bReady; }
    bool IsRunning() const { return bRunning; }
    bool IsPromptInFlight() const { return bPromptInFlight; }
    bool IsCancelRequested() const { return bCancelRequested; }
    bool HasPendingPermission() const { return PendingPermissionId.IsValid(); }
    bool CanSelectModel() const { return bReady && !bPromptInFlight && SetModelRequestId == INDEX_NONE && !ModelConfigId.IsEmpty() && ModelOptions.Num() > 0; }
    bool CanSelectThinking() const { return bReady && !bPromptInFlight && SetThinkingRequestId == INDEX_NONE && !ThinkingConfigId.IsEmpty() && ThinkingOptions.Num() > 0; }
    bool CanSelectAgent() const { return bReady && !bPromptInFlight && SetAgentRequestId == INDEX_NONE && !AgentConfigId.IsEmpty() && AgentOptions.Num() > 0; }

    const FString& GetSessionId() const { return SessionId; }
    const FString& GetResolvedExecutable() const { return ResolvedExecutable; }
    const FString& GetCurrentModel() const { return CurrentModel; }
    const FString& GetCurrentThinking() const { return CurrentThinking; }
    const FString& GetCurrentAgent() const { return CurrentAgent; }
    const TArray<FOpenCodeAcpModelOption>& GetModelOptions() const { return ModelOptions; }
    const TArray<FOpenCodeAcpThinkingOption>& GetThinkingOptions() const { return ThinkingOptions; }
    const TArray<FOpenCodeAcpAgentOption>& GetAgentOptions() const { return AgentOptions; }
    bool HasContextWindowUsage() const { return ContextWindowSizeTokens > 0; }
    int32 GetContextWindowUsedTokens() const { return ContextWindowUsedTokens; }
    int32 GetContextWindowSizeTokens() const { return ContextWindowSizeTokens; }
    const FString& GetLastStudioKitSummary() const { return LastStudioKitSummary; }
    const FString& GetLastEditorContextSummary() const { return LastEditorContextSummary; }
    const FString& GetLastEditorContextEnvelope() const { return LastEditorContextEnvelope; }
    const FString& GetLastValidationSummary() const { return LastValidationSummary; }
    bool ShouldAttachEditorContext() const { return bAttachEditorContext; }

    bool SendPrompt(const FString& PromptText);
    FString RefreshEditorContext();
    bool RunProjectValidation();
    void SetAttachEditorContext(bool bEnabled) { bAttachEditorContext = bEnabled; }
    void CancelPrompt();
    void SetModel(const FString& ModelId);
    void SetThinking(const FString& ThinkingId);
    void SetAgent(const FString& AgentId);
    void ApprovePermissionOnce();
    void ApprovePermissionAlways();
    void RejectPermission();
    bool CanApprovePermissionAlways() const;

    FOnOpenCodeAcpStatus OnStatus;
    FOnOpenCodeAcpTranscript OnTranscript;
    FOnOpenCodeAcpPermission OnPermission;
    FOnOpenCodeAcpModelsChanged OnModelsChanged;
    FOnOpenCodeAcpStopped OnStopped;

private:
    void ReadProcessOutput();
    void DrainProcessErrorOutput();
    void AppendRecentErrorOutput(const FString& Text);
    void CloseProcessPipes();
    void MarkProcessExited(int32 ReturnCode, bool bCanceled);
    void CheckClientOwnedRequestTimeouts();

    bool SendInitialize();
    bool SendNewSession();
    bool EnsureProjectUnrealAgentConfig();
    void AddConfiguredMcpServers(TArray<TSharedPtr<FJsonValue>>& McpServers) const;
    int32 SendRequest(const FString& Method, const TSharedPtr<FJsonObject>& Params);
    bool SendNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params);
    bool SendResponse(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result);
    bool SendError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message);
    bool SendJsonObject(const TSharedPtr<FJsonObject>& Root);

    void ProcessOutputBytes(const TArray<uint8>& Bytes);
    void HandleMessage(const TSharedPtr<FJsonObject>& Message);
    void HandleResponse(const TSharedPtr<FJsonObject>& Message, int32 Id);
    void HandleNotificationOrRequest(const TSharedPtr<FJsonObject>& Message, const FString& Method);
    void HandleSessionUpdate(const TSharedPtr<FJsonObject>& Params);
    void HandlePermissionRequest(const TSharedPtr<FJsonObject>& Message, const TSharedPtr<FJsonObject>& Params);
    bool ParseModelOptionsFromResult(const TSharedPtr<FJsonObject>& Result);
    void ParseModelOption(const TSharedPtr<FJsonObject>& Option);
    bool ParseFallbackModels(const TSharedPtr<FJsonObject>& Result);
    bool ParseAgentOptionsFromResult(const TSharedPtr<FJsonObject>& Result);
    bool ParseAgentOptionsFromConfigOption(const TSharedPtr<FJsonObject>& Option);
    void ParseAgentOption(const TSharedPtr<FJsonObject>& Option);
    bool ParseFallbackAgents(const TSharedPtr<FJsonObject>& Result);
    bool ParseThinkingOptionsFromResult(const TSharedPtr<FJsonObject>& Result);
    bool ParseThinkingOptionsFromConfigOption(const TSharedPtr<FJsonObject>& Option);
    void ParseThinkingOption(const TSharedPtr<FJsonObject>& Option);
    bool TrySelectDefaultAgent();
    void HandleModelUpdate(const TSharedPtr<FJsonObject>& Update);
    void HandleThinkingUpdate(const TSharedPtr<FJsonObject>& Update);
    void HandleAgentUpdate(const TSharedPtr<FJsonObject>& Update);
    void HandleUsageUpdate(const TSharedPtr<FJsonObject>& Update);
    void SetCurrentModel(const FString& NewModel);
    FString FormatToolActivityTranscriptText(const TSharedPtr<FJsonObject>& Update, bool bStarted);

    FString DescribePermissionRequest(const TSharedPtr<FJsonObject>& Params) const;
    FString GetModelDisplayName(const FString& ModelId) const;
    FString GetThinkingDisplayName(const FString& ThinkingId) const;
    FString GetAgentDisplayName(const FString& AgentId) const;
    FString FindPendingPermissionOption(const TArray<FString>& PreferredOptionIds, const TArray<FString>& PreferredKinds) const;
    bool SendCancelledPermissionResponse();
    void ResolvePendingPermission(const FString& PreferredOptionId);
    void StopWithError(const FString& ErrorText);
    void SetStatus(const FString& NewStatus);
    void AppendTranscript(const FString& Role, const FString& Text);
    void ResetState();

    FString FormatProcessErrorText(const FString& ErrorText) const;

    FString ResolveOpenCodeExecutable(FString& OutError) const;
    static FString JsonToString(const TSharedPtr<FJsonObject>& Object);
    static FString JsonValueToString(const TSharedPtr<FJsonValue>& Value);
    static bool TryReadIdAsInt(const TSharedPtr<FJsonObject>& Message, int32& OutId);
    static bool ShouldIgnoreProcessOutputLine(const FString& Line);
    static bool IsSafeProcessArgument(const FString& Value);

private:
    FProcHandle ProcessHandle;
    void* OutputReadPipe = nullptr;
    void* OutputWritePipe = nullptr;
    void* ErrorReadPipe = nullptr;
    void* ErrorWritePipe = nullptr;
    void* InputReadPipe = nullptr;
    void* InputWritePipe = nullptr;

    FString WorkingDirectory;
    FString ResolvedExecutable;
    FString SessionId;
    FString CurrentModel;
    FString ModelConfigId;
    FString CurrentThinking;
    FString ThinkingConfigId;
    FString CurrentAgent;
    FString AgentConfigId;
    FString LastStudioKitSummary;
    FString LastEditorContextSummary;
    FString LastEditorContextEnvelope;
    FString LastValidationSummary;
    TArray<FOpenCodeAcpModelOption> ModelOptions;
    TArray<FOpenCodeAcpThinkingOption> ThinkingOptions;
    TArray<FOpenCodeAcpAgentOption> AgentOptions;
    FString PendingModel;
    FString PendingThinking;
    FString PendingAgent;
    TArray<uint8> OutputBuffer;
    FString RecentErrorOutput;
    TSharedPtr<FJsonValue> PendingPermissionId;
    TArray<FOpenCodeAcpPermissionOption> PendingPermissionOptions;
    TMap<FString, FString> ActiveToolTitlesById;
    TMap<FString, FString> ActiveToolDetailsById;

    int32 NextRequestId = 1;
    int32 InitializeRequestId = INDEX_NONE;
    int32 NewSessionRequestId = INDEX_NONE;
    int32 ActivePromptRequestId = INDEX_NONE;
    int32 SetModelRequestId = INDEX_NONE;
    int32 SetThinkingRequestId = INDEX_NONE;
    int32 SetAgentRequestId = INDEX_NONE;
    int32 ContextWindowUsedTokens = 0;
    int32 ContextWindowSizeTokens = 0;

    double InitializeRequestStartedAt = 0.0;
    double NewSessionRequestStartedAt = 0.0;
    double SetModelRequestStartedAt = 0.0;
    double SetThinkingRequestStartedAt = 0.0;
    double SetAgentRequestStartedAt = 0.0;

    bool bRunning = false;
    bool bInitialized = false;
    bool bReady = false;
    bool bPromptInFlight = false;
    bool bCancelRequested = false;
    bool bAttachEditorContext = true;
};
