#include "McpConnectionManagerPrivate.h"

void FMcpConnectionManager::RecordAutomationTelemetry(
    const FString &RequestId, bool bSuccess, const FString &Message,
    const FString &ErrorCode) {
  const double NowSeconds = FPlatformTime::Seconds();

  FAutomationRequestTelemetry Entry;
  if (!ActiveRequestTelemetry.RemoveAndCopyValue(RequestId, Entry)) {
    return;
  }

  const FString ActionKey =
      Entry.Action.IsEmpty() ? TEXT("unknown") : Entry.Action;
  FAutomationActionStats &Stats =
      AutomationActionTelemetry.FindOrAdd(ActionKey);

  const double DurationSeconds =
      FMath::Max(0.0, NowSeconds - Entry.StartTimeSeconds);
  if (bSuccess) {
    ++Stats.SuccessCount;
    Stats.TotalSuccessDurationSeconds += DurationSeconds;
  } else {
    ++Stats.FailureCount;
    Stats.TotalFailureDurationSeconds += DurationSeconds;
  }

  Stats.LastDurationSeconds = DurationSeconds;
  Stats.LastUpdatedSeconds = NowSeconds;
}

void FMcpConnectionManager::EmitAutomationTelemetrySummaryIfNeeded(
    double NowSeconds) {
  if (TelemetrySummaryIntervalSeconds <= 0.0)
    return;
  if ((NowSeconds - LastTelemetrySummaryLogSeconds) <
      TelemetrySummaryIntervalSeconds)
    return;

  LastTelemetrySummaryLogSeconds = NowSeconds;
  if (AutomationActionTelemetry.Num() == 0)
    return;

  TArray<FString> Lines;
  Lines.Reserve(AutomationActionTelemetry.Num());

  for (const TPair<FString, FAutomationActionStats> &Pair :
       AutomationActionTelemetry) {
    const FString &ActionKey = Pair.Key;
    const FAutomationActionStats &Stats = Pair.Value;
    const double AvgSuccess =
        Stats.SuccessCount > 0
            ? (Stats.TotalSuccessDurationSeconds / Stats.SuccessCount)
            : 0.0;
    const double AvgFailure =
        Stats.FailureCount > 0
            ? (Stats.TotalFailureDurationSeconds / Stats.FailureCount)
            : 0.0;
    Lines.Add(FString::Printf(TEXT("%s success=%d failure=%d last=%.3fs "
                                   "avgSuccess=%.3fs avgFailure=%.3fs"),
                              *ActionKey, Stats.SuccessCount,
                              Stats.FailureCount, Stats.LastDurationSeconds,
                              AvgSuccess, AvgFailure));
  }
  Lines.Sort();
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Automation action telemetry summary (%d actions):\n%s"),
         Lines.Num(), *FString::Join(Lines, TEXT("\n")));
}

int32 FMcpConnectionManager::GetActiveSocketCount() const {
  return ActiveSockets.Num();
}

void FMcpConnectionManager::RegisterRequestSocket(
    const FString &RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!RequestId.IsEmpty() && Socket.IsValid()) {
    FScopeLock Lock(&PendingRequestsMutex);
    PendingRequestsToSockets.Add(RequestId, Socket);
  }
}

void FMcpConnectionManager::StartRequestTelemetry(const FString &RequestId,
                                                  const FString &Action) {
  if (!ActiveRequestTelemetry.Contains(RequestId)) {
    FAutomationRequestTelemetry Entry;
    // Store lowercase action for consistent aggregation, similar to original
    // logic
    const FString LowerAction = Action.ToLower();
    Entry.Action = LowerAction.IsEmpty() ? Action : LowerAction;
    Entry.StartTimeSeconds = FPlatformTime::Seconds();
    ActiveRequestTelemetry.Add(RequestId, Entry);
  }
}
