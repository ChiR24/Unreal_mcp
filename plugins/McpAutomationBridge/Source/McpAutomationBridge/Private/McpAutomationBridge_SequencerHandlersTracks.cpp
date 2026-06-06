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
#endif

bool UMcpAutomationBridgeSubsystem::HandleManageSequencerTrack(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("manage_sequencer_track"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("manage_sequencer_track payload missing"), TEXT("INVALID_PAYLOAD"));
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

    FString PropertyName;
    if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) || PropertyName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("propertyName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString Op;
    if (!Payload->TryGetStringField(TEXT("op"), Op) || Op.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("op required (add/remove)"), TEXT("INVALID_ARGUMENT"));
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

    bool bSuccess = false;

    if (Op.Equals(TEXT("add"), ESearchCase::IgnoreCase))
    {
        UMovieSceneFloatTrack* NewTrack = MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingGuid);
        if (NewTrack)
        {
            NewTrack->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
            UMovieSceneSection* NewSection = NewTrack->CreateNewSection();
            if (NewSection)
            {
                NewTrack->AddSection(*NewSection);
            }
            MovieScene->Modify();
            bSuccess = true;
        }
    }
    else if (Op.Equals(TEXT("remove"), ESearchCase::IgnoreCase))
    {
        for (int32 i = Binding->GetTracks().Num() - 1; i >= 0; --i)
        {
            if (UMovieSceneFloatTrack* FT = Cast<UMovieSceneFloatTrack>(Binding->GetTracks()[i]))
            {
                if (FT->GetPropertyName().ToString().Equals(PropertyName, ESearchCase::IgnoreCase))
                {
                    MovieScene->RemoveTrack(*FT);
                    MovieScene->Modify();
                    bSuccess = true;
                    break;
                }
            }
        }
    }
    else
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Unsupported op; use add/remove"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, LevelSequence);
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("bindingGuid"), BindingGuidStr);
    Result->SetStringField(TEXT("propertyName"), PropertyName);
    Result->SetStringField(TEXT("op"), Op);

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
        bSuccess ? TEXT("Track operation complete") : TEXT("Track operation failed"),
        Result, bSuccess ? FString() : TEXT("TRACK_OP_FAILED"));
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("manage_sequencer_track requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
