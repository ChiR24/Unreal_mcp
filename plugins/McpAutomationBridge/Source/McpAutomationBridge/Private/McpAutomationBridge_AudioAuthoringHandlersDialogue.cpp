#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleDialogueActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("create_dialogue_voice"))
	{
#if MCP_HAS_DIALOGUE && MCP_HAS_DIALOGUE_FACTORY
		FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Dialogue")), false);
		FString Gender = McpHandlerUtils::GetOptionalString(Params, TEXT("gender"), TEXT("Masculine"));
		FString Plurality = McpHandlerUtils::GetOptionalString(Params, TEXT("plurality"), TEXT("Singular"));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }
		if (Name.Len() > 100) { return McpHandlerUtils::BuildErrorResponse(TEXT("NAME_TOO_LONG"), TEXT("Asset name exceeds maximum length of 100 characters")); }

		UPackage* Package = CreatePackage(*(Path / Name));
		if (!Package) { return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package")); }

		UDialogueVoiceFactory* Factory = NewObject<UDialogueVoiceFactory>();
		UDialogueVoice* NewVoice = Cast<UDialogueVoice>(Factory->FactoryCreateNew(UDialogueVoice::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
		if (!NewVoice) { return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create DialogueVoice")); }

		if (Gender.ToLower() == TEXT("masculine")) { NewVoice->Gender = EGrammaticalGender::Masculine; }
		else if (Gender.ToLower() == TEXT("feminine")) { NewVoice->Gender = EGrammaticalGender::Feminine; }
		else if (Gender.ToLower() == TEXT("neuter")) { NewVoice->Gender = EGrammaticalGender::Neuter; }
		if (Plurality.ToLower() == TEXT("singular")) { NewVoice->Plurality = EGrammaticalNumber::Singular; }
		else if (Plurality.ToLower() == TEXT("plural")) { NewVoice->Plurality = EGrammaticalNumber::Plural; }

		SaveAudioAsset(NewVoice, bSave);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("assetPath"), NewVoice->GetPathName());
		McpHandlerUtils::AddVerification(Response, NewVoice);
		return Response;
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("DIALOGUE_NOT_AVAILABLE"), TEXT("Dialogue system not available"));
#endif
	}

	if (SubAction == TEXT("create_dialogue_wave"))
	{
#if MCP_HAS_DIALOGUE && MCP_HAS_DIALOGUE_FACTORY
		FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Dialogue")), false);
		FString SpokenText = McpHandlerUtils::GetOptionalString(Params, TEXT("spokenText"), TEXT(""));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

		if (Name.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required")); }
		if (Name.Len() > 100) { return McpHandlerUtils::BuildErrorResponse(TEXT("NAME_TOO_LONG"), TEXT("Asset name exceeds maximum length of 100 characters")); }

		UPackage* Package = CreatePackage(*(Path / Name));
		if (!Package) { return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package")); }

		UDialogueWaveFactory* Factory = NewObject<UDialogueWaveFactory>();
		UDialogueWave* NewWave = Cast<UDialogueWave>(Factory->FactoryCreateNew(UDialogueWave::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
		if (!NewWave) { return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create DialogueWave")); }

		NewWave->SpokenText = SpokenText;
		SaveAudioAsset(NewWave, bSave);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("assetPath"), NewWave->GetPathName());
		McpHandlerUtils::AddVerification(Response, NewWave);
		return Response;
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("DIALOGUE_NOT_AVAILABLE"), TEXT("Dialogue system not available"));
#endif
	}

	if (SubAction == TEXT("set_dialogue_context"))
	{
#if MCP_HAS_DIALOGUE
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString SpeakerPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("speakerPath"), TEXT("")));
		FString SoundWavePath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("soundWavePath"), TEXT("")));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

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
					if (TargetVoice) { TargetVoices.Add(TargetVoice); }
				}
			}
		}

		FDialogueContextMapping NewMapping;
		NewMapping.Context.Speaker = SpeakerVoice;
		for (UDialogueVoice* TargetVoice : TargetVoices) { NewMapping.Context.Targets.Add(TargetVoice); }
		NewMapping.SoundWave = ContextSoundWave;
		NewMapping.LocalizationKeyFormat = McpHandlerUtils::GetOptionalString(Params, TEXT("localizationKeyFormat"), TEXT("{ContextHash}"));

		bool bReplaceExisting = McpHandlerUtils::GetOptionalBool(Params, TEXT("replace"), false);
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
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("DIALOGUE_NOT_AVAILABLE"), TEXT("Dialogue system not available"));
#endif
	}

	return nullptr;
}
}
#endif
