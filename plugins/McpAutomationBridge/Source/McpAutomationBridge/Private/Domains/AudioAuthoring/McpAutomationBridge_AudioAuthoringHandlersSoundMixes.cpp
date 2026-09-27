#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleSoundMixActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("create_sound_mix"))
	{
		FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/Mixes")), false);
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

		if (Name.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required"));
		}

		FString PackagePath;
		FString PathError;
		if (!BuildAudioCreationPath(Path, Name, PackagePath, PathError))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ASSET_PATH"), PathError);
		}

		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package"));
		}

		USoundMixFactory* Factory = NewObject<USoundMixFactory>();
		USoundMix* NewMix = Cast<USoundMix>(Factory->FactoryCreateNew(USoundMix::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
		if (!NewMix)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create SoundMix"));
		}

		SaveAudioAsset(NewMix, bSave);
		Response->SetStringField(TEXT("assetPath"), NewMix->GetPathName());
		McpHandlerUtils::AddVerification(Response, NewMix);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("add_mix_modifier"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		FString SoundClassPath = GetJsonStringField(Params, TEXT("soundClassPath"), TEXT(""));
		float VolumeAdjust = static_cast<float>(GetJsonNumberField(Params, TEXT("volumeAdjuster"), 1.0));
		float PitchAdjust = static_cast<float>(GetJsonNumberField(Params, TEXT("pitchAdjuster"), 1.0));
		bool bApplyToChildren = GetJsonBoolField(Params, TEXT("applyToChildren"), true);
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

		USoundMix* Mix = LoadSoundMixFromPath(AssetPath);
		if (!Mix)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("MIX_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundMix: %s"), *AssetPath));
		}

		USoundClass* SoundClass = LoadSoundClassFromPath(SoundClassPath);
		if (!SoundClass)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CLASS_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundClass: %s"), *SoundClassPath));
		}

		FSoundClassAdjuster Adjuster;
		Adjuster.SoundClassObject = SoundClass;
		Adjuster.VolumeAdjuster = VolumeAdjust;
		Adjuster.PitchAdjuster = PitchAdjust;
		Adjuster.bApplyToChildren = bApplyToChildren;
		Mix->SoundClassEffects.Add(Adjuster);
		// A class adjuster has no fade of its own: the fade times belong to the mix. They were read and dropped.
		if (Params->HasField(TEXT("fadeInTime"))) { Mix->FadeInTime = static_cast<float>(GetJsonNumberField(Params, TEXT("fadeInTime"))); }
		if (Params->HasField(TEXT("fadeOutTime"))) { Mix->FadeOutTime = static_cast<float>(GetJsonNumberField(Params, TEXT("fadeOutTime"))); }
		SaveAudioAsset(Mix, bSave);
		Response->SetNumberField(TEXT("fadeInTime"), Mix->FadeInTime);
		Response->SetNumberField(TEXT("fadeOutTime"), Mix->FadeOutTime);

		Response->SetStringField(TEXT("soundClassPath"), SoundClassPath);
		Response->SetNumberField(TEXT("volumeAdjuster"), VolumeAdjust);
		Response->SetNumberField(TEXT("pitchAdjuster"), PitchAdjust);
		Response->SetBoolField(TEXT("applyToChildren"), bApplyToChildren);
		Response->SetNumberField(TEXT("modifierCount"), Mix->SoundClassEffects.Num());
		McpHandlerUtils::AddVerification(Response, Mix);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	return nullptr;
}
}
