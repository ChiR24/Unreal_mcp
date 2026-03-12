// =============================================================================
// McpAutomationBridge_SequencerHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Sequencer & Cinematics Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Actions:
//   - add_sequencer_keyframe: Add float keyframe to track
//   - manage_sequencer_track: Add/remove tracks from sequence binding
//   - add_camera_track: Add camera cut track with section
//   - add_animation_track: Add skeletal animation track
//   - add_transform_track: Add 3D transform track
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: LevelSequence, MovieScene, MovieSceneTrack, MovieSceneSection
//   - Tracks: FloatTrack, CameraCutTrack, 3DTransformTrack, SkeletalAnimationTrack
// 
// Version Compatibility Notes:
//   - UE 5.3+: GetChannel() returns mutable reference for MovieSceneFloatChannel
//   - UE 5.0-5.2: GetChannel() returns const reference, requires const_cast
// 
// Architecture:
//   - LevelSequence contains UMovieScene
//   - MovieScene has FMovieSceneBinding objects (GUID-based)
//   - Each binding can have multiple tracks
//   - Tracks contain sections with channels for keyframes
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MovieSceneBindingOwnerInterface.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "UObject/SoftObjectPath.h"
#endif

// =============================================================================
// Handler: add_sequencer_keyframe
// =============================================================================

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
    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("add_sequencer_keyframe payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract required parameters
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

    // Load level sequence
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

    // Parse binding GUID
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

    // Find or create float track
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

    // Get or create section
    UMovieSceneFloatTrack* FloatTrack = CastChecked<UMovieSceneFloatTrack>(Track);
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

    // Add keyframe
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();
    FFrameTime FrameTime = DisplayRate.AsFrameTime(TimeSeconds);
    FFrameNumber FrameNumber = FrameTime.GetFrame();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // UE 5.3+: GetChannel returns mutable reference
    FMovieSceneFloatChannel& Channel = FloatSection->GetChannel();
    Channel.AddCubicKey(FrameNumber, static_cast<float>(Value));
#else
    // UE 5.0-5.2: GetChannel returns const reference
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

// =============================================================================
// Handler: manage_sequencer_track
// =============================================================================

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
    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("manage_sequencer_track payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract required parameters
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

    // Load level sequence
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

    // Parse binding GUID
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

// =============================================================================
// Handler: add_camera_track
// =============================================================================

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
    // Validate payload
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

    // Load level sequence
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

    // Load camera actor
    ACameraActor* CameraActor = LoadObject<ACameraActor>(nullptr, *CameraActorPath);
    if (!CameraActor)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Failed to load camera actor"), TEXT("CAMERA_LOAD_FAILED"));
        return true;
    }

    // Get or create camera cut track
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

            // Find camera binding
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

// =============================================================================
// Handler: add_animation_track
// =============================================================================

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
    // Validate payload
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

    // Load level sequence
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

    // Parse binding GUID
    FGuid BindingGuid;
    if (!FGuid::Parse(BindingGuidStr, BindingGuid))
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Invalid bindingGuid"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Load animation sequence
    UAnimSequence* AnimSequence = LoadObject<UAnimSequence>(nullptr, *AnimSequencePath);
    if (!AnimSequence)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Failed to load animation sequence"), TEXT("ANIM_LOAD_FAILED"));
        return true;
    }

    // Create animation track
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

// =============================================================================
// Handler: add_transform_track
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAddTransformTrack(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("add_transform_track"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("add_transform_track payload missing"), TEXT("INVALID_PAYLOAD"));
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

    // Load level sequence
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

    // Parse binding GUID
    FGuid BindingGuid;
    if (!FGuid::Parse(BindingGuidStr, BindingGuid))
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Invalid bindingGuid"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Create transform track
    UMovieScene3DTransformTrack* TransformTrack = 
        MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
    if (!TransformTrack)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Failed to create transform track"), TEXT("TRACK_CREATION_FAILED"));
        return true;
    }

    UMovieScene3DTransformSection* TransformSection = 
        Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
    if (TransformSection)
    {
        TransformTrack->AddSection(*TransformSection);
        MovieScene->Modify();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, LevelSequence);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("bindingGuid"), BindingGuidStr);
    Result->SetBoolField(TEXT("hasDefaultKeyframes"), true);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Transform track added"), Result);
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false, 
        TEXT("add_transform_track requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
