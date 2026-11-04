// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Foundation/BridgeHelpers/Security/McpProjectFileWrite.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"

namespace
{
// What ends a package name in config text: a character no package name holds ('.' starts the object name, ':' a
// subobject, a quote or a comma closes the value) and the brackets, '=' and ';' that config syntax puts around one.
const TCHAR *const ReportConfigPathEnd = TEXT("\\:*?\"<>|' ,.&!~\r\n\t@#()[]{};=");

/**
 * The /Game packages the project's config files name, each with the project-relative files that name it: Config/*.ini
 * and Saved/Config/*.ini, one folder down included (a platform's settings; the editor's per-user ones, where an
 * importer keeps the parent materials it copies from). The asset registry never sees these references. The text is
 * only searched for /Game/: a config file can hold a token, so nothing else in it is kept, logged or reported.
 */
TMap<FString, TArray<FString>> ReportConfigNamedPackages() {
  TMap<FString, TArray<FString>> Named; // FString keys compare without case, as package names do
  const FString Project = McpProjectRootDir();
  for (const TCHAR *Root : {TEXT("Config"), TEXT("Saved/Config")}) {
    TArray<FString> Subfolders;
    IFileManager::Get().FindFiles(Subfolders, *(Project / Root / TEXT("*")), false, true);
    Subfolders.Sort();
    TArray<FString> Folders;
    Folders.Add(FString(Root));
    for (const FString &Subfolder : Subfolders) {
      Folders.Add(FString(Root) / Subfolder);
    }
    for (const FString &Folder : Folders) {
      TArray<FString> Inis;
      IFileManager::Get().FindFiles(Inis, *(Project / Folder / TEXT("*.ini")), true, false);
      Inis.Sort();
      for (const FString &Ini : Inis) {
        const FString Relative = Folder / Ini;
        FString File, PathError, Text;
        // The shared resolver keeps the read inside the project, with no link on the way.
        if (!McpResolveProjectFilePath(Relative, File, PathError) || !FFileHelper::LoadFileToString(Text, *File)) {
          continue;
        }
        for (int32 At = Text.Find(TEXT("/Game/")); At != INDEX_NONE;) {
          int32 End = At + 6;
          while (End < Text.Len() && !FCString::Strchr(ReportConfigPathEnd, Text[End])) {
            ++End;
          }
          Named.FindOrAdd(Text.Mid(At, End - At)).AddUnique(Relative);
          At = Text.Find(TEXT("/Game/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, End);
        }
      }
    }
  }
  return Named;
}

/** One report row: the asset's name, object path and class. */
TSharedPtr<FJsonObject> ReportAssetRow(const FAssetData &Asset) {
  TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
  Row->SetStringField(TEXT("name"), Asset.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Row->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
  Row->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
  Row->SetStringField(TEXT("path"), Asset.ToSoftObjectPath().ToString());
  Row->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
  return Row;
}
} // namespace

bool UEngineWireSubsystem::HandleGenerateReport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("generate_report payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Directory;
  Payload->TryGetStringField(TEXT("directory"), Directory);
  if (Directory.IsEmpty()) {
    Directory = TEXT("/Game");
  }

  McpAssetPathCanonical::MapContentRootInline(Directory);

  FString ReportType;
  Payload->TryGetStringField(TEXT("reportType"), ReportType);
  if (ReportType.IsEmpty()) {
    ReportType = TEXT("Summary");
  }

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  FARFilter Filter;
  Filter.bRecursivePaths = true;
  if (!Directory.IsEmpty()) {
    Filter.PackagePaths.Add(FName(*Directory));
  }

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistryModule.Get().GetAssets(Filter, AssetList);
  const int32 Scanned = AssetList.Num();
  const bool bUnreferenced = ReportType.Equals(TEXT("unreferenced"), ESearchCase::IgnoreCase);
  TArray<TSharedPtr<FJsonValue>> ConfigRows;
  // unreferenced: only the assets no other package uses, hard or soft, as the packages on disk record it. A level is
  // where play starts and a redirector stands in for a moved asset, so neither is listed.
  if (bUnreferenced) {
    IAssetRegistry &Registry = AssetRegistryModule.Get();
    AssetList.RemoveAll([&Registry](const FAssetData &Asset) {
      if (Asset.IsRedirector() || Asset.GetClass() == UWorld::StaticClass()) {
        return true;
      }
      TArray<FName> Referencers;
      Registry.GetReferencers(Asset.PackageName, Referencers);
      Referencers.Remove(Asset.PackageName);
      return Referencers.Num() > 0;
    });
    // Nor one a config file names (a default map or game mode, the parent materials an importer copies from): the
    // registry never sees that reference, and deleting such an asset breaks the setting. It is listed apart, with the
    // files that name it, so a cleanup never takes it for unused.
    const TMap<FString, TArray<FString>> Named = ReportConfigNamedPackages();
    AssetList.RemoveAll([&Named, &ConfigRows](const FAssetData &Asset) {
      const TArray<FString> *Files = Named.Find(Asset.PackageName.ToString());
      if (Files != nullptr) {
        TArray<TSharedPtr<FJsonValue>> FileValues;
        for (const FString &ConfigFile : *Files) {
          FileValues.Add(MakeShared<FJsonValueString>(ConfigFile));
        }
        TSharedPtr<FJsonObject> Row = ReportAssetRow(Asset);
        Row->SetArrayField(TEXT("configFiles"), FileValues);
        ConfigRows.Add(MakeShared<FJsonValueObject>(Row));
      }
      return Files != nullptr;
    });
  }

  TArray<TSharedPtr<FJsonValue>> AssetsArray;
  for (const FAssetData &Asset : AssetList) {
    AssetsArray.Add(MakeShared<FJsonValueObject>(ReportAssetRow(Asset)));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("directory"), Directory);
  Resp->SetStringField(TEXT("reportType"), ReportType);
  Resp->SetNumberField(TEXT("assetCount"), AssetList.Num());
  Resp->SetNumberField(TEXT("scanned"), Scanned);
  Resp->SetArrayField(TEXT("assets"), AssetsArray);
  if (bUnreferenced) {
    Resp->SetArrayField(TEXT("configReferenced"), ConfigRows);
  }

  bool bFileWritten = false;
  if (!OutputPath.IsEmpty()) {
    // The shared project-file resolver: traversal, device names, containment, and no link on the way (a checked
    // spelling alone let a symbolic link under the project carry the report outside it).
    FString AbsoluteOutput;
    FString PathError;
    if (!McpResolveProjectFilePath(OutputPath, AbsoluteOutput, PathError)) {
      SendAutomationError(Socket, RequestId, PathError, TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString FileContents;
    FJsonSerializer::Serialize(Resp.ToSharedRef(), TJsonWriterFactory<>::Create(&FileContents));
    FString WriteError;
    bFileWritten = McpWriteProjectText(AbsoluteOutput, FileContents, WriteError);
    if (!bFileWritten) {
      UE_LOG(LogEngineWireSubsystem, Warning, TEXT("%s"), *WriteError); // the reply's warnings say why
    }
  }

  if (!OutputPath.IsEmpty()) {
    Resp->SetStringField(TEXT("outputPath"), OutputPath);
    Resp->SetBoolField(TEXT("fileWritten"), bFileWritten);
  }

  const FString Message = !bUnreferenced ? FString(TEXT("Asset report generated"))
      : FString::Printf(TEXT("%d of %d asset(s) under %s are unreferenced: no saved package refers to them and no project config "
                             "file names them. %d more are named only in config files (configReferenced, with the files that name "
                             "them): not candidates to delete. A primary asset the asset manager cooks, or one code loads by path, "
                             "can still be in use."), AssetList.Num(), Scanned, *Directory, ConfigRows.Num());
  SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
  return true;
}
