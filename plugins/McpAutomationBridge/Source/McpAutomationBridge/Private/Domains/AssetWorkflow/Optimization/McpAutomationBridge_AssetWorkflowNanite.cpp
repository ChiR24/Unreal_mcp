// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#include "Engine/StaticMesh.h"

bool UMcpAutomationBridgeSubsystem::HandleNaniteRebuildMesh(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("nanite_rebuild_mesh"), ESearchCase::IgnoreCase)) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nanite_rebuild_mesh payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // The published capability schema names this `assetPath` (and rejects any
  // undeclared field), so reading only `meshPath` made the action uncallable.
  // Prefer the contract spelling and keep `meshPath` for legacy callers.
  FString MeshPath;
  if ((!Payload->TryGetStringField(TEXT("assetPath"), MeshPath) ||
       MeshPath.IsEmpty()) &&
      (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) ||
       MeshPath.IsEmpty())) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath (or meshPath) is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString MeshPathAsGiven = MeshPath;
  MeshPath = SanitizeProjectRelativePath(MeshPath);
  if (MeshPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        McpPathRefusalMessage(TEXT("meshPath"), MeshPathAsGiven),
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

  // Percent of the source triangles Nanite keeps (the record's default: all of them).
  double TrianglePercent = 100.0;
  Payload->TryGetNumberField(TEXT("trianglePercent"), TrianglePercent);
  TrianglePercent = FMath::Clamp(TrianglePercent, 0.0, 100.0);

  // Only bEnabled and the kept share change; the mesh's other Nanite settings stay as they are.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
  FMeshNaniteSettings Settings = StaticMesh->GetNaniteSettings();
  Settings.bEnabled = true;
  Settings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->SetNaniteSettings(Settings);
#else
  StaticMesh->NaniteSettings.bEnabled = true;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
#endif
  // New settings do nothing until the render data is rebuilt, and are gone on the next load
  // unless saved: this used to rebuild on 5.7+ only, save never, and echo the request back.
  StaticMesh->Build(true);
  StaticMesh->MarkPackageDirty();
  if (!McpSafeAssetSave(StaticMesh)) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Nanite was turned on for %s, but the mesh could not be saved."), *MeshPath),
                        TEXT("SAVE_FAILED"));
    return true;
  }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
  const FMeshNaniteSettings &After = StaticMesh->GetNaniteSettings();
#else
  const FMeshNaniteSettings &After = StaticMesh->NaniteSettings;
#endif
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("meshPath"), MeshPath);
  Resp->SetStringField(TEXT("meshName"), StaticMesh->GetName());
  Resp->SetBoolField(TEXT("naniteEnabled"), After.bEnabled);
  Resp->SetNumberField(TEXT("trianglePercent"), FMath::RoundToDouble(After.KeepPercentTriangles * 1000.0) / 10.0);
  Resp->SetBoolField(TEXT("rebuilt"), true);
  Resp->SetBoolField(TEXT("saved"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Nanite on for %s, keeping %.1f%% of its triangles; rebuilt and saved"),
                                         *StaticMesh->GetName(), After.KeepPercentTriangles * 100.0f),
                         Resp, FString());
  return true;
}
