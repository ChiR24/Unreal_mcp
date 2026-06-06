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
