#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"
#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "HAL/FileManager.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace McpSequenceMedia {
namespace {

struct FMediaSourceConfig {
  FString SourceType;
  FString FilePath;
  FString StreamUrl;
  bool bPrecacheFile = false;
  UObject *DefaultSource = nullptr;
  TArray<TPair<FString, UObject *>> PlatformSources;
};

bool ResolveSourceConfig(const TSharedPtr<FJsonObject> &Payload,
                         FMediaSourceConfig &OutConfig, FString &OutClassName,
                         FString &OutCode, FString &OutError) {
  OutConfig.SourceType =
      GetStringAny(Payload, {TEXT("sourceType"), TEXT("type")}, TEXT("file"))
          .ToLower();
  if (OutConfig.SourceType == TEXT("file")) {
    OutClassName = TEXT("FileMediaSource");
    const FString FilePath =
        GetStringAny(Payload, {TEXT("filePath"), TEXT("mediaPath")});
    if (FilePath.IsEmpty()) {
      OutCode = TEXT("INVALID_ARGUMENT");
      OutError = TEXT("filePath is required for a file media source");
      return false;
    }
    if (!McpSequencePathSecurity::ValidateLocalPath(
            FilePath, McpSequencePathSecurity::ELocalPathUse::MediaInput,
            OutConfig.FilePath, OutError)) {
      OutCode = TEXT("MEDIA_PATH_NOT_ALLOWED");
      return false;
    }
    if (!IFileManager::Get().FileExists(*OutConfig.FilePath)) {
      OutCode = TEXT("MEDIA_FILE_NOT_FOUND");
      OutError = TEXT("The media file does not exist");
      return false;
    }
    OutConfig.bPrecacheFile =
        GetBoolAny(Payload, {TEXT("precacheFile"), TEXT("precache")});
    return true;
  }
  if (OutConfig.SourceType == TEXT("stream")) {
    OutClassName = TEXT("StreamMediaSource");
    const FString Url = GetStringAny(Payload, {TEXT("streamUrl"), TEXT("url")});
    McpSequencePathSecurity::ERemoteMediaUrlError UrlErrorType;
    if (!McpSequencePathSecurity::ValidateRemoteMediaUrl(
            Url, OutConfig.StreamUrl, UrlErrorType, OutError)) {
      OutCode =
          UrlErrorType ==
                  McpSequencePathSecurity::ERemoteMediaUrlError::NotAllowed
              ? TEXT("MEDIA_URL_NOT_ALLOWED")
              : TEXT("INVALID_ARGUMENT");
      return false;
    }
    return true;
  }
  if (OutConfig.SourceType != TEXT("platform")) {
    OutCode = TEXT("INVALID_ARGUMENT");
    OutError = TEXT("sourceType must be file, stream, or platform");
    return false;
  }
  OutClassName = TEXT("PlatformMediaSource");
  UClass *MediaSourceClass = ResolveMediaClass(TEXT("MediaSource"), OutError);
  const FString DefaultSourcePath =
      GetStringAny(Payload, {TEXT("defaultSourcePath"), TEXT("sourcePath")});
  FString ResolvedPath;
  OutConfig.DefaultSource =
      MediaSourceClass && !DefaultSourcePath.IsEmpty()
          ? LoadMediaObject(DefaultSourcePath, MediaSourceClass, ResolvedPath,
                            OutError)
          : nullptr;
  if (!OutConfig.DefaultSource) {
    OutCode = TEXT("MEDIA_SOURCE_NOT_FOUND");
    if (OutError.IsEmpty())
      OutError = TEXT("defaultSourcePath is required for a platform source");
    return false;
  }
  if (!ValidateMediaSourcePolicy(OutConfig.DefaultSource, OutCode, OutError))
    return false;
  const TSharedPtr<FJsonObject> *Sources = nullptr;
  if (!Payload->TryGetObjectField(TEXT("platformSources"), Sources) || !Sources ||
      !Sources->IsValid())
    return true;
  for (const TPair<FString, TSharedPtr<FJsonValue>> Entry : (*Sources)->Values) {
    if (!Entry.Value.IsValid() || Entry.Value->Type != EJson::String) {
      OutCode = TEXT("INVALID_PLATFORM_SOURCE");
      OutError = TEXT("Every platformSources value must be an asset path");
      return false;
    }
    UObject *Source = LoadMediaObject(Entry.Value->AsString(), MediaSourceClass,
                                      ResolvedPath, OutError);
    if (!Source) {
      OutCode = TEXT("INVALID_PLATFORM_SOURCE");
      return false;
    }
    if (!ValidateMediaSourcePolicy(Source, OutCode, OutError))
      return false;
    OutConfig.PlatformSources.Emplace(Entry.Key, Source);
  }
  return true;
}

bool ApplySourceConfig(UObject *Source, const FMediaSourceConfig &Config,
                       FString &OutError) {
  if (Config.SourceType == TEXT("file")) {
    if (!McpSequencePathSecurity::RevalidateResolvedLocalPath(
            Config.FilePath,
            McpSequencePathSecurity::ELocalPathUse::MediaInput, OutError))
      return false;
    return CallVoidStringFunction(Source, TEXT("SetFilePath"),
                                  Config.FilePath) &&
           SetBoolProperty(Source, TEXT("PrecacheFile"), Config.bPrecacheFile);
  }
  if (Config.SourceType == TEXT("stream"))
    return SetStringProperty(Source, TEXT("StreamUrl"), Config.StreamUrl);
  if (!SetObjectProperty(Source, TEXT("MediaSource"), Config.DefaultSource))
    return false;
  for (const TPair<FString, UObject *> &Entry : Config.PlatformSources)
    if (!SetStringObjectMapEntry(Source, TEXT("PlatformMediaSources"), Entry.Key,
                                 Entry.Value))
      return false;
  return true;
}

}

bool HandleCreateMediaSource(UMcpAutomationBridgeSubsystem *Subsystem,
                             const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FMediaSourceConfig Config;
  FString ClassName, Code, Error;
  if (!ResolveSourceConfig(Payload, Config, ClassName, Code, Error)) {
    SendMediaError(Subsystem, Socket, RequestId, Code, Error);
    return true;
  }
  UClass *AssetClass = ResolveMediaClass(ClassName, Error);
  UObject *Prototype =
      AssetClass ? NewObject<UObject>(GetTransientPackage(), AssetClass) : nullptr;
  if (!Prototype || !ApplySourceConfig(Prototype, Config, Error) ||
      !CallBoolFunction(Prototype, TEXT("Validate"))) {
    SendMediaError(Subsystem, Socket, RequestId, TEXT("INVALID_MEDIA_SOURCE"),
                   Error.IsEmpty()
                       ? TEXT("The media source configuration did not validate")
                       : Error);
    return true;
  }

  FString PackageName, AssetName, ObjectPath;
  if (!ResolveMediaAssetIdentity(Payload, TEXT("/Game/Media/Sources"), FString(),
                                 PackageName, AssetName, ObjectPath, Error)) {
    SendMediaError(Subsystem, Socket, RequestId,
                   TEXT("MEDIA_ASSET_CREATE_FAILED"), Error);
    return true;
  }
  FMediaAssetCreateResult Created;
  if (!CreateMediaAsset(AssetClass, PackageName, AssetName, Created, Error) ||
      !ApplySourceConfig(Created.Object, Config, Error)) {
    DiscardCreatedMediaAsset(Created);
    const FString ErrorCode =
        Error.Contains(TEXT("[MEDIA_ASSET_ALREADY_EXISTS]"))
            ? TEXT("MEDIA_ASSET_ALREADY_EXISTS")
            : TEXT("MEDIA_ASSET_CREATE_FAILED");
    SendMediaError(Subsystem, Socket, RequestId,
                   ErrorCode,
                   Error.IsEmpty() ? TEXT("Failed to configure media source")
                                   : Error);
    return true;
  }
  Created.Object->Modify();
  Created.Object->MarkPackageDirty();
  Created.bSaved = SaveMediaAsset(Created.Object);
  TSharedPtr<FJsonObject> Result = BuildAssetResponse(Created, ClassName);
  Result->SetStringField(TEXT("sourceType"), Config.SourceType);
  Result->SetStringField(TEXT("url"), GetMediaUrl(Created.Object));
  Result->SetBoolField(TEXT("valid"), true);
  if (!Created.bSaved) {
    DiscardCreatedMediaAsset(Created);
    SendMediaError(Subsystem, Socket, RequestId, TEXT("ASSET_SAVE_FAILED"),
                   TEXT("The media source could not be saved"), Result);
    return true;
  }
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("Media source asset created"), Result);
  return true;
}

}
