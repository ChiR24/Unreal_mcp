#pragma once

#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#define MCP_HAS_PCG_EXECUTION 1
#include "Containers/Ticker.h"
#include <atomic>
#else
#define MCP_HAS_PCG_EXECUTION 0
#endif

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandlePCGExecution(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
#if MCP_HAS_PCG_EXECUTION
struct FPCGOperation
{
    FString Id, RequestId, Principal, ComponentPath, GraphPath, GraphRevision, InstanceRevision, Kind;
    FString State = TEXT("running"), ErrorCode, Message;
    TWeakObjectPtr<UEngineWireSubsystem> Bridge;
    TWeakObjectPtr<UPCGComponent> Component;
    TWeakObjectPtr<UPCGGraph> Graph;
    TSharedPtr<FMcpBridgeWebSocket> Socket;
    FPCGTaskId TaskId = InvalidPCGTaskId;
    double Started = FPlatformTime::Seconds(), Finished = 0, LastInputCheck = 0, Timeout = 60;
    bool bDeferred = true, bSave = false, bFinished = false, bResponded = false, bSaved = false;
    bool bDiagnosticsAvailable = false, bCancelRequested = false, bWatching = true, bNoOp = false;
    std::atomic<int32> GenerationStarts{0};
    std::atomic<bool> bGenerated{false}, bCleaned{false}, bCancelled{false}, bObservationCancelled{false};
    FDelegateHandle GeneratedHandle, CleanedHandle, CancelledHandle, StartedHandle;
    FTSTicker::FDelegateHandle Ticker;
    TArray<TSharedPtr<FJsonValue>> Diagnostics;
    TSharedPtr<FJsonObject> Resources;
};
UPCGComponent* ResolveExactPCGComponent(const TSharedPtr<FJsonObject>& Payload, FString& OutError);
bool ResolvePCGPrincipal(UEngineWireSubsystem* Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, FString& OutPrincipal);
FString GetPCGInstanceRevision(UPCGComponent* Component);
// Returns no partial inventory when traversal, time or response limits are exceeded.
TSharedPtr<FJsonObject> ReadPCGManagedResults(UPCGComponent* Component, int32 Offset = 0, int32 Limit = 50, int32 SampleLimit = 2);
TArray<TSharedPtr<FJsonValue>> ReadPCGExecutionDiagnostics(UPCGComponent* Component, bool& bOutAvailable, int32& OutErrorCount);
void ClearPCGExecutionDiagnostics(UPCGComponent* Component);
TSharedPtr<FPCGOperation> BeginPCGOperation(UEngineWireSubsystem* Bridge, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, UPCGComponent* Component, const FString& Kind, bool bDeferred, bool bSave,
    double Timeout, FString& OutError);
TSharedPtr<FPCGOperation> FindPCGOperation(const FString& Id, const FString& Principal, const FString& ComponentPath);
bool HasActivePCGOperation(UPCGComponent* Component);
TSharedPtr<FPCGOperation> FindActivePCGOperation(UPCGComponent* Component, const FString& Principal);
void FinishPCGOperation(TSharedPtr<FPCGOperation> Operation, const FString& State, const FString& Message, const FString& Code = FString());
TSharedPtr<FJsonObject> DescribePCGOperation(const TSharedPtr<FPCGOperation>& Operation);
void ReleasePCGOperation(const TSharedPtr<FPCGOperation>& Operation);
void ShutdownPCGOperations(UEngineWireSubsystem* Bridge = nullptr);
void StartPCGOperationObserver(const TSharedPtr<FPCGOperation>& Operation);
#endif
}
#endif
