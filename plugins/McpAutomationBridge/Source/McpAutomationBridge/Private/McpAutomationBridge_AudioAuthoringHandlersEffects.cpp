#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleEffectActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("create_reverb_effect"))
	{
#if MCP_HAS_REVERB_EFFECT
		FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Effects")), false);
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }

		UPackage* Package = CreatePackage(*(Path / Name));
		if (!Package) { return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package")); }
		UReverbEffect* NewEffect = NewObject<UReverbEffect>(Package, FName(*Name), RF_Public | RF_Standalone);
		if (!NewEffect) { return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create ReverbEffect")); }

		if (Params->HasField(TEXT("density"))) { NewEffect->Density = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("density"), 1.0)); }
		if (Params->HasField(TEXT("diffusion"))) { NewEffect->Diffusion = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("diffusion"), 1.0)); }
		if (Params->HasField(TEXT("gain"))) { NewEffect->Gain = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("gain"), 0.32)); }
		if (Params->HasField(TEXT("gainHF"))) { NewEffect->GainHF = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("gainHF"), 0.89)); }
		if (Params->HasField(TEXT("decayTime"))) { NewEffect->DecayTime = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("decayTime"), 1.49)); }
		if (Params->HasField(TEXT("decayHFRatio"))) { NewEffect->DecayHFRatio = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("decayHFRatio"), 0.83)); }

		SaveAudioAsset(NewEffect, bSave);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("assetPath"), NewEffect->GetPathName());
		McpHandlerUtils::AddVerification(Response, NewEffect);
		return Response;
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("REVERB_NOT_AVAILABLE"), TEXT("Reverb effect not available"));
#endif
	}

	if (SubAction == TEXT("create_source_effect_chain"))
	{
#if MCP_HAS_SOURCE_EFFECT
		FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Effects")), false);
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }

		UPackage* Package = CreatePackage(*(Path / Name));
		if (!Package) { return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package")); }
		USoundEffectSourcePresetChain* NewChain = NewObject<USoundEffectSourcePresetChain>(Package, FName(*Name), RF_Public | RF_Standalone);
		if (!NewChain) { return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create source effect chain")); }

		McpSafeAssetSave(NewChain);
		Response->SetStringField(TEXT("assetPath"), NewChain->GetPathName());
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Source effect chain '%s' created"), *Name));
		McpHandlerUtils::AddVerification(Response, NewChain);
		return Response;
#else
		FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Effects")), false);
		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }
		Response->SetStringField(TEXT("assetPath"), Path / Name);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Source effect chain '%s' - AudioMixer module not available"), *Name));
		Response->SetStringField(TEXT("note"), TEXT("Enable AudioMixer plugin for full source effect chain support"));
		return Response;
#endif
	}

	if (SubAction == TEXT("add_source_effect"))
	{
#if MCP_HAS_SOURCE_EFFECT
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString EffectPresetPath = McpHandlerUtils::GetOptionalString(Params, TEXT("effectPresetPath"), TEXT(""));
		FString EffectType = McpHandlerUtils::GetOptionalString(Params, TEXT("effectType"), TEXT(""));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		if (AssetPath.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required")); }

		USoundEffectSourcePresetChain* Chain = Cast<USoundEffectSourcePresetChain>(StaticLoadObject(USoundEffectSourcePresetChain::StaticClass(), nullptr, *AssetPath));
		if (!Chain) { return McpHandlerUtils::BuildErrorResponse(TEXT("CHAIN_NOT_FOUND"), FString::Printf(TEXT("Could not load source effect chain: %s"), *AssetPath)); }

		USoundEffectSourcePreset* EffectPreset = nullptr;
		if (!EffectPresetPath.IsEmpty())
		{
			EffectPreset = Cast<USoundEffectSourcePreset>(StaticLoadObject(USoundEffectSourcePreset::StaticClass(), nullptr, *NormalizeAudioPath(EffectPresetPath)));
		}
#if MCP_HAS_SOURCE_EFFECT_PRESETS
		if (!EffectPreset && !EffectType.IsEmpty()) { EffectPreset = CreateSourceEffectPresetByType(EffectType, GetTransientPackage()); }
#endif
		if (EffectPreset)
		{
			FSourceEffectChainEntry NewEntry;
			NewEntry.Preset = EffectPreset;
			NewEntry.bBypass = McpHandlerUtils::GetOptionalBool(Params, TEXT("bypass"), false);
			Chain->Chain.Add(NewEntry);
			McpSafeAssetSave(Chain);
			Response->SetNumberField(TEXT("effectCount"), Chain->Chain.Num());
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("message"), TEXT("Source effect added to chain"));
			McpHandlerUtils::AddVerification(Response, Chain);
		}
		else
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"), TEXT("Effect preset path required or preset not found"));
			Response->SetStringField(TEXT("errorCode"), TEXT("PRESET_NOT_FOUND"));
			Response->SetStringField(TEXT("code"), TEXT("PRESET_NOT_FOUND"));
		}
		return Response;
#else
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString EffectType = McpHandlerUtils::GetOptionalString(Params, TEXT("effectType"), TEXT(""));
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Source effect '%s' noted"), *EffectType));
		Response->SetStringField(TEXT("note"), TEXT("AudioMixer module not available - enable AudioMixer plugin for full support"));
		return Response;
#endif
	}

	if (SubAction == TEXT("create_submix_effect"))
	{
#if MCP_HAS_SUBMIX
		FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
		FString EffectType = McpHandlerUtils::GetOptionalString(Params, TEXT("effectType"), TEXT(""));
		FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Effects")), false);
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }

		UPackage* Package = CreatePackage(*(Path / Name));
		if (!Package) { return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package")); }
		USoundSubmix* NewSubmix = NewObject<USoundSubmix>(Package, FName(*Name), RF_Public | RF_Standalone);
		if (!NewSubmix) { return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create submix")); }

		McpSafeAssetSave(NewSubmix);
		Response->SetStringField(TEXT("assetPath"), NewSubmix->GetPathName());
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Submix '%s' created"), *Name));
		McpHandlerUtils::AddVerification(Response, NewSubmix);
		return Response;
#else
		FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Effects")), false);
		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }
		Response->SetStringField(TEXT("assetPath"), Path / Name);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Submix '%s' noted - AudioMixer module not available"), *Name));
		Response->SetStringField(TEXT("note"), TEXT("Enable AudioMixer plugin for full submix support"));
		return Response;
#endif
	}

	return nullptr;
}
}
#endif
