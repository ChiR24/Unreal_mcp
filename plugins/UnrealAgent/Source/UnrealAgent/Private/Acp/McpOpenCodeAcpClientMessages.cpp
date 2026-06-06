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
