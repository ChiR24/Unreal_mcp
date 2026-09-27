#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleEffectActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("create_reverb_effect"))
	{
		FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/Effects")), false);
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }

		UPackage* Package = CreatePackage(*(Path / Name));
		if (!Package) { return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package")); }
		UReverbEffect* NewEffect = NewObject<UReverbEffect>(Package, FName(*Name), RF_Public | RF_Standalone);
		if (!NewEffect) { return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create ReverbEffect")); }

		if (Params->HasField(TEXT("density"))) { NewEffect->Density = static_cast<float>(GetJsonNumberField(Params, TEXT("density"), 1.0)); }
		if (Params->HasField(TEXT("diffusion"))) { NewEffect->Diffusion = static_cast<float>(GetJsonNumberField(Params, TEXT("diffusion"), 1.0)); }
		if (Params->HasField(TEXT("gain"))) { NewEffect->Gain = static_cast<float>(GetJsonNumberField(Params, TEXT("gain"), 0.32)); }
		if (Params->HasField(TEXT("gainHF"))) { NewEffect->GainHF = static_cast<float>(GetJsonNumberField(Params, TEXT("gainHF"), 0.89)); }
		if (Params->HasField(TEXT("decayTime"))) { NewEffect->DecayTime = static_cast<float>(GetJsonNumberField(Params, TEXT("decayTime"), 1.49)); }
		if (Params->HasField(TEXT("decayHFRatio"))) { NewEffect->DecayHFRatio = static_cast<float>(GetJsonNumberField(Params, TEXT("decayHFRatio"), 0.83)); }

		SaveAudioAsset(NewEffect, bSave);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("assetPath"), NewEffect->GetPathName());
		Response->SetStringField(TEXT("message"),
			FString::Printf(TEXT("ReverbEffect '%s' created"), *NewEffect->GetName()));
		McpHandlerUtils::AddVerification(Response, NewEffect);
		return Response;
	}

	if (SubAction == TEXT("create_source_effect_chain"))
	{
		FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/Effects")), false);
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
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
	}

	if (SubAction == TEXT("add_source_effect"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		FString EffectPresetPath = GetJsonStringField(Params, TEXT("effectPresetPath"), TEXT(""));
		FString EffectType = GetJsonStringField(Params, TEXT("effectType"), TEXT(""));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
		if (AssetPath.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required")); }

		USoundEffectSourcePresetChain* Chain = Cast<USoundEffectSourcePresetChain>(StaticLoadObject(USoundEffectSourcePresetChain::StaticClass(), nullptr, *AssetPath));
		if (!Chain) { return McpHandlerUtils::BuildErrorResponse(TEXT("CHAIN_NOT_FOUND"), FString::Printf(TEXT("Could not load source effect chain: %s"), *AssetPath)); }

		USoundEffectSourcePreset* EffectPreset = nullptr;
		if (!EffectPresetPath.IsEmpty())
		{
			EffectPreset = Cast<USoundEffectSourcePreset>(StaticLoadObject(USoundEffectSourcePreset::StaticClass(), nullptr, *NormalizeAudioPath(EffectPresetPath)));
		}
#if MCP_HAS_SOURCE_EFFECT_PRESETS
		if (!EffectPreset && !EffectType.IsEmpty())
		{
			const FString PresetPkg = FPackageName::ObjectPathToPackageName(AssetPath + TEXT("_") + EffectType);
			UPackage* PresetPackage = CreatePackage(*PresetPkg);
			if (PresetPackage)
			{
				EffectPreset = CreateSourceEffectPresetByType(EffectType, PresetPackage);
				if (EffectPreset)
				{
					EffectPreset->SetFlags(RF_Public | RF_Standalone);
					McpSafeAssetSave(EffectPreset);
					Response->SetStringField(TEXT("effectPresetPath"), EffectPreset->GetPathName());
				}
			}
		}
#endif
		if (EffectPreset)
		{
			FSourceEffectChainEntry NewEntry;
			NewEntry.Preset = EffectPreset;
			NewEntry.bBypass = GetJsonBoolField(Params, TEXT("bypass"), false);
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
			Response->SetStringField(TEXT("code"), TEXT("PRESET_NOT_FOUND"));
		}
		return Response;
	}

	if (SubAction == TEXT("create_submix_effect"))
	{
		FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
		FString EffectType = GetJsonStringField(Params, TEXT("effectType"), TEXT(""));
		FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/Effects")), false);
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
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
	}

	return nullptr;
}
}
