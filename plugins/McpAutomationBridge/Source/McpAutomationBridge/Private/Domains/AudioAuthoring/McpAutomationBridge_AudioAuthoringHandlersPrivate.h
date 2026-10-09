#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Safety/McpSafeOperations.h"

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

#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"

#include "Factories/DialogueVoiceFactory.h"
#include "Factories/DialogueWaveFactory.h"

#include "Sound/SoundEffectSource.h"

#include "Sound/SoundSubmixSend.h"

#include "Sound/SoundSubmix.h"

#if __has_include("AudioMixerTypes.h")
#include "AudioMixerTypes.h"
#endif

#include "Sound/SoundEffectPreset.h"

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

#include "Sound/ReverbEffect.h"

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
#endif

#if __has_include("MetasoundFrontendDocumentBuilder.h")
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocument.h"
#if __has_include("MetasoundFrontendDocumentBuilderRegistry.h")
#include "MetasoundFrontendDocumentBuilderRegistry.h"
#endif
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

// A document builder over a MetaSound: the one the engine already holds for it
// (a play, a register or an open MetaSound editor made it), else the call's own,
// which 5.5+ opens for edits and must FinishBuilding(). A second builder beside
// the engine's logged "prior builder is still active" and left the engine's
// cache stale, so the next register indexed past the edited arrays and crashed.
#define MCP_METASOUND_BUILDER(Name, Document) TOptional<FMetaSoundFrontendDocumentBuilder> Name##Own; \
	FMetaSoundFrontendDocumentBuilder& Name = McpAudioAuthoring::McpMetaSoundBuilder(Document, Name##Own)
#if MCP_HAS_METASOUND_FRONTEND_V2
#define MCP_METASOUND_FINISH(Name) if (Name##Own.IsSet()) { Name.FinishBuilding(); }
#else
#define MCP_METASOUND_FINISH(Name)
#endif

#if MCP_HAS_METASOUND_FRONTEND && __has_include("MetasoundFrontendSearchEngine.h")
#include "MetasoundFrontendSearchEngine.h"
#define MCP_HAS_METASOUND_SEARCH_ENGINE 1
#else
#define MCP_HAS_METASOUND_SEARCH_ENGINE 0
#endif

#if __has_include("MetasoundFactory.h")
#include "MetasoundFactory.h"
#endif

#if __has_include("MetasoundEditorSubsystem.h")
#include "MetasoundEditorSubsystem.h"
#endif

namespace McpAudioAuthoring
{
#if MCP_HAS_METASOUND_FRONTEND
inline FMetaSoundFrontendDocumentBuilder& McpMetaSoundBuilder(const TScriptInterface<IMetaSoundDocumentInterface>& Document, TOptional<FMetaSoundFrontendDocumentBuilder>& Own)
{
#if MCP_HAS_METASOUND_FRONTEND_V2
	if (Metasound::Frontend::IDocumentBuilderRegistry* Builders = Metasound::Frontend::IDocumentBuilderRegistry::Get())
	{
		if (FMetaSoundFrontendDocumentBuilder* Existing = Builders->FindBuilder(Document)) { return *Existing; }
	}
	return Own.Emplace(Document, nullptr, true);
#else
	return Own.Emplace(Document);
#endif
}
#endif
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
TSharedPtr<FJsonObject> HandleSoundCueDopplerAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMetaSoundAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMetaSoundNodeActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMetaSoundNodeConnect(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
// remove_metasound_node, disconnect_metasound_nodes (MetaSound/...MetaSoundGraphEdit.cpp) and get_metasound_graph (...GraphRead.cpp).
TSharedPtr<FJsonObject> HandleMetaSoundGraphEditActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMetaSoundGraphReadAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
#if MCP_HAS_METASOUND_SEARCH_ENGINE
bool ResolveMetaSoundNodeClassName(
	const FString& Namespace,
	const FString& Name,
	const FString& Variant,
	FMetasoundFrontendClassName& OutClassName,
	TArray<FString>& OutCandidates);
/** The default input and output pins of a registered node class, "Name (Type)". */
void FindMetaSoundClassPins(const FMetasoundFrontendClassName& ClassName, TArray<FString>& OutInputs, TArray<FString>& OutOutputs);
#endif
/** The class an add_node names (nodeClassName "Namespace.Name.Variant" or a nodeType shorthand), resolved against the
 *  live registry where the engine has a search engine. Name is empty when it names none; Requested is the spelling. */
struct FMcpMetaSoundNodeClassRequest
{
	FString Namespace, Name, Variant, Requested;
	TArray<FString> Candidates;
	/** The registry class's default pins, "Name (Type)" as connect lists them; empty where the engine has no search engine. */
	TArray<FString> Inputs, Outputs;
	bool bInRegistry = true;
	bool bResolvedByName = false;
};
FMcpMetaSoundNodeClassRequest ResolveMetaSoundAddNodeClass(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleMetaSoundInterfaceActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMetaSoundDefaultAction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
/** `defaultValue` converted to TypeName (e.g. "Float", "Int32:Array"), else the legacy floatValue, intValue, boolValue or stringValue. */
bool MetaSoundLiteralFromParams(const TSharedPtr<FJsonObject>& Params, const FString& TypeName, FMetasoundFrontendLiteral& Out, FString& OutError);
#endif
TSharedPtr<FJsonObject> HandleMetaSoundBatchAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSoundClassActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSoundMixActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSoundMixEqActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleAttenuationActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
/** innerRadius as the shape reads its extents: a Box half-size on every axis, a Capsule radius (Y) with a half height
 *  (X) of at least that, a Sphere or Cone radius (X). Writing X alone left a Box or Capsule flat. */
inline void SetAttenuationInnerRadius(FBaseAttenuationSettings& Settings, double Radius)
{
	FVector& Extents = Settings.AttenuationShapeExtents;
	if (Settings.AttenuationShape == EAttenuationShape::Box) { Extents = FVector(Radius); }
	else if (Settings.AttenuationShape == EAttenuationShape::Capsule) { Extents.Y = Radius; Extents.X = FMath::Max(Extents.X, Radius); }
	else { Extents.X = Radius; }
}
/** The innerRadius SetAttenuationInnerRadius wrote, read from the axis the shape keeps it on. */
inline double GetAttenuationInnerRadius(const FBaseAttenuationSettings& Settings)
{
	return Settings.AttenuationShape == EAttenuationShape::Capsule ? Settings.AttenuationShapeExtents.Y : Settings.AttenuationShapeExtents.X;
}
TSharedPtr<FJsonObject> HandleDialogueActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleEffectActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleAudioInfoActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
}

