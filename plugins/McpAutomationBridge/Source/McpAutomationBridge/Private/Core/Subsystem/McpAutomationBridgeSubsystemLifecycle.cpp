#include "McpAutomationBridgeSubsystem.h"

#include "Interfaces/IPluginManager.h"
#include "MCP/Transport/McpNativeTransport.h"
#include "McpAutomationBridgeSettings.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "McpConnectionManager.h"
#include "Core/Errors/McpRequestErrorDevice.h"
#include "Foundation/McpLiveStateRevisionTracker.h"
#include "Foundation/McpReadinessState.h"

void UMcpAutomationBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (IsRunningCommandlet())
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Log,
            TEXT("McpAutomationBridgeSubsystem skipping initialization - running "
                 "as commandlet (cook/package mode)."));
        return;
    }

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Log,
        TEXT("McpAutomationBridgeSubsystem initializing."));

    // StartAcceptingAutomationRequests() is called explicitly even though the
    // default value of bAcceptingAutomationRequests is true. The explicit call
    // preserves start/stop symmetry (every Deinitialize calls Stop), and it makes
    // the lifecycle intent obvious to readers — any future change to the
    // default (e.g. a deferred-start mode) does not silently flip this code.
    StartAcceptingAutomationRequests();
    McpStartLiveStateTracking();
    ConnectionManager = MakeShared<FMcpConnectionManager>();
    ConnectionManager->Initialize(GetDefault<UMcpAutomationBridgeSettings>());
    ConnectionManager->SetOnMessageReceived(
        FMcpMessageReceivedCallback::CreateWeakLambda(
            this,
            [this](
                const FString& RequestId,
                const FString& Action,
                const TSharedPtr<FJsonObject>& Payload,
                TSharedPtr<FMcpBridgeWebSocket> Socket,
                const FMcpExpectedRevisions& ExpectedRevisions)
            {
                const EAutomationQueueRejection Reason =
                    QueueAutomationRequest(
                        RequestId, Action, Payload, Socket,
                        ERequestOrigin::WebSocket, ExpectedRevisions);
                if (Reason != EAutomationQueueRejection::None)
                {
                    SendAutomationRejection(Socket, RequestId, Reason);
                }
            }));
    ConnectionManager->SetOnAutomationRequestCancelled(
        FMcpRequestCancelledCallback::CreateWeakLambda(
            this,
            [this](const FString& RequestId)
            {
                CancelAutomationRequest(RequestId);
            }));

    InitializeHandlers();
    ConnectionManager->Start();
    StartNativeTransport();

    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UMcpAutomationBridgeSubsystem::Tick),
        0.0f);

    // Published only here, at the END of a non-commandlet initialization that
    // registered handlers and installed the ticker. Readiness is a POSITIVE
    // observation: the early commandlet return above leaves it false.
    FMcpReadinessState::Get().SetEditorReady(true);

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Log,
        TEXT("McpAutomationBridgeSubsystem Initialized."));
}

void UMcpAutomationBridgeSubsystem::Deinitialize()
{
    FMcpReadinessState::Get().Reset();
    StopAcceptingAutomationRequests();
    McpStopLiveStateTracking();

    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }

    if (!IsRunningCommandlet())
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Log,
            TEXT("McpAutomationBridgeSubsystem deinitializing."));
    }

    if (ConnectionManager.IsValid())
    {
        ConnectionManager->Stop();
        ConnectionManager.Reset();
    }

    if (NativeTransport)
    {
        NativeTransport->Shutdown();
        NativeTransport.Reset();
    }

    CancelAllAutomationRequests();

    if (LogCaptureDevice.IsValid())
    {
        if (GLog)
        {
            GLog->RemoveOutputDevice(LogCaptureDevice.Get());
        }
        LogCaptureDevice.Reset();
    }

    if (RequestErrorDevice.IsValid())
    {
        FScopeLock Lock(&ErrorCaptureMutex);
        if (GLog)
        {
            GLog->RemoveOutputDevice(RequestErrorDevice.Get());
        }
        RequestErrorDevice.Reset();
    }

    Super::Deinitialize();
}

bool UMcpAutomationBridgeSubsystem::IsBridgeActive() const
{
    return ConnectionManager.IsValid() && ConnectionManager->GetActiveSocketCount() > 0;
}

EMcpAutomationBridgeState UMcpAutomationBridgeSubsystem::GetBridgeState() const
{
    if (ConnectionManager.IsValid())
    {
        if (ConnectionManager->GetActiveSocketCount() > 0)
        {
            return EMcpAutomationBridgeState::Connected;
        }
        if (ConnectionManager->IsReconnectPending())
        {
            return EMcpAutomationBridgeState::Connecting;
        }
    }
    return EMcpAutomationBridgeState::Disconnected;
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
    if (!GIsSavingPackage && !IsGarbageCollecting() && !IsAsyncLoading())
    {
        ProcessPendingAutomationRequests();
    }
    if (NativeTransport)
    {
        NativeTransport->CleanupStaleRequests();
    }
    ReconcileLogCaptureDevice();
    return true;
}

void UMcpAutomationBridgeSubsystem::StartNativeTransport()
{
    const auto* Settings = GetDefault<UMcpAutomationBridgeSettings>();
    if (!Settings || !Settings->bEnableNativeMCP)
    {
        return;
    }

    FString PluginDir;
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("McpAutomationBridge"));
    if (Plugin.IsValid())
    {
        PluginDir = Plugin->GetBaseDir();
    }

    // Allow an environment-variable override of the native MCP port, mirroring the
    // plugin's existing MCP_MAX_* env overrides. This lets a project select a per-
    // instance port (e.g. to run several editors at once on distinct ports) without
    // editing committed ini — set MCP_NATIVE_PORT in the editor's environment. Falls
    // back to the NativeMCPPort project setting when the var is unset or invalid.
    int32 NativePort = Settings->NativeMCPPort;
    const FString EnvNativePort = FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_NATIVE_PORT"));
    if (!EnvNativePort.IsEmpty())
    {
        int32 ParsedNativePort = 0;
        if (LexTryParseString(ParsedNativePort, *EnvNativePort) && ParsedNativePort > 0 && ParsedNativePort <= 65535)
        {
            NativePort = ParsedNativePort;
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Log,
                TEXT("Native MCP port overridden by env MCP_NATIVE_PORT=%d (project setting NativeMCPPort=%d)"),
                NativePort,
                Settings->NativeMCPPort);
        }
        else
        {
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("Ignoring invalid MCP_NATIVE_PORT='%s' (expected an integer 1-65535); using NativeMCPPort=%d"),
                *EnvNativePort,
                Settings->NativeMCPPort);
        }
    }

    NativeTransport = MakeShared<FMcpNativeTransport>(this);
    if (NativeTransport->Start(
            NativePort,
            PluginDir,
            Settings->bLoadAllToolsOnStart,
            Settings->NativeMCPInstructions,
            Settings->ListenHost,
            Settings->bAllowNonLoopback))
    {
        FMcpReadinessState::Get().SetTransportReady(true);
        return;
    }

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Error,
        TEXT("Failed to start Native MCP server on port %d"),
        NativePort);
    NativeTransport.Reset();
}
