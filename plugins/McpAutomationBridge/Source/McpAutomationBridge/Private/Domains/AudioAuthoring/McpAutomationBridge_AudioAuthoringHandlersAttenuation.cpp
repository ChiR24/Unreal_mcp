#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleAttenuationActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("create_attenuation_settings"))
	{
		FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/Attenuation")), false);
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

		if (Name.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required"));
		}

		FString PackagePath = Path / Name;
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package"));
		}

		USoundAttenuationFactory* Factory = NewObject<USoundAttenuationFactory>();
		USoundAttenuation* NewAtten = Cast<USoundAttenuation>(Factory->FactoryCreateNew(USoundAttenuation::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
		if (!NewAtten)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create SoundAttenuation"));
		}

		if (Params->HasField(TEXT("innerRadius"))) { SetAttenuationInnerRadius(NewAtten->Attenuation, GetJsonNumberField(Params, TEXT("innerRadius"), 400.0)); }
		if (Params->HasField(TEXT("falloffDistance"))) { NewAtten->Attenuation.FalloffDistance = static_cast<float>(GetJsonNumberField(Params, TEXT("falloffDistance"), 3600.0)); }
		SaveAudioAsset(NewAtten, bSave);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("assetPath"), NewAtten->GetPathName());
		// Siblings of this capability name what they made ("SoundCue 'X' created");
		// these three fell through to the generic "Operation complete".
		Response->SetStringField(TEXT("message"),
			FString::Printf(TEXT("SoundAttenuation '%s' created"), *NewAtten->GetName()));
		McpHandlerUtils::AddVerification(Response, NewAtten);
		return Response;
	}

	if (SubAction == TEXT("configure_distance_attenuation"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
		USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
		if (!Atten)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ATTENUATION_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath));
		}

		// Only when sent: defaulting to "linear" reset every curve a caller had chosen before.
		const FString FunctionType = GetJsonStringField(Params, TEXT("distanceAlgorithm")).ToLower();
		if (FunctionType == TEXT("linear")) { Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear; }
		else if (FunctionType == TEXT("logarithmic")) { Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic; }
		else if (FunctionType == TEXT("inverse")) { Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Inverse; }
		else if (FunctionType == TEXT("naturalsound")) { Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; }
		else if (!FunctionType.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), FString::Printf(TEXT("distanceAlgorithm '%s' is not Linear, Logarithmic, Inverse or NaturalSound"), *FunctionType));
		}
		if (Params->HasField(TEXT("innerRadius"))) { SetAttenuationInnerRadius(Atten->Attenuation, GetJsonNumberField(Params, TEXT("innerRadius"), 400.0)); }
		if (Params->HasField(TEXT("falloffDistance"))) { Atten->Attenuation.FalloffDistance = static_cast<float>(GetJsonNumberField(Params, TEXT("falloffDistance"), 3600.0)); }

		SaveAudioAsset(Atten, bSave);
		McpHandlerUtils::AddVerification(Response, Atten);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("configure_spatialization"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
		USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
		if (!Atten)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ATTENUATION_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath));
		}

		// The contract names the algorithm `spatialization`; only `spatializationAlgorithm` was read.
		const FString Algorithm = McpGetFirstStringField(Params, {TEXT("spatialization"), TEXT("spatializationAlgorithm")}).ToLower();
		if (Algorithm == TEXT("default") || Algorithm == TEXT("panner")) { Atten->Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default; }
		else if (Algorithm == TEXT("binaural") || Algorithm == TEXT("hrtf")) { Atten->Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF; }
		else if (!Algorithm.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), FString::Printf(TEXT("spatialization '%s' is not Default or Binaural"), *Algorithm));
		}
		Atten->Attenuation.bSpatialize = GetJsonBoolField(Params, TEXT("spatialize"), true);

		SaveAudioAsset(Atten, bSave);
		Response->SetBoolField(TEXT("spatialize"), Atten->Attenuation.bSpatialize);
		FString AlgoName = TEXT("panner");
		switch (Atten->Attenuation.SpatializationAlgorithm)
		{
		case ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF:
			AlgoName = TEXT("HRTF");
			break;
		default:
			break;
		}
		Response->SetStringField(TEXT("spatializationAlgorithm"), AlgoName);
		McpHandlerUtils::AddVerification(Response, Atten);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("configure_occlusion"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
		USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
		if (!Atten)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ATTENUATION_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath));
		}

		// The contract spells these enable, occlusionVolumeScale and occlusionFilterScale (as set_audio_occlusion
		// does); only the engine field names were read, so enable:false could never arrive and every call turned
		// occlusion on. The engine spellings stay as fallbacks.
		Atten->Attenuation.bEnableOcclusion = GetJsonBoolField(Params, TEXT("enable"), GetJsonBoolField(Params, TEXT("enableOcclusion"), true));
		if (Params->HasField(TEXT("occlusionFilterScale"))) { Atten->Attenuation.OcclusionLowPassFilterFrequency = static_cast<float>(20000.0 * GetJsonNumberField(Params, TEXT("occlusionFilterScale"), 1.0)); }
		else if (Params->HasField(TEXT("occlusionLowPassFilterFrequency"))) { Atten->Attenuation.OcclusionLowPassFilterFrequency = static_cast<float>(GetJsonNumberField(Params, TEXT("occlusionLowPassFilterFrequency"), 20000.0)); }
		if (Params->HasField(TEXT("occlusionVolumeScale"))) { Atten->Attenuation.OcclusionVolumeAttenuation = static_cast<float>(GetJsonNumberField(Params, TEXT("occlusionVolumeScale"), 0.0)); }
		else if (Params->HasField(TEXT("occlusionVolumeAttenuation"))) { Atten->Attenuation.OcclusionVolumeAttenuation = static_cast<float>(GetJsonNumberField(Params, TEXT("occlusionVolumeAttenuation"), 0.0)); }
		if (Params->HasField(TEXT("occlusionInterpolationTime"))) { Atten->Attenuation.OcclusionInterpolationTime = static_cast<float>(GetJsonNumberField(Params, TEXT("occlusionInterpolationTime"), 0.5)); }

		SaveAudioAsset(Atten, bSave);
		Response->SetBoolField(TEXT("enableOcclusion"), Atten->Attenuation.bEnableOcclusion);
		Response->SetNumberField(TEXT("occlusionLowPassFilterFrequency"), Atten->Attenuation.OcclusionLowPassFilterFrequency);
		Response->SetNumberField(TEXT("occlusionVolumeAttenuation"), Atten->Attenuation.OcclusionVolumeAttenuation);
		Response->SetNumberField(TEXT("occlusionInterpolationTime"), Atten->Attenuation.OcclusionInterpolationTime);
		McpHandlerUtils::AddVerification(Response, Atten);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("configure_reverb_send"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
		USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
		if (!Atten)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ATTENUATION_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath));
		}

		Atten->Attenuation.bEnableReverbSend = GetJsonBoolField(Params, TEXT("enableReverbSend"), true);
		if (Params->HasField(TEXT("reverbWetLevelMin"))) { Atten->Attenuation.ReverbWetLevelMin = static_cast<float>(GetJsonNumberField(Params, TEXT("reverbWetLevelMin"), 0.3)); }
		if (Params->HasField(TEXT("reverbWetLevelMax"))) { Atten->Attenuation.ReverbWetLevelMax = static_cast<float>(GetJsonNumberField(Params, TEXT("reverbWetLevelMax"), 0.95)); }
		if (Params->HasField(TEXT("reverbDistanceMin"))) { Atten->Attenuation.ReverbDistanceMin = static_cast<float>(GetJsonNumberField(Params, TEXT("reverbDistanceMin"), 0.0)); }
		if (Params->HasField(TEXT("reverbDistanceMax"))) { Atten->Attenuation.ReverbDistanceMax = static_cast<float>(GetJsonNumberField(Params, TEXT("reverbDistanceMax"), 0.0)); }

		SaveAudioAsset(Atten, bSave);
		Response->SetBoolField(TEXT("enableReverbSend"), Atten->Attenuation.bEnableReverbSend);
		Response->SetNumberField(TEXT("reverbWetLevelMin"), Atten->Attenuation.ReverbWetLevelMin);
		Response->SetNumberField(TEXT("reverbWetLevelMax"), Atten->Attenuation.ReverbWetLevelMax);
		Response->SetNumberField(TEXT("reverbDistanceMin"), Atten->Attenuation.ReverbDistanceMin);
		Response->SetNumberField(TEXT("reverbDistanceMax"), Atten->Attenuation.ReverbDistanceMax);
		McpHandlerUtils::AddVerification(Response, Atten);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	return nullptr;
}
}
