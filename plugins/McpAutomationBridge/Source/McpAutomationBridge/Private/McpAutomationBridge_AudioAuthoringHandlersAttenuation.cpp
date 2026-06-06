#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleAttenuationActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("create_attenuation_settings"))
	{
		FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Attenuation")), false);
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

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

		if (Params->HasField(TEXT("innerRadius"))) { NewAtten->Attenuation.AttenuationShapeExtents.X = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("innerRadius"), 400.0)); }
		if (Params->HasField(TEXT("falloffDistance"))) { NewAtten->Attenuation.FalloffDistance = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("falloffDistance"), 3600.0)); }
		SaveAudioAsset(NewAtten, bSave);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("assetPath"), NewAtten->GetPathName());
		McpHandlerUtils::AddVerification(Response, NewAtten);
		return Response;
	}

	if (SubAction == TEXT("configure_distance_attenuation"))
	{
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
		if (!Atten)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ATTENUATION_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath));
		}

		if (Params->HasField(TEXT("innerRadius"))) { Atten->Attenuation.AttenuationShapeExtents.X = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("innerRadius"), 400.0)); }
		if (Params->HasField(TEXT("falloffDistance"))) { Atten->Attenuation.FalloffDistance = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("falloffDistance"), 3600.0)); }

		FString FunctionType = McpHandlerUtils::GetOptionalString(Params, TEXT("distanceAlgorithm"), TEXT("linear")).ToLower();
		if (FunctionType == TEXT("linear")) { Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear; }
		else if (FunctionType == TEXT("logarithmic")) { Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic; }
		else if (FunctionType == TEXT("inverse")) { Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Inverse; }
		else if (FunctionType == TEXT("naturalsound")) { Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; }

		SaveAudioAsset(Atten, bSave);
		McpHandlerUtils::AddVerification(Response, Atten);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("configure_spatialization"))
	{
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
		if (!Atten)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ATTENUATION_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath));
		}

		Atten->Attenuation.bSpatialize = McpHandlerUtils::GetOptionalBool(Params, TEXT("spatialize"), true);
		if (Params->HasField(TEXT("spatializationAlgorithm")))
		{
			FString Algorithm = McpHandlerUtils::GetOptionalString(Params, TEXT("spatializationAlgorithm"), TEXT("panner"));
			if (Algorithm.ToLower() == TEXT("panner")) { Atten->Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default; }
			else if (Algorithm.ToLower() == TEXT("hrtf") || Algorithm.ToLower() == TEXT("binaural")) { Atten->Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF; }
		}

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
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
		if (!Atten)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ATTENUATION_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath));
		}

		Atten->Attenuation.bEnableOcclusion = McpHandlerUtils::GetOptionalBool(Params, TEXT("enableOcclusion"), true);
		if (Params->HasField(TEXT("occlusionLowPassFilterFrequency"))) { Atten->Attenuation.OcclusionLowPassFilterFrequency = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("occlusionLowPassFilterFrequency"), 20000.0)); }
		if (Params->HasField(TEXT("occlusionVolumeAttenuation"))) { Atten->Attenuation.OcclusionVolumeAttenuation = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("occlusionVolumeAttenuation"), 0.0)); }
		if (Params->HasField(TEXT("occlusionInterpolationTime"))) { Atten->Attenuation.OcclusionInterpolationTime = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("occlusionInterpolationTime"), 0.5)); }

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
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
		if (!Atten)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ATTENUATION_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath));
		}

		Atten->Attenuation.bEnableReverbSend = McpHandlerUtils::GetOptionalBool(Params, TEXT("enableReverbSend"), true);
		if (Params->HasField(TEXT("reverbWetLevelMin"))) { Atten->Attenuation.ReverbWetLevelMin = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("reverbWetLevelMin"), 0.3)); }
		if (Params->HasField(TEXT("reverbWetLevelMax"))) { Atten->Attenuation.ReverbWetLevelMax = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("reverbWetLevelMax"), 0.95)); }
		if (Params->HasField(TEXT("reverbDistanceMin"))) { Atten->Attenuation.ReverbDistanceMin = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("reverbDistanceMin"), 0.0)); }
		if (Params->HasField(TEXT("reverbDistanceMax"))) { Atten->Attenuation.ReverbDistanceMax = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("reverbDistanceMax"), 0.0)); }

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
#endif
