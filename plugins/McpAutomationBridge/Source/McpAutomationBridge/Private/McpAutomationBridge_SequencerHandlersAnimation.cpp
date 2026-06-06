#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBindingOwnerInterface.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Animation/AnimSequence.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAddAnimationTrack(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("add_animation_track"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("add_animation_track payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SequencePath;
    if (!Payload->TryGetStringField(TEXT("sequencePath"), SequencePath) || SequencePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("sequencePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString BindingGuidStr;
    if (!Payload->TryGetStringField(TEXT("bindingGuid"), BindingGuidStr) || BindingGuidStr.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("bindingGuid required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString AnimSequencePath;
    if (!Payload->TryGetStringField(TEXT("animSequencePath"), AnimSequencePath) || AnimSequencePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("animSequencePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    ULevelSequence* LevelSequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!LevelSequence)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to load LevelSequence"), TEXT("LOAD_FAILED"));
        return true;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Sequence has no MovieScene"), TEXT("INVALID_SEQUENCE"));
        return true;
    }

    FGuid BindingGuid;
    if (!FGuid::Parse(BindingGuidStr, BindingGuid))
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Invalid bindingGuid"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UAnimSequence* AnimSequence = LoadObject<UAnimSequence>(nullptr, *AnimSequencePath);
    if (!AnimSequence)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to load animation sequence"), TEXT("ANIM_LOAD_FAILED"));
        return true;
    }

    UMovieSceneSkeletalAnimationTrack* AnimTrack =
        MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(BindingGuid);
    if (!AnimTrack)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to create animation track"), TEXT("TRACK_CREATION_FAILED"));
        return true;
    }

    UMovieSceneSection* NewSection = AnimTrack->CreateNewSection();
    UMovieSceneSkeletalAnimationSection* AnimSection =
        Cast<UMovieSceneSkeletalAnimationSection>(NewSection);

    if (AnimSection)
    {
        AnimTrack->AddSection(*AnimSection);
        AnimSection->Params.Animation = AnimSequence;

        FFrameRate DisplayRate = MovieScene->GetDisplayRate();
        FFrameTime StartFrame = DisplayRate.AsFrameTime(StartTime);
        float AnimLength = AnimSequence->GetPlayLength();
        FFrameTime EndFrame = DisplayRate.AsFrameTime(StartTime + AnimLength);

        AnimSection->SetRange(TRange<FFrameNumber>(StartFrame.GetFrame(), EndFrame.GetFrame()));
        MovieScene->Modify();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, LevelSequence);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("bindingGuid"), BindingGuidStr);
    Result->SetStringField(TEXT("animSequencePath"), AnimSequencePath);
    Result->SetNumberField(TEXT("startTime"), StartTime);
    Result->SetNumberField(TEXT("animLength"), AnimSequence->GetPlayLength());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Animation track added"), Result);
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("add_animation_track requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
