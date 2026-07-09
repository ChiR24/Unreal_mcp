#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"
#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"

#include "HAL/FileManager.h"
#include "McpAutomationBridgeSubsystem.h"
#include "UObject/Package.h"

namespace McpSequenceMedia {
namespace {

struct FPlaylistItems {
  TArray<UObject *> Sources;
  TArray<FString> Urls;
  TArray<FString> Files;
};

bool ReadStringArray(const TSharedPtr<FJsonObject> &Payload,
                     const TCHAR *Field, TArray<FString> &OutValues,
                     FString &OutError) {
  const TArray<TSharedPtr<FJsonValue>> *Values = nullptr;
  if (!Payload.IsValid() || !Payload->HasField(Field)) {
    return true;
  }
  if (!Payload->TryGetArrayField(Field, Values) || !Values) {
    OutError = FString::Printf(TEXT("%s must be an array of strings"), Field);
    return false;
  }
  for (const TSharedPtr<FJsonValue> &Value : *Values) {
    if (!Value.IsValid() || Value->Type != EJson::String ||
        Value->AsString().TrimStartAndEnd().IsEmpty()) {
      OutError =
          FString::Printf(TEXT("%s must contain non-empty strings"), Field);
      return false;
    }
    OutValues.Add(Value->AsString().TrimStartAndEnd());
  }
  return true;
}

bool ResolvePlaylistItems(const TSharedPtr<FJsonObject> &Payload,
                          FPlaylistItems &OutItems, FString &OutCode,
                          FString &OutError) {
  TArray<FString> SourcePaths;
  TArray<FString> Urls;
  TArray<FString> FilePaths;
  if (!ReadStringArray(Payload, TEXT("sourcePaths"), SourcePaths, OutError) ||
      !ReadStringArray(Payload, TEXT("urls"), Urls, OutError) ||
      !ReadStringArray(Payload, TEXT("filePaths"), FilePaths, OutError)) {
    OutCode = TEXT("INVALID_ARGUMENT");
    return false;
  }
  UClass *SourceClass = ResolveMediaClass(TEXT("MediaSource"), OutError);
  if (!SourceClass) {
    OutCode = TEXT("MEDIA_FRAMEWORK_UNAVAILABLE");
    return false;
  }
  for (const FString &SourcePath : SourcePaths) {
    FString ResolvedPath;
    UObject *Source =
        LoadMediaObject(SourcePath, SourceClass, ResolvedPath, OutError);
    if (!Source ||
        !ValidateMediaSourcePolicy(Source, OutCode, OutError)) {
      if (OutCode.IsEmpty())
        OutCode = TEXT("INVALID_MEDIA_SOURCE");
      return false;
    }
    OutItems.Sources.Add(Source);
  }
  for (const FString &Url : Urls) {
    FString ResolvedUrl;
    McpSequencePathSecurity::ERemoteMediaUrlError UrlErrorType;
    if (!McpSequencePathSecurity::ValidateRemoteMediaUrl(
            Url, ResolvedUrl, UrlErrorType, OutError)) {
      OutCode =
          UrlErrorType ==
                  McpSequencePathSecurity::ERemoteMediaUrlError::NotAllowed
              ? TEXT("MEDIA_URL_NOT_ALLOWED")
              : TEXT("INVALID_ARGUMENT");
      return false;
    }
    OutItems.Urls.Add(MoveTemp(ResolvedUrl));
  }
  for (const FString &FilePath : FilePaths) {
    FString ResolvedPath;
    if (!McpSequencePathSecurity::ValidateLocalPath(
            FilePath, McpSequencePathSecurity::ELocalPathUse::MediaInput,
            ResolvedPath, OutError)) {
      OutCode = TEXT("MEDIA_PATH_NOT_ALLOWED");
      return false;
    }
    if (!IFileManager::Get().FileExists(*ResolvedPath)) {
      OutCode = TEXT("MEDIA_FILE_NOT_FOUND");
      OutError = TEXT("A playlist media file does not exist");
      return false;
    }
    OutItems.Files.Add(MoveTemp(ResolvedPath));
  }
  return true;
}

bool ApplyPlaylistItems(UObject *Playlist, const FPlaylistItems &Items,
                        FString &OutError) {
  for (UObject *Source : Items.Sources) {
    if (!CallBoolObjectFunction(Playlist, TEXT("Add"), Source)) {
      OutError = TEXT("The playlist rejected a media source");
      return false;
    }
  }
  for (const FString &Url : Items.Urls) {
    if (!CallBoolStringFunction(Playlist, TEXT("AddUrl"), Url)) {
      OutError = TEXT("The playlist rejected a media URL");
      return false;
    }
  }
  for (const FString &File : Items.Files) {
    if (!McpSequencePathSecurity::RevalidateResolvedLocalPath(
            File, McpSequencePathSecurity::ELocalPathUse::MediaInput,
            OutError))
      return false;
    if (!CallBoolStringFunction(Playlist, TEXT("AddFile"), File)) {
      OutError = TEXT("The playlist rejected a media file");
      return false;
    }
  }
  return true;
}

}

bool HandleCreateMediaPlaylist(UMcpAutomationBridgeSubsystem *Subsystem,
                               const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Error, Code;
  FPlaylistItems Items;
  if (!ResolvePlaylistItems(Payload, Items, Code, Error)) {
    SendMediaError(Subsystem, Socket, RequestId, Code, Error);
    return true;
  }
  UClass *PlaylistClass = ResolveMediaClass(TEXT("MediaPlaylist"), Error);
  UObject *Prototype =
      PlaylistClass
          ? NewObject<UObject>(GetTransientPackage(), PlaylistClass)
          : nullptr;
  if (!Prototype || !ApplyPlaylistItems(Prototype, Items, Error)) {
    SendMediaError(Subsystem, Socket, RequestId,
                   TEXT("PLAYLIST_ITEM_FAILED"), Error);
    return true;
  }

  FMediaAssetCreateResult Created;
  if (!CreateMediaAssetFromPayload(Payload, TEXT("/Game/Media/Playlists"),
                                   TEXT("MediaPlaylist"), Created, Error)) {
    const FString ErrorCode =
        Error.Contains(TEXT("[MEDIA_ASSET_ALREADY_EXISTS]"))
            ? TEXT("MEDIA_ASSET_ALREADY_EXISTS")
            : TEXT("MEDIA_ASSET_CREATE_FAILED");
    SendMediaError(Subsystem, Socket, RequestId,
                   ErrorCode, Error);
    return true;
  }
  UObject *Playlist = Created.Object;
  if (!ApplyPlaylistItems(Playlist, Items, Error)) {
    TSharedPtr<FJsonObject> Result =
        BuildAssetResponse(Created, TEXT("MediaPlaylist"));
    DiscardCreatedMediaAsset(Created);
    SendMediaError(Subsystem, Socket, RequestId, TEXT("PLAYLIST_ITEM_FAILED"),
                   Error, Result);
    return true;
  }
  Playlist->Modify();
  Playlist->MarkPackageDirty();
  Created.bSaved = SaveMediaAsset(Playlist);
  TSharedPtr<FJsonObject> Result =
      BuildAssetResponse(Created, TEXT("MediaPlaylist"));
  Result->SetNumberField(TEXT("addedCount"),
                         Items.Sources.Num() + Items.Urls.Num() +
                             Items.Files.Num());
  Result->SetArrayField(TEXT("failedItems"),
                        TArray<TSharedPtr<FJsonValue>>());
  if (!Created.bSaved) {
    DiscardCreatedMediaAsset(Created);
    SendMediaError(Subsystem, Socket, RequestId, TEXT("ASSET_SAVE_FAILED"),
                   TEXT("The media playlist could not be saved"), Result);
    return true;
  }
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("Media playlist asset created"), Result);
  return true;
}

}
