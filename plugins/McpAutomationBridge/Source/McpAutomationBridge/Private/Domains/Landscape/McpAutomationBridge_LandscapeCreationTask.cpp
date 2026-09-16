#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Landscape/McpAutomationBridge_LandscapeCreation.h"

#if WITH_EDITOR
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeGrassType.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/Landscape/McpLandscapeMetadataTags.h"
#include "Materials/Material.h"
#include "ScopedTransaction.h"

namespace McpLandscapeCreation {
void CreateLandscapeOnGameThread(
    UMcpAutomationBridgeSubsystem &Subsystem, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FLandscapeCreationRequest &Request) {
  if (!GEditor)
    return;
  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World)
    return;

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  ALandscape *Landscape = World->SpawnActor<ALandscape>(
      ALandscape::StaticClass(), Request.Location, FRotator::ZeroRotator,
      SpawnParams);
  if (!Landscape) {
    Subsystem.SendAutomationError(RequestingSocket, RequestId,
                                  TEXT("Failed to spawn landscape actor"),
                                  TEXT("SPAWN_FAILED"));
    return;
  }

  Landscape->SetActorLabel(Request.Name.IsEmpty()
                               ? FString::Printf(TEXT("Landscape_%dx%d"),
                                                 Request.ComponentsX,
                                                 Request.ComponentsY)
                               : Request.Name);
  Landscape->ComponentSizeQuads = Request.QuadsPerComponent;
  Landscape->SubsectionSizeQuads =
      Request.QuadsPerComponent / Request.SectionsPerComponent;
  Landscape->NumSubsections = Request.SectionsPerComponent;
  McpLandscapeMetadataTags::EncodeLandscapeMetadata(
      Landscape, Request.ComponentsX, Request.ComponentsY,
      Request.QuadsPerComponent);

  if (!Request.MaterialPath.IsEmpty()) {
    UMaterialInterface *Mat =
        LoadObject<UMaterialInterface>(nullptr, *Request.MaterialPath);
    if (Mat) {
      Landscape->LandscapeMaterial = Mat;
    }
  }

#if !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
  // Import() owns the landscape GUID and the ULandscapeInfo on 5.5+, exactly
  // as the engine's own New Landscape flow does. Pre-seeding them registered a
  // ULandscapeInfo under a different GUID than the one Import went on to use,
  // and the components it then built carried no data for the default edit
  // layer -- which tripped `check(ComponentLayerData != nullptr)` inside
  // LandscapeEdit.cpp and took the whole editor down.
  if (!Landscape->GetLandscapeGuid().IsValid()) {
    Landscape->SetLandscapeGuid(FGuid::NewGuid());
  }
  Landscape->CreateLandscapeInfo();
#endif

  const int32 VertX = Request.ComponentsX * Request.QuadsPerComponent + 1;
  const int32 VertY = Request.ComponentsY * Request.QuadsPerComponent + 1;
  TArray<uint16> HeightArray;
  HeightArray.Init(32768, VertX * VertY);

  const int32 InMinX = 0;
  const int32 InMinY = 0;
  const int32 InMaxX = Request.ComponentsX * Request.QuadsPerComponent;
  const int32 InMaxY = Request.ComponentsY * Request.QuadsPerComponent;

  TMap<FGuid, TArray<uint16>> ImportHeightData;
  ImportHeightData.Add(FGuid(), HeightArray);
  TMap<FGuid, TArray<FLandscapeImportLayerInfo>> ImportLayerInfos;
  ImportLayerInfos.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
  TArray<FLandscapeLayer> EditLayers;

  {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Create Landscape")));
    Landscape->Modify();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    // Import() is what ALLOCATES the ULandscapeComponents. SetHeightData()
    // only writes into components that already exist, so on its own it built
    // nothing: the actor landed in the level with zero components, zero
    // bounds and no surface, while create_landscape still reported back the
    // requested component count and extent as though it had been built. Every
    // downstream landscape call then failed the same silent way -- the sculpt
    // brush answered "Landscape sculpted" with modifiedVertices 0 because
    // GetLandscapeExtent had no components to report an extent for.
    // Mirrors the engine's own New Landscape call verbatim: a fresh GUID, the
    // vertex-space region, quads PER SUBSECTION, and an empty edit-layer view
    // so Import creates the default layer itself once the components exist.
    Landscape->Import(FGuid::NewGuid(), InMinX, InMinY, InMaxX, InMaxY,
                      Request.SectionsPerComponent,
                      Request.QuadsPerComponent / FMath::Max(1, Request.SectionsPerComponent),
                      ImportHeightData, nullptr, ImportLayerInfos,
                      ELandscapeImportAlphamapType::Layered,
                      TArrayView<const FLandscapeLayer>());
#else
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Landscape->Import(FGuid::NewGuid(), 0, 0, Request.ComponentsX - 1,
                      Request.ComponentsY - 1, Request.SectionsPerComponent,
                      Request.QuadsPerComponent, ImportHeightData, nullptr,
                      ImportLayerInfos, ELandscapeImportAlphamapType::Layered,
                      EditLayers.Num() > 0 ? &EditLayers : nullptr);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    Landscape->CreateDefaultLayer();
#endif
  }

  Landscape->SetActorLabel(Request.Name.IsEmpty()
                               ? FString::Printf(TEXT("Landscape_%dx%d"),
                                                 Request.ComponentsX,
                                                 Request.ComponentsY)
                               : Request.Name);
  if (!Request.MaterialPath.IsEmpty()) {
    UMaterialInterface *Mat =
        LoadObject<UMaterialInterface>(nullptr, *Request.MaterialPath);
    if (Mat) {
      Landscape->LandscapeMaterial = Mat;
      Landscape->PostEditChange();
    }
  }
  if (Landscape->GetRootComponent() &&
      !Landscape->GetRootComponent()->IsRegistered()) {
    Landscape->RegisterAllComponents();
  }
  if (IsValid(Landscape)) {
    Landscape->PostEditChange();
  }

  // Refuse to report success for an empty shell. Everything below used to be
  // echoed straight back from Request, so a landscape that built nothing was
  // indistinguishable from one that built correctly.
  const int32 BuiltComponents = Landscape->LandscapeComponents.Num();
  if (BuiltComponents == 0) {
    Subsystem.SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Landscape actor was spawned but no components were built; it has no geometry, bounds or surface."),
        TEXT("LANDSCAPE_BUILD_FAILED"));
    return;
  }
  const FBox BuiltBounds = Landscape->GetComponentsBoundingBox(true);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("builtComponents"), BuiltComponents);
  Resp->SetStringField(TEXT("landscapePath"),
                       Landscape->GetPackage()->GetPathName());
  Resp->SetStringField(TEXT("actorLabel"), Landscape->GetActorLabel());
  Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
  Resp->SetNumberField(TEXT("componentsX"), Request.ComponentsX);
  Resp->SetNumberField(TEXT("componentsY"), Request.ComponentsY);
  Resp->SetNumberField(TEXT("quadsPerComponent"), Request.QuadsPerComponent);
  // Measured from the built components, not computed from the request.
  Resp->SetNumberField(TEXT("extentX"), BuiltBounds.GetExtent().X);
  Resp->SetNumberField(TEXT("extentY"), BuiltBounds.GetExtent().Y);

  Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Landscape created successfully"), Resp,
                                   FString());
}
}
#endif
