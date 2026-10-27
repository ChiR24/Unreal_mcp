// Copyright (c) 2024 EngineWire Contributors

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "UObject/UObjectIterator.h"

namespace {
// A material draws on a Nanite mesh only with the Nanite usage flag. The editor sets a missing one as it registers the
// mesh's placed copies, or when it next draws them, but never saves it, and a packaged game then draws that surface
// without it. 5.8 gives a material instance flags of its own; before it the material holds them.
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
UMaterialInterface *McpNaniteFlagOwner(UMaterialInterface *Material) { return Material; }
bool McpNaniteHasFlag(UMaterialInterface *Owner) { return Owner->GetUsageByFlag(MATUSAGE_Nanite); }
void McpNaniteSetFlag(UMaterialInterface *Owner) { Owner->SetMaterialUsage(MATUSAGE_Nanite); }
#else
UMaterialInterface *McpNaniteFlagOwner(UMaterialInterface *Material) { return Material ? Material->GetMaterial() : nullptr; }
bool McpNaniteHasFlag(UMaterialInterface *Owner) { return static_cast<UMaterial *>(Owner)->GetUsageByFlag(MATUSAGE_Nanite); }
void McpNaniteSetFlag(UMaterialInterface *Owner) {
  bool bNeedsRecompile = false;
  static_cast<UMaterial *>(Owner)->SetMaterialUsage(bNeedsRecompile, MATUSAGE_Nanite);
}
#endif

// Read before the build, which can set the flag itself: the materials the mesh draws with (its slots, and what its
// placed copies in the open level override them with) whose flag owner lacks it.
TArray<UMaterialInterface *> McpNaniteUnflagged(const UStaticMesh *StaticMesh) {
  TArray<UMaterialInterface *> Used;
  for (const FStaticMaterial &Slot : StaticMesh->GetStaticMaterials()) Used.AddUnique(Slot.MaterialInterface);
  const UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  for (TObjectIterator<UStaticMeshComponent> It; It && World; ++It) {
    if (It->GetStaticMesh() != StaticMesh || It->GetWorld() != World) continue;
    for (int32 Index = 0; Index < It->GetNumMaterials(); ++Index) Used.AddUnique(It->GetMaterial(Index));
  }
  TArray<UMaterialInterface *> Unflagged;
  for (UMaterialInterface *Material : Used) {
    UMaterialInterface *Owner = McpNaniteFlagOwner(Material);
    if (Owner && !McpNaniteHasFlag(Owner)) Unflagged.AddUnique(Owner);
  }
  return Unflagged;
}

// After the build: gives each of them the flag (unless the build already did) and saves it.
TArray<TSharedPtr<FJsonValue>> McpNaniteFlagMaterials(const TArray<UMaterialInterface *> &Unflagged) {
  TArray<TSharedPtr<FJsonValue>> Flagged;
  for (UMaterialInterface *Owner : Unflagged) {
    if (!McpNaniteHasFlag(Owner)) McpNaniteSetFlag(Owner);
    const FString Package = Owner->GetOutermost()->GetName();
    if (!Package.StartsWith(TEXT("/Engine/")) && McpSafeAssetSave(Owner)) Flagged.Add(MakeShared<FJsonValueString>(Package));
  }
  return Flagged;
}

// Turns Nanite on for one mesh, rebuilds and saves it, and writes what it reads back into Resp. Returns why it could
// not, with OutCode, or an empty string.
FString McpNaniteRebuildOne(const FString &Given, double TrianglePercent, const TSharedPtr<FJsonObject> &Resp,
                            FString &OutCode) {
  const FString MeshPath = SanitizeProjectRelativePath(Given);
  if (MeshPath.IsEmpty()) {
    OutCode = TEXT("SECURITY_VIOLATION");
    return McpPathRefusalMessage(TEXT("assetPath"), Given);
  }
  UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
  if (!StaticMesh) {
    OutCode = TEXT("MESH_NOT_FOUND");
    return FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath);
  }
  // Nanite draws only opaque and masked materials (translucency is an off-by-default 5.8 experiment): a glass slot
  // would show the default material, so such a mesh is left as it is.
  for (const FStaticMaterial &Slot : StaticMesh->GetStaticMaterials()) {
    const EBlendMode Blend = Slot.MaterialInterface ? Slot.MaterialInterface->GetBlendMode() : BLEND_Opaque;
    if (Blend == BLEND_Opaque || Blend == BLEND_Masked) continue;
    OutCode = TEXT("NANITE_UNSUPPORTED_MATERIAL");
    return FString::Printf(TEXT("%s has a translucent material in slot '%s' (%s): Nanite draws only opaque and masked ")
                           TEXT("materials, so that slot would show the default material. It was left without Nanite; give the ")
                           TEXT("slot an opaque or masked material first (process_asset process=mesh_materials), or keep it so."),
                           *StaticMesh->GetName(), *Slot.MaterialSlotName.ToString(), *Slot.MaterialInterface->GetName());
  }

  const TArray<UMaterialInterface *> Unflagged = McpNaniteUnflagged(StaticMesh);

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
    OutCode = TEXT("SAVE_FAILED");
    return FString::Printf(TEXT("Nanite was turned on for %s, but the mesh could not be saved."), *MeshPath);
  }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
  const FMeshNaniteSettings &After = StaticMesh->GetNaniteSettings();
#else
  const FMeshNaniteSettings &After = StaticMesh->NaniteSettings;
#endif
  Resp->SetStringField(TEXT("meshPath"), MeshPath);
  Resp->SetStringField(TEXT("meshName"), StaticMesh->GetName());
  Resp->SetBoolField(TEXT("naniteEnabled"), After.bEnabled);
  Resp->SetNumberField(TEXT("trianglePercent"), FMath::RoundToDouble(After.KeepPercentTriangles * 1000.0) / 10.0);
  Resp->SetBoolField(TEXT("rebuilt"), true);
  Resp->SetBoolField(TEXT("saved"), true);
  const TArray<TSharedPtr<FJsonValue>> Flagged = McpNaniteFlagMaterials(Unflagged);
  if (Flagged.Num() > 0) Resp->SetArrayField(TEXT("materialsFlagged"), Flagged);
  return FString();
}
} // namespace

bool UEngineWireSubsystem::HandleNaniteRebuildMesh(
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

  // Percent of the source triangles Nanite keeps (the record's default: all of them).
  double TrianglePercent = 100.0;
  Payload->TryGetNumberField(TEXT("trianglePercent"), TrianglePercent);
  TrianglePercent = FMath::Clamp(TrianglePercent, 0.0, 100.0);

  // The published capability schema names this `assetPath` (and rejects any
  // undeclared field), so reading only `meshPath` made the action uncallable.
  // Prefer the contract spelling and keep `meshPath` for legacy callers.
  FString MeshPath;
  if ((!Payload->TryGetStringField(TEXT("assetPath"), MeshPath) ||
       MeshPath.IsEmpty()) &&
      (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) ||
       MeshPath.IsEmpty())) {
    // A whole imported pack in one call, under one consent: one row per mesh, and a mesh that fails does not stop
    // the others.
    const TArray<TSharedPtr<FJsonValue>> *Paths = nullptr;
    if (!Payload->TryGetArrayField(TEXT("assetPaths"), Paths) || !Paths || Paths->Num() == 0) {
      SendAutomationError(Socket, RequestId,
                          TEXT("assetPath (or assetPaths) is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<FString> Problems;
    for (const TSharedPtr<FJsonValue> &Value : *Paths) {
      const FString Given = Value.IsValid() ? Value->AsString() : FString();
      TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
      Row->SetStringField(TEXT("assetPath"), Given);
      FString Code;
      const FString Error = McpNaniteRebuildOne(Given, TrianglePercent, Row, Code);
      if (!Error.IsEmpty()) {
        Row->SetStringField(TEXT("error"), Error);
        Problems.Add(Error);
      }
      // A row without an error was rebuilt and saved; its name and path are the one given.
      for (const TCHAR *Field : {TEXT("meshName"), TEXT("rebuilt"), TEXT("saved")}) Row->RemoveField(Field);
      FString Read;
      if (Row->TryGetStringField(TEXT("meshPath"), Read) && Read == Given) Row->RemoveField(TEXT("meshPath"));
      Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetArrayField(TEXT("meshes"), Rows);
    const FString Summary = FString::Printf(TEXT("Nanite on for %d of %d meshes, keeping %.1f%% of their triangles; rebuilt and saved"),
                                            Rows.Num() - Problems.Num(), Rows.Num(), TrianglePercent);
    if (Problems.Num() > 0) {
      const FString More = Problems.Num() > 1 ? FString::Printf(TEXT(" (%d more under meshes)"), Problems.Num() - 1) : FString();
      SendAutomationResponse(Socket, RequestId, false, Summary + TEXT("; ") + Problems[0] + More, Resp,
                             TEXT("NANITE_PARTIAL"));
      return true;
    }
    SendAutomationResponse(Socket, RequestId, true, Summary, Resp, FString());
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  FString Code;
  const FString Error = McpNaniteRebuildOne(MeshPath, TrianglePercent, Resp, Code);
  if (!Error.IsEmpty()) {
    SendAutomationError(Socket, RequestId, Error, Code);
    return true;
  }
  FString MeshName;
  Resp->TryGetStringField(TEXT("meshName"), MeshName);
  const TArray<TSharedPtr<FJsonValue>> *Flagged = nullptr;
  const FString FlagNote = Resp->TryGetArrayField(TEXT("materialsFlagged"), Flagged) && Flagged
      ? FString::Printf(TEXT("; gave %d of its materials the Nanite usage flag and saved them"), Flagged->Num()) : FString();
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Nanite on for %s, keeping %.1f%% of its triangles; rebuilt and saved%s"),
                                         *MeshName, Resp->GetNumberField(TEXT("trianglePercent")), *FlagNote),
                         Resp, FString());
  return true;
}
