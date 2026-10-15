#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpInsights
{
enum class ETraceStartMode : uint8
{
    File,
    Network
};

struct FTraceStartRequest
{
    ETraceStartMode Mode = ETraceStartMode::File;
    FString Channels;
    FString OutputPath;
    FString Host = TEXT("localhost");
    int32 Port = 0;
    bool bOverwrite = false;
};

// The payload's subAction (or action), lower-cased, with start_unreal_insights/capture_insights_trace -> start_session.
FString NormalizeSubAction(const TSharedPtr<FJsonObject>& Payload);
// TraceAction defaults to SubAction.
TSharedPtr<FJsonObject> CreateInsightsResult(
    const FString& SubAction,
    const FString& TraceAction = FString());
// Whether a trace consumer is connected (always false before 5.3, which cannot tell).
bool HasActiveTrace();
void AddTraceStatus(TSharedPtr<FJsonObject>& Result);
FString StartModeToString(ETraceStartMode Mode);
bool TryBuildStartRequest(
    const TSharedPtr<FJsonObject>& Payload,
    bool bForceFileMode,
    FTraceStartRequest& OutRequest,
    FString& OutError,
    FString& OutErrorCode);
bool TryReadChannels(
    const TSharedPtr<FJsonObject>& Payload,
    FString& OutChannels,
    FString& OutError);
bool TryResolveTracePath(
    const TSharedPtr<FJsonObject>& Payload,
    bool bRequireExistingFile,
    bool bAppendTraceExtension,
    bool bAllowGeneratedDefault,
    FString& OutPath,
    FString& OutError,
    FString& OutErrorCode,
    const TCHAR* PreferredField = TEXT("traceFile"));
bool TryReadHostAndPort(
    const TSharedPtr<FJsonObject>& Payload,
    bool bRequireHost,
    FString& OutHost,
    int32& OutPort,
    FString& OutError,
    FString& OutErrorCode);

bool HandleStartSession(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStopSession(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandlePauseSession(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleResumeSession(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleGetTraceStatus(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleWriteSnapshot(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSendSnapshot(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleAnalyzeTrace(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
