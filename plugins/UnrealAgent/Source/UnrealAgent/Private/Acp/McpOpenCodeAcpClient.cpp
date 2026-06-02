#include "McpOpenCodeAcpClient.h"

#include "UnrealAgentEditorContext.h"
#include "UnrealAgentStudioKit.h"
#include "UnrealAgentValidationRunner.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_LINUX
#include <signal.h>
#endif

namespace
{
    constexpr double ClientLifecycleRequestTimeoutSeconds = 120.0;
    constexpr double ClientConfigRequestTimeoutSeconds = 30.0;
    constexpr double ClientProcessShutdownWaitSeconds = 2.0;
    constexpr int32 SupportedAcpProtocolVersion = 1;
    constexpr int32 MaxOutputBufferBytes = 1024 * 1024;
    constexpr int32 MaxRecentErrorOutputChars = 4096;
    constexpr int32 MaxPermissionDescriptionChars = 4096;
    constexpr int32 MaxToolActivityDetailChars = 2048;
    constexpr const TCHAR* UnrealAgentId = TEXT("unreal-agent");
    constexpr const TCHAR* UnrealMcpServerName = TEXT("unreal-engine");
    constexpr const TCHAR* AutomationBridgeSettingsSection = TEXT("/Script/McpAutomationBridge.McpAutomationBridgeSettings");

    TSharedPtr<FJsonObject> MakeObject()
    {
        return MakeShared<FJsonObject>();
    }

    TSharedPtr<FJsonValue> CloneJsonId(const TSharedPtr<FJsonObject>& Message)
    {
        return Message.IsValid() ? Message->TryGetField(TEXT("id")) : nullptr;
    }

    FString GetStringFieldOrEmpty(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
    {
        FString Value;
        if (Object.IsValid())
        {
            Object->TryGetStringField(FieldName, Value);
        }
        return Value;
    }

    FString GetConfigOptionId(const TSharedPtr<FJsonObject>& Object)
    {
        FString OptionId = GetStringFieldOrEmpty(Object, TEXT("id"));
        if (OptionId.IsEmpty())
        {
            OptionId = GetStringFieldOrEmpty(Object, TEXT("configId"));
        }
        if (OptionId.IsEmpty())
        {
            OptionId = GetStringFieldOrEmpty(Object, TEXT("configOptionId"));
        }
        return OptionId;
    }

    FString GetConfigOptionValue(const TSharedPtr<FJsonObject>& Object)
    {
        FString Value = GetStringFieldOrEmpty(Object, TEXT("currentValue"));
        if (Value.IsEmpty())
        {
            Value = GetStringFieldOrEmpty(Object, TEXT("value"));
        }
        if (Value.IsEmpty())
        {
            Value = GetStringFieldOrEmpty(Object, TEXT("optionValue"));
        }
        return Value;
    }

    bool IsAgentConfigOption(const TSharedPtr<FJsonObject>& Option)
    {
        const FString OptionId = GetConfigOptionId(Option).ToLower();
        const FString Category = GetStringFieldOrEmpty(Option, TEXT("category")).ToLower();
        const FString Name = GetStringFieldOrEmpty(Option, TEXT("name")).ToLower();
        return OptionId == TEXT("agent")
            || OptionId == TEXT("agents")
            || OptionId == TEXT("mode")
            || OptionId == TEXT("modes")
            || Category == TEXT("agent")
            || Category == TEXT("agents")
            || Category == TEXT("mode")
            || Category == TEXT("modes")
            || Name == TEXT("agent")
            || Name == TEXT("agents")
            || Name == TEXT("mode")
            || Name == TEXT("modes");
    }

    bool IsModelConfigOption(const TSharedPtr<FJsonObject>& Option)
    {
        const FString OptionId = GetConfigOptionId(Option).ToLower();
        const FString Category = GetStringFieldOrEmpty(Option, TEXT("category")).ToLower();
        const FString Name = GetStringFieldOrEmpty(Option, TEXT("name")).ToLower();
        return OptionId == TEXT("model")
            || OptionId == TEXT("models")
            || OptionId == TEXT("current_model")
            || OptionId == TEXT("currentmodel")
            || Category == TEXT("model")
            || Category == TEXT("models")
            || Category == TEXT("current_model")
            || Category == TEXT("currentmodel")
            || Name == TEXT("model")
            || Name == TEXT("models")
            || Name == TEXT("current_model")
            || Name == TEXT("currentmodel");
    }

    bool IsThinkingConfigOption(const TSharedPtr<FJsonObject>& Option)
    {
        const FString OptionId = GetConfigOptionId(Option).ToLower();
        const FString Category = GetStringFieldOrEmpty(Option, TEXT("category")).ToLower();
        const FString Name = GetStringFieldOrEmpty(Option, TEXT("name")).ToLower();
        return OptionId == TEXT("thinking")
            || OptionId == TEXT("think")
            || OptionId == TEXT("reasoning")
            || OptionId == TEXT("reasoning_effort")
            || OptionId == TEXT("reasoningeffort")
            || OptionId == TEXT("reasoning.effort")
            || OptionId == TEXT("thinking_effort")
            || OptionId == TEXT("thinkingeffort")
            || OptionId == TEXT("effort")
            || Category == TEXT("thinking")
            || Category == TEXT("thought_level")
            || Category == TEXT("reasoning")
            || Category == TEXT("reasoning effort")
            || Category == TEXT("thinking effort")
            || Name == TEXT("thinking")
            || Name == TEXT("reasoning")
            || Name == TEXT("reasoning effort")
            || Name == TEXT("thinking effort");
    }

    FString NormalizeMcpHostForUrl(FString Host)
    {
        Host = Host.TrimStartAndEnd();
        if (Host.IsEmpty() || Host == TEXT("0.0.0.0") || Host == TEXT("::") || Host == TEXT("[::]") || Host == TEXT("*"))
        {
            return TEXT("127.0.0.1");
        }

        if (Host.Contains(TEXT(":")) && !Host.StartsWith(TEXT("[")) && !Host.EndsWith(TEXT("]")))
        {
            return FString::Printf(TEXT("[%s]"), *Host);
        }
        return Host;
    }

    FString GetModelProviderField(const TSharedPtr<FJsonObject>& Object)
    {
        if (!Object.IsValid())
        {
            return FString();
        }

        FString Provider;
        if (Object->TryGetStringField(TEXT("providerName"), Provider) && !Provider.IsEmpty())
        {
            return Provider;
        }
        if (Object->TryGetStringField(TEXT("provider"), Provider) && !Provider.IsEmpty())
        {
            return Provider;
        }
        if (Object->TryGetStringField(TEXT("vendorName"), Provider) && !Provider.IsEmpty())
        {
            return Provider;
        }
        if (Object->TryGetStringField(TEXT("vendor"), Provider) && !Provider.IsEmpty())
        {
            return Provider;
        }

        const TSharedPtr<FJsonObject>* ProviderObject = nullptr;
        if (Object->TryGetObjectField(TEXT("provider"), ProviderObject) && ProviderObject && ProviderObject->IsValid())
        {
            if ((*ProviderObject)->TryGetStringField(TEXT("name"), Provider) && !Provider.IsEmpty())
            {
                return Provider;
            }
            if ((*ProviderObject)->TryGetStringField(TEXT("id"), Provider) && !Provider.IsEmpty())
            {
                return Provider;
            }
        }

        return FString();
    }

    int32 GetModelContextWindowTokens(const TSharedPtr<FJsonObject>& Object)
    {
        if (!Object.IsValid())
        {
            return 0;
        }

        static const TCHAR* ContextFields[] =
        {
            TEXT("contextWindow"),
            TEXT("contextWindowTokens"),
            TEXT("contextLength"),
            TEXT("contextSize"),
            TEXT("maxContextTokens"),
            TEXT("maxInputTokens")
        };

        for (const TCHAR* FieldName : ContextFields)
        {
            double NumericValue = 0.0;
            if (Object->TryGetNumberField(FieldName, NumericValue) && NumericValue > 0.0)
            {
                return FMath::RoundToInt(NumericValue);
            }
        }

        return 0;
    }

    bool IsSafeProcessArgumentValue(const FString& Value)
    {
        if (Value.IsEmpty())
        {
            return false;
        }

        for (const TCHAR Character : Value)
        {
            if (Character == TEXT('"') || Character == TEXT('\r') || Character == TEXT('\n') || Character < 32)
            {
                return false;
            }
        }

        return true;
    }

    bool IsAbsoluteExistingExecutable(const FString& Path)
    {
        return IsSafeProcessArgumentValue(Path) && !FPaths::IsRelative(Path) && FPaths::FileExists(Path);
    }

    FString NormalizeExecutablePath(const FString& Path)
    {
        FString Normalized = Path.TrimStartAndEnd();
        FPaths::NormalizeFilename(Normalized);
        return Normalized;
    }

    FString TruncateForDisplay(const FString& Text, int32 MaxChars)
    {
        return Text.Len() <= MaxChars ? Text : Text.Left(MaxChars) + TEXT("\n[truncated]");
    }

    void AddToolActivityDetail(TArray<FString>& Details, const FString& Detail)
    {
        FString NormalizedDetail = Detail.TrimStartAndEnd();
        NormalizedDetail.ReplaceInline(TEXT("\r"), TEXT(" "));
        NormalizedDetail.ReplaceInline(TEXT("\n"), TEXT(" "));
        if (!NormalizedDetail.IsEmpty() && !Details.Contains(NormalizedDetail))
        {
            Details.Add(TruncateForDisplay(NormalizedDetail, MaxToolActivityDetailChars));
        }
    }

    FString GetJsonScalarDisplayText(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid())
        {
            return FString();
        }

        if (Value->Type == EJson::String)
        {
            return Value->AsString();
        }
        if (Value->Type == EJson::Number)
        {
            return FString::SanitizeFloat(Value->AsNumber());
        }
        if (Value->Type == EJson::Boolean)
        {
            return Value->AsBool() ? TEXT("true") : TEXT("false");
        }

        return FString();
    }

    bool IsPathLikeToolField(const FString& FieldName)
    {
        const FString NormalizedField = FieldName.ToLower();
        return NormalizedField.Contains(TEXT("path"))
            || NormalizedField.Contains(TEXT("file"))
            || NormalizedField == TEXT("uri")
            || NormalizedField == TEXT("url");
    }

    bool IsDescriptiveToolField(const FString& FieldName)
    {
        const FString NormalizedField = FieldName.ToLower();
        return IsPathLikeToolField(FieldName)
            || NormalizedField.Contains(TEXT("command"))
            || NormalizedField == TEXT("cmd")
            || NormalizedField.Contains(TEXT("query"))
            || NormalizedField.Contains(TEXT("pattern"))
            || NormalizedField.Contains(TEXT("description"));
    }

    FString FormatRawInputToolDetail(const FString& FieldName, const FString& Value)
    {
        if (IsPathLikeToolField(FieldName))
        {
            return Value;
        }

        FString Label = FieldName.TrimStartAndEnd();
        Label.ReplaceInline(TEXT("_"), TEXT(" "));
        Label.ReplaceInline(TEXT("-"), TEXT(" "));
        return Label.IsEmpty()
            ? Value
            : FString::Printf(TEXT("%s: %s"), *Label, *Value);
    }

    void CollectLocationToolDetails(const TSharedPtr<FJsonValue>& Value, TArray<FString>& Details)
    {
        if (!Value.IsValid())
        {
            return;
        }

        if (Value->Type == EJson::String)
        {
            AddToolActivityDetail(Details, Value->AsString());
            return;
        }

        if (Value->Type == EJson::Array)
        {
            for (const TSharedPtr<FJsonValue>& Entry : Value->AsArray())
            {
                CollectLocationToolDetails(Entry, Details);
            }
            return;
        }

        if (Value->Type != EJson::Object)
        {
            return;
        }

        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid())
        {
            return;
        }

        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
        {
            if (!IsPathLikeToolField(Field.Key))
            {
                continue;
            }

            const FString ScalarText = GetJsonScalarDisplayText(Field.Value);
            if (!ScalarText.IsEmpty())
            {
                AddToolActivityDetail(Details, ScalarText);
            }
        }
    }

    void CollectRawInputToolDetails(const TSharedPtr<FJsonValue>& Value, TArray<FString>& Details, int32 Depth = 0)
    {
        if (!Value.IsValid() || Depth > 2)
        {
            return;
        }

        if (Value->Type == EJson::Array)
        {
            for (const TSharedPtr<FJsonValue>& Entry : Value->AsArray())
            {
                CollectRawInputToolDetails(Entry, Details, Depth + 1);
            }
            return;
        }

        if (Value->Type != EJson::Object)
        {
            if (Depth == 0)
            {
                AddToolActivityDetail(Details, GetJsonScalarDisplayText(Value));
            }
            return;
        }

        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid())
        {
            return;
        }

        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
        {
            const FString ScalarText = GetJsonScalarDisplayText(Field.Value);
            if (IsDescriptiveToolField(Field.Key) && !ScalarText.IsEmpty())
            {
                AddToolActivityDetail(Details, FormatRawInputToolDetail(Field.Key, ScalarText));
                continue;
            }

            if (Field.Value.IsValid() && (Field.Value->Type == EJson::Object || Field.Value->Type == EJson::Array))
            {
                CollectRawInputToolDetails(Field.Value, Details, Depth + 1);
            }
        }
    }

    FString ExtractToolActivityDetail(const TSharedPtr<FJsonObject>& Update)
    {
        if (!Update.IsValid())
        {
            return FString();
        }

        TArray<FString> Details;
        CollectLocationToolDetails(Update->TryGetField(TEXT("locations")), Details);
        CollectRawInputToolDetails(Update->TryGetField(TEXT("rawInput")), Details);

        return FString::Join(Details, TEXT(", "));
    }

    bool IsFinalToolActivityStatus(const FString& Status)
    {
        FString NormalizedStatus = Status.TrimStartAndEnd().ToLower();
        NormalizedStatus.ReplaceInline(TEXT("-"), TEXT("_"));
        return NormalizedStatus == TEXT("completed")
            || NormalizedStatus == TEXT("complete")
            || NormalizedStatus == TEXT("failed")
            || NormalizedStatus == TEXT("cancelled")
            || NormalizedStatus == TEXT("canceled");
    }

    void TerminateAndCloseProcess(FProcHandle& ProcessHandle)
    {
        if (!ProcessHandle.IsValid())
        {
            return;
        }

        if (FPlatformProcess::IsProcRunning(ProcessHandle))
        {
#if PLATFORM_UNIX
            FPlatformProcess::TerminateProc(ProcessHandle, false);
#else
            FPlatformProcess::TerminateProc(ProcessHandle, true);
#endif
            const double ShutdownDeadline = FPlatformTime::Seconds() + ClientProcessShutdownWaitSeconds;
            while (FPlatformProcess::IsProcRunning(ProcessHandle) && FPlatformTime::Seconds() < ShutdownDeadline)
            {
                FPlatformProcess::Sleep(0.01f);
            }
        }

        FPlatformProcess::CloseProc(ProcessHandle);
        ProcessHandle.Reset();
    }

    class FScopedOpenCodeSignalLaunchGuard
    {
    public:
        FScopedOpenCodeSignalLaunchGuard()
        {
#if PLATFORM_LINUX && defined(SIGPWR)
            if (sigaction(SIGPWR, nullptr, &PreviousSigPwrAction) == 0 && PreviousSigPwrAction.sa_handler == SIG_IGN)
            {
                struct sigaction DefaultAction;
                FMemory::Memzero(&DefaultAction, sizeof(DefaultAction));
                DefaultAction.sa_handler = SIG_DFL;
                sigemptyset(&DefaultAction.sa_mask);
                bRestoreSigPwr = sigaction(SIGPWR, &DefaultAction, nullptr) == 0;
            }
#endif
        }

        ~FScopedOpenCodeSignalLaunchGuard()
        {
#if PLATFORM_LINUX && defined(SIGPWR)
            if (bRestoreSigPwr)
            {
                sigaction(SIGPWR, &PreviousSigPwrAction, nullptr);
            }
#endif
        }

    private:
#if PLATFORM_LINUX && defined(SIGPWR)
        struct sigaction PreviousSigPwrAction;
        bool bRestoreSigPwr = false;
#endif
    };
}

FOpenCodeAcpClient::FOpenCodeAcpClient() = default;

FOpenCodeAcpClient::~FOpenCodeAcpClient()
{
    Stop();
}

bool FOpenCodeAcpClient::Start(const FString& InWorkingDirectory)
{
    Stop();
    ResetState();

    WorkingDirectory = FPaths::ConvertRelativePathToFull(InWorkingDirectory.IsEmpty() ? FPaths::ProjectDir() : InWorkingDirectory);
    FPaths::NormalizeDirectoryName(WorkingDirectory);

    if (!FPaths::DirectoryExists(WorkingDirectory) || !IsSafeProcessArgumentValue(WorkingDirectory))
    {
        SetStatus(FString::Printf(TEXT("OpenCode ACP working directory is invalid: %s"), *WorkingDirectory));
        return false;
    }

    if (!EnsureProjectUnrealAgentConfig())
    {
        SetStatus(FString::Printf(TEXT("Failed to prepare Unreal Agent OpenCode config in %s"), *WorkingDirectory));
        return false;
    }

    FString ResolveError;
    ResolvedExecutable = ResolveOpenCodeExecutable(ResolveError);
    if (ResolvedExecutable.IsEmpty())
    {
        SetStatus(ResolveError.IsEmpty() ? TEXT("OpenCode executable not found.") : ResolveError);
        return false;
    }

    const FString Params = TEXT("acp");

    if (!FPlatformProcess::CreatePipe(OutputReadPipe, OutputWritePipe)
        || !FPlatformProcess::CreatePipe(ErrorReadPipe, ErrorWritePipe)
        || !FPlatformProcess::CreatePipe(InputReadPipe, InputWritePipe, true))
    {
        CloseProcessPipes();
        SetStatus(TEXT("Failed to create OpenCode ACP process pipes."));
        return false;
    }

    {
        FScopedOpenCodeSignalLaunchGuard SignalLaunchGuard;
        ProcessHandle = FPlatformProcess::CreateProc(
            *ResolvedExecutable,
            *Params,
            false,
            true,
            true,
            nullptr,
            0,
            *WorkingDirectory,
            OutputWritePipe,
            InputReadPipe,
            ErrorWritePipe);
    }

    if (!ProcessHandle.IsValid())
    {
        CloseProcessPipes();
        SetStatus(FormatProcessErrorText(FString::Printf(TEXT("Failed to launch OpenCode ACP: %s %s"), *ResolvedExecutable, *Params)));
        return false;
    }

    bRunning = true;
    SetStatus(TEXT("Starting OpenCode ACP..."));
    if (!SendInitialize())
    {
        StopWithError(TEXT("Failed to send OpenCode ACP initialize request."));
        return false;
    }
    return true;
}

void FOpenCodeAcpClient::Stop()
{
    const bool bWasActive = bRunning
        || bInitialized
        || bReady
        || ProcessHandle.IsValid()
        || OutputReadPipe != nullptr
        || OutputWritePipe != nullptr
        || ErrorReadPipe != nullptr
        || ErrorWritePipe != nullptr
        || InputReadPipe != nullptr
        || InputWritePipe != nullptr;

    if (ProcessHandle.IsValid())
    {
        TerminateAndCloseProcess(ProcessHandle);
    }

    CloseProcessPipes();
    ResetState();
    if (bWasActive)
    {
        OnStopped.ExecuteIfBound();
    }
}

void FOpenCodeAcpClient::Tick()
{
    if (!bRunning)
    {
        return;
    }

    ReadProcessOutput();
    DrainProcessErrorOutput();

    if (ProcessHandle.IsValid() && !FPlatformProcess::IsProcRunning(ProcessHandle))
    {
        int32 ReturnCode = -1;
        FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
        MarkProcessExited(ReturnCode, false);
        return;
    }

    CheckClientOwnedRequestTimeouts();
}

void FOpenCodeAcpClient::ReadProcessOutput()
{
    if (OutputReadPipe == nullptr)
    {
        return;
    }

    TArray<uint8> Output;
    if (FPlatformProcess::ReadPipeToArray(OutputReadPipe, Output) && Output.Num() > 0)
    {
        ProcessOutputBytes(Output);
    }
}

void FOpenCodeAcpClient::DrainProcessErrorOutput()
{
    if (ErrorReadPipe == nullptr)
    {
        return;
    }

    TArray<uint8> Output;
    if (FPlatformProcess::ReadPipeToArray(ErrorReadPipe, Output) && Output.Num() > 0)
    {
        const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Output.GetData()), Output.Num());
        AppendRecentErrorOutput(FString(Converted.Length(), Converted.Get()));
    }
}

void FOpenCodeAcpClient::AppendRecentErrorOutput(const FString& Text)
{
    RecentErrorOutput += Text;
    if (RecentErrorOutput.Len() > MaxRecentErrorOutputChars)
    {
        RecentErrorOutput = RecentErrorOutput.Right(MaxRecentErrorOutputChars);
    }
}

void FOpenCodeAcpClient::CloseProcessPipes()
{
    if (OutputReadPipe != nullptr || OutputWritePipe != nullptr)
    {
        FPlatformProcess::ClosePipe(OutputReadPipe, OutputWritePipe);
        OutputReadPipe = nullptr;
        OutputWritePipe = nullptr;
    }

    if (ErrorReadPipe != nullptr || ErrorWritePipe != nullptr)
    {
        FPlatformProcess::ClosePipe(ErrorReadPipe, ErrorWritePipe);
        ErrorReadPipe = nullptr;
        ErrorWritePipe = nullptr;
    }

    if (InputReadPipe != nullptr || InputWritePipe != nullptr)
    {
        FPlatformProcess::ClosePipe(InputReadPipe, InputWritePipe);
        InputReadPipe = nullptr;
        InputWritePipe = nullptr;
    }
}

void FOpenCodeAcpClient::MarkProcessExited(int32 ReturnCode, bool bCanceled)
{
    ReadProcessOutput();
    DrainProcessErrorOutput();
    const FString ExitText = FormatProcessErrorText(FString::Printf(TEXT("OpenCode ACP exited with code %d"), ReturnCode));

    if (ProcessHandle.IsValid())
    {
        FPlatformProcess::CloseProc(ProcessHandle);
        ProcessHandle.Reset();
    }

    CloseProcessPipes();
    ResetState();

    if (!bCanceled)
    {
        SetStatus(ExitText);
        AppendTranscript(TEXT("Error"), ExitText);
    }
    OnStopped.ExecuteIfBound();
}

void FOpenCodeAcpClient::CheckClientOwnedRequestTimeouts()
{
    const double Now = FPlatformTime::Seconds();

    if (InitializeRequestId != INDEX_NONE
        && InitializeRequestStartedAt > 0.0
        && Now - InitializeRequestStartedAt > ClientLifecycleRequestTimeoutSeconds)
    {
        StopWithError(TEXT("OpenCode ACP initialize request timed out."));
        return;
    }

    if (NewSessionRequestId != INDEX_NONE
        && NewSessionRequestStartedAt > 0.0
        && Now - NewSessionRequestStartedAt > ClientLifecycleRequestTimeoutSeconds)
    {
        StopWithError(TEXT("OpenCode ACP session creation timed out."));
        return;
    }

    if (SetModelRequestId != INDEX_NONE
        && SetModelRequestStartedAt > 0.0
        && Now - SetModelRequestStartedAt > ClientConfigRequestTimeoutSeconds)
    {
        PendingModel.Reset();
        SetModelRequestId = INDEX_NONE;
        SetModelRequestStartedAt = 0.0;
        OnModelsChanged.ExecuteIfBound();
        SetStatus(TEXT("OpenCode ACP model switch timed out."));
        AppendTranscript(TEXT("Error"), TEXT("OpenCode ACP model switch timed out."));
    }

    if (SetThinkingRequestId != INDEX_NONE
        && SetThinkingRequestStartedAt > 0.0
        && Now - SetThinkingRequestStartedAt > ClientConfigRequestTimeoutSeconds)
    {
        PendingThinking.Reset();
        SetThinkingRequestId = INDEX_NONE;
        SetThinkingRequestStartedAt = 0.0;
        OnModelsChanged.ExecuteIfBound();
        SetStatus(TEXT("OpenCode ACP thinking switch timed out."));
        AppendTranscript(TEXT("Error"), TEXT("OpenCode ACP thinking switch timed out."));
    }

    if (SetAgentRequestId != INDEX_NONE
        && SetAgentRequestStartedAt > 0.0
        && Now - SetAgentRequestStartedAt > ClientConfigRequestTimeoutSeconds)
    {
        PendingAgent.Reset();
        SetAgentRequestId = INDEX_NONE;
        SetAgentRequestStartedAt = 0.0;
        OnModelsChanged.ExecuteIfBound();
        SetStatus(TEXT("OpenCode ACP agent switch timed out."));
        AppendTranscript(TEXT("Error"), TEXT("OpenCode ACP agent switch timed out."));
    }
}

bool FOpenCodeAcpClient::SendPrompt(const FString& PromptText)
{
    if (!bReady || bPromptInFlight || PromptText.TrimStartAndEnd().IsEmpty())
    {
        return false;
    }

    FString PromptForAcp = PromptText;
    if (bAttachEditorContext)
    {
        const FString ContextEnvelope = RefreshEditorContext();
        if (!ContextEnvelope.TrimStartAndEnd().IsEmpty())
        {
            PromptForAcp += TEXT("\n\n");
            PromptForAcp += ContextEnvelope;
        }
    }

    auto TextBlock = MakeObject();
    TextBlock->SetStringField(TEXT("type"), TEXT("text"));
    TextBlock->SetStringField(TEXT("text"), PromptForAcp);

    TArray<TSharedPtr<FJsonValue>> Prompt;
    Prompt.Add(MakeShared<FJsonValueObject>(TextBlock));

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetArrayField(TEXT("prompt"), Prompt);

    const int32 RequestId = SendRequest(TEXT("session/prompt"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP prompt request."));
        return false;
    }

    AppendTranscript(TEXT("You"), PromptText);
    ActivePromptRequestId = RequestId;
    bPromptInFlight = true;
    bCancelRequested = false;
    SetStatus(TEXT("OpenCode is working..."));
    return true;
}

FString FOpenCodeAcpClient::RefreshEditorContext()
{
    const FString ContextProjectDirectory = WorkingDirectory.IsEmpty() ? FPaths::ProjectDir() : WorkingDirectory;
    const FUnrealAgentEditorContextSnapshot Snapshot = FUnrealAgentEditorContext::Capture(ContextProjectDirectory);
    LastEditorContextSummary = Snapshot.Summary;
    LastEditorContextEnvelope = Snapshot.Envelope;
    return LastEditorContextEnvelope;
}

bool FOpenCodeAcpClient::RunProjectValidation()
{
    const FString ValidationProjectDirectory = WorkingDirectory.IsEmpty() ? FPaths::ProjectDir() : WorkingDirectory;
    const FUnrealAgentValidationResult Result = FUnrealAgentValidationRunner::RunFastValidation(ValidationProjectDirectory);
    LastValidationSummary = FUnrealAgentValidationRunner::FormatForTranscript(Result);
    AppendTranscript(Result.bPassed ? TEXT("Tool") : TEXT("Error"), LastValidationSummary);
    return Result.bPassed;
}

void FOpenCodeAcpClient::CancelPrompt()
{
    if (!bRunning || !bPromptInFlight || bCancelRequested || SessionId.IsEmpty())
    {
        return;
    }

    if (!SendCancelledPermissionResponse())
    {
        StopWithError(TEXT("Failed to cancel pending OpenCode ACP permission request."));
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    if (!SendNotification(TEXT("session/cancel"), Params))
    {
        StopWithError(TEXT("Failed to send OpenCode ACP cancellation request."));
        return;
    }

    bCancelRequested = true;
    SetStatus(TEXT("Cancelling OpenCode turn..."));
    AppendTranscript(TEXT("System"), TEXT("Cancel requested. Waiting for OpenCode ACP to finish the turn."));
}

void FOpenCodeAcpClient::SetModel(const FString& ModelId)
{
    if (!CanSelectModel() || ModelId.IsEmpty() || ModelId == CurrentModel)
    {
        return;
    }

    const bool bKnownModel = ModelOptions.ContainsByPredicate([&ModelId](const FOpenCodeAcpModelOption& Option)
    {
        return Option.Id == ModelId;
    });

    if (!bKnownModel)
    {
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetStringField(TEXT("configId"), ModelConfigId);
    Params->SetStringField(TEXT("value"), ModelId);

    const int32 RequestId = SendRequest(TEXT("session/set_config_option"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP model switch request."));
        return;
    }

    PendingModel = ModelId;
    SetModelRequestId = RequestId;
    SetModelRequestStartedAt = FPlatformTime::Seconds();
    SetStatus(FString::Printf(TEXT("Switching model to %s..."), *GetModelDisplayName(ModelId)));
}

void FOpenCodeAcpClient::SetThinking(const FString& ThinkingId)
{
    if (!CanSelectThinking() || ThinkingId.IsEmpty() || ThinkingId == CurrentThinking)
    {
        return;
    }

    const bool bKnownThinking = ThinkingOptions.ContainsByPredicate([&ThinkingId](const FOpenCodeAcpThinkingOption& Option)
    {
        return Option.Id == ThinkingId;
    });

    if (!bKnownThinking)
    {
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetStringField(TEXT("configId"), ThinkingConfigId);
    Params->SetStringField(TEXT("value"), ThinkingId);

    const int32 RequestId = SendRequest(TEXT("session/set_config_option"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP thinking switch request."));
        return;
    }

    PendingThinking = ThinkingId;
    SetThinkingRequestId = RequestId;
    SetThinkingRequestStartedAt = FPlatformTime::Seconds();
    SetStatus(FString::Printf(TEXT("Switching thinking to %s..."), *GetThinkingDisplayName(ThinkingId)));
}

void FOpenCodeAcpClient::SetAgent(const FString& AgentId)
{
    if (!CanSelectAgent() || AgentId.IsEmpty() || AgentId == CurrentAgent)
    {
        return;
    }

    const bool bKnownAgent = AgentOptions.ContainsByPredicate([&AgentId](const FOpenCodeAcpAgentOption& Option)
    {
        return Option.Id == AgentId;
    });

    if (!bKnownAgent)
    {
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetStringField(TEXT("configId"), AgentConfigId);
    Params->SetStringField(TEXT("value"), AgentId);

    const int32 RequestId = SendRequest(TEXT("session/set_config_option"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP agent switch request."));
        return;
    }

    PendingAgent = AgentId;
    SetAgentRequestId = RequestId;
    SetAgentRequestStartedAt = FPlatformTime::Seconds();
    SetStatus(FString::Printf(TEXT("Switching agent to %s..."), *GetAgentDisplayName(AgentId)));
}

void FOpenCodeAcpClient::ApprovePermissionOnce()
{
    ResolvePendingPermission(FindPendingPermissionOption({ TEXT("once"), TEXT("allow-once"), TEXT("allow") }, { TEXT("allow_once"), TEXT("allow") }));
}

void FOpenCodeAcpClient::ApprovePermissionAlways()
{
    ResolvePendingPermission(FindPendingPermissionOption({ TEXT("always"), TEXT("allow-always"), TEXT("allow_always") }, { TEXT("allow_always") }));
}

void FOpenCodeAcpClient::RejectPermission()
{
    ResolvePendingPermission(FindPendingPermissionOption({ TEXT("reject"), TEXT("reject-once"), TEXT("reject_once"), TEXT("reject-always"), TEXT("reject_always"), TEXT("deny") }, { TEXT("reject_once"), TEXT("reject_always"), TEXT("deny") }));
}

bool FOpenCodeAcpClient::CanApprovePermissionAlways() const
{
    return !FindPendingPermissionOption({ TEXT("always"), TEXT("allow-always"), TEXT("allow_always") }, { TEXT("allow_always") }).IsEmpty();
}

bool FOpenCodeAcpClient::SendInitialize()
{
    auto Capabilities = MakeObject();

    auto Fs = MakeObject();
    Fs->SetBoolField(TEXT("readTextFile"), false);
    Fs->SetBoolField(TEXT("writeTextFile"), false);
    Capabilities->SetObjectField(TEXT("fs"), Fs);
    Capabilities->SetBoolField(TEXT("terminal"), false);

    auto ClientInfo = MakeObject();
    ClientInfo->SetStringField(TEXT("name"), TEXT("Unreal Agent"));
    ClientInfo->SetStringField(TEXT("version"), TEXT("0.1.0"));

    auto Params = MakeObject();
    Params->SetNumberField(TEXT("protocolVersion"), 1.0);
    Params->SetObjectField(TEXT("clientCapabilities"), Capabilities);
    Params->SetObjectField(TEXT("clientInfo"), ClientInfo);

    InitializeRequestId = SendRequest(TEXT("initialize"), Params);
    if (InitializeRequestId == INDEX_NONE)
    {
        return false;
    }

    InitializeRequestStartedAt = FPlatformTime::Seconds();
    return true;
}

bool FOpenCodeAcpClient::SendNewSession()
{
    TArray<TSharedPtr<FJsonValue>> McpServers;
    AddConfiguredMcpServers(McpServers);

    auto Params = MakeObject();
    Params->SetStringField(TEXT("cwd"), WorkingDirectory);
    Params->SetArrayField(TEXT("mcpServers"), McpServers);

    NewSessionRequestId = SendRequest(TEXT("session/new"), Params);
    if (NewSessionRequestId == INDEX_NONE)
    {
        return false;
    }

    NewSessionRequestStartedAt = FPlatformTime::Seconds();
    return true;
}

bool FOpenCodeAcpClient::EnsureProjectUnrealAgentConfig()
{
    const FUnrealAgentStudioKitResult Result = FUnrealAgentStudioKit::EnsureForProject(WorkingDirectory);
    LastStudioKitSummary = Result.Summary;
    return Result.WasSuccessful();
}

void FOpenCodeAcpClient::AddConfiguredMcpServers(TArray<TSharedPtr<FJsonValue>>& McpServers) const
{
    bool bEnableNativeMcp = false;
    if (GConfig == nullptr || !GConfig->GetBool(AutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), bEnableNativeMcp, GGameIni) || !bEnableNativeMcp)
    {
        return;
    }

    int32 NativeMcpPort = 3000;
    GConfig->GetInt(AutomationBridgeSettingsSection, TEXT("NativeMCPPort"), NativeMcpPort, GGameIni);
    if (NativeMcpPort <= 0 || NativeMcpPort > 65535)
    {
        NativeMcpPort = 3000;
    }

    FString ListenHost = TEXT("127.0.0.1");
    GConfig->GetString(AutomationBridgeSettingsSection, TEXT("ListenHost"), ListenHost, GGameIni);
    const FString Url = FString::Printf(TEXT("http://%s:%d/mcp"), *NormalizeMcpHostForUrl(ListenHost), NativeMcpPort);

    auto Server = MakeObject();
    Server->SetStringField(TEXT("type"), TEXT("http"));
    Server->SetStringField(TEXT("name"), UnrealMcpServerName);
    Server->SetStringField(TEXT("url"), Url);

    TArray<TSharedPtr<FJsonValue>> Headers;
    bool bRequireCapabilityToken = false;
    FString CapabilityToken;
    GConfig->GetBool(AutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), bRequireCapabilityToken, GGameIni);
    GConfig->GetString(AutomationBridgeSettingsSection, TEXT("CapabilityToken"), CapabilityToken, GGameIni);
    if (bRequireCapabilityToken && !CapabilityToken.IsEmpty())
    {
        auto Header = MakeObject();
        Header->SetStringField(TEXT("name"), TEXT("X-MCP-Capability-Token"));
        Header->SetStringField(TEXT("value"), CapabilityToken);
        Headers.Add(MakeShared<FJsonValueObject>(Header));
    }
    Server->SetArrayField(TEXT("headers"), Headers);

    McpServers.Add(MakeShared<FJsonValueObject>(Server));
}

int32 FOpenCodeAcpClient::SendRequest(const FString& Method, const TSharedPtr<FJsonObject>& Params)
{
    const int32 Id = NextRequestId++;

    auto Root = MakeObject();
    Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    Root->SetNumberField(TEXT("id"), Id);
    Root->SetStringField(TEXT("method"), Method);
    Root->SetObjectField(TEXT("params"), Params.IsValid() ? Params : MakeObject());
    return SendJsonObject(Root) ? Id : INDEX_NONE;
}

bool FOpenCodeAcpClient::SendNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params)
{
    auto Root = MakeObject();
    Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    Root->SetStringField(TEXT("method"), Method);
    Root->SetObjectField(TEXT("params"), Params.IsValid() ? Params : MakeObject());
    return SendJsonObject(Root);
}

bool FOpenCodeAcpClient::SendResponse(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result)
{
    auto Root = MakeObject();
    Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
    Root->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeObject());
    return SendJsonObject(Root);
}

bool FOpenCodeAcpClient::SendError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message)
{
    auto Error = MakeObject();
    Error->SetNumberField(TEXT("code"), Code);
    Error->SetStringField(TEXT("message"), Message);

    auto Root = MakeObject();
    Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
    Root->SetObjectField(TEXT("error"), Error);
    return SendJsonObject(Root);
}

bool FOpenCodeAcpClient::SendJsonObject(const TSharedPtr<FJsonObject>& Root)
{
    if (!ProcessHandle.IsValid() || !FPlatformProcess::IsProcRunning(ProcessHandle) || InputWritePipe == nullptr || !Root.IsValid())
    {
        return false;
    }

    const FString Json = JsonToString(Root);
    if (Json.IsEmpty())
    {
        return false;
    }

    FTCHARToUTF8 Converted(*Json);
    TArray<uint8> Payload;
    Payload.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    Payload.Add(static_cast<uint8>('\n'));

    int32 BytesWritten = 0;
    return FPlatformProcess::WritePipe(InputWritePipe, Payload.GetData(), Payload.Num(), &BytesWritten)
        && BytesWritten == Payload.Num();
}

void FOpenCodeAcpClient::ProcessOutputBytes(const TArray<uint8>& Bytes)
{
    OutputBuffer.Append(Bytes);
    if (OutputBuffer.Num() > MaxOutputBufferBytes)
    {
        OutputBuffer.Reset();
        return;
    }

    while (true)
    {
        const int32 LineEndIndex = OutputBuffer.IndexOfByKey(static_cast<uint8>('\n'));
        if (LineEndIndex == INDEX_NONE)
        {
            return;
        }

        TArray<uint8> LineBytes;
        if (LineEndIndex > 0)
        {
            LineBytes.Append(OutputBuffer.GetData(), LineEndIndex);
        }
        OutputBuffer.RemoveAt(0, LineEndIndex + 1, EAllowShrinking::No);

        if (LineBytes.Num() > 0 && LineBytes.Last() == static_cast<uint8>('\r'))
        {
            LineBytes.Pop(EAllowShrinking::No);
        }
        if (LineBytes.Num() == 0)
        {
            continue;
        }

        const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(LineBytes.GetData()), LineBytes.Num());
        const FString Line = FString(Converted.Length(), Converted.Get()).TrimStartAndEnd();
        if (Line.IsEmpty())
        {
            continue;
        }
        if (ShouldIgnoreProcessOutputLine(Line))
        {
            continue;
        }
        if (!Line.StartsWith(TEXT("{")))
        {
            AppendRecentErrorOutput(FString::Printf(TEXT("Ignored non-JSON ACP stdout line: %s"), *Line));
            continue;
        }

        TSharedPtr<FJsonObject> Message;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
        if (!FJsonSerializer::Deserialize(Reader, Message) || !Message.IsValid())
        {
            AppendRecentErrorOutput(FString::Printf(TEXT("Ignored non-JSON ACP stdout line: %s"), *Line));
            continue;
        }

        HandleMessage(Message);
    }
}

void FOpenCodeAcpClient::HandleMessage(const TSharedPtr<FJsonObject>& Message)
{
    int32 Id = INDEX_NONE;
    FString Method;

    if (TryReadIdAsInt(Message, Id) && Message->HasField(TEXT("result")))
    {
        HandleResponse(Message, Id);
        return;
    }

    if (TryReadIdAsInt(Message, Id) && Message->HasField(TEXT("error")))
    {
        FString ErrorText = TEXT("OpenCode request failed");
        const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
        if (Message->TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject && ErrorObject->IsValid())
        {
            FString ErrorMessage;
            if ((*ErrorObject)->TryGetStringField(TEXT("message"), ErrorMessage))
            {
                ErrorText = ErrorMessage;
            }
        }

        if (Id == ActivePromptRequestId)
        {
            bPromptInFlight = false;
            bCancelRequested = false;
            ActivePromptRequestId = INDEX_NONE;
            PendingPermissionId.Reset();
            PendingPermissionOptions.Reset();
        }

        if (Id == SetModelRequestId)
        {
            SetModelRequestId = INDEX_NONE;
            SetModelRequestStartedAt = 0.0;
            PendingModel.Reset();
            OnModelsChanged.ExecuteIfBound();
        }

        if (Id == SetThinkingRequestId)
        {
            SetThinkingRequestId = INDEX_NONE;
            SetThinkingRequestStartedAt = 0.0;
            PendingThinking.Reset();
            OnModelsChanged.ExecuteIfBound();
        }

        if (Id == SetAgentRequestId)
        {
            SetAgentRequestId = INDEX_NONE;
            SetAgentRequestStartedAt = 0.0;
            PendingAgent.Reset();
            OnModelsChanged.ExecuteIfBound();
        }

        if (Id == InitializeRequestId || Id == NewSessionRequestId)
        {
            StopWithError(ErrorText);
            return;
        }

        SetStatus(ErrorText);
        AppendTranscript(TEXT("Error"), ErrorText);
        return;
    }

    if (Message->TryGetStringField(TEXT("method"), Method))
    {
        HandleNotificationOrRequest(Message, Method);
    }
}

void FOpenCodeAcpClient::HandleResponse(const TSharedPtr<FJsonObject>& Message, int32 Id)
{
    const TSharedPtr<FJsonObject>* Result = nullptr;
    Message->TryGetObjectField(TEXT("result"), Result);

    if (Id == InitializeRequestId)
    {
        InitializeRequestId = INDEX_NONE;
        InitializeRequestStartedAt = 0.0;

        double AgentProtocolVersion = 0.0;
        if (!Result || !Result->IsValid() || !(*Result)->TryGetNumberField(TEXT("protocolVersion"), AgentProtocolVersion) || static_cast<int32>(AgentProtocolVersion) != SupportedAcpProtocolVersion)
        {
            StopWithError(FString::Printf(TEXT("OpenCode ACP protocol version is unsupported. Expected %d."), SupportedAcpProtocolVersion));
            return;
        }

        bInitialized = true;
        SetStatus(TEXT("OpenCode ACP initialized. Creating session..."));
        if (!SendNewSession())
        {
            StopWithError(TEXT("Failed to send OpenCode ACP session creation request."));
        }
        return;
    }

    if (Id == NewSessionRequestId)
    {
        NewSessionRequestId = INDEX_NONE;
        NewSessionRequestStartedAt = 0.0;
        if (Result && Result->IsValid())
        {
            (*Result)->TryGetStringField(TEXT("sessionId"), SessionId);
            ParseModelOptionsFromResult(*Result);
            ParseThinkingOptionsFromResult(*Result);
            ParseAgentOptionsFromResult(*Result);
        }

        if (SessionId.IsEmpty())
        {
            StopWithError(TEXT("OpenCode ACP session creation returned no session id."));
            return;
        }

        bReady = true;
        OnModelsChanged.ExecuteIfBound();
        const bool bDefaultAgentSwitchStarted = TrySelectDefaultAgent();
        const FString ModelSuffix = CurrentModel.IsEmpty() ? FString() : FString::Printf(TEXT(" Model: %s."), *GetModelDisplayName(CurrentModel));
        const FString ThinkingSuffix = CurrentThinking.IsEmpty() ? FString() : FString::Printf(TEXT(" Thinking: %s."), *GetThinkingDisplayName(CurrentThinking));
        const FString AgentSuffix = CurrentAgent.IsEmpty() ? FString() : FString::Printf(TEXT(" Agent: %s."), *GetAgentDisplayName(CurrentAgent));
        if (!bDefaultAgentSwitchStarted)
        {
            SetStatus(FString::Printf(TEXT("Connected to OpenCode ACP. Session: %s.%s%s%s"), *SessionId, *ModelSuffix, *ThinkingSuffix, *AgentSuffix));
        }
        AppendTranscript(TEXT("System"), TEXT("OpenCode ACP is connected with Unreal Agent project config and MCP editor tools."));
        return;
    }

    if (Id == SetModelRequestId)
    {
        bool bHasAuthoritativeModel = false;
        if (Result && Result->IsValid())
        {
            bHasAuthoritativeModel = ParseModelOptionsFromResult(*Result);
            ParseThinkingOptionsFromResult(*Result);
            ParseAgentOptionsFromResult(*Result);
        }

        if (!bHasAuthoritativeModel && !PendingModel.IsEmpty())
        {
            SetCurrentModel(PendingModel);
        }

        PendingModel.Reset();
        SetModelRequestId = INDEX_NONE;
        SetModelRequestStartedAt = 0.0;
        OnModelsChanged.ExecuteIfBound();
        SetStatus(FString::Printf(TEXT("Ready. Model: %s."), *GetModelDisplayName(CurrentModel)));
        return;
    }

    if (Id == SetThinkingRequestId)
    {
        bool bHasAuthoritativeThinking = false;
        if (Result && Result->IsValid())
        {
            ParseModelOptionsFromResult(*Result);
            bHasAuthoritativeThinking = ParseThinkingOptionsFromResult(*Result);
            ParseAgentOptionsFromResult(*Result);
        }

        if (!bHasAuthoritativeThinking && !PendingThinking.IsEmpty())
        {
            CurrentThinking = PendingThinking;
        }

        PendingThinking.Reset();
        SetThinkingRequestId = INDEX_NONE;
        SetThinkingRequestStartedAt = 0.0;
        OnModelsChanged.ExecuteIfBound();
        SetStatus(FString::Printf(TEXT("Ready. Thinking: %s."), *GetThinkingDisplayName(CurrentThinking)));
        return;
    }

    if (Id == SetAgentRequestId)
    {
        bool bHasAuthoritativeAgent = false;
        if (Result && Result->IsValid())
        {
            ParseModelOptionsFromResult(*Result);
            ParseThinkingOptionsFromResult(*Result);
            bHasAuthoritativeAgent = ParseAgentOptionsFromResult(*Result);
        }

        if (!bHasAuthoritativeAgent && !PendingAgent.IsEmpty())
        {
            CurrentAgent = PendingAgent;
        }

        PendingAgent.Reset();
        SetAgentRequestId = INDEX_NONE;
        SetAgentRequestStartedAt = 0.0;
        OnModelsChanged.ExecuteIfBound();
        SetStatus(FString::Printf(TEXT("Ready. Agent: %s."), *GetAgentDisplayName(CurrentAgent)));
        return;
    }

    if (Id == ActivePromptRequestId)
    {
        bPromptInFlight = false;
        bCancelRequested = false;
        ActivePromptRequestId = INDEX_NONE;
        PendingPermissionId.Reset();
        PendingPermissionOptions.Reset();
        FString StopReason;
        if (Result && Result->IsValid())
        {
            (*Result)->TryGetStringField(TEXT("stopReason"), StopReason);
        }
        SetStatus(StopReason.IsEmpty() ? TEXT("Ready") : FString::Printf(TEXT("Ready. Stop reason: %s"), *StopReason));
    }
}

void FOpenCodeAcpClient::HandleNotificationOrRequest(const TSharedPtr<FJsonObject>& Message, const FString& Method)
{
    const TSharedPtr<FJsonObject>* Params = nullptr;
    Message->TryGetObjectField(TEXT("params"), Params);

    if (Method == TEXT("session/update") && Params && Params->IsValid())
    {
        HandleSessionUpdate(*Params);
        return;
    }

    if (Method == TEXT("session/request_permission") && Params && Params->IsValid())
    {
        HandlePermissionRequest(Message, *Params);
        return;
    }

    if (Message->HasField(TEXT("id")))
    {
        if (!SendError(CloneJsonId(Message), -32601, FString::Printf(TEXT("Unsupported ACP client method: %s"), *Method)))
        {
            StopWithError(TEXT("Failed to send OpenCode ACP error response."));
        }
    }
}

void FOpenCodeAcpClient::HandleSessionUpdate(const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<FJsonObject>* Update = nullptr;
    if (!Params->TryGetObjectField(TEXT("update"), Update) || !Update || !Update->IsValid())
    {
        return;
    }

    const FString UpdateType = GetStringFieldOrEmpty(*Update, TEXT("sessionUpdate"));

    if (UpdateType == TEXT("config_option_update"))
    {
        const bool bParsedModel = ParseModelOptionsFromResult(*Update);
        const bool bParsedThinking = ParseThinkingOptionsFromResult(*Update);
        const bool bParsedAgent = ParseAgentOptionsFromResult(*Update);
        if (!bParsedModel && !bParsedThinking && !bParsedAgent)
        {
            HandleModelUpdate(*Update);
            HandleThinkingUpdate(*Update);
            HandleAgentUpdate(*Update);
        }
        OnModelsChanged.ExecuteIfBound();
        return;
    }

    if (UpdateType == TEXT("current_model_update"))
    {
        HandleModelUpdate(*Update);
        return;
    }

    if (UpdateType == TEXT("current_thinking_update") || UpdateType == TEXT("current_reasoning_update"))
    {
        HandleThinkingUpdate(*Update);
        return;
    }

    if (UpdateType == TEXT("current_agent_update"))
    {
        HandleAgentUpdate(*Update);
        return;
    }

    if (UpdateType == TEXT("usage_update"))
    {
        HandleUsageUpdate(*Update);
        return;
    }

    if (UpdateType == TEXT("agent_message_chunk") || UpdateType == TEXT("agent_thought_chunk") || UpdateType == TEXT("user_message_chunk"))
    {
        const TSharedPtr<FJsonObject>* Content = nullptr;
        if ((*Update)->TryGetObjectField(TEXT("content"), Content) && Content && Content->IsValid())
        {
            FString Text;
            if ((*Content)->TryGetStringField(TEXT("text"), Text) && !Text.IsEmpty())
            {
                const FString Role = UpdateType == TEXT("agent_thought_chunk") ? TEXT("Thought") : UpdateType == TEXT("user_message_chunk") ? TEXT("User") : TEXT("OpenCode");
                AppendTranscript(Role, Text);
            }
        }
        return;
    }

    if (UpdateType == TEXT("tool_call"))
    {
        AppendTranscript(TEXT("Tool"), FormatToolActivityTranscriptText(*Update, true));
        return;
    }

    if (UpdateType == TEXT("tool_call_update"))
    {
        const FString ActivityText = FormatToolActivityTranscriptText(*Update, false);
        if (!ActivityText.IsEmpty())
        {
            AppendTranscript(TEXT("Tool"), ActivityText);
        }
        return;
    }

    if (UpdateType == TEXT("plan"))
    {
        AppendTranscript(TEXT("Plan"), TEXT("Plan updated."));
    }
}

FString FOpenCodeAcpClient::FormatToolActivityTranscriptText(const TSharedPtr<FJsonObject>& Update, bool bStarted)
{
    if (!Update.IsValid())
    {
        return FString();
    }

    const FString ToolCallId = GetStringFieldOrEmpty(Update, TEXT("toolCallId"));
    FString Title = GetStringFieldOrEmpty(Update, TEXT("title")).TrimStartAndEnd();
    if (Title.IsEmpty())
    {
        Title = GetStringFieldOrEmpty(Update, TEXT("kind")).TrimStartAndEnd();
    }
    if (Title.IsEmpty() && !ToolCallId.IsEmpty())
    {
        if (const FString* ExistingTitle = ActiveToolTitlesById.Find(ToolCallId))
        {
            Title = *ExistingTitle;
        }
    }
    if (Title.IsEmpty())
    {
        Title = TEXT("tool");
    }

    FString Detail = ExtractToolActivityDetail(Update).TrimStartAndEnd();
    if (Detail.IsEmpty() && !ToolCallId.IsEmpty())
    {
        if (const FString* ExistingDetail = ActiveToolDetailsById.Find(ToolCallId))
        {
            Detail = *ExistingDetail;
        }
    }

    if (!ToolCallId.IsEmpty())
    {
        ActiveToolTitlesById.Add(ToolCallId, Title);
        if (!Detail.IsEmpty())
        {
            ActiveToolDetailsById.Add(ToolCallId, Detail);
        }
    }

    FString ActivityText = Detail.IsEmpty()
        ? Title
        : FString::Printf(TEXT("%s: %s"), *Title, *Detail);

    const FString Status = GetStringFieldOrEmpty(Update, TEXT("status")).TrimStartAndEnd();
    if (!bStarted && !Status.IsEmpty())
    {
        ActivityText = FString::Printf(TEXT("%s %s"), *ActivityText, *Status).TrimStartAndEnd();
    }
    else if (bStarted)
    {
        ActivityText = FString::Printf(TEXT("Started %s"), *ActivityText).TrimStartAndEnd();
    }

    if (!ToolCallId.IsEmpty() && IsFinalToolActivityStatus(Status))
    {
        ActiveToolTitlesById.Remove(ToolCallId);
        ActiveToolDetailsById.Remove(ToolCallId);
    }

    return ActivityText;
}

bool FOpenCodeAcpClient::ParseModelOptionsFromResult(const TSharedPtr<FJsonObject>& Result)
{
    if (!Result.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ConfigOptions = nullptr;
    if (Result->TryGetArrayField(TEXT("configOptions"), ConfigOptions))
    {
        for (const TSharedPtr<FJsonValue>& OptionValue : *ConfigOptions)
        {
            const TSharedPtr<FJsonObject> Option = OptionValue.IsValid() ? OptionValue->AsObject() : nullptr;
            if (!Option.IsValid() || !IsModelConfigOption(Option))
            {
                continue;
            }

            const FString OptionId = GetConfigOptionId(Option);
            ModelConfigId = OptionId.IsEmpty() ? TEXT("model") : OptionId;
            const FString NewCurrentModel = GetConfigOptionValue(Option);
            const bool bHasCurrentModel = !NewCurrentModel.IsEmpty();
            if (bHasCurrentModel)
            {
                SetCurrentModel(NewCurrentModel);
            }
            ModelOptions.Reset();

            const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
            if (Option->TryGetArrayField(TEXT("options"), Options))
            {
                for (const TSharedPtr<FJsonValue>& ModelValue : *Options)
                {
                    ParseModelOption(ModelValue.IsValid() ? ModelValue->AsObject() : nullptr);
                }
            }
            return bHasCurrentModel;
        }
    }

    return ParseFallbackModels(Result);
}

bool FOpenCodeAcpClient::ParseAgentOptionsFromResult(const TSharedPtr<FJsonObject>& Result)
{
    if (!Result.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ConfigOptions = nullptr;
    if (Result->TryGetArrayField(TEXT("configOptions"), ConfigOptions))
    {
        for (const TSharedPtr<FJsonValue>& OptionValue : *ConfigOptions)
        {
            const TSharedPtr<FJsonObject> Option = OptionValue.IsValid() ? OptionValue->AsObject() : nullptr;
            if (ParseAgentOptionsFromConfigOption(Option))
            {
                return !CurrentAgent.IsEmpty();
            }
        }
    }

    return ParseFallbackAgents(Result);
}

bool FOpenCodeAcpClient::ParseAgentOptionsFromConfigOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid() || !IsAgentConfigOption(Option))
    {
        return false;
    }

    const FString OptionId = GetConfigOptionId(Option);
    AgentConfigId = OptionId.IsEmpty() ? TEXT("mode") : OptionId;

    const FString NewCurrentAgent = GetConfigOptionValue(Option);
    if (!NewCurrentAgent.IsEmpty())
    {
        CurrentAgent = NewCurrentAgent;
    }

    AgentOptions.Reset();
    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    if (Option->TryGetArrayField(TEXT("options"), Options))
    {
        for (const TSharedPtr<FJsonValue>& AgentValue : *Options)
        {
            ParseAgentOption(AgentValue.IsValid() ? AgentValue->AsObject() : nullptr);
        }
    }

    return true;
}

bool FOpenCodeAcpClient::ParseThinkingOptionsFromResult(const TSharedPtr<FJsonObject>& Result)
{
    if (!Result.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ConfigOptions = nullptr;
    if (Result->TryGetArrayField(TEXT("configOptions"), ConfigOptions))
    {
        for (const TSharedPtr<FJsonValue>& OptionValue : *ConfigOptions)
        {
            const TSharedPtr<FJsonObject> Option = OptionValue.IsValid() ? OptionValue->AsObject() : nullptr;
            if (ParseThinkingOptionsFromConfigOption(Option))
            {
                return !CurrentThinking.IsEmpty();
            }
        }
    }

    return false;
}

bool FOpenCodeAcpClient::ParseThinkingOptionsFromConfigOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid() || !IsThinkingConfigOption(Option))
    {
        return false;
    }

    const FString OptionId = GetConfigOptionId(Option);
    ThinkingConfigId = OptionId.IsEmpty() ? TEXT("reasoning") : OptionId;

    const FString NewCurrentThinking = GetConfigOptionValue(Option);
    if (!NewCurrentThinking.IsEmpty())
    {
        CurrentThinking = NewCurrentThinking;
    }

    ThinkingOptions.Reset();
    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    if (Option->TryGetArrayField(TEXT("options"), Options))
    {
        for (const TSharedPtr<FJsonValue>& ThinkingValue : *Options)
        {
            ParseThinkingOption(ThinkingValue.IsValid() ? ThinkingValue->AsObject() : nullptr);
        }
    }

    return true;
}

void FOpenCodeAcpClient::ParseThinkingOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid())
    {
        return;
    }

    FOpenCodeAcpThinkingOption ThinkingOption;
    Option->TryGetStringField(TEXT("value"), ThinkingOption.Id);
    if (ThinkingOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("id"), ThinkingOption.Id);
    }
    if (ThinkingOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("optionId"), ThinkingOption.Id);
    }
    Option->TryGetStringField(TEXT("name"), ThinkingOption.Name);
    if (ThinkingOption.Name.IsEmpty())
    {
        Option->TryGetStringField(TEXT("label"), ThinkingOption.Name);
    }
    Option->TryGetStringField(TEXT("description"), ThinkingOption.Description);
    if (!ThinkingOption.Id.IsEmpty())
    {
        ThinkingOptions.Add(MoveTemp(ThinkingOption));
    }
}

void FOpenCodeAcpClient::ParseAgentOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid())
    {
        return;
    }

    FOpenCodeAcpAgentOption AgentOption;
    Option->TryGetStringField(TEXT("value"), AgentOption.Id);
    if (AgentOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("agentId"), AgentOption.Id);
    }
    if (AgentOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("modeId"), AgentOption.Id);
    }
    if (AgentOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("id"), AgentOption.Id);
    }
    Option->TryGetStringField(TEXT("name"), AgentOption.Name);
    Option->TryGetStringField(TEXT("description"), AgentOption.Description);
    if (!AgentOption.Id.IsEmpty())
    {
        AgentOptions.Add(MoveTemp(AgentOption));
    }
}

bool FOpenCodeAcpClient::ParseFallbackAgents(const TSharedPtr<FJsonObject>& Result)
{
    const TSharedPtr<FJsonObject>* Agents = nullptr;
    if (!Result.IsValid()
        || ((!Result->TryGetObjectField(TEXT("agents"), Agents) || !Agents || !Agents->IsValid())
            && (!Result->TryGetObjectField(TEXT("modes"), Agents) || !Agents || !Agents->IsValid())))
    {
        return false;
    }

    FString NewCurrentAgent;
    const bool bHasCurrentAgent = ((*Agents)->TryGetStringField(TEXT("currentAgentId"), NewCurrentAgent)
        || (*Agents)->TryGetStringField(TEXT("currentAgent"), NewCurrentAgent)
        || (*Agents)->TryGetStringField(TEXT("currentModeId"), NewCurrentAgent)
        || (*Agents)->TryGetStringField(TEXT("currentMode"), NewCurrentAgent))
        && !NewCurrentAgent.IsEmpty();
    if (bHasCurrentAgent)
    {
        CurrentAgent = NewCurrentAgent;
    }

    const TArray<TSharedPtr<FJsonValue>>* AvailableAgents = nullptr;
    if (!(*Agents)->TryGetArrayField(TEXT("availableAgents"), AvailableAgents)
        && !(*Agents)->TryGetArrayField(TEXT("availableModes"), AvailableAgents))
    {
        return bHasCurrentAgent;
    }

    AgentConfigId = TEXT("mode");
    AgentOptions.Reset();
    for (const TSharedPtr<FJsonValue>& AgentValue : *AvailableAgents)
    {
        const TSharedPtr<FJsonObject> Agent = AgentValue.IsValid() ? AgentValue->AsObject() : nullptr;
        if (!Agent.IsValid())
        {
            continue;
        }

        FOpenCodeAcpAgentOption AgentOption;
        Agent->TryGetStringField(TEXT("agentId"), AgentOption.Id);
        if (AgentOption.Id.IsEmpty())
        {
            Agent->TryGetStringField(TEXT("modeId"), AgentOption.Id);
        }
        if (AgentOption.Id.IsEmpty())
        {
            Agent->TryGetStringField(TEXT("id"), AgentOption.Id);
        }
        Agent->TryGetStringField(TEXT("name"), AgentOption.Name);
        Agent->TryGetStringField(TEXT("description"), AgentOption.Description);
        if (!AgentOption.Id.IsEmpty())
        {
            AgentOptions.Add(MoveTemp(AgentOption));
        }
    }
    return bHasCurrentAgent;
}

void FOpenCodeAcpClient::ParseModelOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid())
    {
        return;
    }

    FOpenCodeAcpModelOption ModelOption;
    Option->TryGetStringField(TEXT("value"), ModelOption.Id);
    if (ModelOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("id"), ModelOption.Id);
    }
    if (ModelOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("optionId"), ModelOption.Id);
    }
    if (ModelOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("modelId"), ModelOption.Id);
    }
    Option->TryGetStringField(TEXT("name"), ModelOption.Name);
    if (ModelOption.Name.IsEmpty())
    {
        Option->TryGetStringField(TEXT("label"), ModelOption.Name);
    }
    ModelOption.Provider = GetModelProviderField(Option);
    ModelOption.ContextWindowTokens = GetModelContextWindowTokens(Option);
    if (!ModelOption.Id.IsEmpty())
    {
        ModelOptions.Add(MoveTemp(ModelOption));
    }
}

bool FOpenCodeAcpClient::ParseFallbackModels(const TSharedPtr<FJsonObject>& Result)
{
    const TSharedPtr<FJsonObject>* Models = nullptr;
    if (!Result.IsValid() || !Result->TryGetObjectField(TEXT("models"), Models) || !Models || !Models->IsValid())
    {
        return false;
    }

    FString NewCurrentModel;
    const bool bHasCurrentModel = ((*Models)->TryGetStringField(TEXT("currentModelId"), NewCurrentModel)
        || (*Models)->TryGetStringField(TEXT("currentModel"), NewCurrentModel)
        || (*Models)->TryGetStringField(TEXT("currentValue"), NewCurrentModel)
        || (*Models)->TryGetStringField(TEXT("value"), NewCurrentModel)
        || (*Models)->TryGetStringField(TEXT("optionValue"), NewCurrentModel))
        && !NewCurrentModel.IsEmpty();
    if (bHasCurrentModel)
    {
        SetCurrentModel(NewCurrentModel);
    }

    const TArray<TSharedPtr<FJsonValue>>* AvailableModels = nullptr;
    if (!(*Models)->TryGetArrayField(TEXT("availableModels"), AvailableModels))
    {
        return bHasCurrentModel;
    }

    ModelConfigId = TEXT("model");
    ModelOptions.Reset();
    for (const TSharedPtr<FJsonValue>& ModelValue : *AvailableModels)
    {
        const TSharedPtr<FJsonObject> Model = ModelValue.IsValid() ? ModelValue->AsObject() : nullptr;
        if (!Model.IsValid())
        {
            continue;
        }

        FOpenCodeAcpModelOption ModelOption;
        Model->TryGetStringField(TEXT("modelId"), ModelOption.Id);
        if (ModelOption.Id.IsEmpty())
        {
            Model->TryGetStringField(TEXT("value"), ModelOption.Id);
        }
        if (ModelOption.Id.IsEmpty())
        {
            Model->TryGetStringField(TEXT("id"), ModelOption.Id);
        }
        if (ModelOption.Id.IsEmpty())
        {
            Model->TryGetStringField(TEXT("optionId"), ModelOption.Id);
        }
        Model->TryGetStringField(TEXT("name"), ModelOption.Name);
        if (ModelOption.Name.IsEmpty())
        {
            Model->TryGetStringField(TEXT("label"), ModelOption.Name);
        }
        ModelOption.Provider = GetModelProviderField(Model);
        ModelOption.ContextWindowTokens = GetModelContextWindowTokens(Model);
        if (!ModelOption.Id.IsEmpty())
        {
            ModelOptions.Add(MoveTemp(ModelOption));
        }
    }
    return bHasCurrentModel;
}

void FOpenCodeAcpClient::HandleModelUpdate(const TSharedPtr<FJsonObject>& Update)
{
    if (!Update.IsValid())
    {
        return;
    }

    const FString OptionId = GetConfigOptionId(Update);
    FString NewModel = GetConfigOptionValue(Update);
    if (NewModel.IsEmpty())
    {
        NewModel = GetStringFieldOrEmpty(Update, TEXT("modelId"));
    }
    if (NewModel.IsEmpty())
    {
        NewModel = GetStringFieldOrEmpty(Update, TEXT("currentModelId"));
    }
    if (NewModel.IsEmpty())
    {
        NewModel = GetStringFieldOrEmpty(Update, TEXT("currentModel"));
    }

    if (!NewModel.IsEmpty() && (OptionId.IsEmpty() || OptionId == ModelConfigId || IsModelConfigOption(Update)))
    {
        SetCurrentModel(NewModel);
        OnModelsChanged.ExecuteIfBound();
    }
}

void FOpenCodeAcpClient::SetCurrentModel(const FString& NewModel)
{
    if (NewModel.IsEmpty())
    {
        return;
    }

    if (NewModel != CurrentModel)
    {
        ContextWindowUsedTokens = 0;
        ContextWindowSizeTokens = 0;
    }
    CurrentModel = NewModel;
}

bool FOpenCodeAcpClient::TrySelectDefaultAgent()
{
    if (CurrentAgent == UnrealAgentId || !CanSelectAgent())
    {
        return false;
    }

    const FOpenCodeAcpAgentOption* UnrealAgentOption = AgentOptions.FindByPredicate([](const FOpenCodeAcpAgentOption& Option)
    {
        return Option.Id == UnrealAgentId;
    });

    if (UnrealAgentOption == nullptr)
    {
        return false;
    }

    SetAgent(UnrealAgentOption->Id);
    return SetAgentRequestId != INDEX_NONE;
}

void FOpenCodeAcpClient::HandleThinkingUpdate(const TSharedPtr<FJsonObject>& Update)
{
    if (!Update.IsValid())
    {
        return;
    }

    const FString OptionId = GetConfigOptionId(Update);
    FString NewThinking = GetConfigOptionValue(Update);
    if (NewThinking.IsEmpty())
    {
        NewThinking = GetStringFieldOrEmpty(Update, TEXT("thinkingId"));
    }
    if (NewThinking.IsEmpty())
    {
        NewThinking = GetStringFieldOrEmpty(Update, TEXT("currentThinking"));
    }
    if (NewThinking.IsEmpty())
    {
        NewThinking = GetStringFieldOrEmpty(Update, TEXT("reasoning"));
    }
    if (NewThinking.IsEmpty())
    {
        NewThinking = GetStringFieldOrEmpty(Update, TEXT("currentReasoning"));
    }

    if (!NewThinking.IsEmpty() && (OptionId.IsEmpty() || OptionId == ThinkingConfigId || IsThinkingConfigOption(Update)))
    {
        CurrentThinking = NewThinking;
        OnModelsChanged.ExecuteIfBound();
    }
}

void FOpenCodeAcpClient::HandleAgentUpdate(const TSharedPtr<FJsonObject>& Update)
{
    if (!Update.IsValid())
    {
        return;
    }

    const FString OptionId = GetConfigOptionId(Update);
    FString NewAgent = GetConfigOptionValue(Update);
    if (NewAgent.IsEmpty())
    {
        NewAgent = GetStringFieldOrEmpty(Update, TEXT("agentId"));
    }
    if (NewAgent.IsEmpty())
    {
        NewAgent = GetStringFieldOrEmpty(Update, TEXT("currentAgentId"));
    }
    if (NewAgent.IsEmpty())
    {
        NewAgent = GetStringFieldOrEmpty(Update, TEXT("currentAgent"));
    }

    if (!NewAgent.IsEmpty() && (OptionId.IsEmpty() || OptionId == AgentConfigId || OptionId == TEXT("agent") || OptionId == TEXT("mode")))
    {
        CurrentAgent = NewAgent;
        OnModelsChanged.ExecuteIfBound();
    }
}

void FOpenCodeAcpClient::HandleUsageUpdate(const TSharedPtr<FJsonObject>& Update)
{
    if (!Update.IsValid())
    {
        return;
    }

    double UsedTokens = 0.0;
    double SizeTokens = 0.0;
    if (!Update->TryGetNumberField(TEXT("used"), UsedTokens)
        || !Update->TryGetNumberField(TEXT("size"), SizeTokens)
        || UsedTokens < 0.0
        || SizeTokens <= 0.0)
    {
        return;
    }

    ContextWindowUsedTokens = FMath::Max(0, FMath::RoundToInt(UsedTokens));
    ContextWindowSizeTokens = FMath::Max(1, FMath::RoundToInt(SizeTokens));
    OnModelsChanged.ExecuteIfBound();
}

void FOpenCodeAcpClient::HandlePermissionRequest(const TSharedPtr<FJsonObject>& Message, const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<FJsonValue> RequestId = CloneJsonId(Message);
    if (!RequestId.IsValid())
    {
        const FString ErrorText = TEXT("OpenCode ACP permission request is missing an id.");
        SetStatus(ErrorText);
        AppendTranscript(TEXT("Error"), ErrorText);
        return;
    }

    if (PendingPermissionId.IsValid())
    {
        if (!SendError(RequestId, -32000, TEXT("A permission request is already pending.")))
        {
            StopWithError(TEXT("Failed to reject overlapping OpenCode ACP permission request."));
        }
        return;
    }

    TArray<FOpenCodeAcpPermissionOption> ParsedOptions;

    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    if (Params->TryGetArrayField(TEXT("options"), Options))
    {
        for (const TSharedPtr<FJsonValue>& OptionValue : *Options)
        {
            const TSharedPtr<FJsonObject> Option = OptionValue.IsValid() ? OptionValue->AsObject() : nullptr;
            FOpenCodeAcpPermissionOption PermissionOption;
            if (Option.IsValid() && Option->TryGetStringField(TEXT("optionId"), PermissionOption.Id) && !PermissionOption.Id.IsEmpty())
            {
                Option->TryGetStringField(TEXT("kind"), PermissionOption.Kind);
                ParsedOptions.Add(MoveTemp(PermissionOption));
            }
        }
    }

    if (ParsedOptions.IsEmpty())
    {
        if (!SendError(RequestId, -32602, TEXT("Permission request has no selectable options.")))
        {
            StopWithError(TEXT("Failed to reject malformed OpenCode ACP permission request."));
            return;
        }

        const FString ErrorText = TEXT("OpenCode ACP permission request had no selectable options.");
        SetStatus(ErrorText);
        AppendTranscript(TEXT("Error"), ErrorText);
        return;
    }

    PendingPermissionId = RequestId;
    PendingPermissionOptions = MoveTemp(ParsedOptions);

    const FString Description = DescribePermissionRequest(Params);
    AppendTranscript(TEXT("Permission"), Description);
    OnPermission.ExecuteIfBound(Description);
    SetStatus(TEXT("OpenCode is waiting for tool permission."));
}

FString FOpenCodeAcpClient::DescribePermissionRequest(const TSharedPtr<FJsonObject>& Params) const
{
    const TSharedPtr<FJsonObject>* ToolCall = nullptr;
    if (Params->TryGetObjectField(TEXT("toolCall"), ToolCall) && ToolCall && ToolCall->IsValid())
    {
        const FString Title = GetStringFieldOrEmpty(*ToolCall, TEXT("title"));
        const FString Kind = GetStringFieldOrEmpty(*ToolCall, TEXT("kind"));
        const FString RawInput = TruncateForDisplay(JsonValueToString((*ToolCall)->TryGetField(TEXT("rawInput"))), MaxPermissionDescriptionChars);
        return FString::Printf(TEXT("%s %s wants permission. %s"), *Kind, *Title, *RawInput).TrimStartAndEnd();
    }

    return TEXT("OpenCode requested permission for a tool call.");
}

FString FOpenCodeAcpClient::GetModelDisplayName(const FString& ModelId) const
{
    const FOpenCodeAcpModelOption* Option = ModelOptions.FindByPredicate([&ModelId](const FOpenCodeAcpModelOption& Candidate)
    {
        return Candidate.Id == ModelId;
    });

    return Option != nullptr ? Option->GetDisplayName() : ModelId;
}

FString FOpenCodeAcpClient::GetThinkingDisplayName(const FString& ThinkingId) const
{
    const FOpenCodeAcpThinkingOption* ThinkingOption = ThinkingOptions.FindByPredicate([&ThinkingId](const FOpenCodeAcpThinkingOption& Option)
    {
        return Option.Id == ThinkingId;
    });
    return ThinkingOption == nullptr ? ThinkingId : ThinkingOption->GetDisplayName();
}

FString FOpenCodeAcpClient::GetAgentDisplayName(const FString& AgentId) const
{
    const FOpenCodeAcpAgentOption* Option = AgentOptions.FindByPredicate([&AgentId](const FOpenCodeAcpAgentOption& Candidate)
    {
        return Candidate.Id == AgentId;
    });

    if (AgentId == UnrealAgentId)
    {
        return TEXT("Unreal - Creator");
    }
    return Option != nullptr ? Option->GetDisplayName() : AgentId;
}

FString FOpenCodeAcpClient::FindPendingPermissionOption(const TArray<FString>& PreferredOptionIds, const TArray<FString>& PreferredKinds) const
{
    for (const FString& PreferredKind : PreferredKinds)
    {
        const FOpenCodeAcpPermissionOption* Match = PendingPermissionOptions.FindByPredicate([&PreferredKind](const FOpenCodeAcpPermissionOption& Option)
        {
            return Option.Kind == PreferredKind;
        });
        if (Match != nullptr)
        {
            return Match->Id;
        }
    }

    for (const FString& PreferredOptionId : PreferredOptionIds)
    {
        const FOpenCodeAcpPermissionOption* Match = PendingPermissionOptions.FindByPredicate([&PreferredOptionId](const FOpenCodeAcpPermissionOption& Option)
        {
            return Option.Id == PreferredOptionId && Option.Kind.IsEmpty();
        });
        if (Match != nullptr)
        {
            return Match->Id;
        }
    }

    return FString();
}

bool FOpenCodeAcpClient::SendCancelledPermissionResponse()
{
    if (!PendingPermissionId.IsValid())
    {
        return true;
    }

    auto Outcome = MakeObject();
    Outcome->SetStringField(TEXT("outcome"), TEXT("cancelled"));

    auto Result = MakeObject();
    Result->SetObjectField(TEXT("outcome"), Outcome);

    if (!SendResponse(PendingPermissionId, Result))
    {
        return false;
    }

    AppendTranscript(TEXT("Permission"), TEXT("Cancelled permission request."));
    PendingPermissionId.Reset();
    PendingPermissionOptions.Reset();
    return true;
}

void FOpenCodeAcpClient::ResolvePendingPermission(const FString& PreferredOptionId)
{
    if (!PendingPermissionId.IsValid())
    {
        return;
    }

    if (PreferredOptionId.IsEmpty())
    {
        const FString ErrorText = TEXT("OpenCode ACP permission option is unavailable.");
        SetStatus(ErrorText);
        AppendTranscript(TEXT("Error"), ErrorText);
        return;
    }

    auto Outcome = MakeObject();
    Outcome->SetStringField(TEXT("outcome"), TEXT("selected"));
    Outcome->SetStringField(TEXT("optionId"), PreferredOptionId);

    auto Result = MakeObject();
    Result->SetObjectField(TEXT("outcome"), Outcome);

    if (!SendResponse(PendingPermissionId, Result))
    {
        StopWithError(TEXT("Failed to send OpenCode ACP permission response."));
        return;
    }

    AppendTranscript(TEXT("Permission"), FString::Printf(TEXT("Responded: %s"), *PreferredOptionId));
    PendingPermissionId.Reset();
    PendingPermissionOptions.Reset();
    SetStatus(TEXT("OpenCode is working..."));
}

void FOpenCodeAcpClient::StopWithError(const FString& ErrorText)
{
    const FString FullErrorText = FormatProcessErrorText(ErrorText);
    if (ProcessHandle.IsValid())
    {
        TerminateAndCloseProcess(ProcessHandle);
    }

    CloseProcessPipes();
    ResetState();
    SetStatus(FullErrorText);
    AppendTranscript(TEXT("Error"), FullErrorText);
    OnStopped.ExecuteIfBound();
}

void FOpenCodeAcpClient::SetStatus(const FString& NewStatus)
{
    OnStatus.ExecuteIfBound(NewStatus);
}

void FOpenCodeAcpClient::AppendTranscript(const FString& Role, const FString& Text)
{
    OnTranscript.ExecuteIfBound(Role, Text);
}

void FOpenCodeAcpClient::ResetState()
{
    SessionId.Reset();
    CurrentModel.Reset();
    ModelConfigId.Reset();
    CurrentThinking.Reset();
    ThinkingConfigId.Reset();
    CurrentAgent.Reset();
    AgentConfigId.Reset();
    ModelOptions.Reset();
    ThinkingOptions.Reset();
    AgentOptions.Reset();
    PendingModel.Reset();
    PendingThinking.Reset();
    PendingAgent.Reset();
    OutputBuffer.Reset();
    RecentErrorOutput.Reset();
    PendingPermissionId.Reset();
    PendingPermissionOptions.Reset();
    ActiveToolTitlesById.Reset();
    ActiveToolDetailsById.Reset();
    ContextWindowUsedTokens = 0;
    ContextWindowSizeTokens = 0;
    NextRequestId = 1;
    InitializeRequestId = INDEX_NONE;
    NewSessionRequestId = INDEX_NONE;
    ActivePromptRequestId = INDEX_NONE;
    SetModelRequestId = INDEX_NONE;
    SetThinkingRequestId = INDEX_NONE;
    SetAgentRequestId = INDEX_NONE;
    InitializeRequestStartedAt = 0.0;
    NewSessionRequestStartedAt = 0.0;
    SetModelRequestStartedAt = 0.0;
    SetThinkingRequestStartedAt = 0.0;
    SetAgentRequestStartedAt = 0.0;
    bRunning = false;
    bInitialized = false;
    bReady = false;
    bPromptInFlight = false;
    bCancelRequested = false;
}

FString FOpenCodeAcpClient::FormatProcessErrorText(const FString& ErrorText) const
{
    const FString ErrorOutput = RecentErrorOutput.TrimStartAndEnd();
    return ErrorOutput.IsEmpty()
        ? ErrorText
        : FString::Printf(TEXT("%s\nOpenCode stderr:\n%s"), *ErrorText, *ErrorOutput);
}

FString FOpenCodeAcpClient::ResolveOpenCodeExecutable(FString& OutError) const
{
    const FString Override = NormalizeExecutablePath(FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_ACP_COMMAND")));
    if (!Override.IsEmpty())
    {
        if (IsAbsoluteExistingExecutable(Override))
        {
            return Override;
        }

        OutError = FString::Printf(TEXT("OPENCODE_ACP_COMMAND must be an absolute executable path without quotes or control characters: %s"), *Override);
        return FString();
    }

    const FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
    if (!Home.IsEmpty())
    {
        const FString UserInstall = NormalizeExecutablePath(FPaths::Combine(Home, TEXT(".opencode/bin/opencode")));
        if (IsAbsoluteExistingExecutable(UserInstall))
        {
            return UserInstall;
        }
    }

#if PLATFORM_WINDOWS
    const FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
    if (!UserProfile.IsEmpty())
    {
        const FString UserInstall = NormalizeExecutablePath(FPaths::Combine(UserProfile, TEXT(".opencode/bin/opencode.exe")));
        if (IsAbsoluteExistingExecutable(UserInstall))
        {
            return UserInstall;
        }
    }
    const TCHAR* ExecutableName = TEXT("opencode.exe");
#else
    const TCHAR* ExecutableName = TEXT("opencode");
#endif

    TArray<FString> PathEntries;
    FPlatformMisc::GetEnvironmentVariable(TEXT("PATH")).ParseIntoArray(PathEntries, FPlatformMisc::GetPathVarDelimiter(), true);
    for (FString PathEntry : PathEntries)
    {
        PathEntry = NormalizeExecutablePath(PathEntry);
        if (PathEntry.IsEmpty() || FPaths::IsRelative(PathEntry) || FPaths::IsSamePath(PathEntry, WorkingDirectory) || FPaths::IsUnderDirectory(PathEntry, WorkingDirectory))
        {
            continue;
        }

        const FString Candidate = NormalizeExecutablePath(FPaths::Combine(PathEntry, ExecutableName));
        if (IsAbsoluteExistingExecutable(Candidate))
        {
            return Candidate;
        }
    }

    OutError = TEXT("OpenCode executable not found. Set OPENCODE_ACP_COMMAND to an absolute opencode executable path.");
    return FString();
}

FString FOpenCodeAcpClient::JsonToString(const TSharedPtr<FJsonObject>& Object)
{
    FString Output;
    if (Object.IsValid())
    {
        TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
        FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
    }
    return Output;
}

FString FOpenCodeAcpClient::JsonValueToString(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return FString();
    }

    FString Output;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
    FJsonSerializer::Serialize(Value, TEXT(""), Writer);
    return Output;
}

bool FOpenCodeAcpClient::TryReadIdAsInt(const TSharedPtr<FJsonObject>& Message, int32& OutId)
{
    if (!Message.IsValid())
    {
        return false;
    }

    const TSharedPtr<FJsonValue> IdValue = Message->TryGetField(TEXT("id"));
    if (!IdValue.IsValid() || IdValue->Type != EJson::Number)
    {
        return false;
    }

    OutId = static_cast<int32>(IdValue->AsNumber());
    return true;
}

bool FOpenCodeAcpClient::ShouldIgnoreProcessOutputLine(const FString& Line)
{
    return Line.StartsWith(TEXT("Config warning:"))
        || Line.StartsWith(TEXT("Overriding existing handler for signal"));
}

bool FOpenCodeAcpClient::IsSafeProcessArgument(const FString& Value)
{
    return IsSafeProcessArgumentValue(Value);
}
