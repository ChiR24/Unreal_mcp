// Copyright (c) 2024 EngineWire Contributors
//
// manage_audio measure_loudness: each Sound Wave's programme loudness (EBU R128, McpLoudness) from its own samples and
// the loudness it plays at through its Volume; normalize_loudness sets the Volume that brings each to targetLufs, in one
// undo step.

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAudioAuthoringPrivate.h"
#include "Domains/AudioAuthoring/McpLoudness.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpAudioAuthoring
{
namespace
{
struct FLoudnessRow
{
    USoundWave* Wave = nullptr;
    TOptional<double> Lufs;
    double PeakDb = -120.0;
};

double LoudnessDb(double Linear)
{
    return Linear > 1e-6 ? 20.0 * FMath::LogX(10.0, Linear) : -120.0;
}

double LoudnessRound(double Value)
{
    return FMath::RoundToDouble(Value * 10.0) / 10.0;
}

// The wave's samples (16-bit PCM as imported) to programme loudness and sample peak.
FLoudnessRow LoudnessOfWave(USoundWave* Wave)
{
    FLoudnessRow Row;
    Row.Wave = Wave;
    TArray<uint8> Pcm;
    uint32 SampleRate = 0;
    uint16 Channels = 0;
    if (!Wave->GetImportedSoundWaveData(Pcm, SampleRate, Channels) || SampleRate == 0 || Channels == 0)
    {
        return Row;
    }
    const int16* Samples = reinterpret_cast<const int16*>(Pcm.GetData());
    TArray<float> Interleaved;
    Interleaved.SetNumUninitialized(Pcm.Num() / int32(sizeof(int16)));
    double Peak = 0.0;
    for (int32 Index = 0; Index < Interleaved.Num(); ++Index)
    {
        Interleaved[Index] = Samples[Index] / 32768.f;
        Peak = FMath::Max(Peak, double(FMath::Abs(Interleaved[Index])));
    }
    Row.Lufs = McpLoudness::Integrated(Interleaved, Channels, int32(SampleRate));
    Row.PeakDb = LoudnessDb(Peak);
    return Row;
}
} // namespace

TSharedPtr<FJsonObject> HandleLoudnessActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    const bool bNormalize = SubAction == TEXT("normalize_loudness");
    if (!bNormalize && SubAction != TEXT("measure_loudness"))
    {
        return nullptr;
    }
    double Target = 0.0;
    if (bNormalize && !Params->TryGetNumberField(TEXT("targetLufs"), Target))
    {
        return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), TEXT("targetLufs is the loudness each wave should play at (-23 is broadcast, -14 to -16 streaming and games)."));
    }
    TArray<FLoudnessRow> Rows;
    for (const FString& Path : McpHandlerUtils::GetStringArrayField(Params, TEXT("soundPaths")))
    {
        USoundWave* Wave = LoadSoundWaveFromPath(Path);
        if (!Wave)
        {
            return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("%s is not a Sound Wave (a Sound Cue mixes several: "
                "measure the waves it plays). Nothing was measured or changed."), *Path));
        }
        if (bNormalize && Wave->GetPathName().StartsWith(TEXT("/Engine/")))
        {
            return McpHandlerUtils::BuildErrorResponse(TEXT("ENGINE_CONTENT"), FString::Printf(TEXT("%s is engine content, which is never changed: "
                "duplicate it into /Game first. Nothing was changed."), *Wave->GetPathName()));
        }
        Rows.Add(LoudnessOfWave(Wave));
    }
    if (Rows.Num() == 0)
    {
        return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), TEXT("soundPaths names the Sound Waves to measure (1-64)."));
    }
    if (bNormalize)
    {
        TArray<UObject*> Changed;
        for (const FLoudnessRow& Row : Rows)
        {
            if (Row.Lufs.IsSet())
            {
                Changed.Add(Row.Wave);
            }
        }
        const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpLoudness", "Normalize", "Normalize Loudness"), EMcpMutationDurability::EditorStateOnly, Changed);
        for (UObject* Object : Changed)
        {
            USoundWave* Wave = CastChecked<USoundWave>(Object);
            const FLoudnessRow& Row = *Rows.FindByPredicate([Wave](const FLoudnessRow& Each) { return Each.Wave == Wave; });
            Wave->Modify();
            Wave->Volume = float(FMath::Pow(10.0, (Target - Row.Lufs.GetValue()) / 20.0));
            Wave->PostEditChange();
        }
        Transaction.DescribeInto(Response);
    }
    const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
    TArray<TSharedPtr<FJsonValue>> Sounds;
    int32 Measured = 0;
    double Lowest = TNumericLimits<double>::Max(), Highest = TNumericLimits<double>::Lowest();
    for (const FLoudnessRow& Row : Rows)
    {
        TSharedPtr<FJsonObject> Sound = MakeShared<FJsonObject>();
        Sound->SetStringField(TEXT("soundPath"), Row.Wave->GetPathName());
        Sound->SetNumberField(TEXT("peakDb"), LoudnessRound(Row.PeakDb));
        Sound->SetNumberField(TEXT("volume"), FMath::RoundToDouble(Row.Wave->Volume * 1000.0) / 1000.0);
        if (Row.Lufs.IsSet())
        {
            const double PlaysAt = Row.Lufs.GetValue() + LoudnessDb(Row.Wave->Volume);
            Sound->SetNumberField(TEXT("lufs"), LoudnessRound(Row.Lufs.GetValue()));
            Sound->SetNumberField(TEXT("playsAtLufs"), LoudnessRound(PlaysAt));
            Sound->SetNumberField(TEXT("peakAtVolumeDb"), LoudnessRound(Row.PeakDb + LoudnessDb(Row.Wave->Volume)));
            ++Measured;
            Lowest = FMath::Min(Lowest, PlaysAt);
            Highest = FMath::Max(Highest, PlaysAt);
        }
        else
        {
            Sound->SetStringField(TEXT("note"), TEXT("silent, or under 400 ms of sound: no loudness, so its Volume is left as it was"));
        }
        if (bNormalize && Row.Lufs.IsSet())
        {
            Sound->SetBoolField(TEXT("saved"), SaveAudioAsset(Row.Wave, bSave) && bSave);
        }
        Sounds.Add(MakeShared<FJsonValueObject>(Sound));
    }
    Response->SetArrayField(TEXT("sounds"), Sounds);
    if (Highest >= Lowest)
    {
        Response->SetNumberField(TEXT("spreadLu"), LoudnessRound(Highest - Lowest));
    }
    if (!bNormalize)
    {
        McpHandlerUtils::MarkNoAssetsChanged(Response);
    }
    Response->SetStringField(TEXT("message"), bNormalize
        ? FString::Printf(TEXT("Set the Volume of %d of %d Sound Wave(s) to play at %.1f LUFS."), Measured, Sounds.Num(), Target)
        : FString::Printf(TEXT("Measured %d Sound Wave(s); they play %.1f LU apart."), Sounds.Num(), Highest >= Lowest ? Highest - Lowest : 0.0));
    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
} // namespace McpAudioAuthoring
