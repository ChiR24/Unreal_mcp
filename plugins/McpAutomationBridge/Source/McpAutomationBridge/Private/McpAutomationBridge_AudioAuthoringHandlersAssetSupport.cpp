#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
FString NormalizeAudioPath(const FString& Path, bool bForLoad)
{
	FString Sanitized = SanitizeProjectRelativePath(Path);
	if (Sanitized.IsEmpty() && !Path.IsEmpty())
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("NormalizeAudioPath: Rejected malicious path: %s"), *Path);
		return FString();
	}

	FString Normalized = Sanitized;
	if (Normalized.StartsWith(TEXT("/Content/")))
	{
		Normalized = TEXT("/Game/") + Normalized.Mid(9);
	}
	else if (Normalized == TEXT("/Content"))
	{
		Normalized = TEXT("/Game");
	}

	Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Normalized.EndsWith(TEXT("/")))
	{
		Normalized.LeftChopInline(1);
	}

	if (bForLoad && !Normalized.Contains(TEXT(".")) && !Normalized.IsEmpty())
	{
		FString AssetName = Normalized;
		int32 LastSlashIdx;
		if (Normalized.FindLastChar(TEXT('/'), LastSlashIdx) && LastSlashIdx < Normalized.Len() - 1)
		{
			AssetName = Normalized.Mid(LastSlashIdx + 1);
		}
		Normalized = Normalized + TEXT(".") + AssetName;
	}

	return Normalized;
}

bool BuildAudioCreationPath(const FString& Directory, const FString& Name, FString& OutPackagePath, FString& OutError)
{
	if (Name.IsEmpty())
	{
		OutError = TEXT("Name is required");
		return false;
	}

	const FString TrimmedName = Name.TrimStartAndEnd();
	if (TrimmedName.IsEmpty() || !FName::IsValidXName(TrimmedName, INVALID_OBJECTNAME_CHARACTERS))
	{
		OutError = FString::Printf(TEXT("Invalid asset name: %s"), *Name);
		return false;
	}

	const FString SafeDirectory = NormalizeAudioPath(Directory, false);
	if (SafeDirectory.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Invalid asset path: %s"), *Directory);
		return false;
	}

	OutPackagePath = SanitizeProjectRelativePath(SafeDirectory / TrimmedName);
	if (OutPackagePath.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Invalid asset path: %s"), *(SafeDirectory / TrimmedName));
		return false;
	}

	FText ValidationError;
	if (!FPackageName::IsValidLongPackageName(OutPackagePath, true, &ValidationError))
	{
		OutError = FString::Printf(TEXT("Invalid asset path: %s (%s)"), *OutPackagePath, *ValidationError.ToString());
		return false;
	}

	return true;
}

bool SaveAudioAsset(UObject* Asset, bool bShouldSave)
{
	if (!bShouldSave || !Asset)
	{
		return true;
	}

	Asset->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Asset);
	return true;
}

USoundWave* LoadSoundWaveFromPath(const FString& SoundPath)
{
	FString NormalizedPath = NormalizeAudioPath(SoundPath);
	return Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, *NormalizedPath));
}

USoundCue* LoadSoundCueFromPath(const FString& CuePath)
{
	FString NormalizedPath = NormalizeAudioPath(CuePath);
	return Cast<USoundCue>(StaticLoadObject(USoundCue::StaticClass(), nullptr, *NormalizedPath));
}

USoundClass* LoadSoundClassFromPath(const FString& ClassPath)
{
	FString NormalizedPath = NormalizeAudioPath(ClassPath);
	return Cast<USoundClass>(StaticLoadObject(USoundClass::StaticClass(), nullptr, *NormalizedPath));
}

USoundAttenuation* LoadSoundAttenuationFromPath(const FString& AttenPath)
{
	FString NormalizedPath = NormalizeAudioPath(AttenPath);
	return Cast<USoundAttenuation>(StaticLoadObject(USoundAttenuation::StaticClass(), nullptr, *NormalizedPath));
}

USoundMix* LoadSoundMixFromPath(const FString& MixPath)
{
	FString NormalizedPath = NormalizeAudioPath(MixPath);
	return Cast<USoundMix>(StaticLoadObject(USoundMix::StaticClass(), nullptr, *NormalizedPath));
}

#if MCP_HAS_SOURCE_EFFECT_PRESETS
USoundEffectSourcePreset* CreateSourceEffectPresetByType(const FString& EffectType, UObject* Outer)
{
	FString LowerType = EffectType.ToLower();
	UClass* PresetClass = nullptr;

	if (LowerType == TEXT("eq") || LowerType == TEXT("equalizer")) { PresetClass = USourceEffectEQPreset::StaticClass(); }
	else if (LowerType == TEXT("chorus")) { PresetClass = USourceEffectChorusPreset::StaticClass(); }
	else if (LowerType == TEXT("delay") || LowerType == TEXT("simpledelay")) { PresetClass = USourceEffectSimpleDelayPreset::StaticClass(); }
	else if (LowerType == TEXT("filter")) { PresetClass = USourceEffectFilterPreset::StaticClass(); }
	else if (LowerType == TEXT("dynamics") || LowerType == TEXT("dynamicsprocessor") || LowerType == TEXT("compressor")) { PresetClass = USourceEffectDynamicsProcessorPreset::StaticClass(); }
	else if (LowerType == TEXT("bitcrusher") || LowerType == TEXT("bit_crusher")) { PresetClass = USourceEffectBitCrusherPreset::StaticClass(); }
	else if (LowerType == TEXT("phaser")) { PresetClass = USourceEffectPhaserPreset::StaticClass(); }
	else if (LowerType == TEXT("waveshaper") || LowerType == TEXT("wave_shaper") || LowerType == TEXT("distortion")) { PresetClass = USourceEffectWaveShaperPreset::StaticClass(); }
	else if (LowerType == TEXT("panner")) { PresetClass = USourceEffectPannerPreset::StaticClass(); }
	else if (LowerType == TEXT("stereodelay") || LowerType == TEXT("stereo_delay")) { PresetClass = USourceEffectStereoDelayPreset::StaticClass(); }
	else if (LowerType == TEXT("foldbackdistortion") || LowerType == TEXT("foldback")) { PresetClass = USourceEffectFoldbackDistortionPreset::StaticClass(); }
	else if (LowerType == TEXT("ringmodulation") || LowerType == TEXT("ring_mod")) { PresetClass = USourceEffectRingModulationPreset::StaticClass(); }
	else if (LowerType == TEXT("midsidespreader") || LowerType == TEXT("mid_side")) { PresetClass = USourceEffectMidSideSpreaderPreset::StaticClass(); }
	else if (LowerType == TEXT("motionfilter")) { PresetClass = USourceEffectMotionFilterPreset::StaticClass(); }
	else if (LowerType == TEXT("envelopefollower") || LowerType == TEXT("envelope")) { PresetClass = USourceEffectEnvelopeFollowerPreset::StaticClass(); }
#if MCP_HAS_SOURCE_EFFECT_CONVOLUTION_REVERB
	else if (LowerType == TEXT("convolutionreverb") || LowerType == TEXT("conv_reverb")) { PresetClass = USourceEffectConvolutionReverbPreset::StaticClass(); }
#endif

	if (PresetClass)
	{
		FName PresetName = MakeUniqueObjectName(GetTransientPackage(), PresetClass, FName(*EffectType));
		return Cast<USoundEffectSourcePreset>(NewObject<UObject>(GetTransientPackage(), PresetClass, PresetName));
	}
	return nullptr;
}
#endif
}
#endif
