// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"

#if WITH_EDITOR
#include "Engine/StaticMesh.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleGenerateLODs(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_lods"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
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

  // Support both lodCount and numLODs
  int32 NumLODs = 4;
  Payload->TryGetNumberField(TEXT("lodCount"), NumLODs);
  Payload->TryGetNumberField(TEXT("numLODs"), NumLODs);
  NumLODs = FMath::Clamp(NumLODs, 1, 50);

  // Build list of paths to process
  TArray<FString> Paths;

  // Add landscape path if provided
  if (!LandscapePath.IsEmpty()) {
    // Validate landscape path
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"), *LandscapePath),
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
                          FString::Printf(TEXT("Invalid or unsafe asset path: %s"), *SingleAssetPath),
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

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    int32 VerifiedCount = 0;
    TArray<FString> NotFoundPaths;
    TArray<FString> NotMeshPaths;
    TArray<TSharedPtr<FJsonValue>> MeshDetails;

    for (const FString &Path : Paths) {
      UObject *Obj = LoadObject<UObject>(nullptr, *Path);
      if (!Obj) {
        NotFoundPaths.Add(Path);
        continue;
      }

      UStaticMesh *Mesh = Cast<UStaticMesh>(Obj);
      if (!Mesh) {
        NotMeshPaths.Add(Path);
        continue;
      }

      TSharedPtr<FJsonObject> MeshInfo = MakeShared<FJsonObject>();
      MeshInfo->SetStringField(TEXT("assetPath"), Path);
      MeshInfo->SetStringField(TEXT("assetClass"), Mesh->GetClass()->GetName());
      MeshInfo->SetNumberField(TEXT("currentLODCount"), Mesh->GetNumLODs());
      MeshInfo->SetNumberField(TEXT("requestedLODCount"), NumLODs);
      MeshDetails.Add(MakeShared<FJsonValueObject>(MeshInfo));
      VerifiedCount++;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    const bool bSuccess = VerifiedCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    Resp->SetBoolField(TEXT("lodBuildSkipped"), true);
    Resp->SetNumberField(TEXT("verified"), VerifiedCount);
    Resp->SetNumberField(TEXT("requested"), Paths.Num());
    Resp->SetNumberField(TEXT("lodCount"), NumLODs);
    Resp->SetArrayField(TEXT("meshes"), MeshDetails);

    if (NotFoundPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotFoundArray;
      for (const FString &Path : NotFoundPaths) {
        NotFoundArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
      Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
    }

    if (NotMeshPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotMeshArray;
      for (const FString &Path : NotMeshPaths) {
        NotMeshArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Resp->SetArrayField(TEXT("notMeshPaths"), NotMeshArray);
      Resp->SetNumberField(TEXT("notMeshCount"), NotMeshPaths.Num());
    }

    const FString Message = bSuccess
        ? FString::Printf(TEXT("Verified %d mesh(es); LOD build skipped under NullRHI"), VerifiedCount)
        : TEXT("No static meshes verified for LOD generation under NullRHI");
    const FString ErrorCode = bSuccess ? FString() : TEXT("LOD_GENERATION_FAILED");
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  // NOTE: ProcessAutomationRequest already dispatches to GameThread.
  // Wrapping ALL work in AsyncTask(GameThread, ...) caused the queued lambda
  // to sit behind the current dispatch cycle, so responses never reached the
  // MCP server before the 30-second timeout. Execute synchronously instead.
  int32 SuccessCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> NotMeshPaths;

  for (const FString &Path : Paths) {
    SendProgressUpdate(RequestId, -1.0f,
        FString::Printf(TEXT("Processing LOD generation for: %s"), *Path), true);

    UObject *Obj = LoadObject<UObject>(nullptr, *Path);

    if (!Obj) {
      NotFoundPaths.Add(Path);
      continue;
    }

    // Try Static Mesh
    if (UStaticMesh *Mesh = Cast<UStaticMesh>(Obj)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Generating %d LODs for static mesh %s"), NumLODs, *Path);

        Mesh->Modify();
        Mesh->SetNumSourceModels(NumLODs);

        // Configure LOD reduction settings with progressive reduction
        for (int32 LODIndex = 1; LODIndex < NumLODs; LODIndex++) {
          FStaticMeshSourceModel &SourceModel = Mesh->GetSourceModel(LODIndex);
          FMeshReductionSettings &ReductionSettings =
              SourceModel.ReductionSettings;

          // Progressive reduction: 50%, 25%, 12.5%...
          float ReductionPercent =
              1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
          ReductionSettings.PercentTriangles = ReductionPercent;
          ReductionSettings.PercentVertices = ReductionPercent;

          // Enable reduction for this LOD level
          SourceModel.BuildSettings.bRecomputeNormals = false;
          SourceModel.BuildSettings.bRecomputeTangents = false;
          SourceModel.BuildSettings.bUseMikkTSpace = true;
        }

        // Build the mesh with new LOD settings
        Mesh->Build();
        Mesh->PostEditChange();
        McpSafeAssetSave(Mesh);

        SuccessCount++;
      } else {
        // Asset exists but is not a static mesh
        NotMeshPaths.Add(Path);
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

    // CRITICAL FIX: Return proper success/failure based on actual results
    // Previously always returned success=true even when 0 meshes processed
    bool bSuccess = SuccessCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetNumberField(TEXT("processed"), SuccessCount);
    Resp->SetNumberField(TEXT("requested"), Paths.Num());
    Resp->SetNumberField(TEXT("lodCount"), NumLODs);

    // Add details about failures
    if (NotFoundPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotFoundArray;
      for (const FString& P : NotFoundPaths) {
        NotFoundArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
      Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
    }

    if (NotMeshPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotMeshArray;
      for (const FString& P : NotMeshPaths) {
        NotMeshArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notMeshPaths"), NotMeshArray);
      Resp->SetNumberField(TEXT("notMeshCount"), NotMeshPaths.Num());
    }

    FString Message;
    FString ErrorCode;

    if (bSuccess) {
      Message = FString::Printf(TEXT("Generated LODs for %d mesh(es)"), SuccessCount);
    } else if (NotFoundPaths.Num() > 0 && NotMeshPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No assets found. %d path(s) not found."), NotFoundPaths.Num());
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else if (NotMeshPaths.Num() > 0 && NotFoundPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No static meshes found. %d asset(s) are not meshes."), NotMeshPaths.Num());
      ErrorCode = TEXT("INVALID_ASSET_TYPE");
    } else {
      Message = FString::Printf(TEXT("No LODs generated. %d not found, %d not meshes."),
                                NotFoundPaths.Num(), NotMeshPaths.Num());
      ErrorCode = TEXT("LOD_GENERATION_FAILED");
    }

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                                      Message, Resp, ErrorCode);

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Requires editor"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 8. METADATA
// ============================================================================
