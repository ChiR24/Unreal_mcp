#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAudioAuthoringPrivate.h"

namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleSoundMixEqActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction != TEXT("configure_mix_eq"))
	{
		return nullptr;
	}

	bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
	TSharedPtr<FJsonObject> LoadError;
	USoundMix* Mix = LoadAudioAssetOrError<USoundMix>(Params, TEXT("MIX_NOT_FOUND"), TEXT("SoundMix"), LoadError);
	if (!Mix) { return LoadError; }

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

	// Every band value under its eqSettings name, with the most it may be: gain 4, frequency 20000, bandwidth 2.
	struct FMixEqField { const TCHAR* Name; float* Value; float Max; };
	FAudioEQEffect& EQ = Mix->EQSettings;
	const FMixEqField Bands[] = {
		{TEXT("frequencyCenter0"), &EQ.FrequencyCenter0, 20000.0f}, {TEXT("gain0"), &EQ.Gain0, 4.0f}, {TEXT("bandwidth0"), &EQ.Bandwidth0, 2.0f},
		{TEXT("frequencyCenter1"), &EQ.FrequencyCenter1, 20000.0f}, {TEXT("gain1"), &EQ.Gain1, 4.0f}, {TEXT("bandwidth1"), &EQ.Bandwidth1, 2.0f},
		{TEXT("frequencyCenter2"), &EQ.FrequencyCenter2, 20000.0f}, {TEXT("gain2"), &EQ.Gain2, 4.0f}, {TEXT("bandwidth2"), &EQ.Bandwidth2, 2.0f},
		{TEXT("frequencyCenter3"), &EQ.FrequencyCenter3, 20000.0f}, {TEXT("gain3"), &EQ.Gain3, 4.0f}, {TEXT("bandwidth3"), &EQ.Bandwidth3, 2.0f}};
	const TSharedPtr<FJsonObject>* EQObj;
	if (Params->TryGetObjectField(TEXT("eqSettings"), EQObj) && EQObj->IsValid())
	{
		for (const FMixEqField& Band : Bands)
		{
			if ((*EQObj)->HasField(Band.Name)) { *Band.Value = static_cast<float>(GetJsonNumberField(*EQObj, Band.Name)); }
		}
	}
	// The flat band fields apply on top of eqSettings rather than being dropped when both are sent. Each names the
	// Bands entry it writes and the value a field that is no number leaves there.
	struct FMixEqFlatField { const TCHAR* Name; int32 Band; double Default; };
	static const FMixEqFlatField FlatFields[] = {
		{TEXT("lowFrequency"), 0, 600.0}, {TEXT("lowGain"), 1, 1.0}, {TEXT("midFrequency"), 3, 1000.0}, {TEXT("midGain"), 4, 1.0},
		{TEXT("highMidFrequency"), 6, 2000.0}, {TEXT("highMidGain"), 7, 1.0}, {TEXT("highFrequency"), 9, 10000.0}, {TEXT("highGain"), 10, 1.0}};
	for (const FMixEqFlatField& Field : FlatFields)
	{
		if (Params->HasField(Field.Name)) { *Bands[Field.Band].Value = static_cast<float>(GetJsonNumberField(Params, Field.Name, Field.Default)); }
	}
	for (const FMixEqField& Band : Bands)
	{
		*Band.Value = FMath::Clamp(*Band.Value, 0.0f, Band.Max);
	}

	Response->SetBoolField(TEXT("saved"), SaveAudioAsset(Mix, bSave));
	// Every band after clamping, under the eqSettings names.
	TSharedPtr<FJsonObject> EQInfo = McpHandlerUtils::CreateResultObject();
	for (const FMixEqField& Band : Bands)
	{
		EQInfo->SetNumberField(Band.Name, *Band.Value);
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
