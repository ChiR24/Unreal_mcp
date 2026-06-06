#include "McpOpenCodeAcpClient.h"
#include "McpOpenCodeAcpClientPrivate.h"

#include "UnrealAgentEditorContext.h"
#include "UnrealAgentStudioKit.h"
#include "UnrealAgentValidationRunner.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

using namespace UnrealAgent::OpenCodeAcp;

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

bool FOpenCodeAcpClient::IsSafeProcessArgument(const FString& Value)
{
    return IsSafeProcessArgumentValue(Value);
}
