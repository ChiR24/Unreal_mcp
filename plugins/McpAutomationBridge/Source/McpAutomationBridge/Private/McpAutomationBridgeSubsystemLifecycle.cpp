#include "McpAutomationBridgeSubsystem.h"

#include "Interfaces/IPluginManager.h"
#include "MCP/McpNativeTransport.h"
#include "McpAutomationBridgeSettings.h"
#include "McpBridgeWebSocket.h"
#include "McpConnectionManager.h"
#include "McpRequestErrorDevice.h"

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

    ConnectionManager = MakeShared<FMcpConnectionManager>();
    ConnectionManager->Initialize(GetDefault<UMcpAutomationBridgeSettings>());
    ConnectionManager->SetOnMessageReceived(
        FMcpMessageReceivedCallback::CreateWeakLambda(
            this,
            [this](
                const FString& RequestId,
                const FString& Action,
                const TSharedPtr<FJsonObject>& Payload,
                TSharedPtr<FMcpBridgeWebSocket> Socket)
            {
                QueueAutomationRequest(RequestId, Action, Payload, Socket);
            }));

    InitializeHandlers();
    ConnectionManager->Start();
    StartNativeTransport();

    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UMcpAutomationBridgeSubsystem::Tick),
        0.0f);

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Log,
        TEXT("McpAutomationBridgeSubsystem Initialized."));
}

void UMcpAutomationBridgeSubsystem::Deinitialize()
{
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

    if (NativeTransport)
    {
        NativeTransport->Shutdown();
        NativeTransport.Reset();
    }

    if (ConnectionManager.IsValid())
    {
        ConnectionManager->Stop();
        ConnectionManager.Reset();
    }

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

    NativeTransport = MakeShared<FMcpNativeTransport>(this);
    if (NativeTransport->Start(
            Settings->NativeMCPPort,
            PluginDir,
            Settings->bLoadAllToolsOnStart,
            Settings->NativeMCPInstructions,
            Settings->ListenHost,
            Settings->bAllowNonLoopback))
    {
        return;
    }

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Error,
        TEXT("Failed to start Native MCP server on port %d"),
        Settings->NativeMCPPort);
    NativeTransport.Reset();
}
