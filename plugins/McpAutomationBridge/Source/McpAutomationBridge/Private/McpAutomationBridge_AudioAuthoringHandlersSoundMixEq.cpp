#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
static void ClampMixEq(USoundMix* Mix)
{
	auto ClampGain = [](float& Value) { Value = FMath::Clamp(Value, 0.0f, 4.0f); };
	auto ClampFreq = [](float& Value) { Value = FMath::Clamp(Value, 0.0f, 20000.0f); };
	auto ClampBandwidth = [](float& Value) { Value = FMath::Clamp(Value, 0.0f, 2.0f); };
	ClampGain(Mix->EQSettings.Gain0);
	ClampGain(Mix->EQSettings.Gain1);
	ClampGain(Mix->EQSettings.Gain2);
	ClampGain(Mix->EQSettings.Gain3);
	ClampFreq(Mix->EQSettings.FrequencyCenter0);
	ClampFreq(Mix->EQSettings.FrequencyCenter1);
	ClampFreq(Mix->EQSettings.FrequencyCenter2);
	ClampFreq(Mix->EQSettings.FrequencyCenter3);
	ClampBandwidth(Mix->EQSettings.Bandwidth0);
	ClampBandwidth(Mix->EQSettings.Bandwidth1);
	ClampBandwidth(Mix->EQSettings.Bandwidth2);
	ClampBandwidth(Mix->EQSettings.Bandwidth3);
}

TSharedPtr<FJsonObject> HandleSoundMixEqActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction != TEXT("configure_mix_eq"))
	{
		return nullptr;
	}

	FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
	bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
	USoundMix* Mix = LoadSoundMixFromPath(AssetPath);
	if (!Mix)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("MIX_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundMix: %s"), *AssetPath));
	}

	Mix->bApplyEQ = McpHandlerUtils::GetOptionalBool(Params, TEXT("applyEQ"), true);
	if (Params->HasField(TEXT("eqPriority")))
	{
		Mix->EQPriority = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("eqPriority"), 1.0));
	}

	const TSharedPtr<FJsonObject>* EQObj;
	if (Params->TryGetObjectField(TEXT("eqSettings"), EQObj) && EQObj->IsValid())
	{
		if ((*EQObj)->HasField(TEXT("frequencyCenter0"))) { Mix->EQSettings.FrequencyCenter0 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("frequencyCenter0"))); }
		if ((*EQObj)->HasField(TEXT("gain0"))) { Mix->EQSettings.Gain0 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("gain0"))); }
		if ((*EQObj)->HasField(TEXT("bandwidth0"))) { Mix->EQSettings.Bandwidth0 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("bandwidth0"))); }
		if ((*EQObj)->HasField(TEXT("frequencyCenter1"))) { Mix->EQSettings.FrequencyCenter1 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("frequencyCenter1"))); }
		if ((*EQObj)->HasField(TEXT("gain1"))) { Mix->EQSettings.Gain1 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("gain1"))); }
		if ((*EQObj)->HasField(TEXT("bandwidth1"))) { Mix->EQSettings.Bandwidth1 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("bandwidth1"))); }
		if ((*EQObj)->HasField(TEXT("frequencyCenter2"))) { Mix->EQSettings.FrequencyCenter2 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("frequencyCenter2"))); }
		if ((*EQObj)->HasField(TEXT("gain2"))) { Mix->EQSettings.Gain2 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("gain2"))); }
		if ((*EQObj)->HasField(TEXT("bandwidth2"))) { Mix->EQSettings.Bandwidth2 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("bandwidth2"))); }
		if ((*EQObj)->HasField(TEXT("frequencyCenter3"))) { Mix->EQSettings.FrequencyCenter3 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("frequencyCenter3"))); }
		if ((*EQObj)->HasField(TEXT("gain3"))) { Mix->EQSettings.Gain3 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("gain3"))); }
		if ((*EQObj)->HasField(TEXT("bandwidth3"))) { Mix->EQSettings.Bandwidth3 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("bandwidth3"))); }
	}
	else
	{
		if (Params->HasField(TEXT("lowFrequency"))) { Mix->EQSettings.FrequencyCenter0 = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("lowFrequency"), 600.0)); }
		if (Params->HasField(TEXT("lowGain"))) { Mix->EQSettings.Gain0 = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("lowGain"), 1.0)); }
		if (Params->HasField(TEXT("midFrequency"))) { Mix->EQSettings.FrequencyCenter1 = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("midFrequency"), 1000.0)); }
		if (Params->HasField(TEXT("midGain"))) { Mix->EQSettings.Gain1 = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("midGain"), 1.0)); }
		if (Params->HasField(TEXT("highMidFrequency"))) { Mix->EQSettings.FrequencyCenter2 = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("highMidFrequency"), 2000.0)); }
		if (Params->HasField(TEXT("highMidGain"))) { Mix->EQSettings.Gain2 = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("highMidGain"), 1.0)); }
		if (Params->HasField(TEXT("highFrequency"))) { Mix->EQSettings.FrequencyCenter3 = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("highFrequency"), 10000.0)); }
		if (Params->HasField(TEXT("highGain"))) { Mix->EQSettings.Gain3 = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("highGain"), 1.0)); }
	}

	ClampMixEq(Mix);
	SaveAudioAsset(Mix, bSave);
	TSharedPtr<FJsonObject> EQInfo = McpHandlerUtils::CreateResultObject();
	EQInfo->SetNumberField(TEXT("frequencyCenter0"), Mix->EQSettings.FrequencyCenter0);
	EQInfo->SetNumberField(TEXT("gain0"), Mix->EQSettings.Gain0);
	EQInfo->SetNumberField(TEXT("frequencyCenter1"), Mix->EQSettings.FrequencyCenter1);
	EQInfo->SetNumberField(TEXT("gain1"), Mix->EQSettings.Gain1);
	EQInfo->SetNumberField(TEXT("frequencyCenter2"), Mix->EQSettings.FrequencyCenter2);
	EQInfo->SetNumberField(TEXT("gain2"), Mix->EQSettings.Gain2);
	EQInfo->SetNumberField(TEXT("frequencyCenter3"), Mix->EQSettings.FrequencyCenter3);
	EQInfo->SetNumberField(TEXT("gain3"), Mix->EQSettings.Gain3);
	Response->SetObjectField(TEXT("eqSettings"), EQInfo);
	McpHandlerUtils::AddVerification(Response, Mix);
	Response->SetBoolField(TEXT("success"), true);
	return Response;
}
}
#endif
