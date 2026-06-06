#include "McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Exporters/Exporter.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#endif

namespace McpSystemControlHandlers {

bool HandleExportAsset(UMcpAutomationBridgeSubsystem* Self,
                       const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload,
                       FSystemControlSocket RequestingSocket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

  FString ExportPath;
  Payload->TryGetStringField(TEXT("exportPath"), ExportPath);

  if (AssetPath.IsEmpty()) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("assetPath is required for export"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Invalid asset path for export"),
                              TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (ExportPath.IsEmpty()) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("exportPath is required for export"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SafeExportPath = SanitizeProjectFilePath(ExportPath);
  if (SafeExportPath.IsEmpty()) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid or unsafe export path: %s"), *ExportPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  FString AbsoluteExportPath = FPaths::ProjectDir() / SafeExportPath;
  FPaths::MakeStandardFilename(AbsoluteExportPath);
  AbsoluteExportPath = FPaths::ConvertRelativePathToFull(AbsoluteExportPath);
  FPaths::NormalizeFilename(AbsoluteExportPath);

  FString NormalizedProjectDir =
      FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
  FPaths::NormalizeDirectoryName(NormalizedProjectDir);
  if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
    NormalizedProjectDir += TEXT("/");
  }

  if (!AbsoluteExportPath.StartsWith(NormalizedProjectDir,
                                     ESearchCase::IgnoreCase)) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Export path escapes project directory: %s"),
                        *ExportPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString ExportDir = FPaths::GetPath(AbsoluteExportPath);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  if (!PlatformFile.DirectoryExists(*ExportDir)) {
    PlatformFile.CreateDirectoryTree(*ExportDir);
  }

  UObject* Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Failed to load asset: %s"), *SafeAssetPath),
        TEXT("LOAD_FAILED"));
    return true;
  }

  FString Extension = FPaths::GetExtension(AbsoluteExportPath).ToLower();
  bool bExportSuccess = false;
  FString ExportError;

  FAssetToolsModule& AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
  IAssetTools& AssetTools = AssetToolsModule.Get();

  TArray<UObject*> AssetsToExport;
  AssetsToExport.Add(Asset);
  AssetTools.ExportAssets(AssetsToExport, ExportDir);

  FString ExpectedExportPath =
      ExportDir / FPaths::GetBaseFilename(SafeAssetPath) + TEXT(".") +
      Extension;
  if (FPaths::FileExists(ExpectedExportPath)) {
    bExportSuccess = true;
  } else {
    bExportSuccess = FPaths::FileExists(AbsoluteExportPath);
  }

  if (!bExportSuccess) {
    UExporter* Exporter = nullptr;
    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
      UClass* CurrentClass = *ClassIt;
      if (CurrentClass->IsChildOf(UExporter::StaticClass()) &&
          !CurrentClass->HasAnyClassFlags(CLASS_Abstract)) {
        UExporter* DefaultExporter = Cast<UExporter>(CurrentClass->GetDefaultObject());
        if (DefaultExporter && DefaultExporter->SupportedClass &&
            Asset->GetClass()->IsChildOf(DefaultExporter->SupportedClass)) {
          if (DefaultExporter->PreferredFormatIndex <
              DefaultExporter->FormatExtension.Num()) {
            FString PreferredExt =
                DefaultExporter
                    ->FormatExtension[DefaultExporter->PreferredFormatIndex]
                    .ToLower();
            if (PreferredExt == Extension || PreferredExt.Contains(Extension)) {
              Exporter = DefaultExporter;
              break;
            }
          }
          if (!Exporter) {
            Exporter = DefaultExporter;
          }
        }
      }
    }

    if (Exporter) {
      int32 ExportResult =
          UExporter::ExportToFile(Asset, Exporter, *AbsoluteExportPath, false,
                                  false, false);
      bExportSuccess = (ExportResult != 0);
    }

    if (!bExportSuccess) {
      ExportError = FString::Printf(
          TEXT("Export failed for asset type '%s' and format '%s'"),
          *Asset->GetClass()->GetName(), *Extension);
    }
  }

  if (bExportSuccess) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    AddAssetVerification(Result, Asset);
    Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Result->SetStringField(TEXT("exportPath"), AbsoluteExportPath);
    Result->SetStringField(TEXT("format"), Extension);
    Result->SetBoolField(TEXT("success"), true);
    Self->SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Asset exported to: %s"), *AbsoluteExportPath),
        Result);
  } else {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Result->SetStringField(TEXT("exportPath"), AbsoluteExportPath);
    Result->SetStringField(TEXT("format"), Extension);
    Result->SetStringField(TEXT("error"), ExportError);
    Self->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Export failed: %s"), *ExportError), Result,
        TEXT("EXPORT_FAILED"));
  }
  return true;
#else
  return false;
#endif
}

}
