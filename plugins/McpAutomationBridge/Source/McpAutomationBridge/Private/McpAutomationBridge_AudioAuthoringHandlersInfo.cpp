#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleAudioInfoActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction != TEXT("get_audio_info"))
	{
		return nullptr;
	}

	FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Asset)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("Could not load asset: %s"), *AssetPath));
	}

	Response->SetStringField(TEXT("assetPath"), AssetPath);
	Response->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
	if (USoundCue* Cue = Cast<USoundCue>(Asset))
	{
		Response->SetStringField(TEXT("type"), TEXT("SoundCue"));
		Response->SetNumberField(TEXT("duration"), Cue->Duration);
		Response->SetNumberField(TEXT("nodeCount"), Cue->AllNodes.Num());
		if (Cue->AttenuationSettings)
		{
			Response->SetStringField(TEXT("attenuationPath"), Cue->AttenuationSettings->GetPathName());
		}
	}
	else if (USoundWave* Wave = Cast<USoundWave>(Asset))
	{
		Response->SetStringField(TEXT("type"), TEXT("SoundWave"));
		Response->SetNumberField(TEXT("duration"), Wave->Duration);
		Response->SetNumberField(TEXT("sampleRate"), Wave->GetSampleRateForCurrentPlatform());
		Response->SetNumberField(TEXT("numChannels"), Wave->NumChannels);
	}
	else if (USoundClass* SoundClass = Cast<USoundClass>(Asset))
	{
		Response->SetStringField(TEXT("type"), TEXT("SoundClass"));
		Response->SetNumberField(TEXT("volume"), SoundClass->Properties.Volume);
		Response->SetNumberField(TEXT("pitch"), SoundClass->Properties.Pitch);
		if (SoundClass->ParentClass)
		{
			Response->SetStringField(TEXT("parentClass"), SoundClass->ParentClass->GetPathName());
		}
	}
	else if (USoundMix* Mix = Cast<USoundMix>(Asset))
	{
		Response->SetStringField(TEXT("type"), TEXT("SoundMix"));
		Response->SetNumberField(TEXT("modifierCount"), Mix->SoundClassEffects.Num());
	}
	else if (USoundAttenuation* Atten = Cast<USoundAttenuation>(Asset))
	{
		Response->SetStringField(TEXT("type"), TEXT("SoundAttenuation"));
		Response->SetNumberField(TEXT("falloffDistance"), Atten->Attenuation.FalloffDistance);
		Response->SetBoolField(TEXT("spatialize"), Atten->Attenuation.bSpatialize);
	}
	else
	{
		Response->SetStringField(TEXT("type"), TEXT("Unknown"));
	}

	Response->SetBoolField(TEXT("success"), true);
	return Response;
}
}
#endif
