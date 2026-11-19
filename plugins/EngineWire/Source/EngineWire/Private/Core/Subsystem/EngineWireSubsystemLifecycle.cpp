#include "EngineWireSubsystem.h"

#include "Core/Module/McpOwnPlugin.h"
#include "MCP/Transport/McpNativeTransport.h"
#include "EngineWireSettings.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "McpConnectionManager.h"
#include "Core/Subsystem/McpRequestErrorDevice.h"
#include "Core/Subsystem/McpEndpointFile.h"
#include "Core/Requests/McpInflightLedger.h"
#include "Core/Journal/McpChangeJournal.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"
#include "Foundation/Diagnostics/McpWorldTickTimer.h"
#include "Domains/ControlEditor/Checkpoint/McpEditorCheckpoint.h"
#include "Domains/ControlEditor/McpEditorSupport.h"
#include "Foundation/Viewport/McpEditorScreenshotSupport.h"
#include "Domains/Log/McpLogHistory.h"
#include "Domains/Log/McpScriptErrors.h"
#include "Domains/SystemControl/McpSystemPrivate.h"
#include "Foundation/McpLiveStateRevisionTracker.h"
#include "Foundation/McpReadinessState.h"
#include "Framework/Application/SlateApplication.h"

void UEngineWireSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (IsRunningCommandlet())
    {
        UE_LOG(
            LogEngineWireSubsystem,
            Log,
            TEXT("EngineWireSubsystem skipping initialization - running "
                 "as commandlet (cook/package mode)."));
        return;
    }

    // Cache the diagnostics root on the game thread (socket-thread
    // hooks never resolve project paths), then perform crash-tolerant startup
    // rotation before request acceptance - a non-empty current is promoted to
    // previous so a hard crash in a prior run leaves readable evidence.
    FMcpDiagnosticsSnapshot::Get().InitializeFromGameThread();
    FMcpDiagnosticsSnapshot::Get().RotateOnStartup();
    FMcpLogHistory::Get().Register();
    FMcpScriptErrors::Get().Watch(true);
    McpChangeJournal::Start();
    McpSystemControlHandlers::McpWatchMaterialUsage(true);

    UE_LOG(
        LogEngineWireSubsystem,
        Log,
        TEXT("EngineWireSubsystem initializing."));

    // Explicit although accepting is the default: Deinitialize always calls Stop.
    StartAcceptingAutomationRequests();
    McpStartLiveStateTracking();
    ConnectionManager = MakeShared<FMcpConnectionManager>();
    ConnectionManager->Initialize(GetDefault<UEngineWireSettings>());
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
        FTickerDelegate::CreateUObject(this, &UEngineWireSubsystem::Tick),
        0.0f);
    // The ticker above stops while a modal dialog runs its own loop; this answers what queues meanwhile.
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetOnModalLoopTickEvent().AddUObject(this, &UEngineWireSubsystem::OnModalLoopTick);
    }

    HoldEditorMinimizedAtStartupForMcp();

    // Published only here, at the END of a non-commandlet initialization that
    // registered handlers and installed the ticker. Readiness is a POSITIVE
    // observation: the early commandlet return above leaves it false.
    McpWorldTickTimer::Start();
    FMcpReadinessState::Get().SetEditorReady(true);

    UE_LOG(
        LogEngineWireSubsystem,
        Log,
        TEXT("EngineWireSubsystem Initialized."));
}

void UEngineWireSubsystem::Deinitialize()
{
    // First: Live Coding unloads this module routinely, and a binding left behind would call into unmapped code.
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetOnModalLoopTickEvent().RemoveAll(this);
    }
    FMcpReadinessState::Get().Reset();
    McpWorldTickTimer::Stop();
    StopAcceptingAutomationRequests();
    McpStopLiveStateTracking();
    // An open restore point holds lambdas on engine delegates that outlive this module.
    McpEditorCheckpoint::Shutdown();
    // Enhanced Input holds live on the core ticker, which outlives this module.
    // Live Coding unloads the module routinely, so a hold left registered would
    // tick into code that is no longer mapped.
    StopAllEnhancedInputHoldsForMcp();

    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }

    if (!IsRunningCommandlet())
    {
        UE_LOG(
            LogEngineWireSubsystem,
            Log,
            TEXT("EngineWireSubsystem deinitializing."));
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
        McpEndpointFile::WriteClosed();
    }

    CancelAllAutomationRequests();
    FMcpInflightLedger::Get().Stop();

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

    FMcpDiagnosticsSnapshot::Get().PersistCurrent();
    FMcpLogHistory::Get().Unregister();
    FMcpScriptErrors::Get().Watch(false);
    McpChangeJournal::Stop();
    McpSystemControlHandlers::McpWatchMaterialUsage(false);

    Super::Deinitialize();
}
bool UEngineWireSubsystem::Tick(float DeltaTime)
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

void UEngineWireSubsystem::StartNativeTransport()
{
    // Before any call: calls an earlier editor of this project left running (it ended in them) go to last-crash.json.
    if (const int32 Left = FMcpInflightLedger::Get().Start())
    {
        UE_LOG(LogEngineWireSubsystem, Warning, TEXT("The last editor of this project ended while running %d call(s): Saved/MCP/last-crash.json names them."), Left);
    }
    const auto* Settings = GetDefault<UEngineWireSettings>();
    if (!Settings->bEnableNativeMCP)
    {
        return;
    }

    const TSharedPtr<IPlugin> Plugin = McpOwnPlugin();
    const FString PluginDir = Plugin.IsValid() ? Plugin->GetBaseDir() : FString();

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
                LogEngineWireSubsystem,
                Log,
                TEXT("Native MCP port overridden by env MCP_NATIVE_PORT=%d (project setting NativeMCPPort=%d)"),
                NativePort,
                Settings->NativeMCPPort);
        }
        else
        {
            UE_LOG(
                LogEngineWireSubsystem,
                Warning,
                TEXT("Ignoring invalid MCP_NATIVE_PORT='%s' (expected an integer 1-65535); using NativeMCPPort=%d"),
                *EnvNativePort,
                Settings->NativeMCPPort);
        }
    }

    // Port 0: this project's own port, so editors of several projects never meet on one; a taken one moves up.
    const bool bProjectPort = NativePort == 0;
    const int32 FirstPort = bProjectPort ? McpEndpointFile::ProjectPort() : NativePort;
    for (int32 Try = 0; Try < (bProjectPort ? 16 : 1); ++Try)
    {
        NativePort = bProjectPort ? 49152 + (FirstPort - 49152 + Try) % 16384 : FirstPort;
        NativeTransport = MakeShared<FMcpNativeTransport>(this);
        if (NativeTransport->Start(NativePort, PluginDir, Settings->bLoadAllToolsOnStart, Settings->NativeMCPInstructions,
                Settings->ListenHost, Settings->bAllowNonLoopback))
        {
            FMcpReadinessState::Get().SetTransportReady(true);
            // Where the stdio proxy finds this server (Saved/MCP/endpoint.json).
            McpEndpointFile::WriteListening(NativePort);
            return;
        }
    }

    UE_LOG(
        LogEngineWireSubsystem,
        Error,
        TEXT("Failed to start Native MCP server on port %d"),
        NativePort);
    NativeTransport.Reset();
}
