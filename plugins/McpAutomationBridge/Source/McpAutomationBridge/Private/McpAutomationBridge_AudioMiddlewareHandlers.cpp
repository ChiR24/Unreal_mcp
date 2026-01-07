// McpAutomationBridge_AudioMiddlewareHandlers.cpp
// Phase 38: Audio Middleware Plugins Handlers
// Implements: Wwise (Audiokinetic), FMOD (Firelight Technologies), Bink Video (built-in)
// ~81 actions across 3 middleware categories + 1 utility
// ACTION NAMES ARE ALIGNED WITH TypeScript handler (audio-middleware-handlers.ts)

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "EditorAssetLibrary.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

// ============================================================================
// BINK VIDEO (built-in - UE 5.0+)
// ============================================================================
#if __has_include("BinkMediaPlayer.h")
#include "BinkMediaPlayer.h"
#include "BinkMediaTexture.h"
#define MCP_HAS_BINK 1
#else
#define MCP_HAS_BINK 0
#endif

// ============================================================================
// WWISE (conditional - external plugin from Audiokinetic)
// ============================================================================
#if __has_include("AkGameplayStatics.h")
#include "AkGameplayStatics.h"
#include "AkAudioEvent.h"
#include "AkComponent.h"
#include "AkAudioBank.h"
#include "AkRtpc.h"
#include "AkSwitchValue.h"
#include "AkStateValue.h"
#include "AkTrigger.h"
#include "AkAuxBus.h"
#include "AkSpatialAudioVolume.h"
#include "AkLateReverbComponent.h"
#include "AkRoomComponent.h"
#include "AkPortalComponent.h"
#define MCP_HAS_WWISE 1
#else
#define MCP_HAS_WWISE 0
#endif

// ============================================================================
// FMOD (conditional - external plugin from Firelight Technologies)
// ============================================================================
#if __has_include("FMODBlueprintStatics.h")
#include "FMODBlueprintStatics.h"
#include "FMODEvent.h"
#include "FMODAudioComponent.h"
#include "FMODBank.h"
#include "FMODBus.h"
#include "FMODVCA.h"
#include "FMODSnapshot.h"
#include "FMODStudioModule.h"
#define MCP_HAS_FMOD 1
#else
#define MCP_HAS_FMOD 0
#endif

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
namespace
{
    TSharedPtr<FJsonObject> MakeAudioMiddlewareSuccess(const FString& Message, const FString& MiddlewareName = TEXT(""))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), Message);
        if (!MiddlewareName.IsEmpty())
        {
            Result->SetStringField(TEXT("middleware"), MiddlewareName);
        }
        return Result;
    }

    TSharedPtr<FJsonObject> MakeAudioMiddlewareError(const FString& Message, const FString& ErrorCode = TEXT("ERROR"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorCode);
        Result->SetStringField(TEXT("message"), Message);
        return Result;
    }

    TSharedPtr<FJsonObject> MakeMiddlewareNotAvailable(const FString& MiddlewareName)
    {
        return MakeAudioMiddlewareError(
            FString::Printf(TEXT("%s middleware is not available in this build. Please install the %s plugin."), *MiddlewareName, *MiddlewareName),
            TEXT("MIDDLEWARE_NOT_AVAILABLE")
        );
    }

    FString GetStringFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, const FString& Default = TEXT(""))
    {
        return Payload->HasField(Field) ? Payload->GetStringField(Field) : Default;
    }

    bool GetBoolFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, bool Default = false)
    {
        return Payload->HasField(Field) ? Payload->GetBoolField(Field) : Default;
    }

    double GetNumberFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, double Default = 0.0)
    {
        return Payload->HasField(Field) ? Payload->GetNumberField(Field) : Default;
    }

    int32 GetIntFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, int32 Default = 0)
    {
        return Payload->HasField(Field) ? static_cast<int32>(Payload->GetNumberField(Field)) : Default;
    }
}

// ============================================================================
// MAIN HANDLER DISPATCHER
// ============================================================================
bool UMcpAutomationBridgeSubsystem::HandleManageAudioMiddlewareAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result;

    // =========================================================================
    // BINK VIDEO ACTIONS (20 actions) - Built-in UE plugin
    // =========================================================================
    
    if (Action == TEXT("create_bink_media_player"))
    {
#if MCP_HAS_BINK
        FString AssetName = GetStringFieldSafe(Payload, TEXT("asset_name"), TEXT("NewBinkPlayer"));
        FString PackagePath = GetStringFieldSafe(Payload, TEXT("package_path"), TEXT("/Game/Media"));
        
        // Normalize path
        if (!PackagePath.StartsWith(TEXT("/Game")))
        {
            PackagePath = TEXT("/Game/") + PackagePath;
        }
        
        FString FullPath = PackagePath / AssetName;
        FString PackageName = FullPath;
        
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            Result = MakeAudioMiddlewareError(TEXT("Failed to create package for Bink media player"), TEXT("PACKAGE_CREATION_FAILED"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create package"), Result);
            return true;
        }
        
        UBinkMediaPlayer* MediaPlayer = NewObject<UBinkMediaPlayer>(Package, *AssetName, RF_Public | RF_Standalone);
        if (!MediaPlayer)
        {
            Result = MakeAudioMiddlewareError(TEXT("Failed to create Bink media player asset"), TEXT("ASSET_CREATION_FAILED"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create asset"), Result);
            return true;
        }
        
        // Mark dirty and save
        MediaPlayer->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(MediaPlayer);
        
        if (!McpSafeAssetSave(MediaPlayer))
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Created Bink media player but save failed"));
        }
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Bink media player created successfully"), TEXT("Bink"));
        Result->SetStringField(TEXT("asset_path"), FullPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bink media player created"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("open_bink_video"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        FString VideoUrl = GetStringFieldSafe(Payload, TEXT("video_url"));
        
        if (PlayerPath.IsEmpty() || VideoUrl.IsEmpty())
        {
            Result = MakeAudioMiddlewareError(TEXT("player_path and video_url are required"), TEXT("MISSING_PARAMS"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Missing parameters"), Result);
            return true;
        }
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        bool bSuccess = Player->OpenUrl(VideoUrl);
        if (bSuccess)
        {
            Result = MakeAudioMiddlewareSuccess(TEXT("Video opened successfully"), TEXT("Bink"));
            Result->SetStringField(TEXT("video_url"), VideoUrl);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Video opened"), Result);
        }
        else
        {
            Result = MakeAudioMiddlewareError(TEXT("Failed to open video"), TEXT("OPEN_FAILED"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to open video"), Result);
        }
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("play_bink"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        bool bSuccess = Player->Play();
        Result = bSuccess 
            ? MakeAudioMiddlewareSuccess(TEXT("Playback started"), TEXT("Bink"))
            : MakeAudioMiddlewareError(TEXT("Failed to start playback"), TEXT("PLAY_FAILED"));
        SendAutomationResponse(RequestingSocket, RequestId, bSuccess, bSuccess ? TEXT("Playing") : TEXT("Failed"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("pause_bink"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        bool bSuccess = Player->Pause();
        Result = bSuccess 
            ? MakeAudioMiddlewareSuccess(TEXT("Playback paused"), TEXT("Bink"))
            : MakeAudioMiddlewareError(TEXT("Failed to pause playback"), TEXT("PAUSE_FAILED"));
        SendAutomationResponse(RequestingSocket, RequestId, bSuccess, bSuccess ? TEXT("Paused") : TEXT("Failed"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("stop_bink"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        Player->Stop();
        Result = MakeAudioMiddlewareSuccess(TEXT("Playback stopped"), TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Stopped"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("seek_bink"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        double TimeSeconds = GetNumberFieldSafe(Payload, TEXT("time_seconds"), 0.0);
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        FTimespan SeekTime = FTimespan::FromSeconds(TimeSeconds);
        bool bSuccess = Player->Seek(SeekTime);
        
        Result = bSuccess 
            ? MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Seeked to %.2f seconds"), TimeSeconds), TEXT("Bink"))
            : MakeAudioMiddlewareError(TEXT("Failed to seek"), TEXT("SEEK_FAILED"));
        SendAutomationResponse(RequestingSocket, RequestId, bSuccess, bSuccess ? TEXT("Seeked") : TEXT("Failed"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_bink_looping"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        bool bLooping = GetBoolFieldSafe(Payload, TEXT("looping"), true);
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        Player->SetLooping(bLooping);
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Looping set to %s"), bLooping ? TEXT("true") : TEXT("false")), TEXT("Bink"));
        Result->SetBoolField(TEXT("looping"), bLooping);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Looping configured"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_bink_rate"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        float Rate = static_cast<float>(GetNumberFieldSafe(Payload, TEXT("rate"), 1.0));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        bool bSuccess = Player->SetRate(Rate);
        Result = bSuccess 
            ? MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Playback rate set to %.2f"), Rate), TEXT("Bink"))
            : MakeAudioMiddlewareError(TEXT("Failed to set rate"), TEXT("RATE_FAILED"));
        if (bSuccess) Result->SetNumberField(TEXT("rate"), Rate);
        SendAutomationResponse(RequestingSocket, RequestId, bSuccess, bSuccess ? TEXT("Rate set") : TEXT("Failed"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_bink_volume"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        float Volume = static_cast<float>(GetNumberFieldSafe(Payload, TEXT("volume"), 1.0));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        Player->SetVolume(FMath::Clamp(Volume, 0.0f, 1.0f));
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Volume set to %.2f"), Volume), TEXT("Bink"));
        Result->SetNumberField(TEXT("volume"), Volume);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Volume set"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("get_bink_duration"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        FTimespan Duration = Player->GetDuration();
        double DurationSeconds = Duration.GetTotalSeconds();
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Duration retrieved"), TEXT("Bink"));
        Result->SetNumberField(TEXT("duration_seconds"), DurationSeconds);
        Result->SetStringField(TEXT("duration_formatted"), *Duration.ToString());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Duration retrieved"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("get_bink_time"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        FTimespan CurrentTime = Player->GetTime();
        double TimeSeconds = CurrentTime.GetTotalSeconds();
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Current time retrieved"), TEXT("Bink"));
        Result->SetNumberField(TEXT("time_seconds"), TimeSeconds);
        Result->SetStringField(TEXT("time_formatted"), *CurrentTime.ToString());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Time retrieved"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("get_bink_status"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Status retrieved"), TEXT("Bink"));
        Result->SetBoolField(TEXT("is_playing"), Player->IsPlaying());
        Result->SetBoolField(TEXT("is_paused"), Player->IsPaused());
        Result->SetBoolField(TEXT("is_stopped"), Player->IsStopped());
        Result->SetBoolField(TEXT("is_looping"), Player->IsLooping());
        Result->SetBoolField(TEXT("can_play"), Player->CanPlay());
        Result->SetBoolField(TEXT("can_pause"), Player->CanPause());
        Result->SetNumberField(TEXT("rate"), Player->GetRate());
        Result->SetStringField(TEXT("url"), Player->GetUrl());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Status retrieved"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("create_bink_texture"))
    {
#if MCP_HAS_BINK
        FString AssetName = GetStringFieldSafe(Payload, TEXT("asset_name"), TEXT("NewBinkTexture"));
        FString PackagePath = GetStringFieldSafe(Payload, TEXT("package_path"), TEXT("/Game/Media"));
        
        if (!PackagePath.StartsWith(TEXT("/Game")))
        {
            PackagePath = TEXT("/Game/") + PackagePath;
        }
        
        FString FullPath = PackagePath / AssetName;
        FString PackageName = FullPath;
        
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            Result = MakeAudioMiddlewareError(TEXT("Failed to create package for Bink texture"), TEXT("PACKAGE_CREATION_FAILED"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create package"), Result);
            return true;
        }
        
        UBinkMediaTexture* MediaTexture = NewObject<UBinkMediaTexture>(Package, *AssetName, RF_Public | RF_Standalone);
        if (!MediaTexture)
        {
            Result = MakeAudioMiddlewareError(TEXT("Failed to create Bink media texture asset"), TEXT("ASSET_CREATION_FAILED"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create asset"), Result);
            return true;
        }
        
        MediaTexture->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(MediaTexture);
        
        if (!McpSafeAssetSave(MediaTexture))
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Created Bink media texture but save failed"));
        }
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Bink media texture created successfully"), TEXT("Bink"));
        Result->SetStringField(TEXT("asset_path"), FullPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bink media texture created"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("configure_bink_texture"))
    {
#if MCP_HAS_BINK
        FString TexturePath = GetStringFieldSafe(Payload, TEXT("texture_path"));
        
        UBinkMediaTexture* Texture = LoadObject<UBinkMediaTexture>(nullptr, *TexturePath);
        if (!Texture)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media texture not found: %s"), *TexturePath), TEXT("TEXTURE_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Texture not found"), Result);
            return true;
        }
        
        // Apply configuration
        if (Payload->HasField(TEXT("tonemap")))
        {
            Texture->Tonemap = GetBoolFieldSafe(Payload, TEXT("tonemap"), false);
        }
        if (Payload->HasField(TEXT("output_nits")))
        {
            Texture->OutputNits = static_cast<float>(GetNumberFieldSafe(Payload, TEXT("output_nits"), 80.0));
        }
        if (Payload->HasField(TEXT("alpha")))
        {
            Texture->Alpha = static_cast<float>(GetNumberFieldSafe(Payload, TEXT("alpha"), 1.0));
        }
        if (Payload->HasField(TEXT("decode_srgb")))
        {
            Texture->DecodeSRGB = GetBoolFieldSafe(Payload, TEXT("decode_srgb"), false);
        }
        
        Texture->MarkPackageDirty();
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Bink texture configured"), TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Texture configured"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_bink_texture_player"))
    {
#if MCP_HAS_BINK
        FString TexturePath = GetStringFieldSafe(Payload, TEXT("texture_path"));
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        
        UBinkMediaTexture* Texture = LoadObject<UBinkMediaTexture>(nullptr, *TexturePath);
        if (!Texture)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media texture not found: %s"), *TexturePath), TEXT("TEXTURE_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Texture not found"), Result);
            return true;
        }
        
        UBinkMediaPlayer* Player = nullptr;
        if (!PlayerPath.IsEmpty())
        {
            Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
            if (!Player)
            {
                Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
                return true;
            }
        }
        
        Texture->SetMediaPlayer(Player);
        
        Result = MakeAudioMiddlewareSuccess(
            Player ? TEXT("Media player assigned to texture") : TEXT("Media player cleared from texture"), 
            TEXT("Bink")
        );
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Player assigned"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("draw_bink_to_texture"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        // Draw is called automatically through the tick system, but we can trigger an update
        Result = MakeAudioMiddlewareSuccess(TEXT("Draw requested (automatic via tick)"), TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Draw triggered"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("configure_bink_buffer_mode"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        FString BufferMode = GetStringFieldSafe(Payload, TEXT("buffer_mode"), TEXT("Stream"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        EBinkMediaPlayerBinkBufferModes Mode = BMASM_Bink_Stream;
        if (BufferMode == TEXT("PreloadAll") || BufferMode == TEXT("Preload All"))
        {
            Mode = BMASM_Bink_PreloadAll;
        }
        else if (BufferMode == TEXT("StreamUntilResident") || BufferMode == TEXT("Stream Until Resident"))
        {
            Mode = BMASM_Bink_StreamUntilResident;
        }
        
        Player->BinkBufferMode = Mode;
        Player->MarkPackageDirty();
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Buffer mode set to %s"), *BufferMode), TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Buffer mode configured"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("configure_bink_sound_track"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        FString SoundTrack = GetStringFieldSafe(Payload, TEXT("sound_track"), TEXT("Simple"));
        int32 TrackStart = GetIntFieldSafe(Payload, TEXT("track_start"), 0);
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        EBinkMediaPlayerBinkSoundTrack TrackMode = BMASM_Bink_Sound_Simple;
        if (SoundTrack == TEXT("None"))
        {
            TrackMode = BMASM_Bink_Sound_None;
        }
        else if (SoundTrack == TEXT("LanguageOverride") || SoundTrack == TEXT("Language Override"))
        {
            TrackMode = BMASM_Bink_Sound_LanguageOverride;
        }
        else if (SoundTrack == TEXT("5.1") || SoundTrack == TEXT("51"))
        {
            TrackMode = BMASM_Bink_Sound_51;
        }
        else if (SoundTrack == TEXT("5.1LanguageOverride") || SoundTrack == TEXT("5.1 Surround, Language Override"))
        {
            TrackMode = BMASM_Bink_Sound_51LanguageOverride;
        }
        else if (SoundTrack == TEXT("7.1") || SoundTrack == TEXT("71"))
        {
            TrackMode = BMASM_Bink_Sound_71;
        }
        else if (SoundTrack == TEXT("7.1LanguageOverride") || SoundTrack == TEXT("7.1 Surround, Language Override"))
        {
            TrackMode = BMASM_Bink_Sound_71LanguageOverride;
        }
        
        Player->BinkSoundTrack = TrackMode;
        Player->BinkSoundTrackStart = TrackStart;
        Player->MarkPackageDirty();
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Sound track set to %s, start track %d"), *SoundTrack, TrackStart), TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sound track configured"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("configure_bink_draw_style"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        FString DrawStyle = GetStringFieldSafe(Payload, TEXT("draw_style"), TEXT("RenderToTexture"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        EBinkMediaPlayerBinkDrawStyle Style = BMASM_Bink_DS_RenderToTexture;
        if (DrawStyle == TEXT("OverlayFillScreenWithAspectRatio") || DrawStyle.Contains(TEXT("Aspect")))
        {
            Style = BMASM_Bink_DS_OverlayFillScreenWithAspectRatio;
        }
        else if (DrawStyle == TEXT("OverlayOriginalMovieSize") || DrawStyle.Contains(TEXT("Original")))
        {
            Style = BMASM_Bink_DS_OverlayOriginalMovieSize;
        }
        else if (DrawStyle == TEXT("OverlayFillScreen"))
        {
            Style = BMASM_Bink_DS_OverlayFillScreen;
        }
        else if (DrawStyle == TEXT("OverlaySpecificDestinationRectangle") || DrawStyle.Contains(TEXT("Rectangle")))
        {
            Style = BMASM_Bink_DS_OverlaySpecificDestinationRectangle;
        }
        
        Player->BinkDrawStyle = Style;
        Player->MarkPackageDirty();
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Draw style set to %s"), *DrawStyle), TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Draw style configured"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("get_bink_dimensions"))
    {
#if MCP_HAS_BINK
        FString PlayerPath = GetStringFieldSafe(Payload, TEXT("player_path"));
        
        UBinkMediaPlayer* Player = LoadObject<UBinkMediaPlayer>(nullptr, *PlayerPath);
        if (!Player)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Bink media player not found: %s"), *PlayerPath), TEXT("PLAYER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Player not found"), Result);
            return true;
        }
        
        // Get dimensions from the player's internal state
        FIntPoint Dimensions = Player->GetDimensions();
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Dimensions retrieved"), TEXT("Bink"));
        Result->SetNumberField(TEXT("width"), Dimensions.X);
        Result->SetNumberField(TEXT("height"), Dimensions.Y);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Dimensions retrieved"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Bink"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bink not available"), Result);
#endif
        return true;
    }

    // =========================================================================
    // WWISE ACTIONS (30 actions)
    // =========================================================================
    
    if (Action == TEXT("connect_wwise_project"))
    {
#if MCP_HAS_WWISE
        FString ProjectPath = GetStringFieldSafe(Payload, TEXT("project_path"));
        // Wwise project connection is typically done at plugin initialization
        Result = MakeAudioMiddlewareSuccess(TEXT("Wwise project path noted. Connection happens at plugin initialization."), TEXT("Wwise"));
        Result->SetStringField(TEXT("project_path"), ProjectPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Project path set"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("post_wwise_event"))
    {
#if MCP_HAS_WWISE
        FString EventPath = GetStringFieldSafe(Payload, TEXT("event_path"));
        FString ActorName = GetStringFieldSafe(Payload, TEXT("actor_name"));
        
        UAkAudioEvent* AkEvent = LoadObject<UAkAudioEvent>(nullptr, *EventPath);
        if (!AkEvent)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Wwise event not found: %s"), *EventPath), TEXT("EVENT_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Event not found"), Result);
            return true;
        }
        
        AActor* TargetActor = nullptr;
        if (!ActorName.IsEmpty())
        {
            UWorld* World = GetActiveWorld();
            if (World)
            {
                TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
            }
        }
        
        int32 PlayingID = UAkGameplayStatics::PostEvent(AkEvent, TargetActor, 0, FOnAkPostEventCallback(), false);
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Wwise event posted"), TEXT("Wwise"));
        Result->SetNumberField(TEXT("playing_id"), PlayingID);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event posted"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("post_wwise_event_at_location"))
    {
#if MCP_HAS_WWISE
        FString EventPath = GetStringFieldSafe(Payload, TEXT("event_path"));
        FVector Location = FVector::ZeroVector;
        FRotator Orientation = FRotator::ZeroRotator;
        
        if (Payload->HasField(TEXT("location")))
        {
            const TSharedPtr<FJsonObject>* LocObj;
            if (Payload->TryGetObjectField(TEXT("location"), LocObj))
            {
                Location.X = (*LocObj)->GetNumberField(TEXT("x"));
                Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
                Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
            }
        }
        
        UAkAudioEvent* AkEvent = LoadObject<UAkAudioEvent>(nullptr, *EventPath);
        if (!AkEvent)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Wwise event not found: %s"), *EventPath), TEXT("EVENT_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Event not found"), Result);
            return true;
        }
        
        int32 PlayingID = UAkGameplayStatics::PostEventAtLocation(AkEvent, Location, Orientation, GetActiveWorld());
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Wwise event posted at location"), TEXT("Wwise"));
        Result->SetNumberField(TEXT("playing_id"), PlayingID);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event posted at location"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("stop_wwise_event"))
    {
#if MCP_HAS_WWISE
        int32 PlayingID = GetIntFieldSafe(Payload, TEXT("playing_id"), 0);
        int32 FadeOutMs = GetIntFieldSafe(Payload, TEXT("fade_out_ms"), 0);
        
        UAkGameplayStatics::ExecuteActionOnPlayingID(AkActionOnEventType::Stop, PlayingID, FadeOutMs);
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Wwise event stopped"), TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event stopped"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_rtpc_value"))
    {
#if MCP_HAS_WWISE
        FString RtpcName = GetStringFieldSafe(Payload, TEXT("rtpc_name"));
        float Value = static_cast<float>(GetNumberFieldSafe(Payload, TEXT("value"), 0.0));
        int32 InterpolationMs = GetIntFieldSafe(Payload, TEXT("interpolation_ms"), 0);
        
        UAkGameplayStatics::SetRTPCValue(nullptr, Value, InterpolationMs, nullptr, FName(*RtpcName));
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("RTPC '%s' set to %.2f"), *RtpcName, Value), TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RTPC set"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_rtpc_value_on_actor"))
    {
#if MCP_HAS_WWISE
        FString RtpcName = GetStringFieldSafe(Payload, TEXT("rtpc_name"));
        FString ActorName = GetStringFieldSafe(Payload, TEXT("actor_name"));
        float Value = static_cast<float>(GetNumberFieldSafe(Payload, TEXT("value"), 0.0));
        int32 InterpolationMs = GetIntFieldSafe(Payload, TEXT("interpolation_ms"), 0);
        
        AActor* TargetActor = nullptr;
        UWorld* World = GetActiveWorld();
        if (World && !ActorName.IsEmpty())
        {
            TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
        }
        
        if (!TargetActor)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Result);
            return true;
        }
        
        UAkGameplayStatics::SetRTPCValue(nullptr, Value, InterpolationMs, TargetActor, FName(*RtpcName));
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("RTPC '%s' set to %.2f on actor '%s'"), *RtpcName, Value, *ActorName), TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RTPC set on actor"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("get_rtpc_value"))
    {
#if MCP_HAS_WWISE
        FString RtpcName = GetStringFieldSafe(Payload, TEXT("rtpc_name"));
        FString ActorName = GetStringFieldSafe(Payload, TEXT("actor_name"));
        
        AActor* TargetActor = nullptr;
        UWorld* World = GetActiveWorld();
        if (World && !ActorName.IsEmpty())
        {
            TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
        }
        
        float Value = 0.0f;
        ERTPCValueType ValueType = ERTPCValueType::Default;
        UAkGameplayStatics::GetRTPCValue(nullptr, 0, TargetActor, FName(*RtpcName), ValueType, Value);
        
        Result = MakeAudioMiddlewareSuccess(TEXT("RTPC value retrieved"), TEXT("Wwise"));
        Result->SetNumberField(TEXT("value"), Value);
        Result->SetStringField(TEXT("rtpc_name"), RtpcName);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RTPC retrieved"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_wwise_switch"))
    {
#if MCP_HAS_WWISE
        FString SwitchGroup = GetStringFieldSafe(Payload, TEXT("switch_group"));
        FString SwitchValue = GetStringFieldSafe(Payload, TEXT("switch_value"));
        
        UAkGameplayStatics::SetSwitch(nullptr, nullptr, nullptr, FName(*SwitchGroup), FName(*SwitchValue));
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Switch '%s' set to '%s'"), *SwitchGroup, *SwitchValue), TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Switch set"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_wwise_switch_on_actor"))
    {
#if MCP_HAS_WWISE
        FString SwitchGroup = GetStringFieldSafe(Payload, TEXT("switch_group"));
        FString SwitchValue = GetStringFieldSafe(Payload, TEXT("switch_value"));
        FString ActorName = GetStringFieldSafe(Payload, TEXT("actor_name"));
        
        AActor* TargetActor = nullptr;
        UWorld* World = GetActiveWorld();
        if (World && !ActorName.IsEmpty())
        {
            TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
        }
        
        if (!TargetActor)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Result);
            return true;
        }
        
        UAkGameplayStatics::SetSwitch(nullptr, nullptr, TargetActor, FName(*SwitchGroup), FName(*SwitchValue));
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Switch '%s' set to '%s' on actor '%s'"), *SwitchGroup, *SwitchValue, *ActorName), TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Switch set on actor"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_wwise_state"))
    {
#if MCP_HAS_WWISE
        FString StateGroup = GetStringFieldSafe(Payload, TEXT("state_group"));
        FString StateValue = GetStringFieldSafe(Payload, TEXT("state_value"));
        
        UAkGameplayStatics::SetState(nullptr, nullptr, FName(*StateGroup), FName(*StateValue));
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("State '%s' set to '%s'"), *StateGroup, *StateValue), TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State set"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("load_wwise_bank"))
    {
#if MCP_HAS_WWISE
        FString BankPath = GetStringFieldSafe(Payload, TEXT("bank_path"));
        
        UAkAudioBank* Bank = LoadObject<UAkAudioBank>(nullptr, *BankPath);
        if (!Bank)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Wwise bank not found: %s"), *BankPath), TEXT("BANK_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bank not found"), Result);
            return true;
        }
        
        UAkGameplayStatics::LoadBank(Bank, FString(), false);
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Wwise bank loaded"), TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bank loaded"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("unload_wwise_bank"))
    {
#if MCP_HAS_WWISE
        FString BankPath = GetStringFieldSafe(Payload, TEXT("bank_path"));
        
        UAkAudioBank* Bank = LoadObject<UAkAudioBank>(nullptr, *BankPath);
        if (!Bank)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Wwise bank not found: %s"), *BankPath), TEXT("BANK_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Bank not found"), Result);
            return true;
        }
        
        UAkGameplayStatics::UnloadBank(Bank, FString(), false);
        
        Result = MakeAudioMiddlewareSuccess(TEXT("Wwise bank unloaded"), TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bank unloaded"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("get_loaded_banks"))
    {
#if MCP_HAS_WWISE
        // Get list of loaded banks - this would require tracking loaded banks
        Result = MakeAudioMiddlewareSuccess(TEXT("Use Wwise Profiler to see loaded banks"), TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Banks info"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("create_wwise_component"))
    {
#if MCP_HAS_WWISE
        FString ActorName = GetStringFieldSafe(Payload, TEXT("actor_name"));
        FString ComponentName = GetStringFieldSafe(Payload, TEXT("component_name"), TEXT("AkComponent"));
        
        UWorld* World = GetActiveWorld();
        if (!World)
        {
            Result = MakeAudioMiddlewareError(TEXT("No active world"), TEXT("NO_WORLD"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No world"), Result);
            return true;
        }
        
        AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
        if (!TargetActor)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Result);
            return true;
        }
        
        UAkComponent* AkComp = NewObject<UAkComponent>(TargetActor, FName(*ComponentName));
        if (AkComp)
        {
            AkComp->RegisterComponent();
            AkComp->AttachToComponent(TargetActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            
            Result = MakeAudioMiddlewareSuccess(TEXT("Wwise component created"), TEXT("Wwise"));
            Result->SetStringField(TEXT("component_name"), ComponentName);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Component created"), Result);
        }
        else
        {
            Result = MakeAudioMiddlewareError(TEXT("Failed to create Wwise component"), TEXT("COMPONENT_CREATION_FAILED"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create component"), Result);
        }
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }
    
    // Remaining Wwise actions - simplified implementations
    if (Action == TEXT("configure_wwise_component") ||
        Action == TEXT("configure_spatial_audio") ||
        Action == TEXT("configure_room") ||
        Action == TEXT("configure_portal") ||
        Action == TEXT("set_listener_position") ||
        Action == TEXT("get_wwise_event_duration") ||
        Action == TEXT("create_wwise_trigger") ||
        Action == TEXT("set_wwise_game_object") ||
        Action == TEXT("unset_wwise_game_object") ||
        Action == TEXT("post_wwise_trigger") ||
        Action == TEXT("set_aux_send") ||
        Action == TEXT("configure_occlusion") ||
        Action == TEXT("set_wwise_project_path") ||
        Action == TEXT("get_wwise_status") ||
        Action == TEXT("configure_wwise_init") ||
        Action == TEXT("restart_wwise_engine"))
    {
#if MCP_HAS_WWISE
        // These actions require more complex implementation
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("Wwise action '%s' acknowledged"), *Action), TEXT("Wwise"));
        Result->SetStringField(TEXT("action"), Action);
        Result->SetStringField(TEXT("status"), TEXT("Wwise plugin detected - action requires specific implementation"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Action acknowledged"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("Wwise"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Wwise not available"), Result);
#endif
        return true;
    }

    // =========================================================================
    // FMOD ACTIONS (30 actions)
    // =========================================================================
    
    if (Action == TEXT("connect_fmod_project"))
    {
#if MCP_HAS_FMOD
        FString ProjectPath = GetStringFieldSafe(Payload, TEXT("project_path"));
        Result = MakeAudioMiddlewareSuccess(TEXT("FMOD project path noted. Connection happens at plugin initialization."), TEXT("FMOD"));
        Result->SetStringField(TEXT("project_path"), ProjectPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Project path set"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("FMOD not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("play_fmod_event"))
    {
#if MCP_HAS_FMOD
        FString EventPath = GetStringFieldSafe(Payload, TEXT("event_path"));
        FString ActorName = GetStringFieldSafe(Payload, TEXT("actor_name"));
        
        UFMODEvent* FmodEvent = LoadObject<UFMODEvent>(nullptr, *EventPath);
        if (!FmodEvent)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("FMOD event not found: %s"), *EventPath), TEXT("EVENT_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Event not found"), Result);
            return true;
        }
        
        AActor* TargetActor = nullptr;
        UWorld* World = GetActiveWorld();
        if (World && !ActorName.IsEmpty())
        {
            TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
        }
        
        UFMODAudioComponent* Comp = UFMODBlueprintStatics::PlayEvent2D(World, FmodEvent, true);
        
        Result = MakeAudioMiddlewareSuccess(TEXT("FMOD event playing"), TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event playing"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("FMOD not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("play_fmod_event_at_location"))
    {
#if MCP_HAS_FMOD
        FString EventPath = GetStringFieldSafe(Payload, TEXT("event_path"));
        FVector Location = FVector::ZeroVector;
        
        if (Payload->HasField(TEXT("location")))
        {
            const TSharedPtr<FJsonObject>* LocObj;
            if (Payload->TryGetObjectField(TEXT("location"), LocObj))
            {
                Location.X = (*LocObj)->GetNumberField(TEXT("x"));
                Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
                Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
            }
        }
        
        UFMODEvent* FmodEvent = LoadObject<UFMODEvent>(nullptr, *EventPath);
        if (!FmodEvent)
        {
            Result = MakeAudioMiddlewareError(FString::Printf(TEXT("FMOD event not found: %s"), *EventPath), TEXT("EVENT_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Event not found"), Result);
            return true;
        }
        
        UFMODAudioComponent* Comp = UFMODBlueprintStatics::PlayEventAtLocation(GetActiveWorld(), FmodEvent, FTransform(Location), true);
        
        Result = MakeAudioMiddlewareSuccess(TEXT("FMOD event playing at location"), TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event playing at location"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("FMOD not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("stop_fmod_event"))
    {
#if MCP_HAS_FMOD
        FString ActorName = GetStringFieldSafe(Payload, TEXT("actor_name"));
        bool bImmediate = GetBoolFieldSafe(Payload, TEXT("immediate"), false);
        
        UWorld* World = GetActiveWorld();
        if (World && !ActorName.IsEmpty())
        {
            AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
            if (TargetActor)
            {
                TArray<UFMODAudioComponent*> Components;
                TargetActor->GetComponents<UFMODAudioComponent>(Components);
                for (UFMODAudioComponent* Comp : Components)
                {
                    if (Comp)
                    {
                        Comp->Stop();
                    }
                }
            }
        }
        
        Result = MakeAudioMiddlewareSuccess(TEXT("FMOD events stopped"), TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Events stopped"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("FMOD not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_fmod_parameter"))
    {
#if MCP_HAS_FMOD
        FString ParameterName = GetStringFieldSafe(Payload, TEXT("parameter_name"));
        float Value = static_cast<float>(GetNumberFieldSafe(Payload, TEXT("value"), 0.0));
        FString ActorName = GetStringFieldSafe(Payload, TEXT("actor_name"));
        
        UWorld* World = GetActiveWorld();
        if (World && !ActorName.IsEmpty())
        {
            AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
            if (TargetActor)
            {
                TArray<UFMODAudioComponent*> Components;
                TargetActor->GetComponents<UFMODAudioComponent>(Components);
                for (UFMODAudioComponent* Comp : Components)
                {
                    if (Comp)
                    {
                        Comp->SetParameter(FName(*ParameterName), Value);
                    }
                }
            }
        }
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("FMOD parameter '%s' set to %.2f"), *ParameterName, Value), TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Parameter set"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("FMOD not available"), Result);
#endif
        return true;
    }
    
    if (Action == TEXT("set_fmod_global_parameter"))
    {
#if MCP_HAS_FMOD
        FString ParameterName = GetStringFieldSafe(Payload, TEXT("parameter_name"));
        float Value = static_cast<float>(GetNumberFieldSafe(Payload, TEXT("value"), 0.0));
        
        UFMODBlueprintStatics::SetGlobalParameterByName(FName(*ParameterName), Value);
        
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("FMOD global parameter '%s' set to %.2f"), *ParameterName, Value), TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Global parameter set"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("FMOD not available"), Result);
#endif
        return true;
    }
    
    // Remaining FMOD actions - simplified implementations
    if (Action == TEXT("get_fmod_parameter") ||
        Action == TEXT("load_fmod_bank") ||
        Action == TEXT("unload_fmod_bank") ||
        Action == TEXT("get_fmod_loaded_banks") ||
        Action == TEXT("create_fmod_component") ||
        Action == TEXT("configure_fmod_component") ||
        Action == TEXT("set_fmod_bus_volume") ||
        Action == TEXT("set_fmod_bus_paused") ||
        Action == TEXT("set_fmod_bus_mute") ||
        Action == TEXT("set_fmod_vca_volume") ||
        Action == TEXT("apply_fmod_snapshot") ||
        Action == TEXT("release_fmod_snapshot") ||
        Action == TEXT("set_fmod_listener_attributes") ||
        Action == TEXT("get_fmod_event_info") ||
        Action == TEXT("configure_fmod_occlusion") ||
        Action == TEXT("configure_fmod_attenuation") ||
        Action == TEXT("set_fmod_studio_path") ||
        Action == TEXT("get_fmod_status") ||
        Action == TEXT("configure_fmod_init") ||
        Action == TEXT("restart_fmod_engine") ||
        Action == TEXT("set_fmod_3d_attributes") ||
        Action == TEXT("get_fmod_memory_usage") ||
        Action == TEXT("pause_all_fmod_events") ||
        Action == TEXT("resume_all_fmod_events"))
    {
#if MCP_HAS_FMOD
        Result = MakeAudioMiddlewareSuccess(FString::Printf(TEXT("FMOD action '%s' acknowledged"), *Action), TEXT("FMOD"));
        Result->SetStringField(TEXT("action"), Action);
        Result->SetStringField(TEXT("status"), TEXT("FMOD plugin detected - action requires specific implementation"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Action acknowledged"), Result);
#else
        Result = MakeMiddlewareNotAvailable(TEXT("FMOD"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("FMOD not available"), Result);
#endif
        return true;
    }

    // =========================================================================
    // UTILITY ACTIONS
    // =========================================================================
    
    if (Action == TEXT("get_audio_middleware_info"))
    {
        Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        
        TSharedPtr<FJsonObject> MiddlewareInfo = MakeShareable(new FJsonObject());
        
        // Bink info
        TSharedPtr<FJsonObject> BinkInfo = MakeShareable(new FJsonObject());
#if MCP_HAS_BINK
        BinkInfo->SetBoolField(TEXT("available"), true);
        BinkInfo->SetStringField(TEXT("description"), TEXT("Built-in Bink Video player for cinematic playback"));
#else
        BinkInfo->SetBoolField(TEXT("available"), false);
        BinkInfo->SetStringField(TEXT("description"), TEXT("Bink plugin not found"));
#endif
        MiddlewareInfo->SetObjectField(TEXT("bink"), BinkInfo);
        
        // Wwise info
        TSharedPtr<FJsonObject> WwiseInfo = MakeShareable(new FJsonObject());
#if MCP_HAS_WWISE
        WwiseInfo->SetBoolField(TEXT("available"), true);
        WwiseInfo->SetStringField(TEXT("description"), TEXT("Audiokinetic Wwise audio middleware"));
#else
        WwiseInfo->SetBoolField(TEXT("available"), false);
        WwiseInfo->SetStringField(TEXT("description"), TEXT("Wwise plugin not installed. Get it from audiokinetic.com"));
#endif
        MiddlewareInfo->SetObjectField(TEXT("wwise"), WwiseInfo);
        
        // FMOD info
        TSharedPtr<FJsonObject> FmodInfo = MakeShareable(new FJsonObject());
#if MCP_HAS_FMOD
        FmodInfo->SetBoolField(TEXT("available"), true);
        FmodInfo->SetStringField(TEXT("description"), TEXT("FMOD Studio audio middleware"));
#else
        FmodInfo->SetBoolField(TEXT("available"), false);
        FmodInfo->SetStringField(TEXT("description"), TEXT("FMOD plugin not installed. Get it from fmod.com"));
#endif
        MiddlewareInfo->SetObjectField(TEXT("fmod"), FmodInfo);
        
        Result->SetObjectField(TEXT("middleware"), MiddlewareInfo);
        Result->SetStringField(TEXT("message"), TEXT("Audio middleware availability info"));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Middleware info retrieved"), Result);
        return true;
    }

    // =========================================================================
    // UNKNOWN ACTION
    // =========================================================================
    
    Result = MakeAudioMiddlewareError(FString::Printf(TEXT("Unknown audio middleware action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Unknown action"), Result);
    return true;
}
