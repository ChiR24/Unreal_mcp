#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LandscapeCreation.h"

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
#include "McpHandlerUtils.h"
#include "McpLandscapeMetadataTags.h"
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

  if (!Landscape->GetLandscapeGuid().IsValid()) {
    Landscape->SetLandscapeGuid(FGuid::NewGuid());
  }
  Landscape->CreateLandscapeInfo();

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
    if (Landscape->GetLayersConst().Num() == 0) {
      Landscape->CreateDefaultLayer();
    }
    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (LandscapeInfo && HeightArray.Num() > 0) {
      if (Landscape->GetRootComponent() &&
          !Landscape->GetRootComponent()->IsRegistered()) {
        Landscape->RegisterAllComponents();
      }
      FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
      LandscapeEdit.SetHeightData(InMinX, InMinY, InMaxX, InMaxY,
                                  HeightArray.GetData(), 0, true);
      LandscapeEdit.Flush();
    }
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (LandscapeInfo && HeightArray.Num() > 0) {
      if (Landscape->GetRootComponent() &&
          !Landscape->GetRootComponent()->IsRegistered()) {
        Landscape->RegisterAllComponents();
      }
      FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
      LandscapeEdit.SetHeightData(InMinX, InMinY, InMaxX, InMaxY,
                                  HeightArray.GetData(), 0, true);
      LandscapeEdit.Flush();
    }
    Landscape->CreateDefaultLayer();
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

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("landscapePath"),
                       Landscape->GetPackage()->GetPathName());
  Resp->SetStringField(TEXT("actorLabel"), Landscape->GetActorLabel());
  Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
  Resp->SetNumberField(TEXT("componentsX"), Request.ComponentsX);
  Resp->SetNumberField(TEXT("componentsY"), Request.ComponentsY);
  Resp->SetNumberField(TEXT("quadsPerComponent"), Request.QuadsPerComponent);
  Resp->SetNumberField(
      TEXT("extentX"), Request.ComponentsX * Request.QuadsPerComponent *
                           Landscape->GetActorScale3D().X * 0.5);
  Resp->SetNumberField(
      TEXT("extentY"), Request.ComponentsY * Request.QuadsPerComponent *
                           Landscape->GetActorScale3D().Y * 0.5);

  Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Landscape created successfully"), Resp,
                                   FString());
}
}
#endif
