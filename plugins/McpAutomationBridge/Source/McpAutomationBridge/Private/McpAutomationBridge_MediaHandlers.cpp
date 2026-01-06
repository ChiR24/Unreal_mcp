// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 30: Media Framework Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/Factory.h"

// Media Framework Headers (conditionally included)
#if __has_include("MediaPlayer.h")
#define MCP_HAS_MEDIA_FRAMEWORK 1
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "FileMediaSource.h"
#include "StreamMediaSource.h"
#include "MediaTexture.h"
#include "MediaPlaylist.h"
#include "MediaSoundComponent.h"
#include "IMediaControls.h"
#include "IMediaTracks.h"
#else
#define MCP_HAS_MEDIA_FRAMEWORK 0
#endif

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleMediaAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_media"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_media")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_media payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR && MCP_HAS_MEDIA_FRAMEWORK
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message = FString::Printf(TEXT("Media action '%s' completed"), *LowerSub);
  FString ErrorCode;

  if (!GEditor) {
    bSuccess = false;
    Message = TEXT("Editor not available");
    ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  // ========================================================================
  // CREATE MEDIA PLAYER
  // ========================================================================
  if (LowerSub == TEXT("create_media_player")) {
    FString AssetName;
    Payload->TryGetStringField(TEXT("assetName"), AssetName);
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    
    if (AssetName.IsEmpty()) {
      AssetName = TEXT("NewMediaPlayer");
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game");
    }
    
    // Normalize path
    SavePath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    if (!SavePath.StartsWith(TEXT("/Game"))) {
      SavePath = TEXT("/Game") / SavePath;
    }
    
    FString FullPath = SavePath / AssetName;
    UPackage* Package = CreatePackage(*FullPath);
    if (Package) {
      Package->FullyLoad();
      
      UMediaPlayer* MediaPlayer = NewObject<UMediaPlayer>(Package, *AssetName, RF_Public | RF_Standalone);
      if (MediaPlayer) {
        FAssetRegistryModule::AssetCreated(MediaPlayer);
        MediaPlayer->MarkPackageDirty();
        
        bool bAutoPlay = false;
        if (Payload->TryGetBoolField(TEXT("autoPlay"), bAutoPlay)) {
          MediaPlayer->PlayOnOpen = bAutoPlay;
        }
        
        McpSafeAssetSave(MediaPlayer);
        
        bSuccess = true;
        Message = TEXT("Media player created");
        Resp->SetStringField(TEXT("mediaPlayerPath"), FullPath);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create media player");
        ErrorCode = TEXT("CREATION_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_FAILED");
    }
  }
  // ========================================================================
  // CREATE FILE MEDIA SOURCE
  // ========================================================================
  else if (LowerSub == TEXT("create_file_media_source")) {
    FString AssetName;
    Payload->TryGetStringField(TEXT("assetName"), AssetName);
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    FString FilePath;
    Payload->TryGetStringField(TEXT("filePath"), FilePath);
    
    if (AssetName.IsEmpty()) {
      AssetName = TEXT("NewFileMediaSource");
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game");
    }
    
    // Normalize path
    SavePath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    if (!SavePath.StartsWith(TEXT("/Game"))) {
      SavePath = TEXT("/Game") / SavePath;
    }
    
    FString FullPath = SavePath / AssetName;
    UPackage* Package = CreatePackage(*FullPath);
    if (Package) {
      Package->FullyLoad();
      
      UFileMediaSource* MediaSource = NewObject<UFileMediaSource>(Package, *AssetName, RF_Public | RF_Standalone);
      if (MediaSource) {
        if (!FilePath.IsEmpty()) {
          MediaSource->SetFilePath(FilePath);
        }
        
        FAssetRegistryModule::AssetCreated(MediaSource);
        MediaSource->MarkPackageDirty();
        McpSafeAssetSave(MediaSource);
        
        bSuccess = true;
        Message = TEXT("File media source created");
        Resp->SetStringField(TEXT("mediaSourcePath"), FullPath);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create file media source");
        ErrorCode = TEXT("CREATION_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_FAILED");
    }
  }
  // ========================================================================
  // CREATE STREAM MEDIA SOURCE
  // ========================================================================
  else if (LowerSub == TEXT("create_stream_media_source")) {
    FString AssetName;
    Payload->TryGetStringField(TEXT("assetName"), AssetName);
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    FString Url;
    Payload->TryGetStringField(TEXT("url"), Url);
    
    if (AssetName.IsEmpty()) {
      AssetName = TEXT("NewStreamMediaSource");
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game");
    }
    
    // Normalize path
    SavePath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    if (!SavePath.StartsWith(TEXT("/Game"))) {
      SavePath = TEXT("/Game") / SavePath;
    }
    
    FString FullPath = SavePath / AssetName;
    UPackage* Package = CreatePackage(*FullPath);
    if (Package) {
      Package->FullyLoad();
      
      UStreamMediaSource* MediaSource = NewObject<UStreamMediaSource>(Package, *AssetName, RF_Public | RF_Standalone);
      if (MediaSource) {
        if (!Url.IsEmpty()) {
          MediaSource->StreamUrl = Url;
        }
        
        FAssetRegistryModule::AssetCreated(MediaSource);
        MediaSource->MarkPackageDirty();
        McpSafeAssetSave(MediaSource);
        
        bSuccess = true;
        Message = TEXT("Stream media source created");
        Resp->SetStringField(TEXT("mediaSourcePath"), FullPath);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create stream media source");
        ErrorCode = TEXT("CREATION_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_FAILED");
    }
  }
  // ========================================================================
  // CREATE MEDIA TEXTURE
  // ========================================================================
  else if (LowerSub == TEXT("create_media_texture")) {
    FString AssetName;
    Payload->TryGetStringField(TEXT("assetName"), AssetName);
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    
    if (AssetName.IsEmpty()) {
      AssetName = TEXT("NewMediaTexture");
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game");
    }
    
    // Normalize path
    SavePath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    if (!SavePath.StartsWith(TEXT("/Game"))) {
      SavePath = TEXT("/Game") / SavePath;
    }
    
    FString FullPath = SavePath / AssetName;
    UPackage* Package = CreatePackage(*FullPath);
    if (Package) {
      Package->FullyLoad();
      
      UMediaTexture* MediaTexture = NewObject<UMediaTexture>(Package, *AssetName, RF_Public | RF_Standalone);
      if (MediaTexture) {
        // Link to media player if provided
        if (!MediaPlayerPath.IsEmpty()) {
          UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
          if (MediaPlayer) {
            MediaTexture->SetMediaPlayer(MediaPlayer);
          }
        }
        
        // Configure texture properties
        bool bSrgb = true;
        if (Payload->TryGetBoolField(TEXT("srgb"), bSrgb)) {
          MediaTexture->SRGB = bSrgb;
        }
        
        FAssetRegistryModule::AssetCreated(MediaTexture);
        MediaTexture->MarkPackageDirty();
        McpSafeAssetSave(MediaTexture);
        
        bSuccess = true;
        Message = TEXT("Media texture created");
        Resp->SetStringField(TEXT("mediaTexturePath"), FullPath);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create media texture");
        ErrorCode = TEXT("CREATION_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_FAILED");
    }
  }
  // ========================================================================
  // CREATE MEDIA PLAYLIST
  // ========================================================================
  else if (LowerSub == TEXT("create_media_playlist")) {
    FString AssetName;
    Payload->TryGetStringField(TEXT("assetName"), AssetName);
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    
    if (AssetName.IsEmpty()) {
      AssetName = TEXT("NewMediaPlaylist");
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game");
    }
    
    // Normalize path
    SavePath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    if (!SavePath.StartsWith(TEXT("/Game"))) {
      SavePath = TEXT("/Game") / SavePath;
    }
    
    FString FullPath = SavePath / AssetName;
    UPackage* Package = CreatePackage(*FullPath);
    if (Package) {
      Package->FullyLoad();
      
      UMediaPlaylist* Playlist = NewObject<UMediaPlaylist>(Package, *AssetName, RF_Public | RF_Standalone);
      if (Playlist) {
        FAssetRegistryModule::AssetCreated(Playlist);
        Playlist->MarkPackageDirty();
        McpSafeAssetSave(Playlist);
        
        bSuccess = true;
        Message = TEXT("Media playlist created");
        Resp->SetStringField(TEXT("playlistPath"), FullPath);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create media playlist");
        ErrorCode = TEXT("CREATION_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_FAILED");
    }
  }
  // ========================================================================
  // GET MEDIA INFO
  // ========================================================================
  else if (LowerSub == TEXT("get_media_info")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
        
        InfoObj->SetNumberField(TEXT("duration"), MediaPlayer->GetDuration().GetTotalSeconds());
        InfoObj->SetBoolField(TEXT("isPlaying"), MediaPlayer->IsPlaying());
        InfoObj->SetBoolField(TEXT("isPaused"), MediaPlayer->IsPaused());
        InfoObj->SetBoolField(TEXT("isLooping"), MediaPlayer->IsLooping());
        InfoObj->SetBoolField(TEXT("isReady"), MediaPlayer->IsReady());
        InfoObj->SetNumberField(TEXT("currentTime"), MediaPlayer->GetTime().GetTotalSeconds());
        InfoObj->SetNumberField(TEXT("rate"), MediaPlayer->GetRate());
        
        // Get video track info if available
        int32 NumVideoTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Video);
        if (NumVideoTracks > 0) {
          InfoObj->SetBoolField(TEXT("hasVideo"), true);
          InfoObj->SetNumberField(TEXT("videoTrackCount"), NumVideoTracks);
        } else {
          InfoObj->SetBoolField(TEXT("hasVideo"), false);
        }
        
        // Get audio track info if available
        int32 NumAudioTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Audio);
        if (NumAudioTracks > 0) {
          InfoObj->SetBoolField(TEXT("hasAudio"), true);
          InfoObj->SetNumberField(TEXT("audioTrackCount"), NumAudioTracks);
        } else {
          InfoObj->SetBoolField(TEXT("hasAudio"), false);
        }
        
        Resp->SetObjectField(TEXT("mediaInfo"), InfoObj);
        bSuccess = true;
        Message = TEXT("Media info retrieved");
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // OPEN SOURCE
  // ========================================================================
  else if (LowerSub == TEXT("open_source")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    FString MediaSourcePath;
    Payload->TryGetStringField(TEXT("mediaSourcePath"), MediaSourcePath);
    
    if (MediaPlayerPath.IsEmpty() || MediaSourcePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath and mediaSourcePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      UMediaSource* MediaSource = LoadObject<UMediaSource>(nullptr, *MediaSourcePath);
      
      if (MediaPlayer && MediaSource) {
        if (MediaPlayer->OpenSource(MediaSource)) {
          bSuccess = true;
          Message = TEXT("Media source opened");
        } else {
          bSuccess = false;
          Message = TEXT("Failed to open media source");
          ErrorCode = TEXT("OPEN_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Media player or source not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // OPEN URL
  // ========================================================================
  else if (LowerSub == TEXT("open_url")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    FString Url;
    Payload->TryGetStringField(TEXT("url"), Url);
    
    if (MediaPlayerPath.IsEmpty() || Url.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath and url required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        if (MediaPlayer->OpenUrl(Url)) {
          bSuccess = true;
          Message = TEXT("URL opened");
        } else {
          bSuccess = false;
          Message = TEXT("Failed to open URL");
          ErrorCode = TEXT("OPEN_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // PLAY
  // ========================================================================
  else if (LowerSub == TEXT("play")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        if (MediaPlayer->Play()) {
          bSuccess = true;
          Message = TEXT("Playback started");
        } else {
          bSuccess = false;
          Message = TEXT("Failed to start playback");
          ErrorCode = TEXT("PLAY_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // PAUSE
  // ========================================================================
  else if (LowerSub == TEXT("pause")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        if (MediaPlayer->Pause()) {
          bSuccess = true;
          Message = TEXT("Playback paused");
        } else {
          bSuccess = false;
          Message = TEXT("Failed to pause playback");
          ErrorCode = TEXT("PAUSE_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // STOP
  // ========================================================================
  else if (LowerSub == TEXT("stop")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        MediaPlayer->Close();
        bSuccess = true;
        Message = TEXT("Playback stopped");
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // CLOSE
  // ========================================================================
  else if (LowerSub == TEXT("close")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        MediaPlayer->Close();
        bSuccess = true;
        Message = TEXT("Media closed");
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // SEEK
  // ========================================================================
  else if (LowerSub == TEXT("seek")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        FTimespan SeekTime = FTimespan::FromSeconds(Time);
        if (MediaPlayer->Seek(SeekTime)) {
          bSuccess = true;
          Message = FString::Printf(TEXT("Seeked to %.2f seconds"), Time);
          Resp->SetNumberField(TEXT("time"), Time);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to seek");
          ErrorCode = TEXT("SEEK_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // SET RATE
  // ========================================================================
  else if (LowerSub == TEXT("set_rate")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    double Rate = 1.0;
    Payload->TryGetNumberField(TEXT("rate"), Rate);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        if (MediaPlayer->SetRate(static_cast<float>(Rate))) {
          bSuccess = true;
          Message = FString::Printf(TEXT("Rate set to %.2f"), Rate);
          Resp->SetNumberField(TEXT("rate"), Rate);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to set rate");
          ErrorCode = TEXT("RATE_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // SET LOOPING
  // ========================================================================
  else if (LowerSub == TEXT("set_looping")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    bool bLooping = false;
    Payload->TryGetBoolField(TEXT("looping"), bLooping);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        MediaPlayer->SetLooping(bLooping);
        bSuccess = true;
        Message = bLooping ? TEXT("Looping enabled") : TEXT("Looping disabled");
        Resp->SetBoolField(TEXT("looping"), bLooping);
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET DURATION
  // ========================================================================
  else if (LowerSub == TEXT("get_duration")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        double Duration = MediaPlayer->GetDuration().GetTotalSeconds();
        bSuccess = true;
        Message = FString::Printf(TEXT("Duration: %.2f seconds"), Duration);
        Resp->SetNumberField(TEXT("duration"), Duration);
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET TIME
  // ========================================================================
  else if (LowerSub == TEXT("get_time")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        double CurrentTime = MediaPlayer->GetTime().GetTotalSeconds();
        bSuccess = true;
        Message = FString::Printf(TEXT("Current time: %.2f seconds"), CurrentTime);
        Resp->SetNumberField(TEXT("currentTime"), CurrentTime);
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET STATE
  // ========================================================================
  else if (LowerSub == TEXT("get_state")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    
    if (MediaPlayerPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      if (MediaPlayer) {
        TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
        
        FString State;
        if (!MediaPlayer->IsReady()) {
          State = TEXT("Closed");
        } else if (MediaPlayer->IsPlaying()) {
          State = TEXT("Playing");
        } else if (MediaPlayer->IsPaused()) {
          State = TEXT("Paused");
        } else {
          State = TEXT("Stopped");
        }
        
        StateObj->SetStringField(TEXT("state"), State);
        StateObj->SetNumberField(TEXT("currentTime"), MediaPlayer->GetTime().GetTotalSeconds());
        StateObj->SetNumberField(TEXT("duration"), MediaPlayer->GetDuration().GetTotalSeconds());
        StateObj->SetNumberField(TEXT("rate"), MediaPlayer->GetRate());
        StateObj->SetBoolField(TEXT("isLooping"), MediaPlayer->IsLooping());
        StateObj->SetBoolField(TEXT("isBuffering"), MediaPlayer->IsBuffering());
        
        Resp->SetObjectField(TEXT("playbackState"), StateObj);
        bSuccess = true;
        Message = FString::Printf(TEXT("State: %s"), *State);
      } else {
        bSuccess = false;
        Message = TEXT("Media player not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // ADD TO PLAYLIST
  // ========================================================================
  else if (LowerSub == TEXT("add_to_playlist")) {
    FString PlaylistPath;
    Payload->TryGetStringField(TEXT("playlistPath"), PlaylistPath);
    FString MediaSourcePath;
    Payload->TryGetStringField(TEXT("mediaSourcePath"), MediaSourcePath);
    
    if (PlaylistPath.IsEmpty() || MediaSourcePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("playlistPath and mediaSourcePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlaylist* Playlist = LoadObject<UMediaPlaylist>(nullptr, *PlaylistPath);
      UMediaSource* MediaSource = LoadObject<UMediaSource>(nullptr, *MediaSourcePath);
      
      if (Playlist && MediaSource) {
        Playlist->Add(MediaSource);
        Playlist->MarkPackageDirty();
        McpSafeAssetSave(Playlist);
        
        bSuccess = true;
        Message = TEXT("Source added to playlist");
        Resp->SetNumberField(TEXT("playlistLength"), Playlist->Num());
      } else {
        bSuccess = false;
        Message = TEXT("Playlist or media source not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET PLAYLIST
  // ========================================================================
  else if (LowerSub == TEXT("get_playlist")) {
    FString PlaylistPath;
    Payload->TryGetStringField(TEXT("playlistPath"), PlaylistPath);
    
    if (PlaylistPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("playlistPath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlaylist* Playlist = LoadObject<UMediaPlaylist>(nullptr, *PlaylistPath);
      if (Playlist) {
        TArray<TSharedPtr<FJsonValue>> SourcesArray;
        for (int32 i = 0; i < Playlist->Num(); i++) {
          UMediaSource* Source = Playlist->Get(i);
          if (Source) {
            SourcesArray.Add(MakeShared<FJsonValueString>(Source->GetPathName()));
          }
        }
        
        Resp->SetArrayField(TEXT("playlist"), SourcesArray);
        Resp->SetNumberField(TEXT("playlistLength"), SourcesArray.Num());
        bSuccess = true;
        Message = FString::Printf(TEXT("Playlist has %d items"), SourcesArray.Num());
      } else {
        bSuccess = false;
        Message = TEXT("Playlist not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // BIND TO TEXTURE
  // ========================================================================
  else if (LowerSub == TEXT("bind_to_texture")) {
    FString MediaPlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), MediaPlayerPath);
    FString MediaTexturePath;
    Payload->TryGetStringField(TEXT("mediaTexturePath"), MediaTexturePath);
    
    if (MediaPlayerPath.IsEmpty() || MediaTexturePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaPlayerPath and mediaTexturePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
      UMediaTexture* MediaTexture = LoadObject<UMediaTexture>(nullptr, *MediaTexturePath);
      
      if (MediaPlayer && MediaTexture) {
        MediaTexture->SetMediaPlayer(MediaPlayer);
        MediaTexture->MarkPackageDirty();
        McpSafeAssetSave(MediaTexture);
        
        bSuccess = true;
        Message = TEXT("Media player bound to texture");
      } else {
        bSuccess = false;
        Message = TEXT("Media player or texture not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // UNBIND FROM TEXTURE
  // ========================================================================
  else if (LowerSub == TEXT("unbind_from_texture")) {
    FString MediaTexturePath;
    Payload->TryGetStringField(TEXT("mediaTexturePath"), MediaTexturePath);
    
    if (MediaTexturePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("mediaTexturePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMediaTexture* MediaTexture = LoadObject<UMediaTexture>(nullptr, *MediaTexturePath);
      if (MediaTexture) {
        MediaTexture->SetMediaPlayer(nullptr);
        MediaTexture->MarkPackageDirty();
        McpSafeAssetSave(MediaTexture);
        
        bSuccess = true;
        Message = TEXT("Media player unbound from texture");
      } else {
        bSuccess = false;
        Message = TEXT("Media texture not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  else {
    bSuccess = false;
    Message = FString::Printf(TEXT("Media action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
  return true;

#else
  // Media Framework not available or not in editor
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Media actions require editor build with Media Framework enabled."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
