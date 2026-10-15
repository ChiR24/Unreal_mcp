#include "MCP/Transport/McpNativeTransportTimeoutPolicy.h"

namespace McpNativeTransportTimeoutPolicy {
double ResolveToolCallTimeoutSeconds(
    const FString &ToolName, const TSharedPtr<FJsonObject> &Arguments,
    int32 MaxMovieRenderTimeoutMs,
    int32 MaxMovieRenderCancellationWaitMs) {
  constexpr double DefaultRequestTimeoutSeconds = 300.0;
  if (ToolName != TEXT("manage_sequence") || !Arguments.IsValid()) {
    return DefaultRequestTimeoutSeconds;
  }
  FString Action;
  if (!Arguments->TryGetStringField(TEXT("action"), Action) ||
      Action != TEXT("start_render")) {
    return DefaultRequestTimeoutSeconds;
  }

  const double MaximumRenderTimeoutMs =
      FMath::Max(1, MaxMovieRenderTimeoutMs);
  double RenderTimeoutMs =
      FMath::Min(300000.0, MaximumRenderTimeoutMs);
  double RequestedTimeoutMs = 0.0;
  if (Arguments->TryGetNumberField(TEXT("timeoutMs"), RequestedTimeoutMs) &&
      FMath::IsFinite(RequestedTimeoutMs) && RequestedTimeoutMs > 0.0) {
    RenderTimeoutMs =
        FMath::Min(RequestedTimeoutMs, MaximumRenderTimeoutMs);
  }
  const double CancellationWaitMs =
      FMath::Max(0, MaxMovieRenderCancellationWaitMs);
  constexpr double NativeResponseGraceMs = 5000.0;
  return (RenderTimeoutMs + CancellationWaitMs + NativeResponseGraceMs) /
         1000.0;
}
}
