// Copyright (c) 2024 EngineWire Contributors
//
// manage_audio set_sound_looping: each Sound Wave's Looping flag (USoundWave::bLooping), which loops the wave wherever
// it plays by itself. A Sound Cue loops through a Looping node in its graph and a MetaSound through its own graph, so
// both are refused.

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAudioAuthoringPrivate.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Sound/SoundWaveProcedural.h"

namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleLoopingActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction != TEXT("set_sound_looping"))
    {
        return nullptr;
    }
    const bool bLoop = GetJsonBoolField(Params, TEXT("looping"), true);
    TArray<USoundWave*> Waves;
    for (const FString& Path : McpHandlerUtils::GetStringArrayField(Params, TEXT("soundPaths")))
    {
        UObject* Sound = StaticLoadObject(USoundBase::StaticClass(), nullptr, *NormalizeAudioPath(Path));
        if (Cast<USoundCue>(Sound) || Cast<USoundWaveProcedural>(Sound))
        {
            return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), FString::Printf(TEXT("%s loops through its own graph: a "
                "Sound Cue with a Looping node (edit_sound_cue), a MetaSound by how it is built. Nothing was changed."), *Path));
        }
        USoundWave* Wave = Cast<USoundWave>(Sound);
        if (!Wave)
        {
            return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("%s is not a Sound Wave. Nothing was changed."), *Path));
        }
        if (Wave->GetPathName().StartsWith(TEXT("/Engine/")))
        {
            return McpHandlerUtils::BuildErrorResponse(TEXT("ENGINE_CONTENT"), FString::Printf(TEXT("%s is engine content, which is never changed: "
                "duplicate it into /Game first. Nothing was changed."), *Wave->GetPathName()));
        }
        Waves.AddUnique(Wave);
    }
    if (Waves.Num() == 0)
    {
        return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), TEXT("soundPaths names the Sound Waves to change (1-64)."));
    }
    // Only the waves that change enter the transaction: it marks what it holds as edited.
    TArray<UObject*> Changing;
    for (USoundWave* Wave : Waves)
    {
        if ((Wave->bLooping != 0) != bLoop)
        {
            Changing.Add(Wave);
        }
    }
    if (Changing.Num() > 0)
    {
        const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpLoudness", "Looping", "Set Sound Looping"), EMcpMutationDurability::EditorStateOnly, Changing);
        for (UObject* Sound : Changing)
        {
            USoundWave* Wave = CastChecked<USoundWave>(Sound);
            Wave->Modify();
            Wave->bLooping = bLoop;
            Wave->PostEditChange();
        }
        Transaction.DescribeInto(Response);
    }
    const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (USoundWave* Wave : Waves)
    {
        const bool bChanged = Changing.Contains(Wave);
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("soundPath"), Wave->GetPathName());
        Row->SetBoolField(TEXT("wasLooping"), bChanged ? !bLoop : bLoop);
        Row->SetBoolField(TEXT("looping"), Wave->bLooping != 0);
        Row->SetBoolField(TEXT("saved"), bChanged && bSave && SaveAudioAsset(Wave, true));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Response->SetArrayField(TEXT("sounds"), Rows);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("%d sound(s) set to %s, %d already did."), Changing.Num(),
        bLoop ? TEXT("loop") : TEXT("play once"), Waves.Num() - Changing.Num()));
    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
} // namespace McpAudioAuthoring
