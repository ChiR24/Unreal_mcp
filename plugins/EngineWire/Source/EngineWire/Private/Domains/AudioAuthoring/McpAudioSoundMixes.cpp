#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAudioAuthoringPrivate.h"

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

		FString CreateError, CreateCode;
		UPackage* Package = McpCreatePackageForNewAsset(PackagePath, CreateError, CreateCode);
		if (!Package)
		{
			return McpHandlerUtils::BuildErrorResponse(CreateCode, CreateError);
		}

		USoundMixFactory* Factory = NewObject<USoundMixFactory>();
		USoundMix* NewMix = Cast<USoundMix>(Factory->FactoryCreateNew(USoundMix::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
		if (!NewMix)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create SoundMix"));
		}

		Response->SetBoolField(TEXT("saved"), SaveAudioAsset(NewMix, bSave));
		Response->SetStringField(TEXT("assetPath"), NewMix->GetPathName());
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("SoundMix '%s' created"), *NewMix->GetName()));
		McpHandlerUtils::AddVerification(Response, NewMix);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("add_mix_modifier"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		FString SoundClassPath = GetJsonStringField(Params, TEXT("soundClassPath"), TEXT(""));
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

		// One adjuster per class: a second call for the class stacked a second adjuster on it. It edits the one there
		// instead, keeping what it is not sent; a new one starts at volume and pitch 1, applying to child classes.
		Mix->Modify();
		FSoundClassAdjuster* Adjuster = Mix->SoundClassEffects.FindByPredicate([SoundClass](const FSoundClassAdjuster& Entry) { return Entry.SoundClassObject == SoundClass; });
		const bool bReplaced = Adjuster != nullptr;
		if (!Adjuster)
		{
			Adjuster = &Mix->SoundClassEffects.AddDefaulted_GetRef();
			Adjuster->SoundClassObject = SoundClass;
			Adjuster->bApplyToChildren = true;
		}
		if (Params->HasField(TEXT("volumeAdjuster"))) { Adjuster->VolumeAdjuster = static_cast<float>(GetJsonNumberField(Params, TEXT("volumeAdjuster"))); }
		if (Params->HasField(TEXT("pitchAdjuster"))) { Adjuster->PitchAdjuster = static_cast<float>(GetJsonNumberField(Params, TEXT("pitchAdjuster"))); }
		if (Params->HasField(TEXT("applyToChildren"))) { Adjuster->bApplyToChildren = GetJsonBoolField(Params, TEXT("applyToChildren"), true); }
		// A class adjuster has no fade of its own: the fade times belong to the mix. They were read and dropped.
		if (Params->HasField(TEXT("fadeInTime"))) { Mix->FadeInTime = static_cast<float>(GetJsonNumberField(Params, TEXT("fadeInTime"))); }
		if (Params->HasField(TEXT("fadeOutTime"))) { Mix->FadeOutTime = static_cast<float>(GetJsonNumberField(Params, TEXT("fadeOutTime"))); }
		Response->SetBoolField(TEXT("saved"), SaveAudioAsset(Mix, bSave));
		Response->SetNumberField(TEXT("fadeInTime"), Mix->FadeInTime);
		Response->SetNumberField(TEXT("fadeOutTime"), Mix->FadeOutTime);

		Response->SetStringField(TEXT("soundClassPath"), SoundClass->GetPathName());
		Response->SetBoolField(TEXT("replaced"), bReplaced);
		Response->SetNumberField(TEXT("volumeAdjuster"), Adjuster->VolumeAdjuster);
		Response->SetNumberField(TEXT("pitchAdjuster"), Adjuster->PitchAdjuster);
		Response->SetBoolField(TEXT("applyToChildren"), Adjuster->bApplyToChildren != 0);
		Response->SetNumberField(TEXT("modifierCount"), Mix->SoundClassEffects.Num());
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Modifier for %s %s %s"), *SoundClass->GetName(),
			bReplaced ? TEXT("updated on") : TEXT("added to"), *Mix->GetName()));
		McpHandlerUtils::AddVerification(Response, Mix);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	return nullptr;
}
}
