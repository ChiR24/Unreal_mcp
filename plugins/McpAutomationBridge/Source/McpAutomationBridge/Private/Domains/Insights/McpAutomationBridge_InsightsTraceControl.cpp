#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Insights/McpAutomationBridge_InsightsRequests.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "McpAutomationBridgeSubsystem.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

namespace McpInsights
{
namespace
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
using FMcpTraceOptions = FTraceAuxiliary::Options;
#else
using FMcpTraceOptions = FTraceAuxiliary::FOptions;
#endif

FString BuildNetworkTarget(const FString& Host, int32 Port)
{
    return Port > 0 ? FString::Printf(TEXT("%s:%d"), *Host, Port) : Host;
}

bool HasActiveTrace()
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    return FTraceAuxiliary::IsConnected();
#else
    return false;
#endif
}

void AddStartFields(
    const FTraceStartRequest& Request,
    TSharedPtr<FJsonObject>& Result)
{
    Result->SetStringField(TEXT("connectionType"), StartModeToString(Request.Mode));
    Result->SetStringField(TEXT("channels"), Request.Channels);
    if (Request.Mode == ETraceStartMode::File)
    {
        Result->SetStringField(TEXT("traceFilePath"), Request.OutputPath);
    }
    if (Request.Mode == ETraceStartMode::Network)
    {
        Result->SetStringField(TEXT("host"), Request.Host);
        Result->SetNumberField(TEXT("port"), Request.Port);
    }
}
}

bool TryBuildStartRequest(
    const TSharedPtr<FJsonObject>& Payload,
    bool bForceFileMode,
    FTraceStartRequest& OutRequest,
    FString& OutError,
    FString& OutErrorCode)
{
    FString Mode;
    Payload->TryGetStringField(TEXT("connectionType"), Mode);
    Mode.TrimStartAndEndInline();
    Mode.ToLowerInline();
    if (bForceFileMode || Mode.IsEmpty() || Mode == TEXT("file"))
    {
        OutRequest.Mode = ETraceStartMode::File;
    }
    else if (Mode == TEXT("network") || Mode == TEXT("server"))
    {
        OutRequest.Mode = ETraceStartMode::Network;
    }
    else if (Mode == TEXT("relay") || Mode == TEXT("none") || Mode == TEXT("memory"))
    {
        OutError = TEXT("Trace connectionType is not supported by the live bridge.");
        OutErrorCode = TEXT("NOT_SUPPORTED");
        return false;
    }
    else
    {
        OutError = TEXT("Unsupported trace connectionType.");
        OutErrorCode = TEXT("INVALID_CONNECTION_TYPE");
        return false;
    }

    FString ChannelsError;
    if (!TryReadChannels(Payload, OutRequest.Channels, ChannelsError))
    {
        OutError = ChannelsError;
        OutErrorCode = TEXT("INVALID_CHANNELS");
        return false;
    }
    OutRequest.bOverwrite = ReadOverwrite(Payload);
    Payload->TryGetBoolField(TEXT("excludeTail"), OutRequest.bExcludeTail);
    OutRequest.MaxTailSize = ReadMaxTailSize(Payload);

    if (OutRequest.Mode == ETraceStartMode::File)
    {
        return TryResolveTracePath(Payload, false, true, true,
            OutRequest.OutputPath, OutError, OutErrorCode);
    }
    if (OutRequest.Mode == ETraceStartMode::Network)
    {
        return TryReadHostAndPort(Payload, true, OutRequest.Host,
            OutRequest.Port, OutError, OutErrorCode);
    }
    return true;
}

bool HandleStartSession(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FTraceStartRequest Request;
    FString Error;
    FString ErrorCode;
    FString RequestedAction;
    FString RequestedSubAction;
    Payload->TryGetStringField(TEXT("action"), RequestedAction);
    Payload->TryGetStringField(TEXT("subAction"), RequestedSubAction);
    const bool bForceFile = Action.Equals(
        TEXT("capture_insights_trace"), ESearchCase::IgnoreCase) ||
        RequestedAction.Equals(
            TEXT("capture_insights_trace"), ESearchCase::IgnoreCase) ||
        RequestedSubAction.Equals(
            TEXT("capture_insights_trace"), ESearchCase::IgnoreCase);
    if (!TryBuildStartRequest(Payload, bForceFile, Request, Error, ErrorCode))
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        return true;
    }

    if (HasActiveTrace())
    {
        TSharedPtr<FJsonObject> Result =
            CreateInsightsResult(TEXT("start_session"), TEXT("start_trace"));
        Result->SetStringField(TEXT("status"), TEXT("already_started"));
        AddStartFields(Request, Result);
        AddTraceStatus(Result);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Trace session already active."), Result);
        return true;
    }

    FMcpTraceOptions Options;
    Options.bTruncateFile = Request.bOverwrite;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    Options.bExcludeTail = Request.bExcludeTail;
#endif
    FTraceAuxiliary::EConnectionType Type = FTraceAuxiliary::EConnectionType::File;
    FString Target;
    if (Request.Mode == ETraceStartMode::Network)
    {
        Type = FTraceAuxiliary::EConnectionType::Network;
        Target = BuildNetworkTarget(Request.Host, Request.Port);
    }
    else
    {
        Target = Request.OutputPath;
    }

    const TCHAR* TargetArg = Target.IsEmpty() ? nullptr : *Target;
    const TCHAR* ChannelsArg = Request.Channels.IsEmpty() ? nullptr : *Request.Channels;
    const bool bStarted = FTraceAuxiliary::Start(
        Type, TargetArg, ChannelsArg, &Options);
    if (!bStarted)
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to start trace session."), TEXT("TRACE_START_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result =
        CreateInsightsResult(TEXT("start_session"), TEXT("start_trace"));
    Result->SetStringField(TEXT("status"), TEXT("started"));
    AddStartFields(Request, Result);
    AddTraceStatus(Result);
    // Optionally open the Unreal Insights viewer on the new trace (dogfood #173).
    bool bLaunchViewer = false;
    Payload->TryGetBoolField(TEXT("launchViewer"), bLaunchViewer);
    if (bLaunchViewer)
    {
        const FString ViewerPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries"), TEXT("Win64"), TEXT("UnrealInsights.exe")));
        const FString ViewerArgs = (Request.Mode == ETraceStartMode::Network || Target.IsEmpty()) ? FString() : FString::Printf(TEXT("-OpenTraceFile=\"%s\""), *Target);
        FProcHandle ViewerHandle = FPlatformProcess::CreateProc(*ViewerPath, *ViewerArgs, true, false, false, nullptr, 0, nullptr, nullptr);
        Result->SetBoolField(TEXT("viewerLaunched"), ViewerHandle.IsValid());
        Result->SetStringField(TEXT("viewerPath"), ViewerPath);
    }
    else
    {
        Result->SetStringField(TEXT("hint"), TEXT("Pass launchViewer:true to open Unreal Insights on this trace"));
    }
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Trace session started."), Result);
    return true;
}

}
