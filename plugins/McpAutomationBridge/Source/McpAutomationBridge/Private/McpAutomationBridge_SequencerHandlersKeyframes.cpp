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
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAddSequencerKeyframe(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("add_sequencer_keyframe"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("add_sequencer_keyframe payload missing"), TEXT("INVALID_PAYLOAD"));
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
            TEXT("bindingGuid required (existing object binding GUID)"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString PropertyName;
    if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) || PropertyName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("propertyName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double TimeSeconds = 0.0;
    if (!Payload->TryGetNumberField(TEXT("time"), TimeSeconds))
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("time (seconds) required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double Value = 0.0;
    if (!Payload->TryGetNumberField(TEXT("value"), Value))
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("value (number) required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

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

    FMovieSceneBinding* Binding = MovieScene->FindBinding(BindingGuid);
    if (!Binding)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Binding not found in sequence"), TEXT("BINDING_NOT_FOUND"));
        return true;
    }

    UMovieSceneTrack* Track = nullptr;
    for (UMovieSceneTrack* T : Binding->GetTracks())
    {
        if (UMovieSceneFloatTrack* FT = Cast<UMovieSceneFloatTrack>(T))
        {
            if (FT->GetPropertyName().ToString().Equals(PropertyName, ESearchCase::IgnoreCase))
            {
                Track = FT;
                break;
            }
        }
    }

    if (!Track)
    {
        UMovieSceneFloatTrack* NewTrack = MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingGuid);
        if (!NewTrack)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to create float track"), TEXT("CREATE_TRACK_FAILED"));
            return true;
        }
        NewTrack->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
        Track = NewTrack;
    }

    UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
    if (!FloatTrack)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Track is not a float track"), TEXT("INVALID_TRACK_TYPE"));
        return true;
    }
    UMovieSceneSection* Section = nullptr;
    const TArray<UMovieSceneSection*>& Sections = FloatTrack->GetAllSections();
    if (Sections.Num() > 0)
    {
        Section = Sections[0];
    }
    else
    {
        Section = FloatTrack->CreateNewSection();
        FloatTrack->AddSection(*Section);
    }

    if (!Section)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to create/find section"), TEXT("SECTION_FAILED"));
        return true;
    }

    UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(Section);
    if (!FloatSection)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Section is not a float section"), TEXT("SECTION_TYPE_MISMATCH"));
        return true;
    }

    FFrameRate DisplayRate = MovieScene->GetDisplayRate();
    FFrameTime FrameTime = DisplayRate.AsFrameTime(TimeSeconds);
    FFrameNumber FrameNumber = FrameTime.GetFrame();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    FMovieSceneFloatChannel& Channel = FloatSection->GetChannel();
    Channel.AddCubicKey(FrameNumber, static_cast<float>(Value));
#else
    const FMovieSceneFloatChannel& Channel = FloatSection->GetChannel();
    const_cast<FMovieSceneFloatChannel&>(Channel).AddCubicKey(FrameNumber, static_cast<float>(Value));
#endif

    MovieScene->Modify();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, LevelSequence);
    Result->SetStringField(TEXT("bindingGuid"), BindingGuidStr);
    Result->SetStringField(TEXT("propertyName"), PropertyName);
    Result->SetNumberField(TEXT("time"), TimeSeconds);
    Result->SetNumberField(TEXT("value"), Value);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Keyframe added"), Result);
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("add_sequencer_keyframe requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
