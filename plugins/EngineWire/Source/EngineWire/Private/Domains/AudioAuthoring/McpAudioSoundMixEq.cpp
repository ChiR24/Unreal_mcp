#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAudioAuthoringPrivate.h"

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

	FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
	bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
	USoundMix* Mix = LoadSoundMixFromPath(AssetPath);
	if (!Mix)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("MIX_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundMix: %s"), *AssetPath));
	}

	// A call sending nothing switched the EQ on, saved and answered success. Bands switch it on unless applyEQ is sent.
	static const TCHAR* BandFields[] = {TEXT("eqSettings"), TEXT("lowFrequency"), TEXT("lowGain"), TEXT("midFrequency"), TEXT("midGain"),
		TEXT("highMidFrequency"), TEXT("highMidGain"), TEXT("highFrequency"), TEXT("highGain")};
	bool bBandsSent = false;
	for (const TCHAR* Field : BandFields)
	{
		bBandsSent |= Params->HasField(Field);
	}
	if (!bBandsSent && !Params->HasField(TEXT("applyEQ")) && !Params->HasField(TEXT("eqPriority")))
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), TEXT("Nothing to set: send applyEQ, eqPriority, eqSettings or a band (lowFrequency, lowGain, midFrequency, ...)."));
	}
	Mix->Modify();
	if (Params->HasField(TEXT("applyEQ")) || bBandsSent)
	{
		Mix->bApplyEQ = GetJsonBoolField(Params, TEXT("applyEQ"), true);
	}
	if (Params->HasField(TEXT("eqPriority")))
	{
		Mix->EQPriority = static_cast<float>(GetJsonNumberField(Params, TEXT("eqPriority"), 1.0));
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
	// The flat band fields apply on top of eqSettings rather than being dropped when both are sent.
	{
		if (Params->HasField(TEXT("lowFrequency"))) { Mix->EQSettings.FrequencyCenter0 = static_cast<float>(GetJsonNumberField(Params, TEXT("lowFrequency"), 600.0)); }
		if (Params->HasField(TEXT("lowGain"))) { Mix->EQSettings.Gain0 = static_cast<float>(GetJsonNumberField(Params, TEXT("lowGain"), 1.0)); }
		if (Params->HasField(TEXT("midFrequency"))) { Mix->EQSettings.FrequencyCenter1 = static_cast<float>(GetJsonNumberField(Params, TEXT("midFrequency"), 1000.0)); }
		if (Params->HasField(TEXT("midGain"))) { Mix->EQSettings.Gain1 = static_cast<float>(GetJsonNumberField(Params, TEXT("midGain"), 1.0)); }
		if (Params->HasField(TEXT("highMidFrequency"))) { Mix->EQSettings.FrequencyCenter2 = static_cast<float>(GetJsonNumberField(Params, TEXT("highMidFrequency"), 2000.0)); }
		if (Params->HasField(TEXT("highMidGain"))) { Mix->EQSettings.Gain2 = static_cast<float>(GetJsonNumberField(Params, TEXT("highMidGain"), 1.0)); }
		if (Params->HasField(TEXT("highFrequency"))) { Mix->EQSettings.FrequencyCenter3 = static_cast<float>(GetJsonNumberField(Params, TEXT("highFrequency"), 10000.0)); }
		if (Params->HasField(TEXT("highGain"))) { Mix->EQSettings.Gain3 = static_cast<float>(GetJsonNumberField(Params, TEXT("highGain"), 1.0)); }
	}

	ClampMixEq(Mix);
	Response->SetBoolField(TEXT("saved"), SaveAudioAsset(Mix, bSave));
	// Every band after clamping (gain 0-4, frequency 0-20000, bandwidth 0-2), under the eqSettings names.
	const FAudioEQEffect& EQ = Mix->EQSettings;
	const TPair<const TCHAR*, float> Bands[] = {
		{TEXT("frequencyCenter0"), EQ.FrequencyCenter0}, {TEXT("gain0"), EQ.Gain0}, {TEXT("bandwidth0"), EQ.Bandwidth0},
		{TEXT("frequencyCenter1"), EQ.FrequencyCenter1}, {TEXT("gain1"), EQ.Gain1}, {TEXT("bandwidth1"), EQ.Bandwidth1},
		{TEXT("frequencyCenter2"), EQ.FrequencyCenter2}, {TEXT("gain2"), EQ.Gain2}, {TEXT("bandwidth2"), EQ.Bandwidth2},
		{TEXT("frequencyCenter3"), EQ.FrequencyCenter3}, {TEXT("gain3"), EQ.Gain3}, {TEXT("bandwidth3"), EQ.Bandwidth3}};
	TSharedPtr<FJsonObject> EQInfo = McpHandlerUtils::CreateResultObject();
	for (const TPair<const TCHAR*, float>& Band : Bands)
	{
		EQInfo->SetNumberField(Band.Key, Band.Value);
	}
	Response->SetObjectField(TEXT("eqSettings"), EQInfo);
	Response->SetBoolField(TEXT("applyEQ"), Mix->bApplyEQ != 0);
	Response->SetNumberField(TEXT("eqPriority"), Mix->EQPriority);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("EQ of %s configured (%s)"), *Mix->GetName(), Mix->bApplyEQ ? TEXT("applied") : TEXT("off")));
	McpHandlerUtils::AddVerification(Response, Mix);
	Response->SetBoolField(TEXT("success"), true);
	return Response;
}
}
