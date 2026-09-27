#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleDialogueActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("create_dialogue_voice"))
	{
		FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/Dialogue")), false);
		FString Gender = GetJsonStringField(Params, TEXT("gender"), TEXT("Masculine"));
		FString Plurality = GetJsonStringField(Params, TEXT("plurality"), TEXT("Singular"));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }
		if (Name.Len() > 100) { return McpHandlerUtils::BuildErrorResponse(TEXT("NAME_TOO_LONG"), TEXT("Asset name exceeds maximum length of 100 characters")); }
		const FString LowerGender = Gender.ToLower();
		const FString LowerPlurality = Plurality.ToLower();
		if (LowerGender != TEXT("masculine") && LowerGender != TEXT("feminine") && LowerGender != TEXT("neuter"))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), FString::Printf(TEXT("gender '%s' is not Masculine, Feminine or Neuter"), *Gender));
		}
		if (LowerPlurality != TEXT("singular") && LowerPlurality != TEXT("plural"))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), FString::Printf(TEXT("plurality '%s' is not Singular or Plural"), *Plurality));
		}

		UPackage* Package = CreatePackage(*(Path / Name));
		if (!Package) { return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package")); }

		UDialogueVoiceFactory* Factory = NewObject<UDialogueVoiceFactory>();
		UDialogueVoice* NewVoice = Cast<UDialogueVoice>(Factory->FactoryCreateNew(UDialogueVoice::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
		if (!NewVoice) { return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create DialogueVoice")); }

		NewVoice->Gender = LowerGender == TEXT("feminine") ? EGrammaticalGender::Feminine
			: LowerGender == TEXT("neuter") ? EGrammaticalGender::Neuter : EGrammaticalGender::Masculine;
		NewVoice->Plurality = LowerPlurality == TEXT("plural") ? EGrammaticalNumber::Plural : EGrammaticalNumber::Singular;

		SaveAudioAsset(NewVoice, bSave);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("assetPath"), NewVoice->GetPathName());
		McpHandlerUtils::AddVerification(Response, NewVoice);
		return Response;
	}

	if (SubAction == TEXT("create_dialogue_wave"))
	{
		FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/Dialogue")), false);
		FString SpokenText = GetJsonStringField(Params, TEXT("spokenText"), TEXT(""));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }
		if (Name.Len() > 100) { return McpHandlerUtils::BuildErrorResponse(TEXT("NAME_TOO_LONG"), TEXT("Asset name exceeds maximum length of 100 characters")); }

		// wavePath and speakerPath were declared and never read: they now seed the first context mapping.
		const FString WavePath = GetJsonStringField(Params, TEXT("wavePath"));
		const FString SpeakerPath = GetJsonStringField(Params, TEXT("speakerPath"));
		USoundWave* ContextWave = WavePath.IsEmpty() ? nullptr : LoadSoundWaveFromPath(WavePath);
		UDialogueVoice* Speaker = SpeakerPath.IsEmpty() ? nullptr : Cast<UDialogueVoice>(StaticLoadObject(UDialogueVoice::StaticClass(), nullptr, *NormalizeAudioPath(SpeakerPath)));
		if (!WavePath.IsEmpty() && !ContextWave) { return McpHandlerUtils::BuildErrorResponse(TEXT("SOUNDWAVE_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundWave: %s"), *WavePath)); }
		if (!SpeakerPath.IsEmpty() && !Speaker) { return McpHandlerUtils::BuildErrorResponse(TEXT("SPEAKER_NOT_FOUND"), FString::Printf(TEXT("Could not load speaker DialogueVoice: %s"), *SpeakerPath)); }

		UPackage* Package = CreatePackage(*(Path / Name));
		if (!Package) { return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package")); }

		UDialogueWaveFactory* Factory = NewObject<UDialogueWaveFactory>();
		UDialogueWave* NewWave = Cast<UDialogueWave>(Factory->FactoryCreateNew(UDialogueWave::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
		if (!NewWave) { return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create DialogueWave")); }

		NewWave->SpokenText = SpokenText;
		if (ContextWave || Speaker)
		{
			// A new wave already carries one empty mapping (UDialogueWave's constructor adds it): fill that one.
			if (NewWave->ContextMappings.Num() == 0) { NewWave->ContextMappings.AddDefaulted(); }
			NewWave->ContextMappings[0].Context.Speaker = Speaker;
			NewWave->ContextMappings[0].SoundWave = ContextWave;
		}
		SaveAudioAsset(NewWave, bSave);
		Response->SetNumberField(TEXT("contextCount"), NewWave->ContextMappings.Num());
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("assetPath"), NewWave->GetPathName());
		McpHandlerUtils::AddVerification(Response, NewWave);
		return Response;
	}

	if (SubAction == TEXT("set_dialogue_context"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		FString SpeakerPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("speakerPath"), TEXT("")));
		FString SoundWavePath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("soundWavePath"), TEXT("")));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

		UDialogueWave* Wave = Cast<UDialogueWave>(StaticLoadObject(UDialogueWave::StaticClass(), nullptr, *AssetPath));
		if (!Wave) { return McpHandlerUtils::BuildErrorResponse(TEXT("WAVE_NOT_FOUND"), FString::Printf(TEXT("Could not load DialogueWave: %s"), *AssetPath)); }

		UDialogueVoice* SpeakerVoice = nullptr;
		if (!SpeakerPath.IsEmpty())
		{
			SpeakerVoice = Cast<UDialogueVoice>(StaticLoadObject(UDialogueVoice::StaticClass(), nullptr, *SpeakerPath));
			if (!SpeakerVoice) { return McpHandlerUtils::BuildErrorResponse(TEXT("SPEAKER_NOT_FOUND"), FString::Printf(TEXT("Could not load speaker DialogueVoice: %s"), *SpeakerPath)); }
		}

		USoundWave* ContextSoundWave = nullptr;
		if (!SoundWavePath.IsEmpty())
		{
			ContextSoundWave = LoadSoundWaveFromPath(SoundWavePath);
			if (!ContextSoundWave) { return McpHandlerUtils::BuildErrorResponse(TEXT("SOUNDWAVE_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundWave: %s"), *SoundWavePath)); }
		}

		TArray<UDialogueVoice*> TargetVoices;
		const TArray<TSharedPtr<FJsonValue>>* TargetArray;
		if (Params->TryGetArrayField(TEXT("targetVoices"), TargetArray))
		{
			for (const TSharedPtr<FJsonValue>& TargetVal : *TargetArray)
			{
				FString TargetPath = NormalizeAudioPath(TargetVal->AsString());
				if (!TargetPath.IsEmpty())
				{
					UDialogueVoice* TargetVoice = Cast<UDialogueVoice>(StaticLoadObject(UDialogueVoice::StaticClass(), nullptr, *TargetPath));
					if (!TargetVoice) { return McpHandlerUtils::BuildErrorResponse(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("Could not load target DialogueVoice: %s"), *TargetPath)); }
					TargetVoices.Add(TargetVoice);
				}
			}
		}

		FDialogueContextMapping NewMapping;
		NewMapping.Context.Speaker = SpeakerVoice;
		for (UDialogueVoice* TargetVoice : TargetVoices) { NewMapping.Context.Targets.Add(TargetVoice); }
		NewMapping.SoundWave = ContextSoundWave;
		NewMapping.LocalizationKeyFormat = GetJsonStringField(Params, TEXT("localizationKeyFormat"), TEXT("{ContextHash}"));

		bool bReplaceExisting = GetJsonBoolField(Params, TEXT("replace"), false);
		if (bReplaceExisting)
		{
			bool bFound = false;
			for (FDialogueContextMapping& Mapping : Wave->ContextMappings)
			{
				if (Mapping.Context.Speaker == SpeakerVoice)
				{
					Mapping = NewMapping;
					bFound = true;
					break;
				}
			}
			if (!bFound) { Wave->ContextMappings.Add(NewMapping); }
		}
		else
		{
			Wave->ContextMappings.Add(NewMapping);
		}

		SaveAudioAsset(Wave, bSave);
		Response->SetNumberField(TEXT("contextCount"), Wave->ContextMappings.Num());
		McpHandlerUtils::AddVerification(Response, Wave);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	return nullptr;
}
}
