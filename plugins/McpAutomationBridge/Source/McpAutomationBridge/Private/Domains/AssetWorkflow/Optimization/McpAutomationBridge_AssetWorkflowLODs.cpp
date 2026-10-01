// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"

#include "Engine/StaticMesh.h"

bool UMcpAutomationBridgeSubsystem::HandleGenerateLODs(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_lods"), ESearchCase::IgnoreCase)) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Support both landscapePath (single) and assetPaths (array)
  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);

  // Support both assetPath (single) and assetPaths (array)
  FString SingleAssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), SingleAssetPath);

  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray)) {
    Payload->TryGetArrayField(TEXT("assets"), AssetPathsArray);
  }

  // lodCount is the contract name; the legacy numLODs used to override it when
  // both were sent, and now only fills in when lodCount is absent.
  int32 NumLODs = 4;
  if (!Payload->TryGetNumberField(TEXT("lodCount"), NumLODs)) {
    Payload->TryGetNumberField(TEXT("numLODs"), NumLODs);
  }
  NumLODs = FMath::Clamp(NumLODs, 1, 50);

  // Build list of paths to process
  TArray<FString> Paths;

  // Add landscape path if provided
  if (!LandscapePath.IsEmpty()) {
    // Validate landscape path
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          McpPathRefusalMessage(TEXT("landscape path"), LandscapePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }

  // Add single asset path if provided
  if (!SingleAssetPath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(SingleAssetPath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          McpPathRefusalMessage(TEXT("asset path"), SingleAssetPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }

  // Add asset paths if provided
  if (AssetPathsArray) {
    for (const auto &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        FString SafePath = SanitizeProjectRelativePath(Val->AsString());
        if (!SafePath.IsEmpty()) {
          Paths.Add(SafePath);
        }
      }
    }
  }

  if (Paths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("landscapePath or assetPaths required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // A headless (NullRHI) editor cannot build LODs: each mesh is verified and reported instead.
  const bool bHeadless = FParse::Param(FCommandLine::Get(), TEXT("NullRHI"));
  int32 SuccessCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> NotMeshPaths;
  TArray<TSharedPtr<FJsonValue>> MeshDetails;
  // ProcessAutomationRequest already runs on the game thread; wrapping this in AsyncTask(GameThread)
  // queued it behind the current dispatch cycle and the reply missed the 30-second timeout.
  for (const FString &Path : Paths) {
    UObject *Obj = LoadObject<UObject>(nullptr, *Path);
    UStaticMesh *Mesh = Cast<UStaticMesh>(Obj);
    if (!Mesh) {
      (Obj ? NotMeshPaths : NotFoundPaths).Add(Path);
      continue;
    }
    if (bHeadless) {
      TSharedPtr<FJsonObject> MeshInfo = MakeShared<FJsonObject>();
      MeshInfo->SetStringField(TEXT("assetPath"), Path);
      MeshInfo->SetStringField(TEXT("assetClass"), Mesh->GetClass()->GetName());
      MeshInfo->SetNumberField(TEXT("currentLODCount"), Mesh->GetNumLODs());
      MeshInfo->SetNumberField(TEXT("requestedLODCount"), NumLODs);
      MeshDetails.Add(MakeShared<FJsonValueObject>(MeshInfo));
    } else {
      SendProgressUpdate(RequestId, -1.0f,
          FString::Printf(TEXT("Processing LOD generation for: %s"), *Path), true);
      McpHandlerUtils::ApplyProgressiveLods(Mesh, NumLODs);
    }
    ++SuccessCount;
  }

  // success reflects what actually happened: it used to be true even when no mesh was processed.
  const bool bSuccess = SuccessCount > 0;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetNumberField(bHeadless ? TEXT("verified") : TEXT("processed"), SuccessCount);
  Resp->SetNumberField(TEXT("requested"), Paths.Num());
  Resp->SetNumberField(TEXT("lodCount"), NumLODs);
  if (bHeadless) {
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    Resp->SetBoolField(TEXT("lodBuildSkipped"), true);
    Resp->SetArrayField(TEXT("meshes"), MeshDetails);
  }
  if (NotFoundPaths.Num() > 0) {
    Resp->SetArrayField(TEXT("notFoundPaths"), McpHandlerUtils::ToJsonStringArray(NotFoundPaths));
    Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
  }
  if (NotMeshPaths.Num() > 0) {
    Resp->SetArrayField(TEXT("notMeshPaths"), McpHandlerUtils::ToJsonStringArray(NotMeshPaths));
    Resp->SetNumberField(TEXT("notMeshCount"), NotMeshPaths.Num());
  }

  FString Message;
  FString ErrorCode;
  if (bSuccess) {
    Message = bHeadless
        ? FString::Printf(TEXT("Verified %d mesh(es); LOD build skipped under NullRHI"), SuccessCount)
        : FString::Printf(TEXT("Generated LODs for %d mesh(es)"), SuccessCount);
  } else if (NotMeshPaths.Num() == 0) {
    Message = FString::Printf(TEXT("No assets found. %d path(s) not found."), NotFoundPaths.Num());
    ErrorCode = TEXT("ASSET_NOT_FOUND");
  } else if (NotFoundPaths.Num() == 0) {
    Message = FString::Printf(TEXT("No static meshes found. %d asset(s) are not meshes."), NotMeshPaths.Num());
    ErrorCode = TEXT("INVALID_ASSET_TYPE");
  } else {
    Message = FString::Printf(TEXT("No LODs generated. %d not found, %d not meshes."),
                              NotFoundPaths.Num(), NotMeshPaths.Num());
    ErrorCode = TEXT("LOD_GENERATION_FAILED");
  }
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);

  return true;
}
