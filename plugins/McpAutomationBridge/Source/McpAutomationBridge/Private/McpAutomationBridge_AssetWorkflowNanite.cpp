// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Engine/StaticMesh.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleNaniteRebuildMesh(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("nanite_rebuild_mesh"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nanite_rebuild_mesh payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MeshPath;
  if (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) ||
      MeshPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("meshPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MeshPath = SanitizeProjectRelativePath(MeshPath);
  if (MeshPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid meshPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the static mesh
  UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
  if (!StaticMesh) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath),
                        TEXT("MESH_NOT_FOUND"));
    return true;
  }

  // Check if mesh supports Nanite
  bool bEnableNanite = true;
  Payload->TryGetBoolField(TEXT("enableNanite"), bEnableNanite);

  // Nanite settings
  bool bPreserveArea = true;
  double TrianglePercent = 100.0;
  double FallbackPercent = 0.0;

  Payload->TryGetBoolField(TEXT("preserveArea"), bPreserveArea);
  Payload->TryGetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Payload->TryGetNumberField(TEXT("fallbackPercent"), FallbackPercent);

  // Clamp values
  TrianglePercent = FMath::Clamp(TrianglePercent, 0.0, 100.0);
  FallbackPercent = FMath::Clamp(FallbackPercent, 0.0, 100.0);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
  // UE 5.7+: Use accessor functions to avoid deprecation warnings
  FMeshNaniteSettings Settings = StaticMesh->GetNaniteSettings();
  Settings.bEnabled = bEnableNanite;
  Settings.PositionPrecision = 8; // Default precision

  // bPreserveArea replaced with ShapePreservation enum
  if (bPreserveArea) {
    Settings.ShapePreservation = ENaniteShapePreservation::PreserveArea;
  } else {
    Settings.ShapePreservation = ENaniteShapePreservation::None;
  }
  Settings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  Settings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
  if (FallbackPercent > 0.0) {
    Settings.GenerateFallback = ENaniteGenerateFallback::Enabled;
  } else {
    Settings.GenerateFallback = ENaniteGenerateFallback::PlatformDefault;
  }
  StaticMesh->SetNaniteSettings(Settings);
  StaticMesh->NotifyNaniteSettingsChanged();
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  // UE 5.1-5.6: Uses KeepPercentTriangles, FallbackPercentTriangles, and bPreserveArea
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.bPreserveArea = bPreserveArea;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#else
  // UE 5.0: Uses KeepPercentTriangles (no bPreserveArea)
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#endif

  // Mark mesh as modified
  StaticMesh->MarkPackageDirty();

  // Build response
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("meshPath"), MeshPath);
  Resp->SetStringField(TEXT("meshName"), StaticMesh->GetName());
  Resp->SetBoolField(TEXT("naniteEnabled"), bEnableNanite);
  Resp->SetBoolField(TEXT("preserveArea"), bPreserveArea);
  Resp->SetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Resp->SetNumberField(TEXT("fallbackPercent"), FallbackPercent);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Nanite settings updated for %s"), *StaticMesh->GetName()),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("nanite_rebuild_mesh requires UE 5.0+ editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
