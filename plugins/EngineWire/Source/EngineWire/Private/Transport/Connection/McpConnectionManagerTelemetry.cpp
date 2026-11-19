#include "Transport/Connection/McpConnectionManagerPrivate.h"

#include "Core/Requests/McpInflightLedger.h"
#include "Foundation/McpTelemetryRegistry.h"

void FMcpConnectionManager::RecordAutomationTelemetry(
    const FString &RequestId, bool bSuccess, const FString &Message,
    const FString &ErrorCode) {
  // Only the bounded error CODE is forwarded; Message is deliberately dropped
  // because it routinely carries asset paths and object names.
  FMcpTelemetryRegistry::Get().EndRequest(
      RequestId, bSuccess ? TEXT("success") : TEXT("failure"), ErrorCode);
  // Every reply ends here (the native path calls this too): the call is no longer running.
  FMcpInflightLedger::Get().Done(RequestId);

  ActiveRequestActions.Remove(RequestId);
}

void FMcpConnectionManager::StartRequestTelemetry(const FString &RequestId,
                                                  const FString &Action) {
  ActiveRequestActions.FindOrAdd(RequestId, Action.ToLower());
}
