#pragma once

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpSafeOperations.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"

#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeBranch.h"
#include "Sound/SoundNodeConcatenator.h"
#include "Sound/SoundNodeDelay.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeMixer.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeSwitch.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"

#include "SoundCueGraph/SoundCueGraphNode.h"
#include "SoundCueGraph/SoundCueGraphNode_Root.h"

#include "Factories/SoundAttenuationFactory.h"
#include "Factories/SoundClassFactory.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Factories/SoundMixFactory.h"

#if __has_include("Sound/DialogueVoice.h")
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"
#define MCP_HAS_DIALOGUE 1
#else
#define MCP_HAS_DIALOGUE 0
#endif

#if __has_include("Factories/DialogueVoiceFactory.h")
#include "Factories/DialogueVoiceFactory.h"
#include "Factories/DialogueWaveFactory.h"
#define MCP_HAS_DIALOGUE_FACTORY 1
#else
#define MCP_HAS_DIALOGUE_FACTORY 0
#endif

#if __has_include("Sound/SoundEffectSource.h")
#include "Sound/SoundEffectSource.h"
#define MCP_HAS_SOURCE_EFFECT 1
#else
#define MCP_HAS_SOURCE_EFFECT 0
#endif

#if __has_include("Sound/SoundSubmixSend.h")
#include "Sound/SoundSubmixSend.h"
#endif

#if __has_include("Sound/SoundSubmix.h")
#include "Sound/SoundSubmix.h"
#define MCP_HAS_SUBMIX 1
#else
#define MCP_HAS_SUBMIX 0
#endif

#if __has_include("AudioMixerTypes.h")
#include "AudioMixerTypes.h"
#endif

#if __has_include("SourceEffects/SourceEffectChain.h")
#include "SourceEffects/SourceEffectChain.h"
#define MCP_HAS_EFFECT_CHAIN 0
#elif __has_include("Sound/SoundEffectPreset.h")
#include "Sound/SoundEffectPreset.h"
#define MCP_HAS_EFFECT_CHAIN 0
#else
#define MCP_HAS_EFFECT_CHAIN 0
#endif

#if __has_include("SourceEffects/SourceEffectEQ.h")
#include "SourceEffects/SourceEffectEQ.h"
#include "SourceEffects/SourceEffectChorus.h"
#include "SourceEffects/SourceEffectSimpleDelay.h"
#include "SourceEffects/SourceEffectFilter.h"
#include "SourceEffects/SourceEffectDynamicsProcessor.h"
#include "SourceEffects/SourceEffectBitCrusher.h"
#include "SourceEffects/SourceEffectPhaser.h"
#include "SourceEffects/SourceEffectWaveShaper.h"
#include "SourceEffects/SourceEffectPanner.h"
#include "SourceEffects/SourceEffectStereoDelay.h"
#include "SourceEffects/SourceEffectFoldbackDistortion.h"
#include "SourceEffects/SourceEffectRingModulation.h"
#include "SourceEffects/SourceEffectMidSideSpreader.h"
#include "SourceEffects/SourceEffectMotionFilter.h"
#include "SourceEffects/SourceEffectEnvelopeFollower.h"
#if __has_include("SourceEffects/SourceEffectConvolutionReverb.h")
#include "SourceEffects/SourceEffectConvolutionReverb.h"
#define MCP_HAS_SOURCE_EFFECT_CONVOLUTION_REVERB 1
#else
#define MCP_HAS_SOURCE_EFFECT_CONVOLUTION_REVERB 0
#endif
#define MCP_HAS_SOURCE_EFFECT_PRESETS 1
#else
#define MCP_HAS_SOURCE_EFFECT_CONVOLUTION_REVERB 0
#define MCP_HAS_SOURCE_EFFECT_PRESETS 0
#endif

#if __has_include("Sound/ReverbEffect.h")
#include "Sound/ReverbEffect.h"
#define MCP_HAS_REVERB_EFFECT 1
#else
#define MCP_HAS_REVERB_EFFECT 0
#endif

#if __has_include("MetasoundSource.h")
#include "MetasoundSource.h"
#define MCP_HAS_METASOUND 1
#else
#define MCP_HAS_METASOUND 0
#endif

#if __has_include("Metasound.h")
#include "Metasound.h"
#endif

#if __has_include("MetasoundBuilderSubsystem.h")
#include "MetasoundBuilderSubsystem.h"
#define MCP_HAS_METASOUND_BUILDER 1
#else
#define MCP_HAS_METASOUND_BUILDER 0
#endif

#if __has_include("MetasoundFrontendDocumentBuilder.h")
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocument.h"
#define MCP_HAS_METASOUND_FRONTEND 1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#define MCP_HAS_METASOUND_FRONTEND_V2 1
#else
#define MCP_HAS_METASOUND_FRONTEND_V2 0
#endif
#else
#define MCP_HAS_METASOUND_FRONTEND 0
#define MCP_HAS_METASOUND_FRONTEND_V2 0
#endif

#if __has_include("MetasoundFactory.h")
#include "MetasoundFactory.h"
#define MCP_HAS_METASOUND_FACTORY 1
#else
#define MCP_HAS_METASOUND_FACTORY 0
#endif

#if __has_include("MetasoundEditorSubsystem.h")
#include "MetasoundEditorSubsystem.h"
#define MCP_HAS_METASOUND_EDITOR 1
#else
#define MCP_HAS_METASOUND_EDITOR 0
#endif

namespace McpAudioAuthoring
{
FString NormalizeAudioPath(const FString& Path, bool bForLoad = true);
bool BuildAudioCreationPath(const FString& Directory, const FString& Name, FString& OutPackagePath, FString& OutError);
bool SaveAudioAsset(UObject* Asset, bool bShouldSave);
USoundWave* LoadSoundWaveFromPath(const FString& SoundPath);
USoundCue* LoadSoundCueFromPath(const FString& CuePath);
USoundClass* LoadSoundClassFromPath(const FString& ClassPath);
USoundAttenuation* LoadSoundAttenuationFromPath(const FString& AttenPath);
USoundMix* LoadSoundMixFromPath(const FString& MixPath);
#if MCP_HAS_SOURCE_EFFECT_PRESETS
USoundEffectSourcePreset* CreateSourceEffectPresetByType(const FString& EffectType, UObject* Outer);
#endif
TSharedPtr<FJsonObject> HandleSoundCueAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSoundCueNodeActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMetaSoundAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMetaSoundNodeActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMetaSoundInterfaceActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSoundClassActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSoundMixActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSoundMixEqActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleAttenuationActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleDialogueActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleEffectActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleAudioInfoActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
}

#endif
