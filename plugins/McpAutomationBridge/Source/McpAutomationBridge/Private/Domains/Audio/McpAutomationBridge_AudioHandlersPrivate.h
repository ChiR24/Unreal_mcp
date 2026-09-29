#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Components/BrushComponent.h"
#include "Components/SceneComponent.h"
#include "EditorAssetLibrary.h"
#include "Factories/SoundAttenuationFactory.h"
#include "Factories/SoundClassFactory.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Factories/SoundMixFactory.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/AmbientSound.h"
#include "Sound/AudioVolume.h"
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UObject/UObjectHash.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMcpAudioHandlers, Log, All);

namespace McpAudioHandlers
{
bool BuildSanitizedAssetPath(
    const FString& InDirectory,
    const FString& AssetName,
    FString& OutDirectory,
    FString& OutFullPath);
AActor* FindAudioActorByName(const FString& ActorName, UWorld* World);
USceneComponent* EnsureAudioAttachRoot(AActor* Actor);
UAudioComponent* CreateRegisteredAudioComponent(
    AActor* Owner,
    USoundBase* Sound,
    const FVector& Location,
    const FRotator& Rotation);
UAudioComponent* CreateAudioComponentAtEditorLocation(
    UWorld* World,
    USoundBase* Sound,
    const FVector& Location,
    const FRotator& Rotation,
    const FString& ActorName);
// "/Game/A/B" names the asset B inside package /Game/A/B; an object path passes through.
FString AudioObjectPath(const FString& Path);
// componentName (a rename when free), volume and pitch, each only when sent.
void ApplyAudioComponentOptions(UAudioComponent* AudioComp, const TSharedPtr<FJsonObject>& Payload);
// attenuationPath and concurrencyPath, each optional; false with OutError when one is sent and does not load.
bool LoadOptionalAudioSettings(const TSharedPtr<FJsonObject>& Payload, USoundAttenuation*& OutAttenuation, USoundConcurrency*& OutConcurrency, FString& OutError);
// The 2D sounds play_sound started; stop_sound, pause_sound and resume_sound act on them.
TArray<TWeakObjectPtr<UAudioComponent>>& McpPreviewSounds();
USoundBase* ResolveSoundAsset(const FString& SoundPath);
USoundMix* ResolveSoundMix(const FString& MixPath);
USoundClass* ResolveSoundClass(const FString& ClassPath);

using FAudioActionHandler = bool (*)(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandlePlaybackActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandlePauseActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleAmbientActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleMixActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleComponentActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSpatialActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleFadeAndReverbActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
