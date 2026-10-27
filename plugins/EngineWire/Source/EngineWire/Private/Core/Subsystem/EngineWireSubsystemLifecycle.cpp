#include "EngineWireSubsystem.h"

#include "Interfaces/IPluginManager.h"
#include "MCP/Transport/McpNativeTransport.h"
#include "EngineWireSettings.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "McpConnectionManager.h"
#include "Core/Errors/McpRequestErrorDevice.h"
#include "Core/Journal/McpChangeJournal.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"
#include "Foundation/Diagnostics/McpWorldTickTimer.h"
#include "Domains/ControlEditor/Checkpoint/McpEditorCheckpoint.h"
#include "Domains/ControlEditor/McpEditorSupport.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "Domains/Log/McpLogHistory.h"
#include "Domains/Log/McpScriptErrors.h"
#include "Domains/SystemControl/McpSystemPrivate.h"
#include "Foundation/McpLiveStateRevisionTracker.h"
#include "Foundation/McpReadinessState.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Parse.h"

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

    // -McpStartMinimized: a launch for an automated session sat in the foreground, rendering flat out, until a caller
    // could minimize it over the bridge. The editor adds its main window hidden and shows it, maximized, at the end of
    // startup (FMainFrameHandler::ShowMainFrameWindow), which undid a minimize made before that. So this waits until
    // the window is on screen, puts it away without taking focus, and keeps it down for 10 s in case startup brings it
    // back. Bounded, because a ticker left registered would outlive a Live Coding module unload (see Deinitialize).
    // A restore made over the bridge ends the hold: one asked for 3 minutes after launch was put away again within
    // seconds, while a second one 23 s later stayed up.
    if (FParse::Param(FCommandLine::Get(), TEXT("McpStartMinimized")))
    {
        const double GiveUpAt = FPlatformTime::Seconds() + 600.0;
        const TSharedRef<double> HoldUntil = MakeShared<double>(0.0);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([GiveUpAt, HoldUntil](float)
        {
            if (McpWindowRestoreCount() != 0)
            {
                return false;
            }
            const double Now = FPlatformTime::Seconds();
            const TSharedPtr<SWindow> Root = FGlobalTabmanager::Get()->GetRootWindow();
            // The hold starts the first time the window is on screen, down already or not: a window that came up
            // minimized kept this waiting, and the first restore made over the bridge minutes later was put away.
            if (Root.IsValid() && Root->GetNativeWindow().IsValid() && Root->IsVisible())
            {
                if (!Root->IsWindowMinimized())
                {
                    MinimizeWindowForMcp(Root.ToSharedRef());
                }
                if (*HoldUntil == 0.0)
                {
                    *HoldUntil = Now + 10.0;
                }
            }
            return *HoldUntil == 0.0 ? Now < GiveUpAt : Now < *HoldUntil;
        }), 0.25f);
    }

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
    const auto* Settings = GetDefault<UEngineWireSettings>();
    if (!Settings->bEnableNativeMCP)
    {
        return;
    }

    FString PluginDir;
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("EngineWire"));
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
        LogEngineWireSubsystem,
        Error,
        TEXT("Failed to start Native MCP server on port %d"),
        NativePort);
    NativeTransport.Reset();
}
