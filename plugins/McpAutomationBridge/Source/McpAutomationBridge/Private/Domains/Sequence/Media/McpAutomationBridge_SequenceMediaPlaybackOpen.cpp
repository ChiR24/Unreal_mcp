#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMediaPlaybackInternal.h"

#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"
#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"
#include "HAL/FileManager.h"

namespace McpSequenceMedia {

UObject *LoadMediaPlayer(const TSharedPtr<FJsonObject> &Payload,
                         FString &OutResolvedPath, FString &OutError) {
  const FString PlayerPath =
      GetStringAny(Payload, {TEXT("mediaPlayerPath"), TEXT("playerPath")});
  UClass *PlayerClass = ResolveMediaClass(TEXT("MediaPlayer"), OutError);
  return PlayerClass && !PlayerPath.IsEmpty()
             ? LoadMediaObject(PlayerPath, PlayerClass, OutResolvedPath,
                               OutError)
             : nullptr;
}

bool OpenRequestedMedia(UObject *Player,
                        const TSharedPtr<FJsonObject> &Payload,
                        TSharedPtr<FJsonObject> Result,
                        bool &OutOpenRequested, FString &OutErrorCode,
                        FString &OutErrorMessage, FString &OutExpectedUrl) {
  OutOpenRequested = false;
  OutExpectedUrl.Reset();
  FString Error;
  const FString SourcePath =
      GetStringAny(Payload, {TEXT("mediaSourcePath"), TEXT("sourcePath")});
  if (!SourcePath.IsEmpty()) {
    UClass *SourceClass = ResolveMediaClass(TEXT("MediaSource"), Error);
    FString ResolvedPath;
    UObject *Source =
        SourceClass
            ? LoadMediaObject(SourcePath, SourceClass, ResolvedPath, Error)
            : nullptr;
    if (!Source ||
        !ValidateMediaSourcePolicy(Source, OutErrorCode, OutErrorMessage)) {
      if (OutErrorCode.IsEmpty())
        OutErrorCode = TEXT("INVALID_MEDIA_SOURCE");
      OutErrorMessage =
          OutErrorMessage.IsEmpty()
              ? (Error.IsEmpty() ? TEXT("The supplied media source is invalid")
                                 : Error)
              : OutErrorMessage;
      return false;
    }
    if (!CallBoolObjectFunction(Player, TEXT("CanPlaySource"), Source) ||
        !CallBoolObjectFunction(Player, TEXT("OpenSource"), Source)) {
      OutErrorCode = TEXT("MEDIA_OPEN_FAILED");
      OutErrorMessage = TEXT("The media player rejected the supplied source");
      return false;
    }
    Result->SetStringField(TEXT("mediaSourcePath"), ResolvedPath);
    OutOpenRequested = true;
    OutExpectedUrl = GetMediaUrl(Source);
  }

  const FString PlaylistPath = GetStringAny(Payload, {TEXT("playlistPath")});
  if (!OutOpenRequested && !PlaylistPath.IsEmpty()) {
    UClass *PlaylistClass = ResolveMediaClass(TEXT("MediaPlaylist"), Error);
    FString ResolvedPath;
    UObject *Playlist =
        PlaylistClass
            ? LoadMediaObject(PlaylistPath, PlaylistClass, ResolvedPath, Error)
            : nullptr;
    const double IndexValue =
        GetNumberAny(Payload, {TEXT("playlistIndex"), TEXT("index")});
    const int32 Index = FMath::Max(0, FMath::TruncToInt32(IndexValue));
    UObject *PlaylistSource =
        Playlist ? CallObjectIntFunction(Playlist, TEXT("Get"), Index) : nullptr;
    if (!PlaylistSource ||
        !ValidateMediaSourcePolicy(PlaylistSource, OutErrorCode,
                                   OutErrorMessage) ||
        !CallBoolObjectIntFunction(Player, TEXT("OpenPlaylistIndex"), Playlist,
                                   Index)) {
      if (OutErrorCode.IsEmpty())
        OutErrorCode = TEXT("MEDIA_OPEN_FAILED");
      OutErrorMessage =
          OutErrorMessage.IsEmpty()
              ? (Error.IsEmpty() ? TEXT("The media playlist could not be opened")
                                 : Error)
              : OutErrorMessage;
      return false;
    }
    Result->SetStringField(TEXT("playlistPath"), ResolvedPath);
    Result->SetNumberField(TEXT("playlistIndex"), Index);
    OutOpenRequested = true;
    OutExpectedUrl = GetMediaUrl(PlaylistSource);
  }

  const FString Url = GetStringAny(Payload, {TEXT("url"), TEXT("streamUrl")});
  if (!OutOpenRequested && !Url.IsEmpty()) {
    FString ResolvedUrl;
    FString ValidationError;
    McpSequencePathSecurity::ERemoteMediaUrlError UrlErrorType;
    McpSequencePathSecurity::ValidateRemoteMediaUrl(
        Url, ResolvedUrl, UrlErrorType, ValidationError);
    OutErrorCode =
        UrlErrorType == McpSequencePathSecurity::ERemoteMediaUrlError::NotAllowed
            ? TEXT("MEDIA_URL_NOT_ALLOWED")
            : TEXT("INVALID_ARGUMENT");
    OutErrorMessage = ValidationError;
    return false;
  }

  const FString FilePath =
      GetStringAny(Payload, {TEXT("filePath"), TEXT("mediaPath")});
  if (!OutOpenRequested && !FilePath.IsEmpty()) {
    FString ResolvedFilePath;
    FString ValidationError;
    if (!McpSequencePathSecurity::ValidateLocalPath(
            FilePath, McpSequencePathSecurity::ELocalPathUse::MediaInput,
            ResolvedFilePath, ValidationError)) {
      OutErrorCode = TEXT("MEDIA_PATH_NOT_ALLOWED");
      OutErrorMessage = ValidationError;
      return false;
    }
    if (!IFileManager::Get().FileExists(*ResolvedFilePath)) {
      OutErrorCode = TEXT("MEDIA_FILE_NOT_FOUND");
      OutErrorMessage = TEXT("The media file does not exist");
      return false;
    }
    if (!McpSequencePathSecurity::RevalidateResolvedLocalPath(
            ResolvedFilePath,
            McpSequencePathSecurity::ELocalPathUse::MediaInput,
            ValidationError)) {
      OutErrorCode = TEXT("MEDIA_PATH_NOT_ALLOWED");
      OutErrorMessage = ValidationError;
      return false;
    }
    if (!CallBoolStringFunction(Player, TEXT("OpenFile"), ResolvedFilePath)) {
      OutErrorCode = TEXT("MEDIA_OPEN_FAILED");
      OutErrorMessage = TEXT("The media file could not be opened");
      return false;
    }
    Result->SetStringField(TEXT("filePath"), ResolvedFilePath);
    OutOpenRequested = true;
    OutExpectedUrl = TEXT("file://") + ResolvedFilePath;
  }

  if (OutOpenRequested) {
    Result->SetStringField(TEXT("openStatus"), TEXT("pending"));
  }
  return true;
}

}
