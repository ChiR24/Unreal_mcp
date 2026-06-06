#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
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
#include "Sound/SoundCue.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UObject/UObjectHash.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogMcpAudioHandlers, Log, All);

namespace McpAudioHandlers
{
#if WITH_EDITOR
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
USoundBase* ResolveSoundAsset(const FString& SoundPath);
USoundMix* ResolveSoundMix(const FString& MixPath);
USoundClass* ResolveSoundClass(const FString& ClassPath);

using FAudioActionHandler = bool (*)(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleAssetActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandlePlaybackActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleAmbientActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleMixActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleComponentActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleAnalysisActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSpatialActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleFadeAndReverbActions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
#endif
}
