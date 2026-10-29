// Copyright (c) 2024 EngineWire Contributors
//
// manage_audio set_subtitles: a Sound Wave's subtitle cues (USoundWave::Subtitles: the words, and the time into the
// sound each shows from), which the engine shows at the bottom of the game's screen while the sound plays.

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAudioAuthoringPrivate.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleSubtitleActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction != TEXT("set_subtitles"))
    {
        return nullptr;
    }
    const FString Path = GetJsonStringField(Params, TEXT("soundPath"));
    USoundWave* Wave = LoadSoundWaveFromPath(Path);
    if (!Wave)
    {
        return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("%s is not a Sound Wave (a Sound Cue plays "
            "waves: give each wave it plays its own subtitles). Nothing was changed."), *Path));
    }
    if (Wave->GetPathName().StartsWith(TEXT("/Engine/")))
    {
        return McpHandlerUtils::BuildErrorResponse(TEXT("ENGINE_CONTENT"), FString::Printf(TEXT("%s is engine content, which is never changed: "
            "duplicate it into /Game first. Nothing was changed."), *Wave->GetPathName()));
    }
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Params->TryGetArrayField(TEXT("subtitles"), Entries) || !Entries)
    {
        return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), TEXT("subtitles lists the cues, [{text, time}]; [] takes them all off."));
    }
    // Every cue is read before any is set, so a bad one changes nothing.
    const float Duration = Wave->GetDuration();
    TArray<FSubtitleCue> Cues;
    for (const TSharedPtr<FJsonValue>& Entry : *Entries)
    {
        const TSharedPtr<FJsonObject>* Cue = nullptr;
        FString Text;
        double Time = 0.0;
        if (!Entry.IsValid() || !Entry->TryGetObject(Cue) || !Cue || !(*Cue)->TryGetStringField(TEXT("text"), Text) ||
            Text.TrimStartAndEnd().IsEmpty())
        {
            return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), TEXT("Each cue is {text, time}: the words, and the seconds "
                "into the sound they show from. Nothing was changed."));
        }
        (*Cue)->TryGetNumberField(TEXT("time"), Time);
        if (Time < 0.0 || Time > Duration)
        {
            return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), FString::Printf(TEXT("A cue at %.2f s is outside the sound, "
                "which lasts %.2f s. Nothing was changed."), Time, Duration));
        }
        FSubtitleCue& Added = Cues.AddDefaulted_GetRef();
        Added.Text = FText::FromString(Text.TrimStartAndEnd());
        Added.Time = static_cast<float>(Time);
    }
    // The engine shows each cue until the next one starts: in time order.
    Cues.StableSort([](const FSubtitleCue& A, const FSubtitleCue& B) { return A.Time < B.Time; });
    {
        const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpSubtitles", "Set", "Set Subtitles"),
                                                      EMcpMutationDurability::EditorStateOnly, TArray<UObject*>{Wave});
        Wave->Modify();
        Wave->Subtitles = Cues;
        Wave->PostEditChange();
        Transaction.DescribeInto(Response);
    }
    const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
    Response->SetBoolField(TEXT("saved"), SaveAudioAsset(Wave, bSave) && bSave);
    TArray<TSharedPtr<FJsonValue>> ReadBack;
    for (const FSubtitleCue& Cue : Wave->Subtitles)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("text"), Cue.Text.ToString());
        Row->SetNumberField(TEXT("time"), FMath::RoundToDouble(Cue.Time * 100.0) / 100.0);
        ReadBack.Add(MakeShared<FJsonValueObject>(Row));
    }
    Response->SetStringField(TEXT("soundPath"), Wave->GetPathName());
    Response->SetArrayField(TEXT("subtitles"), ReadBack);
    if (GEngine && (GEngine->bSubtitlesForcedOff || !GEngine->bSubtitlesEnabled))
    {
        Response->SetArrayField(TEXT("warnings"), McpHandlerUtils::ToJsonStringArray({TEXT("Subtitles are switched off for the game "
            "(the engine's Subtitles Enabled and Subtitles Forced Off settings), so these will not show while it is.")}));
    }
    Response->SetStringField(TEXT("message"), ReadBack.Num() > 0
        ? FString::Printf(TEXT("%d subtitle cue(s) on %s; they show while it plays in the game."), ReadBack.Num(), *Wave->GetName())
        : FString::Printf(TEXT("Took the subtitles off %s."), *Wave->GetName()));
    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
} // namespace McpAudioAuthoring
