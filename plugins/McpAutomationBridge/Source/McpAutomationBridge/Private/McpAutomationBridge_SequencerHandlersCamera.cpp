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
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Camera/CameraActor.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAddCameraTrack(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("add_camera_track"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("add_camera_track payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SequencePath;
    if (!Payload->TryGetStringField(TEXT("sequencePath"), SequencePath) || SequencePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("sequencePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString CameraActorPath;
    if (!Payload->TryGetStringField(TEXT("cameraActorPath"), CameraActorPath) || CameraActorPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("cameraActorPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    double EndTime = 5.0;
    Payload->TryGetNumberField(TEXT("endTime"), EndTime);

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

    ACameraActor* CameraActor = LoadObject<ACameraActor>(nullptr, *CameraActorPath);
    if (!CameraActor)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to load camera actor"), TEXT("CAMERA_LOAD_FAILED"));
        return true;
    }

    UMovieSceneTrack* CutBase = MovieScene->GetCameraCutTrack();
    UMovieSceneCameraCutTrack* CameraCutTrack = CutBase ?
        Cast<UMovieSceneCameraCutTrack>(CutBase) : nullptr;

    if (!CameraCutTrack)
    {
        UMovieSceneTrack* NewTrack = MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
        CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(NewTrack);
    }

    if (CameraCutTrack)
    {
        FFrameRate DisplayRate = MovieScene->GetDisplayRate();
        FFrameTime StartFrameTime = DisplayRate.AsFrameTime(StartTime);
        FFrameTime EndFrameTime = DisplayRate.AsFrameTime(EndTime);
        FFrameNumber StartFrame = StartFrameTime.GetFrame();
        FFrameNumber EndFrame = EndFrameTime.GetFrame();

        UMovieSceneCameraCutSection* CameraCutSection =
            Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
        if (CameraCutSection)
        {
            CameraCutTrack->AddSection(*CameraCutSection);
            CameraCutSection->SetRange(TRange<FFrameNumber>(StartFrame, EndFrame));

            FGuid CameraGuid;
            for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
            {
                FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
                if (Possessable.GetPossessedObjectClass() &&
                    Possessable.GetPossessedObjectClass()->IsChildOf(ACameraActor::StaticClass()))
                {
                    CameraGuid = Possessable.GetGuid();
                    break;
                }
            }

            if (CameraGuid.IsValid())
            {
                CameraCutSection->SetCameraBindingID(FMovieSceneObjectBindingID(CameraGuid));
            }

            MovieScene->Modify();
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, LevelSequence);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("cameraActorPath"), CameraActorPath);
    Result->SetNumberField(TEXT("startTime"), StartTime);
    Result->SetNumberField(TEXT("endTime"), EndTime);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera track added"), Result);
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("add_camera_track requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
