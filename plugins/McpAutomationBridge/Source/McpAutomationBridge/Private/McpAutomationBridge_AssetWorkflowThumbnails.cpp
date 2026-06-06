// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "ObjectTools.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleGenerateThumbnail(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_thumbnail"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("create_thumbnail"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("generate_thumbnail payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // SECURITY: Validate asset path
  FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *AssetPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  int32 Width = 512;
  int32 Height = 512;

  double TempWidth = 0, TempHeight = 0;
  if (Payload->TryGetNumberField(TEXT("width"), TempWidth))
    Width = static_cast<int32>(TempWidth);
  if (Payload->TryGetNumberField(TEXT("height"), TempHeight))
    Height = static_cast<int32>(TempHeight);

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  // NOTE: ProcessAutomationRequest already dispatches to GameThread.
  // Wrapping ALL work (including fast existence checks) in AsyncTask(GameThread, ...)
  // caused the queued lambda to sit behind the current dispatch cycle, so responses
  // never reached the MCP server before the 30-second timeout (issues #138, #139).
  // Execute synchronously instead.
  SendProgressUpdate(RequestId, 0.0f,
      FString::Printf(TEXT("Starting thumbnail generation for: %s"), *SafeAssetPath), true);

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Asset not found"), nullptr,
                           TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetBoolField(TEXT("thumbnailRendered"), false);
    Result->SetBoolField(TEXT("headlessSafe"), true);
    if (!OutputPath.IsEmpty()) {
      Result->SetStringField(TEXT("outputPath"), OutputPath);
    }
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Asset verified; thumbnail rendering skipped under NullRHI"),
                           Result, FString());
    return true;
  }

  // Send progress update before GPU operation
  SendProgressUpdate(RequestId, 50.0f,
      TEXT("Rendering thumbnail (GPU operation)..."), true);

  FObjectThumbnail ObjectThumbnail;
  ThumbnailTools::RenderThumbnail(
      Asset, Width, Height,
      ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr,
      &ObjectThumbnail);

  bool bSuccess = ObjectThumbnail.GetImageWidth() > 0 &&
                  ObjectThumbnail.GetImageHeight() > 0;

  if (bSuccess && !OutputPath.IsEmpty()) {
    const TArray<uint8> &ImageData = ObjectThumbnail.GetUncompressedImageData();

    if (ImageData.Num() > 0) {
      TArray<FColor> ColorData;
      ColorData.Reserve(Width * Height);

      // Fixed: Ensure we don't read out of bounds if ImageData length isn't a multiple of 4
      for (int32 i = 0; i + 3 < ImageData.Num(); i += 4) {
        FColor Color;
        Color.B = ImageData[i + 0];
        Color.G = ImageData[i + 1];
        Color.R = ImageData[i + 2];
        Color.A = ImageData[i + 3];
        ColorData.Add(Color);
      }

      // SECURITY: Sanitize and validate the output path to prevent path traversal
      FString SafeOutputPath = SanitizeProjectFilePath(OutputPath);
      if (SafeOutputPath.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("Invalid or unsafe output path: %s"), *OutputPath),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }

      FString AbsolutePath = FPaths::ProjectDir() / SafeOutputPath;
      AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
      FPaths::NormalizeFilename(AbsolutePath);

      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }

      if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("Output path escapes project directory: %s"), *OutputPath),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }

      TArray<uint8> CompressedData;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      FImageUtils::ThumbnailCompressImageArray(Width, Height, ColorData,
                                               CompressedData);
#else
      // UE 5.0: Use CompressImageArray instead
      FImageUtils::CompressImageArray(Width, Height, ColorData, CompressedData);
#endif
      bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *AbsolutePath);
    }
  }

  if (Asset->GetOutermost()) {
    Asset->GetOutermost()->MarkPackageDirty();
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Result->SetNumberField(TEXT("width"), Width);
  Result->SetNumberField(TEXT("height"), Height);

  if (!OutputPath.IsEmpty()) {
    Result->SetStringField(TEXT("outputPath"), OutputPath);
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Thumbnail generated successfully")
               : TEXT("Thumbnail generation failed"),
      Result, bSuccess ? FString() : TEXT("THUMBNAIL_GENERATION_FAILED"));

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("generate_thumbnail requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 7. BASIC ASSET OPERATIONS (Import, Duplicate, Rename, Move, etc.)
// ============================================================================

/**
 * Handles asset import requests.
 *
 * IMPORTANT: In UE 5.7+, the Interchange Framework is the default importer for
 * FBX/glTF files. Interchange uses the TaskGraph internally for async operations.
 * If we call ImportAssetsAutomated() synchronously from within an AsyncTask callback
 * (which is how WebSocket messages are dispatched), we hit a TaskGraph recursion
 * guard assertion: "++Queue(QueueIndex).RecursionGuard == 1".
 *
 * The fix is to defer the import to the next editor tick using GEditor->GetTimerManager(),
 * which breaks out of the TaskGraph callback chain and allows Interchange to function
 * correctly.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
