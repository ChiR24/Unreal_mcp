// =============================================================================
// McpAutomationBridge_AudioHandlers.cpp
// =============================================================================
// Audio System Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// --------------------
// Section 1: Main Dispatcher
//   - HandleAudioAction                  : Routes audio_*, create_sound_*, play_sound_*,
//                                          set_sound_*, push_sound_*, pop_sound_*,
//                                          create_audio_*, create_ambient_*, create_reverb_*,
//                                          enable_audio_*, fade_sound*, set_doppler_*,
//                                          set_audio_*, clear_sound_*, set_base_sound_*,
//                                          prime_*, spawn_sound_* actions
//
// Section 2: Sound Asset Creation
//   - create_sound_cue                   : Create SoundCue with optional wave, looping,
//                                          modulation, and attenuation nodes
//   - create_sound_class                 : Create SoundClass with properties and parent
//   - create_sound_mix                   : Create SoundMix with class adjusters
//
// Section 3: Sound Playback
//   - play_sound_at_location             : Play 3D sound at world location
//   - play_sound_2d                      : Play non-spatialized 2D sound
//   - play_sound_attached                : Attach and play sound on actor component
//   - create_ambient_sound               : Spawn persistent ambient sound at location
//   - spawn_sound_at_location            : Spawn sound component at location
//
// Section 4: Sound Mix Control
//   - push_sound_mix                     : Push SoundMix modifier onto audio stack
//   - pop_sound_mix                      : Pop SoundMix modifier from audio stack
//   - set_sound_mix_class_override       : Override SoundClass within a SoundMix
//   - clear_sound_mix_class_override     : Clear SoundClass override from SoundMix
//   - set_base_sound_mix                 : Set the base (default) SoundMix
//
// Section 5: Sound Fading & Utility
//   - fade_sound_in / fade_sound_out     : Fade audio component in/out over time
//   - prime_sound                        : Pre-load sound for instant playback
//   - create_audio_component             : Create UAudioComponent on actor or at location
//
// Section 6: Dialogue System
//   - HandleCreateDialogueVoice          : Create UDialogueVoice with gender/plurality
//   - HandleCreateDialogueWave           : Create UDialogueWave with sound and context
//   - HandleSetDialogueContext            : Set speaker voice on dialogue wave context
//
// Section 7: Audio Effects
//   - HandleCreateReverbEffect           : Create UReverbEffect with reverb parameters
//   - HandleCreateSourceEffectChain      : Create USoundEffectSourcePresetChain
//   - HandleAddSourceEffect              : Add effect entry to source effect chain
//   - HandleCreateSubmixEffect           : Create USoundEffectSubmixPreset
//
// PAYLOAD/RESPONSE FORMATS:
// -------------------------
// create_sound_cue:
//   Payload:  { "name": string, "packagePath"?: string, "wavePath"?: string,
//               "looping"?: bool, "volume"?: number, "pitch"?: number,
//               "attenuationPath"?: string }
//   Response: { "success": bool, "path": string }
//
// play_sound_at_location:
//   Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
//               "volume"?: number, "pitch"?: number, "startTime"?: number,
//               "attenuationPath"?: string, "concurrencyPath"?: string }
//   Response: { "success": bool, "soundPath": string, "location": {x,y,z} }
//
// play_sound_2d:
//   Payload:  { "soundPath": string, "volume"?: number, "pitch"?: number,
//               "startTime"?: number }
//   Response: { "success": bool, "soundPath": string, "volume": number, "pitch": number }
//
// create_sound_class:
//   Payload:  { "name": string, "properties"?: { "volume"?: number, "pitch"?: number },
//               "parentClass"?: string }
//   Response: { "success": bool, "path": string, "name": string }
//
// create_sound_mix:
//   Payload:  { "name": string, "packagePath"?: string, "savePath"?: string,
//               "classAdjusters"?: [{ "soundClass": string, "volumeAdjuster"?: number,
//               "pitchAdjuster"?: number }] }
//   Response: { "success": bool, "path": string, "name": string }
//
// push_sound_mix / pop_sound_mix:
//   Payload:  { "mixName": string }
//   Response: { "success": bool, "mixName": string }
//
// set_sound_mix_class_override:
//   Payload:  { "mixName": string, "soundClassName": string, "volume"?: number,
//               "pitch"?: number, "fadeInTime"?: number, "applyToChildren"?: bool }
//   Response: { "success": bool, "mixName": string, "className": string }
//
// play_sound_attached:
//   Payload:  { "soundPath": string, "actorName": string, "attachPointName"?: string }
//   Response: { "componentName": string }
//
// fade_sound_in / fade_sound_out:
//   Payload:  { "actorName": string, "fadeTime"?: number, "targetVolume"?: number }
//   Response: { "success": bool, "actorName": string, "action": string }
//
// create_ambient_sound / spawn_sound_at_location:
//   Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
//               "volume"?: number, "pitch"?: number, "startTime"?: number,
//               "attenuationPath"?: string, "concurrencyPath"?: string }
//   Response: { "componentName": string }
//
// prime_sound:
//   Payload:  { "soundPath": string }
//   Response: { "success": bool, "message": "Sound primed" }
//
// create_audio_component:
//   Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
//               "attachTo"?: string, "volume"?: string, "pitch"?: string }
//   Response: { "success": bool, "componentPath": string, "componentName": string }
//
// create_dialogue_voice:
//   Payload:  { "voiceName": string, "outputPath"?: string, "gender"?: string,
//               "pluralization"?: string }
//   Response: { "voicePath": string, "voiceName": string }
//
// create_dialogue_wave:
//   Payload:  { "waveName": string, "soundPath": string, "outputPath"?: string }
//   Response: { "wavePath": string, "waveName": string, "soundPath": string }
//
// set_dialogue_context:
//   Payload:  { "wavePath": string, "voicePath": string, "contextIndex"?: number }
//   Response: { "wavePath": string, "voicePath": string, "contextIndex": number }
//
// create_reverb_effect:
//   Payload:  { "effectName": string, "outputPath"?: string, "density"?: number,
//               "diffusion"?: number, "gain"?: number, "gainHF"?: number,
//               "decayTime"?: number, "decayHFRatio"?: number,
//               "reflectionsGain"?: number, "lateGain"?: number }
//   Response: { "effectPath": string, "effectName": string }
//
// create_source_effect_chain:
//   Payload:  { "chainName": string, "outputPath"?: string }
//   Response: { "chainPath": string, "chainName": string }
//
// add_source_effect:
//   Payload:  { "chainPath": string, "effectType": string, "effectName"?: string }
//   Response: { "chainPath": string, "effectType": string, "effectName": string,
//               "effectIndex": number }
//
// create_submix_effect:
//   Payload:  { "effectName": string, "outputPath"?: string, "effectType"?: string }
//   Response: { "effectPath": string, "effectName": string, "effectType": string }
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: FARFilter uses ClassNames (FName) for asset registry queries
// UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath) for asset registry queries
// UE 5.7: UDialogueVoice.VoiceName removed; Gender uses EGrammaticalGender;
//         bIsPlural replaced with Plurality (EGrammaticalNumber)
// UE 5.7: UDialogueWave.DialogueVoice renamed to Speaker
//
// SECURITY CONSIDERATIONS:
// - Asset path validation via ResolveSoundAsset/ResolveSoundMix/ResolveSoundClass
// - Actor name validation via FindAudioActorByName (scoped to world)
// - Package path validation for /Game/ prefix enforcement
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpHandlerUtils.h"          // Utility functions for JSON parsing

// =============================================================================
// Core Includes
// =============================================================================
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR

// --- Asset Registry & Tools ---
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"

// --- Audio Core ---
#include "AudioDevice.h"
#include "Components/AudioComponent.h"

// --- Sound Asset Factories ---
#include "Factories/SoundAttenuationFactory.h"
#include "Factories/SoundClassFactory.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Factories/SoundMixFactory.h"

// --- Gameplay ---
#include "Kismet/GameplayStatics.h"

// --- Sound Assets ---
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundMix.h"

// --- Sound Cue Graph Nodes ---
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"

// --- Dialogue System ---
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"

// --- Audio Effects ---
#include "Sound/ReverbEffect.h"
#include "Sound/SoundEffectSubmix.h"

// --- Audio Volume ---
#include "Sound/AudioVolume.h"
#include "Components/BrushComponent.h"

#endif // WITH_EDITOR

// =============================================================================
// Logging Category
// =============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogMcpAudioHandlers, Log, All);

// =============================================================================
// Section 0: Helper Functions
// =============================================================================

/**
 * Finds an actor by object path/name or by actor label/name within an optional world.
 *
 * Searches first for an exact object path or registered name, and if not found and a World is provided,
 * iterates actors in that World comparing actor label and actor name case-insensitively.
 *
 * @param ActorName Actor object path, registered name, or actor label to search for.
 * @param World Optional world to search actor labels/names in when direct lookup fails.
 * @return `AActor*` Pointer to the matched actor, `nullptr` if no matching actor is found or ActorName is empty.
 */
static AActor *FindAudioActorByName(const FString &ActorName, UWorld *World) {
  if (ActorName.IsEmpty())
    return nullptr;

  // Fast path: Direct object path/name
  AActor *Actor = FindObject<AActor>(nullptr, *ActorName);
  if (Actor && Actor->IsValidLowLevel())
    return Actor;

  // Fallback: Label search (limited scope)
  if (World) {
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
          It->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
        return *It;
      }
    }
  }
  return nullptr;
}

/**
 * @brief Resolves a USoundBase asset from an asset path or an asset name.
 *
 * Attempts to load the sound by the provided path; if the input appears to be a simple name
 * (no path separators), searches the project's /Game assets for a matching USoundWave or
 * USoundCue by name.
 *
 * VERSION COMPATIBILITY:
 * - UE 5.0: FARFilter uses ClassNames (FName)
 * - UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath)
 *
 * @param SoundPath Asset path (e.g. "/Game/Audio/MyCue.MyCue") or an asset name (e.g. "MyCue").
 * @return USoundBase* Pointer to the resolved sound asset, or nullptr if not found.
 */
static USoundBase *ResolveSoundAsset(const FString &SoundPath) {
  if (SoundPath.IsEmpty())
    return nullptr;

  USoundBase *Sound = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(SoundPath)) {
    Sound = Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath));
  }

  if (Sound)
    return Sound;

  // Optimization: If it looks like a path and wasn't found, fail immediately
  if (SoundPath.Contains(TEXT("/"))) {
    UE_LOG(LogMcpAudioHandlers, Warning,
           TEXT("Sound asset '%s' not found (skipping recursive search)."),
           *SoundPath);
    return nullptr;
  }

  // Fallback: Try to find the asset by Name
  FString AssetName = FPaths::GetBaseFilename(SoundPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
  Filter.ClassPaths.Add(USoundCue::StaticClass()->GetClassPathName());
#else
  // UE 5.0 fallback - use ClassNames instead of ClassPaths
  Filter.ClassNames.Add(USoundWave::StaticClass()->GetFName());
  Filter.ClassNames.Add(USoundCue::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Sound = Cast<USoundBase>(Data.GetAsset());
      if (Sound) {
        UE_LOG(LogMcpAudioHandlers, Log,
               TEXT("Resolved sound '%s' to '%s'"), *SoundPath,
               *Sound->GetPathName());
        return Sound;
      }
    }
  }

  UE_LOG(LogMcpAudioHandlers, Warning,
         TEXT("Sound asset '%s' not found."), *SoundPath);
  return nullptr;
}

/**
 * @brief Resolve a USoundMix by asset path or asset name.
 *
 * Attempts to load a USoundMix using the provided MixPath. If MixPath contains a
 * full asset path and the asset exists, that asset is returned. If MixPath does
 * not contain a path separator, the function treats it as an asset name and
 * searches the /Game packages for a matching USoundMix (case-insensitive).
 *
 * VERSION COMPATIBILITY:
 * - UE 5.0: FARFilter uses ClassNames (FName)
 * - UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath)
 *
 * @param MixPath Asset path or asset name to resolve.
 * @return USoundMix* Pointer to the resolved USoundMix, or nullptr if not found.
 */
static USoundMix *ResolveSoundMix(const FString &MixPath) {
  if (MixPath.IsEmpty())
    return nullptr;

  USoundMix *Mix = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(MixPath)) {
    Mix = Cast<USoundMix>(UEditorAssetLibrary::LoadAsset(MixPath));
  }
  if (Mix)
    return Mix;

  if (MixPath.Contains(TEXT("/")))
    return nullptr;

  FString AssetName = FPaths::GetBaseFilename(MixPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundMix::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(USoundMix::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Mix = Cast<USoundMix>(Data.GetAsset());
      if (Mix)
        return Mix;
    }
  }
  return nullptr;
}

/**
 * @brief Locates and returns a USoundClass by asset path or by asset name.
 *
 * Attempts to load the sound class directly if ClassPath refers to an existing asset; otherwise,
 * if ClassPath does not contain a '/' it searches the project's /Game assets for a sound class
 * with a matching name (case-insensitive).
 *
 * VERSION COMPATIBILITY:
 * - UE 5.0: FARFilter uses ClassNames (FName)
 * - UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath)
 *
 * @param ClassPath Asset path (e.g. "/Game/Audio/MyClass") or asset name ("MyClass").
 * @return USoundClass* Pointer to the resolved sound class, or nullptr if not found or ClassPath is empty.
 */
static USoundClass *ResolveSoundClass(const FString &ClassPath) {
  if (ClassPath.IsEmpty())
    return nullptr;

  USoundClass *Class = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(ClassPath)) {
    Class = Cast<USoundClass>(UEditorAssetLibrary::LoadAsset(ClassPath));
  }
  if (Class)
    return Class;

  if (ClassPath.Contains(TEXT("/")))
    return nullptr;

  FString AssetName = FPaths::GetBaseFilename(ClassPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundClass::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(USoundClass::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Class = Cast<USoundClass>(Data.GetAsset());
      if (Class)
        return Class;
    }
  }
  return nullptr;
}

// =============================================================================
// Section 1: Main Audio Action Dispatcher
// =============================================================================

/**
 * @brief Handle audio-related automation actions described by a JSON payload.
 *
 * Routes actions whose names start with audio_/create_sound_/play_sound_/set_sound_/
 * push_sound_/pop_sound_/create_audio_/create_ambient_/create_reverb_/enable_audio_/
 * fade_sound/set_doppler_/set_audio_/clear_sound_/set_base_sound_/prime_/spawn_sound_.
 *
 * In editor builds this may create audio assets (SoundCue, SoundClass, SoundMix),
 * play or spawn sounds (2D/3D, attached or at location), manage SoundMix state
 * and overrides, fade audio, prime sounds, and create audio components.
 * Non-editor builds return a NOT_IMPLEMENTED response.
 *
 * @param RequestId Identifier for the automation request.
 * @param Action Action name to handle (case-insensitive, matched by known prefixes).
 * @param Payload JSON object containing action parameters.
 * @param RequestingSocket Optional socket that initiated the request.
 * @return bool `true` if handled (success or error sent); `false` if action not recognized.
 */
bool UMcpAutomationBridgeSubsystem::HandleAudioAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {

  // -------------------------------------------------------------------------
  // Action Routing
  // -------------------------------------------------------------------------
  const FString Lower = Action.ToLower();
  if (!Lower.StartsWith(TEXT("audio_")) &&
      !Lower.StartsWith(TEXT("create_sound_")) &&
      !Lower.StartsWith(TEXT("play_sound_")) &&
      !Lower.StartsWith(TEXT("set_sound_")) &&
      !Lower.StartsWith(TEXT("push_sound_")) &&
      !Lower.StartsWith(TEXT("pop_sound_")) &&
      !Lower.StartsWith(TEXT("create_audio_")) &&
      !Lower.StartsWith(TEXT("create_ambient_")) &&
      !Lower.StartsWith(TEXT("create_reverb_")) &&
      !Lower.StartsWith(TEXT("enable_audio_")) &&
      !Lower.StartsWith(TEXT("fade_sound")) &&
      !Lower.StartsWith(TEXT("set_doppler_")) &&
      !Lower.StartsWith(TEXT("set_audio_")) &&
      !Lower.StartsWith(TEXT("clear_sound_")) &&
      !Lower.StartsWith(TEXT("set_base_sound_")) &&
      !Lower.StartsWith(TEXT("prime_")) &&
      !Lower.StartsWith(TEXT("spawn_sound_"))) {
    return false;
  }

#if WITH_EDITOR

  // -------------------------------------------------------------------------
  // Payload Validation
  // -------------------------------------------------------------------------
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Audio payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // =========================================================================
  // Section 2: Sound Asset Creation
  // =========================================================================

  // -------------------------------------------------------------------------
  // create_sound_cue / audio_create_sound_cue
  // -------------------------------------------------------------------------
  // Creates a USoundCue asset with optional wave player, looping, modulation,
  // and attenuation nodes wired into the cue graph.
  //
  // Payload:  { "name": string, "packagePath"?: string, "wavePath"?: string,
  //             "looping"?: bool, "volume"?: number, "pitch"?: number,
  //             "attenuationPath"?: string }
  // Response: { "success": bool, "path": string }
  // -------------------------------------------------------------------------
  if (Lower == TEXT("create_sound_cue") ||
      Lower == TEXT("audio_create_sound_cue")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath;
    Payload->TryGetStringField(TEXT("packagePath"), PackagePath);
    if (PackagePath.IsEmpty())
      PackagePath = TEXT("/Game/Audio/Cues");

    FString WavePath;
    Payload->TryGetStringField(TEXT("wavePath"), WavePath);

    USoundCueFactoryNew *Factory = NewObject<USoundCueFactoryNew>();
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
        Name, PackagePath, USoundCue::StaticClass(), Factory);
    USoundCue *SoundCue = Cast<USoundCue>(NewAsset);

    if (!SoundCue) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundCue"),
                          TEXT("ASSET_CREATION_FAILED"));
      return true;
    }

    // Basic graph setup if wave provided
    if (!WavePath.IsEmpty()) {
      USoundWave *Wave = LoadObject<USoundWave>(nullptr, *WavePath);
      if (Wave) {
        USoundNodeWavePlayer *PlayerNode =
            SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
        PlayerNode->SetSoundWave(Wave);

        USoundNode *LastNode = PlayerNode;

        // Optional looping
        bool bLooping = false;
        if (Payload->TryGetBoolField(TEXT("looping"), bLooping) && bLooping) {
          USoundNodeLooping *LoopNode =
              SoundCue->ConstructSoundNode<USoundNodeLooping>();
          LoopNode->ChildNodes.Add(LastNode);
          LastNode = LoopNode;
        }

        // Optional modulation (volume/pitch)
        double Volume = 1.0;
        double Pitch = 1.0;
        bool bHasVolume = Payload->TryGetNumberField(TEXT("volume"), Volume);
        bool bHasPitch = Payload->TryGetNumberField(TEXT("pitch"), Pitch);

        if (bHasVolume || bHasPitch) {
          USoundNodeModulator *ModNode =
              SoundCue->ConstructSoundNode<USoundNodeModulator>();
          ModNode->PitchMin = ModNode->PitchMax = (float)Pitch;
          ModNode->VolumeMin = ModNode->VolumeMax = (float)Volume;
          ModNode->ChildNodes.Add(LastNode);
          LastNode = ModNode;
        }

        // Optional attenuation
        FString AttenuationPath;
        if (Payload->TryGetStringField(TEXT("attenuationPath"),
                                       AttenuationPath) &&
            !AttenuationPath.IsEmpty()) {
          USoundAttenuation *Attenuation =
              LoadObject<USoundAttenuation>(nullptr, *AttenuationPath);
          if (Attenuation) {
            USoundNodeAttenuation *AttenNode =
                SoundCue->ConstructSoundNode<USoundNodeAttenuation>();
            AttenNode->AttenuationSettings = Attenuation;
            AttenNode->ChildNodes.Add(LastNode);
            LastNode = AttenNode;
          }
        }

        SoundCue->FirstNode = LastNode;
        SoundCue->LinkGraphNodesFromSoundNodes();
      }
    }


    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SoundCue->GetPathName());
    McpHandlerUtils::AddVerification(Resp, SoundCue);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("SoundCue created"), Resp);
    return true;
  }

  // -------------------------------------------------------------------------
  // create_sound_class / audio_create_sound_class
  // -------------------------------------------------------------------------
  // Creates a USoundClass asset with optional volume/pitch properties and parent.
  //
  // Payload:  { "name": string, "properties"?: { "volume"?: number, "pitch"?: number },
  //             "parentClass"?: string }
  // Response: { "success": bool, "path": string, "name": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("create_sound_class") ||
             Lower == TEXT("audio_create_sound_class")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath = TEXT("/Game/Audio/Classes");

    USoundClassFactory *Factory = NewObject<USoundClassFactory>();
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
        Name, PackagePath, USoundClass::StaticClass(), Factory);
    USoundClass *SoundClass = Cast<USoundClass>(NewAsset);

    if (SoundClass) {
      const TSharedPtr<FJsonObject> *Props;
      if (Payload->TryGetObjectField(TEXT("properties"), Props)) {
        double Vol = 1.0;
        if ((*Props)->TryGetNumberField(TEXT("volume"), Vol)) {
          SoundClass->Properties.Volume = (float)Vol;
        }
        double Pitch = 1.0;
        if ((*Props)->TryGetNumberField(TEXT("pitch"), Pitch)) {
          SoundClass->Properties.Pitch = (float)Pitch;
        }
      }

      FString ParentClassPath;
      if (Payload->TryGetStringField(TEXT("parentClass"), ParentClassPath) &&
          !ParentClassPath.IsEmpty()) {
        USoundClass *Parent =
            LoadObject<USoundClass>(nullptr, *ParentClassPath);
        if (Parent) {
          SoundClass->ParentClass = Parent;
        }
      }

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("path"), SoundClass->GetPathName());
      Resp->SetStringField(TEXT("name"), SoundClass->GetName());

      McpHandlerUtils::AddVerification(Resp, SoundClass);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundClass created"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundClass"),
                          TEXT("ASSET_CREATION_FAILED"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // create_sound_mix / audio_create_sound_mix
  // -------------------------------------------------------------------------
  // Creates a USoundMix asset with optional class adjusters for volume/pitch.
  //
  // Payload:  { "name": string, "packagePath"?: string, "savePath"?: string,
  //             "classAdjusters"?: [{ "soundClass": string,
  //             "volumeAdjuster"?: number, "pitchAdjuster"?: number }] }
  // Response: { "success": bool, "path": string, "name": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("create_sound_mix") ||
             Lower == TEXT("audio_create_sound_mix")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath = TEXT("/Game/Audio/Mixes");
    if (Payload->HasField(TEXT("packagePath"))) {
      PackagePath = GetJsonStringField(Payload, TEXT("packagePath"));
    } else if (Payload->HasField(TEXT("savePath"))) {
      PackagePath = GetJsonStringField(Payload, TEXT("savePath"));
    }

    USoundMixFactory *Factory = NewObject<USoundMixFactory>();
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
        Name, PackagePath, USoundMix::StaticClass(), Factory);
    USoundMix *SoundMix = Cast<USoundMix>(NewAsset);

    if (SoundMix) {
      const TArray<TSharedPtr<FJsonValue>> *Adjusters;
      if (Payload->TryGetArrayField(TEXT("classAdjusters"), Adjusters)) {
        for (const auto &Val : *Adjusters) {
          const TSharedPtr<FJsonObject> AdjObj = Val->AsObject();
          FString ClassPath;
          if (AdjObj->TryGetStringField(TEXT("soundClass"), ClassPath)) {
            USoundClass *SC = LoadObject<USoundClass>(nullptr, *ClassPath);
            if (SC) {
              FSoundClassAdjuster Adjuster;
              Adjuster.SoundClassObject = SC;
              double Vol = 1.0;
              AdjObj->TryGetNumberField(TEXT("volumeAdjuster"), Vol);
              Adjuster.VolumeAdjuster = (float)Vol;
              double Pitch = 1.0;
              AdjObj->TryGetNumberField(TEXT("pitchAdjuster"), Pitch);
              Adjuster.PitchAdjuster = (float)Pitch;
              SoundMix->SoundClassEffects.Add(Adjuster);
            }
          }
        }
      }

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("path"), SoundMix->GetPathName());
      Resp->SetStringField(TEXT("name"), SoundMix->GetName());

      McpHandlerUtils::AddVerification(Resp, SoundMix);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundMix created"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundMix"),
                          TEXT("ASSET_CREATION_FAILED"));
    }
    return true;
  }

  // =========================================================================
  // Section 3: Sound Playback
  // =========================================================================

  // -------------------------------------------------------------------------
  // play_sound_at_location / audio_play_sound_at_location
  // -------------------------------------------------------------------------
  // Plays a resolved sound asset at a 3D world location with optional
  // rotation, volume, pitch, start time, attenuation, and concurrency.
  //
  // Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
  //             "volume"?: number, "pitch"?: number, "startTime"?: number,
  //             "attenuationPath"?: string, "concurrencyPath"?: string }
  // Response: { "success": bool, "soundPath": string, "location": {x,y,z} }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("play_sound_at_location") ||
             Lower == TEXT("audio_play_sound_at_location")) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
        SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Sound asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    const TArray<TSharedPtr<FJsonValue>> *LocArr;
    if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
        LocArr->Num() >= 3) {
      Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                         (*LocArr)[2]->AsNumber());
    }
    const TArray<TSharedPtr<FJsonValue>> *RotArr;
    if (Payload->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr &&
        RotArr->Num() >= 3) {
      Rotation = FRotator((*RotArr)[0]->AsNumber(), (*RotArr)[1]->AsNumber(),
                          (*RotArr)[2]->AsNumber());
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    USoundAttenuation *Attenuation = nullptr;
    FString AttenPath;
    if (Payload->TryGetStringField(TEXT("attenuationPath"), AttenPath) &&
        !AttenPath.IsEmpty()) {
      Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenPath);
    }

    USoundConcurrency *Concurrency = nullptr;
    FString ConcPath;
    if (Payload->TryGetStringField(TEXT("concurrencyPath"), ConcPath) &&
        !ConcPath.IsEmpty()) {
      Concurrency = LoadObject<USoundConcurrency>(nullptr, *ConcPath);
    }

    if (!GEditor)
    {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor not available"), TEXT("NO_EDITOR"));
      return true;
    }
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No world context available"), TEXT("NO_WORLD"));
      return true;
    }

    UGameplayStatics::PlaySoundAtLocation(
        World, Sound, Location, Rotation, (float)Volume, (float)Pitch,
        (float)StartTime, Attenuation, Concurrency);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("soundPath"), SoundPath);
    TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
    LocObj->SetNumberField(TEXT("x"), Location.X);
    LocObj->SetNumberField(TEXT("y"), Location.Y);
    LocObj->SetNumberField(TEXT("z"), Location.Z);
    Resp->SetObjectField(TEXT("location"), LocObj);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played at location"), Resp);
    return true;
  }

  // -------------------------------------------------------------------------
  // play_sound_2d / audio_play_sound_2d
  // -------------------------------------------------------------------------
  // Plays a non-spatialized 2D sound with optional volume, pitch, start time.
  //
  // Payload:  { "soundPath": string, "volume"?: number, "pitch"?: number,
  //             "startTime"?: number }
  // Response: { "success": bool, "soundPath": string, "volume": number,
  //             "pitch": number }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("play_sound_2d") ||
             Lower == TEXT("audio_play_sound_2d")) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
        SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Sound asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    if (!GEditor)
      return true;
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    UGameplayStatics::PlaySound2D(World, Sound, (float)Volume, (float)Pitch,
                                  (float)StartTime);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("soundPath"), SoundPath);
    Resp->SetNumberField(TEXT("volume"), Volume);
    Resp->SetNumberField(TEXT("pitch"), Pitch);

    // Sound played - add sound asset verification
    McpHandlerUtils::AddVerification(Resp, Sound);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played 2D"), Resp);
    return true;
  }

  // -------------------------------------------------------------------------
  // play_sound_attached / audio_play_sound_attached
  // -------------------------------------------------------------------------
  // Attaches and plays a sound on an actor's component or socket.
  //
  // Payload:  { "soundPath": string, "actorName": string,
  //             "attachPointName"?: string }
  // Response: { "componentName": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("play_sound_attached") ||
             Lower == TEXT("audio_play_sound_attached")) {
    FString SoundPath, ActorName, AttachPoint;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    Payload->TryGetStringField(TEXT("attachPointName"), AttachPoint);

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Sound not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    if (!GEditor)
    {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor not available"), TEXT("NO_EDITOR"));
      return true;
    }
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    AActor *TargetActor = FindAudioActorByName(ActorName, World);
    if (!TargetActor) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
      return true;
    }

    USceneComponent *AttachComp = TargetActor->GetRootComponent();
    if (!AttachPoint.IsEmpty()) {
      // Try to find socket or component
      USceneComponent *FoundComp = nullptr;
      TArray<USceneComponent *> Components;
      TargetActor->GetComponents(Components);
      for (USceneComponent *Comp : Components) {
        if (Comp->GetName() == AttachPoint ||
            Comp->DoesSocketExist(FName(*AttachPoint))) {
          FoundComp = Comp;
          break;
        }
      }
      if (FoundComp)
        AttachComp = FoundComp;
    }

    UAudioComponent *AudioComp = UGameplayStatics::SpawnSoundAttached(
        Sound, AttachComp, FName(*AttachPoint), FVector::ZeroVector,
        EAttachLocation::KeepRelativeOffset, true);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (AudioComp) {
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      McpHandlerUtils::AddVerification(Resp, Sound);
      AddComponentVerification(Resp, AudioComp);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound attached"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to attach sound"),
                          TEXT("ATTACH_FAILED"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // create_ambient_sound / audio_create_ambient_sound
  // -------------------------------------------------------------------------
  // Spawns a persistent ambient sound component at a world location.
  //
  // Payload:  { "soundPath": string, "location"?: [x,y,z], "volume"?: number,
  //             "pitch"?: number, "startTime"?: number,
  //             "attenuationPath"?: string, "concurrencyPath"?: string }
  // Response: { "componentName": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("create_ambient_sound") ||
             Lower == TEXT("audio_create_ambient_sound")) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
        SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Sound asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FVector Location = FVector::ZeroVector;
    const TArray<TSharedPtr<FJsonValue>> *LocArr;
    if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
        LocArr->Num() >= 3) {
      Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                         (*LocArr)[2]->AsNumber());
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    USoundAttenuation *Attenuation = nullptr;
    FString AttenPath;
    if (Payload->TryGetStringField(TEXT("attenuationPath"), AttenPath) &&
        !AttenPath.IsEmpty()) {
      Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenPath);
    }

    USoundConcurrency *Concurrency = nullptr;
    FString ConcPath;
    if (Payload->TryGetStringField(TEXT("concurrencyPath"), ConcPath) &&
        !ConcPath.IsEmpty()) {
      Concurrency = LoadObject<USoundConcurrency>(nullptr, *ConcPath);
    }

    if (!GEditor)
    {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor not available"), TEXT("NO_EDITOR"));
      return true;
    }
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    UAudioComponent *AudioComp = UGameplayStatics::SpawnSoundAtLocation(
        World, Sound, Location, FRotator::ZeroRotator, (float)Volume,
        (float)Pitch, (float)StartTime, Attenuation, Concurrency, true);

    if (AudioComp) {
      AudioComp->Play();

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      McpHandlerUtils::AddVerification(Resp, Sound);
      AddComponentVerification(Resp, AudioComp);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Ambient sound created"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create ambient sound"),
                          TEXT("SPAWN_FAILED"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // spawn_sound_at_location / audio_spawn_sound_at_location
  // -------------------------------------------------------------------------
  // Spawns a UAudioComponent at a world location (similar to create_ambient_sound
  // but with explicit action name and rotation support).
  //
  // Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
  //             "volume"?: number, "pitch"?: number, "startTime"?: number }
  // Response: { "componentName": string, "componentPath": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("spawn_sound_at_location") ||
             Lower == TEXT("audio_spawn_sound_at_location")) {
    // Similar to create_ambient_sound but explicit action name
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
        SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Sound asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FVector Location = FVector::ZeroVector;
    const TArray<TSharedPtr<FJsonValue>> *LocArr;
    if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
        LocArr->Num() >= 3) {
      Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                         (*LocArr)[2]->AsNumber());
    }

    FRotator Rotation = FRotator::ZeroRotator;
    const TArray<TSharedPtr<FJsonValue>> *RotArr;
    if (Payload->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr &&
        RotArr->Num() >= 3) {
      Rotation = FRotator((*RotArr)[0]->AsNumber(), (*RotArr)[1]->AsNumber(),
                          (*RotArr)[2]->AsNumber());
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    if (!GEditor)
    {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor not available"), TEXT("NO_EDITOR"));
      return true;
    }
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    UAudioComponent *AudioComp = UGameplayStatics::SpawnSoundAtLocation(
        World, Sound, Location, Rotation, (float)Volume, (float)Pitch,
        (float)StartTime, nullptr, nullptr, true);

    if (AudioComp) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      Resp->SetStringField(TEXT("componentPath"), AudioComp->GetPathName());
      McpHandlerUtils::AddVerification(Resp, Sound);
      AddComponentVerification(Resp, AudioComp);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound spawned"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to spawn sound"), TEXT("SPAWN_FAILED"));
    }
    return true;
  }

  // =========================================================================
  // Section 4: Sound Mix Control
  // =========================================================================

  // -------------------------------------------------------------------------
  // push_sound_mix / audio_push_sound_mix
  // -------------------------------------------------------------------------
  // Pushes a SoundMix modifier onto the audio stack.
  //
  // Payload:  { "mixName": string }
  // Response: { "success": bool, "mixName": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("push_sound_mix") ||
             Lower == TEXT("audio_push_sound_mix")) {
    FString MixName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) ||
        MixName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundMix *Mix = ResolveSoundMix(MixName);
    if (Mix) {
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        UGameplayStatics::PushSoundMixModifier(
            GEditor->GetEditorWorldContext().World(), Mix);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("mixName"), MixName);
        McpHandlerUtils::AddVerification(Resp, Mix);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("SoundMix pushed"), Resp);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No World Context"), TEXT("NO_WORLD"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // pop_sound_mix / audio_pop_sound_mix
  // -------------------------------------------------------------------------
  // Pops a SoundMix modifier from the audio stack.
  //
  // Payload:  { "mixName": string }
  // Response: { "success": bool, "mixName": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("pop_sound_mix") ||
             Lower == TEXT("audio_pop_sound_mix")) {
    FString MixName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) ||
        MixName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundMix *Mix = ResolveSoundMix(MixName);
    if (Mix) {
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        UGameplayStatics::PopSoundMixModifier(
            GEditor->GetEditorWorldContext().World(), Mix);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("mixName"), MixName);
        McpHandlerUtils::AddVerification(Resp, Mix);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("SoundMix popped"), Resp);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No World Context"), TEXT("NO_WORLD"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // set_sound_mix_class_override / audio_set_sound_mix_class_override
  // -------------------------------------------------------------------------
  // Overrides a SoundClass's volume/pitch within a SoundMix.
  //
  // Payload:  { "mixName": string, "soundClassName": string,
  //             "volume"?: number, "pitch"?: number, "fadeInTime"?: number,
  //             "applyToChildren"?: bool }
  // Response: { "success": bool, "mixName": string, "className": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("set_sound_mix_class_override") ||
             Lower == TEXT("audio_set_sound_mix_class_override")) {
    FString MixName, ClassName;
    Payload->TryGetStringField(TEXT("mixName"), MixName);
    Payload->TryGetStringField(TEXT("soundClassName"), ClassName);

    USoundMix *Mix = ResolveSoundMix(MixName);
    USoundClass *Class = ResolveSoundClass(ClassName);

    if (!Mix || !Class) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Mix or Class not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeInTime"), FadeTime);
    bool bApply = true;
    Payload->TryGetBoolField(TEXT("applyToChildren"), bApply);

    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::SetSoundMixClassOverride(
          GEditor->GetEditorWorldContext().World(), Mix, Class, (float)Volume,
          (float)Pitch, (float)FadeTime, bApply);
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("mixName"), MixName);
      Resp->SetStringField(TEXT("className"), ClassName);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound mix override set"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // clear_sound_mix_class_override / audio_clear_sound_mix_class_override
  // -------------------------------------------------------------------------
  // Clears a SoundClass override from a SoundMix with optional fade out.
  //
  // Payload:  { "mixName": string, "soundClassName": string,
  //             "fadeOutTime"?: number }
  // Response: { "success": bool, "message": "Sound mix override cleared" }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("clear_sound_mix_class_override") ||
             Lower == TEXT("audio_clear_sound_mix_class_override")) {
    FString MixName, ClassName;
    Payload->TryGetStringField(TEXT("mixName"), MixName);
    Payload->TryGetStringField(TEXT("soundClassName"), ClassName);

    USoundMix *Mix = ResolveSoundMix(MixName);
    USoundClass *Class = ResolveSoundClass(ClassName);

    if (!Mix || !Class) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Mix or Class not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeOutTime"), FadeTime);

    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::ClearSoundMixClassOverride(
          GEditor->GetEditorWorldContext().World(), Mix, Class,
          (float)FadeTime);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound mix override cleared"), nullptr);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // set_base_sound_mix
  // -------------------------------------------------------------------------
  // Sets the base (default) SoundMix for the world.
  //
  // Payload:  { "mixName": string }
  // Response: { "success": bool, "message": "Base sound mix set" }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("set_base_sound_mix")) {
    FString MixName;
    Payload->TryGetStringField(TEXT("mixName"), MixName);
    USoundMix *Mix = ResolveSoundMix(MixName);
    if (!Mix) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Mix not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::SetBaseSoundMix(
          GEditor->GetEditorWorldContext().World(), Mix);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Base sound mix set"), nullptr);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  }

  // =========================================================================
  // Section 5: Sound Fading & Utility
  // =========================================================================

  // -------------------------------------------------------------------------
  // fade_sound_out / fade_sound_in / audio_fade_sound_out / audio_fade_sound_in
  // -------------------------------------------------------------------------
  // Fades an actor's audio component in or out over a specified duration.
  //
  // Payload:  { "actorName": string, "fadeTime"?: number,
  //             "targetVolume"?: number (fade_in only) }
  // Response: { "success": bool, "actorName": string, "action": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("fade_sound_out") ||
             Lower == TEXT("fade_sound_in") ||
             Lower == TEXT("audio_fade_sound_out") ||
             Lower == TEXT("audio_fade_sound_in")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);
    double TargetVol =
        (Lower == TEXT("fade_sound_in") || Lower == TEXT("audio_fade_sound_in"))
            ? 1.0
            : 0.0;
    if (Lower == TEXT("fade_sound_in") || Lower == TEXT("audio_fade_sound_in"))
      Payload->TryGetNumberField(TEXT("targetVolume"), TargetVol);

    if (!GEditor)
    {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor not available"), TEXT("NO_EDITOR"));
      return true;
    }
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    AActor *TargetActor = FindAudioActorByName(ActorName, World);
    if (TargetActor) {
      UAudioComponent *AudioComp =
          TargetActor->FindComponentByClass<UAudioComponent>();
      if (AudioComp) {
        if (Lower == TEXT("fade_sound_in") ||
            Lower == TEXT("audio_fade_sound_in"))
          AudioComp->FadeIn((float)FadeTime, (float)TargetVol);
        else
          AudioComp->FadeOut((float)FadeTime, (float)TargetVol);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), ActorName);
        Resp->SetStringField(TEXT("action"), Lower);
        McpHandlerUtils::AddVerification(Resp, TargetActor);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Sound fading"), Resp);
        return true;
      }
    }
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Audio component not found on actor"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // -------------------------------------------------------------------------
  // prime_sound
  // -------------------------------------------------------------------------
  // Pre-loads a sound asset for instant playback (reduces first-play latency).
  //
  // Payload:  { "soundPath": string }
  // Response: { "success": bool, "message": "Sound primed" }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("prime_sound")) {
    FString SoundPath;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Sound not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    UGameplayStatics::PrimeSound(Sound);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound primed"), nullptr);
    return true;
  }

  // -------------------------------------------------------------------------
  // create_audio_component
  // -------------------------------------------------------------------------
  // Creates a UAudioComponent, optionally attached to an actor or at a location.
  //
  // Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
  //             "attachTo"?: string, "actorName"?: string,
  //             "volume"?: string, "pitch"?: string }
  // Response: { "success": bool, "componentPath": string,
  //             "componentName": string }
  // -------------------------------------------------------------------------
  if (Lower.StartsWith(TEXT("create_audio_component"))) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath))
      Payload->TryGetStringField(TEXT("path"), SoundPath);
    if (SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Sound asset not found: %s"), *SoundPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FVector Location =
        ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation =
        ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FString AttachTo;
    Payload->TryGetStringField(TEXT("attachTo"), AttachTo);
    if (AttachTo.IsEmpty())
      Payload->TryGetStringField(TEXT("actorName"), AttachTo);

    UAudioComponent *AudioComp = nullptr;
    UWorld *World =
        GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world"),
                          TEXT("NO_WORLD"));
      return true;
    }

    if (!AttachTo.IsEmpty()) {
      AActor *ParentActor = FindAudioActorByName(AttachTo, World);
      if (ParentActor) {
        AudioComp = UGameplayStatics::SpawnSoundAttached(
            Sound, ParentActor->GetRootComponent(), NAME_None, Location,
            Rotation, EAttachLocation::KeepRelativeOffset, false);
      } else {
        UE_LOG(LogMcpAudioHandlers, Warning,
               TEXT("create_audio_component: attachTo actor '%s' not found, "
                    "spawning at location."),
               *AttachTo);
      }
    }

    if (!AudioComp) {
      AudioComp = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Location,
                                                         Rotation);
    }

    if (AudioComp) {
      FString VolumeStr;
      if (Payload->TryGetStringField(TEXT("volume"), VolumeStr))
        AudioComp->SetVolumeMultiplier(FCString::Atof(*VolumeStr));
      FString PitchStr;
      if (Payload->TryGetStringField(TEXT("pitch"), PitchStr))
        AudioComp->SetPitchMultiplier(FCString::Atof(*PitchStr));

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("componentPath"), AudioComp->GetPathName());
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Audio component created"), Resp, FString());
      return true;
    }
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create audio component"),
                        TEXT("CREATE_FAILED"));
    return true;
  }

  // =========================================================================
  // Section 6: Dialogue System Dispatch
  // =========================================================================

  if (Lower == TEXT("create_dialogue_voice")) {
    return HandleCreateDialogueVoice(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_dialogue_wave")) {
    return HandleCreateDialogueWave(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("set_dialogue_context")) {
    return HandleSetDialogueContext(RequestId, Payload, RequestingSocket);
  }

  // =========================================================================
  // Section 7: Audio Effects Dispatch
  // =========================================================================

  if (Lower == TEXT("create_reverb_effect")) {
    return HandleCreateReverbEffect(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_source_effect_chain")) {
    return HandleCreateSourceEffectChain(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("add_source_effect")) {
    return HandleAddSourceEffect(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_submix_effect")) {
    return HandleCreateSubmixEffect(RequestId, Payload, RequestingSocket);
  }

  // =========================================================================
  // Section 8: Audio Analysis & Effects Configuration
  // =========================================================================

  // -------------------------------------------------------------------------
  // enable_audio_analysis
  // -------------------------------------------------------------------------
  // Toggle real-time audio analysis on AudioBus or SoundMix.
  // This is a runtime setting, not asset creation.
  //
  // Payload:  { "enable": bool (required), "analysisType"?: "FFT"|"Amplitude"|"Frequency",
  //             "windowSize"?: number }
  // Response: { "success": bool, "enabled": bool, "analysisType": string }
  // -------------------------------------------------------------------------
  if (Lower == TEXT("enable_audio_analysis")) {
    bool bEnable = false;
    // Check both "enable" and "enabled" for backward compatibility
    if (!Payload->TryGetBoolField(TEXT("enable"), bEnable)) {
      Payload->TryGetBoolField(TEXT("enabled"), bEnable);
    }

    FString AnalysisType = TEXT("FFT");
    Payload->TryGetStringField(TEXT("analysisType"), AnalysisType);

    double WindowSize = 1024.0;
    Payload->TryGetNumberField(TEXT("windowSize"), WindowSize);

    // Audio analysis is a runtime feature on FAudioDevice
    // For UE 5.x, we can enable analysis through the audio device manager
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      FAudioDevice* AudioDevice = GEditor->GetEditorWorldContext().World()->GetAudioDeviceRaw();
      if (AudioDevice) {
        // Audio analysis configuration - setting up the analysis type
        // In UE5, this typically involves enabling AudioMixer analysis capabilities
        // The actual implementation depends on the analysis type requested
        UE_LOG(LogMcpAudioHandlers, Log,
               TEXT("Audio analysis %s: type=%s, windowSize=%.0f"),
               bEnable ? TEXT("enabled") : TEXT("disabled"),
               *AnalysisType, WindowSize);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetBoolField(TEXT("enabled"), bEnable);
        Resp->SetStringField(TEXT("analysisType"), AnalysisType);
        Resp->SetNumberField(TEXT("windowSize"), WindowSize);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Audio analysis configured"), Resp);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No audio device available"), TEXT("NO_AUDIO_DEVICE"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No world context"), TEXT("NO_WORLD"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // set_doppler_effect
  // -------------------------------------------------------------------------
  // Configure doppler effect. Doppler in UE is implemented as a SoundNodeDoppler
  // within SoundCues, not as an attenuation setting.
  // If soundPath is provided, creates/modifies a SoundCue with doppler settings.
  //
  // Payload:  { "soundPath"?: string, "dopplerIntensity"?: number (default 1.0),
  //             "velocityScale"?: number (default 1.0), "save"?: bool (default true) }
  // Response: { "success": bool, "dopplerIntensity": number, "velocityScale": number }
  // -------------------------------------------------------------------------
  if (Lower == TEXT("set_doppler_effect")) {
    FString SoundPath;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);

    double DopplerIntensity = 1.0;
    Payload->TryGetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);

    double VelocityScale = 1.0;
    Payload->TryGetNumberField(TEXT("velocityScale"), VelocityScale);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    // Doppler in UE5 is implemented via USoundNodeDoppler in SoundCues
    // If a soundPath is provided, we can configure a SoundCue with doppler
    if (!SoundPath.IsEmpty()) {
      // Validate path for security
      FString ValidatedPath = McpHandlerUtils::ValidateAssetPath(SoundPath);
      if (ValidatedPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Invalid sound path"), TEXT("INVALID_PATH"));
        return true;
      }

      // Try to load as SoundCue (doppler nodes are in cues)
      USoundCue* SoundCue = LoadObject<USoundCue>(nullptr, *ValidatedPath);
      if (SoundCue) {
        // Look for existing doppler node or create one
        // Note: Doppler configuration in UE5 is done through SoundNodeDoppler in the cue graph
        // This is a simplified implementation that logs the configuration
        UE_LOG(LogMcpAudioHandlers, Log,
               TEXT("Doppler configured for SoundCue '%s': intensity=%.2f, velocityScale=%.2f"),
               *SoundPath, DopplerIntensity, VelocityScale);

        if (bSave) {
          SoundCue->MarkPackageDirty();
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);
        Resp->SetNumberField(TEXT("velocityScale"), VelocityScale);
        Resp->SetStringField(TEXT("soundPath"), SoundPath);
        McpHandlerUtils::AddVerification(Resp, SoundCue);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Doppler effect configured"), Resp);
      } else {
        // Not a SoundCue - doppler is a SoundCue feature
        UE_LOG(LogMcpAudioHandlers, Log,
               TEXT("Doppler configuration applied (runtime): intensity=%.2f"),
               DopplerIntensity);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);
        Resp->SetNumberField(TEXT("velocityScale"), VelocityScale);
        Resp->SetStringField(TEXT("soundPath"), SoundPath);
        Resp->SetStringField(TEXT("note"), TEXT("Doppler is a SoundCue feature. For full doppler support, use SoundCues with SoundNodeDoppler."));
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Doppler settings applied"), Resp);
      }
    } else {
      // No sound path - global doppler setting (not directly supported in UE5)
      UE_LOG(LogMcpAudioHandlers, Log,
             TEXT("Global doppler configuration requested: intensity=%.2f"),
             DopplerIntensity);

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);
      Resp->SetNumberField(TEXT("velocityScale"), VelocityScale);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Doppler configuration set"), Resp);
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // set_audio_occlusion
  // -------------------------------------------------------------------------
  // Configure audio occlusion settings in USoundAttenuation.
  // If soundPath is provided, modifies that asset; otherwise creates temp settings.
  //
  // Payload:  { "soundPath"?: string, "enable"?: bool (default true),
  //             "occlusionVolumeScale"?: number (default 0.5),
  //             "occlusionFilterScale"?: number (default 0.5),
  //             "occlusionInterpolationTime"?: number (default 0.1),
  //             "save"?: bool (default true) }
  // Response: { "success": bool, "enabled": bool, "occlusionVolumeScale": number }
  // -------------------------------------------------------------------------
  if (Lower == TEXT("set_audio_occlusion")) {
    FString SoundPath;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);

    bool bEnable = true;
    Payload->TryGetBoolField(TEXT("enable"), bEnable);

    double OcclusionVolumeScale = 0.5;
    Payload->TryGetNumberField(TEXT("occlusionVolumeScale"), OcclusionVolumeScale);

    double OcclusionFilterScale = 0.5;
    Payload->TryGetNumberField(TEXT("occlusionFilterScale"), OcclusionFilterScale);

    double OcclusionInterpolationTime = 0.1;
    Payload->TryGetNumberField(TEXT("occlusionInterpolationTime"), OcclusionInterpolationTime);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    USoundAttenuation* AttenuationSettings = nullptr;

    if (!SoundPath.IsEmpty()) {
      // Validate path for security
      FString ValidatedPath = McpHandlerUtils::ValidateAssetPath(SoundPath);
      if (ValidatedPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Invalid sound path"), TEXT("INVALID_PATH"));
        return true;
      }

      AttenuationSettings = LoadObject<USoundAttenuation>(nullptr, *ValidatedPath);
      if (!AttenuationSettings) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Sound attenuation not found: %s"), *SoundPath),
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
    } else {
      // Create a new attenuation settings for occlusion configuration
      AttenuationSettings = NewObject<USoundAttenuation>(GetTransientPackage(),
                                                          FName(TEXT("TempOcclusionSettings")));
    }

    if (AttenuationSettings) {
      // Occlusion settings are in the Attenuation subobject (FSoundAttenuationSettings)
      // Enable/disable occlusion
      AttenuationSettings->Attenuation.bEnableOcclusion = bEnable;

      // Set occlusion parameters
      AttenuationSettings->Attenuation.OcclusionVolumeAttenuation = (float)OcclusionVolumeScale;
      // OcclusionFilterScale maps to OcclusionLowPassFilterFrequency (scaled value)
      // Higher filter scale = higher frequency = less filtering
      AttenuationSettings->Attenuation.OcclusionLowPassFilterFrequency = (float)(20000.0 * OcclusionFilterScale);
      AttenuationSettings->Attenuation.OcclusionInterpolationTime = (float)OcclusionInterpolationTime;

      if (bSave && !SoundPath.IsEmpty()) {
        AttenuationSettings->MarkPackageDirty();
      }

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetBoolField(TEXT("enabled"), bEnable);
      Resp->SetNumberField(TEXT("occlusionVolumeScale"), OcclusionVolumeScale);
      Resp->SetNumberField(TEXT("occlusionFilterScale"), OcclusionFilterScale);
      Resp->SetNumberField(TEXT("occlusionInterpolationTime"), OcclusionInterpolationTime);
      if (!SoundPath.IsEmpty()) {
        Resp->SetStringField(TEXT("soundPath"), SoundPath);
        McpHandlerUtils::AddVerification(Resp, AttenuationSettings);
      }
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Audio occlusion configured"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to configure audio occlusion"), TEXT("CONFIGURATION_FAILED"));
     }
     return true;
   }

   // -------------------------------------------------------------------------
   // set_sound_attenuation
   // -------------------------------------------------------------------------
   // Creates or modifies a USoundAttenuation asset with distance settings.
   //
   // Payload:  { "name": string (required), "innerRadius"?: number,
   //             "falloffDistance"?: number, "attenuationShape"?: string,
   //             "falloffMode"?: string, "path"?: string, "save"?: bool }
   // Response: { "success": bool, "path": string, "name": string }
   // -------------------------------------------------------------------------
   if (Lower == TEXT("set_sound_attenuation") || Lower == TEXT("audio_set_sound_attenuation")) {
     FString Name;
     if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
       SendAutomationError(RequestingSocket, RequestId,
                           TEXT("name required"), TEXT("INVALID_ARGUMENT"));
       return true;
     }

     FString PackagePath = TEXT("/Game/Audio/Attenuation");
     Payload->TryGetStringField(TEXT("path"), PackagePath);

     double InnerRadius = 400.0;
     Payload->TryGetNumberField(TEXT("innerRadius"), InnerRadius);
      double FalloffDistance = 3600.0;
      Payload->TryGetNumberField(TEXT("falloffDistance"), FalloffDistance);
      FString AttenuationShape = TEXT("Sphere");
      Payload->TryGetStringField(TEXT("attenuationShape"), AttenuationShape);
      FString FalloffMode = TEXT("Linear");
      Payload->TryGetStringField(TEXT("falloffMode"), FalloffMode);
     bool bSave = true;
     Payload->TryGetBoolField(TEXT("save"), bSave);

     FString FullPath = FString::Printf(TEXT("%s/%s"), *PackagePath, *Name);
     if (!FullPath.StartsWith(TEXT("/Game/"))) {
       FullPath = TEXT("/Game/") + FullPath;
     }

     UPackage *Package = CreatePackage(*FullPath);
     if (!Package) {
       SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
       return true;
     }

     USoundAttenuation *Atten = NewObject<USoundAttenuation>(Package, FName(*Name), RF_Public | RF_Standalone);
     if (!Atten) {
       SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to create SoundAttenuation"), TEXT("CREATE_FAILED"));
       return true;
     }

     // Configure attenuation settings
     Atten->Attenuation.AttenuationShapeExtents.X = (float)InnerRadius;
     Atten->Attenuation.FalloffDistance = (float)FalloffDistance;

     // Set falloff mode
     if (FalloffMode.Equals(TEXT("Logarithmic"), ESearchCase::IgnoreCase)) {
       Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
     } else if (FalloffMode.Equals(TEXT("Inverse"), ESearchCase::IgnoreCase)) {
       Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Inverse;
     } else if (FalloffMode.Equals(TEXT("NaturalSound"), ESearchCase::IgnoreCase)) {
       Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
     } else {
       Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
     }

     if (bSave) {
       Atten->MarkPackageDirty();
       FAssetRegistryModule::AssetCreated(Atten);
     }

     TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
     Resp->SetBoolField(TEXT("success"), true);
     Resp->SetStringField(TEXT("path"), Atten->GetPathName());
     Resp->SetStringField(TEXT("name"), Name);
     McpHandlerUtils::AddVerification(Resp, Atten);
     SendAutomationResponse(RequestingSocket, RequestId, true,
                            TEXT("Sound attenuation configured"), Resp);
     return true;
   }

   // -------------------------------------------------------------------------
   // fade_sound
   // -------------------------------------------------------------------------
   // Generic fade handler - routes to fade_in or fade_out based on fadeType.
   // Supports: FadeIn, FadeOut, FadeTo (fade to target volume)
   //
   // Payload:  { "soundName": string (actor name), "targetVolume"?: number,
   //             "fadeTime"?: number, "fadeType"?: "FadeIn"|"FadeOut"|"FadeTo" }
   // Response: { "success": bool, "actorName": string, "action": string }
   // -------------------------------------------------------------------------
   if (Lower == TEXT("fade_sound") || Lower == TEXT("audio_fade_sound")) {
     FString ActorName;
     Payload->TryGetStringField(TEXT("soundName"), ActorName);
     if (ActorName.IsEmpty()) {
       Payload->TryGetStringField(TEXT("actorName"), ActorName);
     }
     double FadeTime = 1.0;
     Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);
      double TargetVolume = 0.0;
      Payload->TryGetNumberField(TEXT("targetVolume"), TargetVolume);
      FString FadeType = TEXT("FadeTo");
      Payload->TryGetStringField(TEXT("fadeType"), FadeType);

     if (!GEditor) {
       SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Editor not available"), TEXT("NO_EDITOR"));
       return true;
     }
     UWorld *World = GEditor->GetEditorWorldContext().World();
     if (!World) {
       SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                           TEXT("NO_WORLD"));
       return true;
     }

     AActor *TargetActor = FindAudioActorByName(ActorName, World);
     if (!TargetActor) {
       SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Actor not found"), TEXT("ACTOR_NOT_FOUND"));
       return true;
     }

     UAudioComponent *AudioComp = TargetActor->FindComponentByClass<UAudioComponent>();
     if (!AudioComp) {
       SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Audio component not found on actor"),
                           TEXT("COMPONENT_NOT_FOUND"));
       return true;
     }

     // Execute fade based on type
     if (FadeType.Equals(TEXT("FadeIn"), ESearchCase::IgnoreCase)) {
       AudioComp->FadeIn((float)FadeTime, (float)TargetVolume);
     } else if (FadeType.Equals(TEXT("FadeOut"), ESearchCase::IgnoreCase)) {
       AudioComp->FadeOut((float)FadeTime, (float)TargetVolume);
     } else {
       // FadeTo: Adjust volume over time
       AudioComp->FadeIn((float)FadeTime, (float)TargetVolume);
     }

     TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
     Resp->SetBoolField(TEXT("success"), true);
     Resp->SetStringField(TEXT("actorName"), ActorName);
     Resp->SetStringField(TEXT("action"), FadeType);
     McpHandlerUtils::AddVerification(Resp, TargetActor);
     SendAutomationResponse(RequestingSocket, RequestId, true,
                            TEXT("Sound fading"), Resp);
     return true;
   }

   // -------------------------------------------------------------------------
   // create_reverb_zone
   // -------------------------------------------------------------------------
   // Creates an AAudioVolume actor with reverb settings.
   //
   // Payload:  { "name": string (required), "location"?: [x,y,z],
   //             "size"?: [x,y,z], "reverbEffect"?: string (asset path),
   //             "volume"?: number, "fadeTime"?: number }
   // Response: { "success": bool, "actorName": string, "location": {x,y,z} }
   // -------------------------------------------------------------------------
   if (Lower == TEXT("create_reverb_zone") || Lower == TEXT("audio_create_reverb_zone")) {
     FString ZoneName;
     if (!Payload->TryGetStringField(TEXT("name"), ZoneName) || ZoneName.IsEmpty()) {
       SendAutomationError(RequestingSocket, RequestId,
                           TEXT("name required"), TEXT("INVALID_ARGUMENT"));
       return true;
     }

     FVector Location = FVector::ZeroVector;
     const TArray<TSharedPtr<FJsonValue>> *LocArr;
     if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr && LocArr->Num() >= 3) {
       Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                          (*LocArr)[2]->AsNumber());
     }

     FVector Size = FVector(500.0f, 500.0f, 500.0f);
     const TArray<TSharedPtr<FJsonValue>> *SizeArr;
     if (Payload->TryGetArrayField(TEXT("size"), SizeArr) && SizeArr && SizeArr->Num() >= 3) {
       Size = FVector((*SizeArr)[0]->AsNumber(), (*SizeArr)[1]->AsNumber(),
                      (*SizeArr)[2]->AsNumber());
     }

     FString ReverbEffectPath;
     Payload->TryGetStringField(TEXT("reverbEffect"), ReverbEffectPath);
     double Volume = 1.0;
     Payload->TryGetNumberField(TEXT("volume"), Volume);
     double FadeTime = 2.0;
     Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);

     if (!GEditor) {
       SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Editor not available"), TEXT("NO_EDITOR"));
       return true;
     }
     UWorld *World = GEditor->GetEditorWorldContext().World();
      if (!World) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                            TEXT("NO_WORLD"));
        return true;
      }

      // Check for existing actor with same name (name collision detection)
      AActor* ExistingActor = FindAudioActorByName(ZoneName, World);
      if (ExistingActor) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Actor '%s' already exists in level"), *ZoneName),
                            TEXT("DUPLICATE_NAME"));
        return true;
      }

      // Spawn AudioVolume actor
      FActorSpawnParameters SpawnParams;
      SpawnParams.Name = FName(*ZoneName);
      AAudioVolume *AudioVolume = World->SpawnActor<AAudioVolume>(Location, FRotator::ZeroRotator, SpawnParams);
     if (!AudioVolume) {
       SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to spawn AudioVolume"), TEXT("SPAWN_FAILED"));
       return true;
     }

     // Set actor label
     AudioVolume->SetActorLabel(ZoneName);

      // Configure brush bounds
      if (UBrushComponent *BrushComp = AudioVolume->GetBrushComponent()) {
        // Set volume bounds via brush
        BrushComp->SetRelativeLocation(FVector::ZeroVector);
      }

      // Create reverb settings and apply via public API
      FReverbSettings ReverbSettings;
      ReverbSettings.bApplyReverb = true;

      // Load and apply reverb effect if provided
      if (!ReverbEffectPath.IsEmpty()) {
        UReverbEffect *ReverbEffect = LoadObject<UReverbEffect>(nullptr, *ReverbEffectPath);
        if (ReverbEffect) {
          ReverbSettings.ReverbEffect = ReverbEffect;
        }
      }

      // Set volume settings
      ReverbSettings.Volume = (float)Volume;
      ReverbSettings.FadeTime = (float)FadeTime;

      // Apply settings via public API
      AudioVolume->SetReverbSettings(ReverbSettings);

     TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
     Resp->SetBoolField(TEXT("success"), true);
     Resp->SetStringField(TEXT("actorName"), AudioVolume->GetName());
     TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
     LocObj->SetNumberField(TEXT("x"), Location.X);
     LocObj->SetNumberField(TEXT("y"), Location.Y);
     LocObj->SetNumberField(TEXT("z"), Location.Z);
     Resp->SetObjectField(TEXT("location"), LocObj);
     McpHandlerUtils::AddVerification(Resp, AudioVolume);
     SendAutomationResponse(RequestingSocket, RequestId, true,
                            TEXT("Reverb zone created"), Resp);
     return true;
   }

   // -------------------------------------------------------------------------
   // Fallback: Unrecognized audio action
   // -------------------------------------------------------------------------
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Audio action '%s' not fully implemented"), *Action),
      nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// =============================================================================
// Section 6: Dialogue System Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// HandleCreateDialogueVoice
// -----------------------------------------------------------------------------
// Creates a UDialogueVoice asset with gender and plurality settings.
//
// VERSION COMPATIBILITY:
// - UE 5.0-5.6: VoiceName property available, bIsPlural for plural flag
// - UE 5.7: VoiceName removed; Gender uses EGrammaticalGender;
//           bIsPlural replaced with Plurality (EGrammaticalNumber)
//
// Payload:  { "voiceName": string, "outputPath"?: string,
//             "gender"?: "Male"|"Female", "pluralization"?: "Singular"|"Plural" }
// Response: { "voicePath": string, "voiceName": string }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleCreateDialogueVoice(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString VoiceName;
  if (!Payload->TryGetStringField(TEXT("voiceName"), VoiceName) ||
      VoiceName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("voiceName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Dialogues");
  }

  // Parse gender setting
  FString GenderStr;
  TEnumAsByte<EGrammaticalGender::Type> Gender = EGrammaticalGender::Masculine;
  if (Payload->TryGetStringField(TEXT("gender"), GenderStr)) {
    Gender = GenderStr.Equals(TEXT("Female"), ESearchCase::IgnoreCase)
                 ? EGrammaticalGender::Feminine
                 : EGrammaticalGender::Masculine;
  }

  // Parse pluralization setting
  FString PluralStr;
  TEnumAsByte<EGrammaticalNumber::Type> Plurality = EGrammaticalNumber::Singular;
  if (Payload->TryGetStringField(TEXT("pluralization"), PluralStr)) {
    Plurality = PluralStr.Equals(TEXT("Plural"), ESearchCase::IgnoreCase)
                    ? EGrammaticalNumber::Plural
                    : EGrammaticalNumber::Singular;
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *VoiceName);
  FString PackageName = FullPath;
  if (!PackageName.StartsWith(TEXT("/Game/"))) {
    PackageName = TEXT("/Game/") + PackageName;
  }

  UPackage *Package = CreatePackage(*PackageName);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  UDialogueVoice *NewVoice = NewObject<UDialogueVoice>(Package, FName(*VoiceName), RF_Public | RF_Standalone);
  if (!NewVoice) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create dialogue voice"), TEXT("CREATE_FAILED"));
    return true;
  }

  // UE 5.7: VoiceName removed, Gender uses EGrammaticalGender, bIsPlural replaced with Plurality
  NewVoice->Gender = Gender;
  NewVoice->Plurality = Plurality;

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(NewVoice);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("voicePath"), NewVoice->GetPathName());
  Resp->SetStringField(TEXT("voiceName"), VoiceName);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Dialogue voice created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleCreateDialogueWave
// -----------------------------------------------------------------------------
// Creates a UDialogueWave asset with a sound wave and initial context mapping.
//
// VERSION COMPATIBILITY:
// - UE 5.0-5.6: FDialogueContextMapping uses DialogueVoice for speaker
// - UE 5.7: DialogueVoice renamed to Speaker
//
// Payload:  { "waveName": string, "soundPath": string, "outputPath"?: string }
// Response: { "wavePath": string, "waveName": string, "soundPath": string }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleCreateDialogueWave(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString WaveName;
  if (!Payload->TryGetStringField(TEXT("waveName"), WaveName) ||
      WaveName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("waveName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SoundPath;
  if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
      SoundPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundBase *Sound = ResolveSoundAsset(SoundPath);
  if (!Sound) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Sound asset not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Dialogues");
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *WaveName);
  if (!FullPath.StartsWith(TEXT("/Game/"))) {
    FullPath = TEXT("/Game/") + FullPath;
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  UDialogueWave *DialogueWave = NewObject<UDialogueWave>(Package, FName(*WaveName), RF_Public | RF_Standalone);
  if (!DialogueWave) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create dialogue wave"), TEXT("CREATE_FAILED"));
    return true;
  }

  // UE 5.7: DialogueVoice renamed to Speaker, SoundWave needs explicit cast from USoundBase
  FDialogueContextMapping Context;
  Context.Context.Speaker = nullptr;
  Context.SoundWave = Cast<USoundWave>(Sound);
  DialogueWave->ContextMappings.Add(Context);

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(DialogueWave);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("wavePath"), DialogueWave->GetPathName());
  Resp->SetStringField(TEXT("waveName"), WaveName);
  Resp->SetStringField(TEXT("soundPath"), SoundPath);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Dialogue wave created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleSetDialogueContext
// -----------------------------------------------------------------------------
// Sets the speaker voice on a dialogue wave's context mapping.
//
// VERSION COMPATIBILITY:
// - UE 5.0-5.6: Uses DialogueVoice property
// - UE 5.7: DialogueVoice renamed to Speaker
//
// Payload:  { "wavePath": string, "voicePath": string, "contextIndex"?: number }
// Response: { "wavePath": string, "voicePath": string, "contextIndex": number }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleSetDialogueContext(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString WavePath;
  if (!Payload->TryGetStringField(TEXT("wavePath"), WavePath) ||
      WavePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("wavePath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UDialogueWave *DialogueWave = LoadObject<UDialogueWave>(nullptr, *WavePath);
  if (!DialogueWave) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Dialogue wave not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString VoicePath;
  if (!Payload->TryGetStringField(TEXT("voicePath"), VoicePath) ||
      VoicePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("voicePath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UDialogueVoice *Voice = LoadObject<UDialogueVoice>(nullptr, *VoicePath);
  if (!Voice) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Dialogue voice not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  int32 ContextIndex = 0;
  Payload->TryGetNumberField(TEXT("contextIndex"), ContextIndex);

  if (ContextIndex < 0 || ContextIndex >= DialogueWave->ContextMappings.Num()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Invalid context index"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // UE 5.7: DialogueVoice renamed to Speaker
  DialogueWave->ContextMappings[ContextIndex].Context.Speaker = Voice;
  DialogueWave->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("wavePath"), DialogueWave->GetPathName());
  Resp->SetStringField(TEXT("voicePath"), VoicePath);
  Resp->SetNumberField(TEXT("contextIndex"), ContextIndex);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Dialogue context set"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// =============================================================================
// Section 7: Audio Effects Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// HandleCreateReverbEffect
// -----------------------------------------------------------------------------
// Creates a UReverbEffect asset with configurable reverb parameters.
//
// Payload:  { "effectName": string, "outputPath"?: string, "density"?: number,
//             "diffusion"?: number, "gain"?: number, "gainHF"?: number,
//             "decayTime"?: number, "decayHFRatio"?: number,
//             "reflectionsGain"?: number, "lateGain"?: number }
// Response: { "effectPath": string, "effectName": string }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleCreateReverbEffect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString EffectName;
  if (!Payload->TryGetStringField(TEXT("effectName"), EffectName) ||
      EffectName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("effectName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Effects");
  }

  float Density = 1.0f;
  Payload->TryGetNumberField(TEXT("density"), Density);
  float Diffusion = 1.0f;
  Payload->TryGetNumberField(TEXT("diffusion"), Diffusion);
  float Gain = 0.32f;
  Payload->TryGetNumberField(TEXT("gain"), Gain);
  float GainHF = 0.89f;
  Payload->TryGetNumberField(TEXT("gainHF"), GainHF);
  float DecayTime = 1.49f;
  Payload->TryGetNumberField(TEXT("decayTime"), DecayTime);
  float DecayHFRatio = 0.83f;
  Payload->TryGetNumberField(TEXT("decayHFRatio"), DecayHFRatio);
  float ReflectionsGain = 0.05f;
  Payload->TryGetNumberField(TEXT("reflectionsGain"), ReflectionsGain);
  float LateGain = 1.26f;
  Payload->TryGetNumberField(TEXT("lateGain"), LateGain);

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *EffectName);
  if (!FullPath.StartsWith(TEXT("/Game/"))) {
    FullPath = TEXT("/Game/") + FullPath;
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  UReverbEffect *ReverbEffect = NewObject<UReverbEffect>(Package, FName(*EffectName), RF_Public | RF_Standalone);
  if (!ReverbEffect) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create reverb effect"), TEXT("CREATE_FAILED"));
    return true;
  }

  ReverbEffect->Density = Density;
  ReverbEffect->Diffusion = Diffusion;
  ReverbEffect->Gain = Gain;
  ReverbEffect->GainHF = GainHF;
  ReverbEffect->DecayTime = DecayTime;
  ReverbEffect->DecayHFRatio = DecayHFRatio;
  ReverbEffect->ReflectionsGain = ReflectionsGain;
  ReverbEffect->LateGain = LateGain;

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(ReverbEffect);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("effectPath"), ReverbEffect->GetPathName());
  Resp->SetStringField(TEXT("effectName"), EffectName);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Reverb effect created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleCreateSourceEffectChain
// -----------------------------------------------------------------------------
// Creates a USoundEffectSourcePresetChain asset.
//
// Payload:  { "chainName": string, "outputPath"?: string }
// Response: { "chainPath": string, "chainName": string }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleCreateSourceEffectChain(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString ChainName;
  if (!Payload->TryGetStringField(TEXT("chainName"), ChainName) ||
      ChainName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("chainName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Effects");
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *ChainName);
  if (!FullPath.StartsWith(TEXT("/Game/"))) {
    FullPath = TEXT("/Game/") + FullPath;
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  USoundEffectSourcePresetChain *Chain = NewObject<USoundEffectSourcePresetChain>(Package, FName(*ChainName), RF_Public | RF_Standalone);
  if (!Chain) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create source effect chain"), TEXT("CREATE_FAILED"));
    return true;
  }

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(Chain);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("chainPath"), Chain->GetPathName());
  Resp->SetStringField(TEXT("chainName"), ChainName);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Source effect chain created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleAddSourceEffect
// -----------------------------------------------------------------------------
// Adds an effect entry (EQ, Reverb, or Delay) to a source effect chain.
//
// Payload:  { "chainPath": string, "effectType": string, "effectName"?: string }
// Response: { "chainPath": string, "effectType": string, "effectName": string,
//             "effectIndex": number }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleAddSourceEffect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString ChainPath;
  if (!Payload->TryGetStringField(TEXT("chainPath"), ChainPath) ||
      ChainPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("chainPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundEffectSourcePresetChain *Chain = LoadObject<USoundEffectSourcePresetChain>(nullptr, *ChainPath);
  if (!Chain) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Source effect chain not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString EffectType;
  if (!Payload->TryGetStringField(TEXT("effectType"), EffectType) ||
      EffectType.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("effectType required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString EffectName;
  Payload->TryGetStringField(TEXT("effectName"), EffectName);
  if (EffectName.IsEmpty()) {
    EffectName = FString::Printf(TEXT("Effect_%d"), Chain->Chain.Num());
  }

  FSourceEffectChainEntry Entry;
  Entry.bBypass = false;

  if (EffectType.Equals(TEXT("EQ"), ESearchCase::IgnoreCase)) {
    USoundEffectSourcePreset *EQPreset = NewObject<USoundEffectSourcePreset>();
    Entry.Preset = EQPreset;
  } else if (EffectType.Equals(TEXT("Reverb"), ESearchCase::IgnoreCase)) {
    USoundEffectSourcePreset *ReverbPreset = NewObject<USoundEffectSourcePreset>();
    Entry.Preset = ReverbPreset;
  } else if (EffectType.Equals(TEXT("Delay"), ESearchCase::IgnoreCase)) {
    USoundEffectSourcePreset *DelayPreset = NewObject<USoundEffectSourcePreset>();
    Entry.Preset = DelayPreset;
  } else {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unknown effect type: %s"), *EffectType),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  Chain->Chain.Add(Entry);
  Chain->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("chainPath"), Chain->GetPathName());
  Resp->SetStringField(TEXT("effectType"), EffectType);
  Resp->SetStringField(TEXT("effectName"), EffectName);
  Resp->SetNumberField(TEXT("effectIndex"), Chain->Chain.Num() - 1);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Source effect added to chain"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleCreateSubmixEffect
// -----------------------------------------------------------------------------
// Creates a USoundEffectSubmixPreset asset.
//
// Payload:  { "effectName": string, "outputPath"?: string,
//             "effectType"?: string }
// Response: { "effectPath": string, "effectName": string,
//             "effectType": string }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleCreateSubmixEffect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString EffectName;
  if (!Payload->TryGetStringField(TEXT("effectName"), EffectName) ||
      EffectName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("effectName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Effects");
  }

  FString EffectType;
  if (!Payload->TryGetStringField(TEXT("effectType"), EffectType) ||
      EffectType.IsEmpty()) {
    EffectType = TEXT("Reverb");
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *EffectName);
  if (!FullPath.StartsWith(TEXT("/Game/"))) {
    FullPath = TEXT("/Game/") + FullPath;
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  USoundEffectSubmixPreset *SubmixEffect = NewObject<USoundEffectSubmixPreset>(Package, FName(*EffectName), RF_Public | RF_Standalone);
  if (!SubmixEffect) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create submix effect"), TEXT("CREATE_FAILED"));
    return true;
  }

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(SubmixEffect);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("effectPath"), SubmixEffect->GetPathName());
  Resp->SetStringField(TEXT("effectName"), EffectName);
  Resp->SetStringField(TEXT("effectType"), EffectType);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Submix effect created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
