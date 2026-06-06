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
