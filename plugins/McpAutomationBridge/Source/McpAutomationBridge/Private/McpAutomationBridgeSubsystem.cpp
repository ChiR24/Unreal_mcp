// Ensure the subsystem type and bridge socket types are available
#include "McpAutomationBridgeSubsystem.h"
#include "McpConnectionManager.h"
#include "McpBridgeWebSocket.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSettings.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"

// Define the subsystem log category declared in the public header.
DEFINE_LOG_CATEGORY(LogMcpAutomationBridgeSubsystem);

// Sanitize incoming text for logging: replace control characters with
// '?' and truncate long messages so logs remain readable and do not
// attempt to render unprintable glyphs in the editor which can spam
// Slate font warnings.
static inline FString SanitizeForLog(const FString& In)
{
    if (In.IsEmpty()) return FString();
    FString Out; Out.Reserve(FMath::Min<int32>(In.Len(), 1024));
    for (int32 i = 0; i < In.Len(); ++i)
    {
        const TCHAR C = In[i];
        if (C >= 32 && C != 127) Out.AppendChar(C);
        else Out.AppendChar('?');
    }
    if (Out.Len() > 512) Out = Out.Left(512) + TEXT("[TRUNCATED]");
    return Out;
}

void UMcpAutomationBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("McpAutomationBridgeSubsystem initializing."));

	// Create and initialize the connection manager
	ConnectionManager = MakeShared<FMcpConnectionManager>();
	ConnectionManager->Initialize(GetDefault<UMcpAutomationBridgeSettings>());

	// Bind message received delegate
	ConnectionManager->SetOnMessageReceived(FMcpMessageReceivedCallback::CreateWeakLambda(this, [this](const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		ProcessAutomationRequest(RequestId, Action, Payload, Socket);
	}));

	// Initialize the handler registry
	InitializeHandlers();

	// Start the connection manager
	ConnectionManager->Start();
}

void UMcpAutomationBridgeSubsystem::Deinitialize()
{
	UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("McpAutomationBridgeSubsystem deinitializing."));
	
	if (ConnectionManager.IsValid())
	{
		ConnectionManager->Stop();
		ConnectionManager.Reset();
	}

	Super::Deinitialize();
}

bool UMcpAutomationBridgeSubsystem::IsBridgeActive() const
{
	return ConnectionManager.IsValid() && ConnectionManager->GetActiveSocketCount() > 0;
}

EMcpAutomationBridgeState UMcpAutomationBridgeSubsystem::GetBridgeState() const
{
	// Map connection manager state if needed, for now just check if we have active sockets
	return IsBridgeActive() ? EMcpAutomationBridgeState::Connected : EMcpAutomationBridgeState::Disconnected;
}

bool UMcpAutomationBridgeSubsystem::SendRawMessage(const FString& Message)
{
	if (ConnectionManager.IsValid())
	{
		return ConnectionManager->SendRawMessage(Message);
	}
	return false;
}

bool UMcpAutomationBridgeSubsystem::Tick(float DeltaTime)
{
	if (ConnectionManager.IsValid())
	{
		ConnectionManager->Tick(DeltaTime);
	}
	return true;
}

// The in-file implementation of ProcessAutomationRequest was intentionally
// removed from this translation unit. The function is now implemented in
// McpAutomationBridge_ProcessRequest.cpp to avoid duplicate definitions and
// to keep this file focused. See that file for the full request dispatcher
// and per-action handlers.

void UMcpAutomationBridgeSubsystem::SendAutomationResponse(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, const bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode)
{
	if (ConnectionManager.IsValid())
	{
		ConnectionManager->SendAutomationResponse(TargetSocket, RequestId, bSuccess, Message, Result, ErrorCode);
	}
}

void UMcpAutomationBridgeSubsystem::SendAutomationError(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, const FString& Message, const FString& ErrorCode)
{
	const FString ResolvedError = ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode;
	UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation request failed (%s): %s"), *ResolvedError, *SanitizeForLog(Message));
	SendAutomationResponse(TargetSocket, RequestId, false, Message, nullptr, ResolvedError);
}

void UMcpAutomationBridgeSubsystem::RecordAutomationTelemetry(const FString& RequestId, const bool bSuccess, const FString& Message, const FString& ErrorCode)
{
	if (ConnectionManager.IsValid())
	{
		ConnectionManager->RecordAutomationTelemetry(RequestId, bSuccess, Message, ErrorCode);
	}
}

void UMcpAutomationBridgeSubsystem::RegisterHandler(const FString& Action, FAutomationHandler Handler)
{
    if (Handler)
    {
        AutomationHandlers.Add(Action, Handler);
    }
}

void UMcpAutomationBridgeSubsystem::InitializeHandlers()
{
    // Core & Properties
    RegisterHandler(TEXT("execute_editor_function"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleExecuteEditorFunction(R, A, P, S); });
    RegisterHandler(TEXT("set_object_property"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleSetObjectProperty(R, A, P, S); });
    RegisterHandler(TEXT("get_object_property"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleGetObjectProperty(R, A, P, S); });

    // Containers (Arrays, Maps, Sets)
    RegisterHandler(TEXT("array_append"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleArrayAppend(R, A, P, S); });
    RegisterHandler(TEXT("array_remove"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleArrayRemove(R, A, P, S); });
    RegisterHandler(TEXT("array_insert"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleArrayInsert(R, A, P, S); });
    RegisterHandler(TEXT("array_get_element"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleArrayGetElement(R, A, P, S); });
    RegisterHandler(TEXT("array_set_element"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleArraySetElement(R, A, P, S); });
    RegisterHandler(TEXT("array_clear"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleArrayClear(R, A, P, S); });
    
    RegisterHandler(TEXT("map_set_value"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleMapSetValue(R, A, P, S); });
    RegisterHandler(TEXT("map_get_value"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleMapGetValue(R, A, P, S); });
    RegisterHandler(TEXT("map_remove_key"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleMapRemoveKey(R, A, P, S); });
    RegisterHandler(TEXT("map_has_key"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleMapHasKey(R, A, P, S); });
    RegisterHandler(TEXT("map_get_keys"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleMapGetKeys(R, A, P, S); });
    RegisterHandler(TEXT("map_clear"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleMapClear(R, A, P, S); });

    RegisterHandler(TEXT("set_add"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleSetAdd(R, A, P, S); });
    RegisterHandler(TEXT("set_remove"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleSetRemove(R, A, P, S); });
    RegisterHandler(TEXT("set_contains"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleSetContains(R, A, P, S); });
    RegisterHandler(TEXT("set_clear"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleSetClear(R, A, P, S); });

    // Asset Dependency
    RegisterHandler(TEXT("get_asset_references"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleGetAssetReferences(R, A, P, S); });
    RegisterHandler(TEXT("get_asset_dependencies"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleGetAssetDependencies(R, A, P, S); });

    // Tools & System
    RegisterHandler(TEXT("console_command"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleConsoleCommandAction(R, A, P, S); });
    RegisterHandler(TEXT("inspect"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleInspectAction(R, A, P, S); });
    RegisterHandler(TEXT("system_control"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleSystemControlAction(R, A, P, S); });
    RegisterHandler(TEXT("manage_blueprint_graph"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleBlueprintGraphAction(R, A, P, S); });
    RegisterHandler(TEXT("list_blueprints"), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return HandleListBlueprints(R, A, P, S); });
}

// Drain and process any automation requests that were enqueued while the
// subsystem was busy. This implementation lives in the primary subsystem
// translation unit to ensure the symbol is available at link time for
// any callsites that reference it (including scope-exit lambdas).
void UMcpAutomationBridgeSubsystem::ProcessPendingAutomationRequests()
{
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [this]() { this->ProcessPendingAutomationRequests(); });
        return;
    }

    TArray<FPendingAutomationRequest> LocalQueue;
    {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        if (PendingAutomationRequests.Num() == 0)
        {
            bPendingRequestsScheduled = false;
            return;
        }
        LocalQueue = MoveTemp(PendingAutomationRequests);
        PendingAutomationRequests.Empty();
        bPendingRequestsScheduled = false;
    }

    for (const FPendingAutomationRequest& Req : LocalQueue)
    {
        ProcessAutomationRequest(Req.RequestId, Req.Action, Req.Payload, Req.RequestingSocket);
    }
}

