#include "Dom/JsonObject.h"
#include "LevelSequence.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "UObject/UObjectIterator.h"

// UE 5.0 compatibility: GetTracks() was introduced in UE 5.1, use GetMasterTracks() in 5.0
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_GET_MOVIESCENE_TRACKS(MovieScene) (MovieScene)->GetTracks()
#define MCP_GET_BINDING_TRACKS(Binding) (Binding).GetTracks()
#else
#define MCP_GET_MOVIESCENE_TRACKS(MovieScene) (MovieScene)->GetMasterTracks()
#define MCP_GET_BINDING_TRACKS(Binding) (Binding).GetTracks()
#endif

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#define MCP_HAS_EDITOR_ACTOR_SUBSYSTEM 1
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#define MCP_HAS_EDITOR_ACTOR_SUBSYSTEM 1
#else
#define MCP_HAS_EDITOR_ACTOR_SUBSYSTEM 0
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"

// Header checks removed causing issues with private headers

// LevelSequenceEditorSubsystem is only available in UE 5.1+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "LevelSequenceEditorSubsystem.h"
#define MCP_HAS_LEVELSEQUENCE_EDITOR_SUBSYSTEM 1
#else
#define MCP_HAS_LEVELSEQUENCE_EDITOR_SUBSYSTEM 0
#endif

#if __has_include("ILevelSequenceEditorToolkit.h")
#include "ILevelSequenceEditorToolkit.h"
#endif

#if __has_include("ISequencer.h")
#include "ISequencer.h"
#include "MovieSceneSequencePlayer.h"
#endif

#if __has_include("Tracks/MovieSceneFloatTrack.h")
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"

#endif

#if __has_include("Tracks/MovieSceneBoolTrack.h")
#include "Sections/MovieSceneBoolSection.h"
#include "Tracks/MovieSceneBoolTrack.h"

#endif

#if __has_include("Tracks/MovieScene3DTransformTrack.h")
#include "Tracks/MovieScene3DTransformTrack.h"
#endif

#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneEventTrack.h"

#if __has_include("Sections/MovieScene3DTransformSection.h")
#include "Sections/MovieScene3DTransformSection.h"
#endif
#if __has_include("Channels/MovieSceneDoubleChannel.h")
#include "Channels/MovieSceneDoubleChannel.h"
#endif
#if __has_include("Channels/MovieSceneChannelProxy.h")
#include "Channels/MovieSceneChannelProxy.h"
#endif

// Optional components check
#if __has_include("Misc/ScopedTransaction.h")
#include "Misc/ScopedTransaction.h"
#endif
#if __has_include("Camera/CameraActor.h")
#include "Camera/CameraActor.h"
#endif
#endif

FString UMcpAutomationBridgeSubsystem::ResolveSequencePath(
    const TSharedPtr<FJsonObject> &Payload) {
  FString Path;
  if (Payload.IsValid() && Payload->TryGetStringField(TEXT("path"), Path) &&
      !Path.IsEmpty()) {
#if WITH_EDITOR
    // Check existence first to avoid error log spam
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      UObject *Obj = UEditorAssetLibrary::LoadAsset(Path);
      if (Obj) {
        return Obj->GetPathName();
      }
    }
#endif
    return Path;
  }
  if (!GCurrentSequencePath.IsEmpty())
    return GCurrentSequencePath;
  return FString();
}

TSharedPtr<FJsonObject>
UMcpAutomationBridgeSubsystem::EnsureSequenceEntry(const FString &SeqPath) {
  if (SeqPath.IsEmpty())
    return nullptr;
  if (TSharedPtr<FJsonObject> *Found = GSequenceRegistry.Find(SeqPath))
    return *Found;
  TSharedPtr<FJsonObject> NewObj = MakeShared<FJsonObject>();
  NewObj->SetStringField(TEXT("sequencePath"), SeqPath);
  GSequenceRegistry.Add(SeqPath, NewObj);
  return NewObj;
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceCreate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString Name;
  LocalPayload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  LocalPayload->TryGetStringField(TEXT("path"), Path);
  if (Name.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_create requires name"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString FullPath = Path.IsEmpty()
                         ? FString::Printf(TEXT("/Game/%s"), *Name)
                         : FString::Printf(TEXT("%s/%s"), *Path, *Name);

  FString DestFolder = Path.IsEmpty() ? TEXT("/Game") : Path;
  if (DestFolder.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    DestFolder = FString::Printf(TEXT("/Game%s"), *DestFolder.RightChop(8));
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;

  // Execute on Game Thread
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleSequenceCreate: Handing RequestID=%s Path=%s"),
         *RequestIdArg, *FullPath);

  // Check existence first to avoid error log spam
  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("sequencePath"), FullPath);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("HandleSequenceCreate: Sequence exists, sending response for "
                "RequestID=%s"),
           *RequestIdArg);
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                      TEXT("Sequence already exists"), Resp,
                                      FString());
    return true;
  }

  // Dynamic factory lookup
  UClass *FactoryClass = FindObject<UClass>(
      nullptr, TEXT("/Script/LevelSequenceEditor.LevelSequenceFactoryNew"));
  if (!FactoryClass)
    FactoryClass = LoadClass<UClass>(
        nullptr, TEXT("/Script/LevelSequenceEditor.LevelSequenceFactoryNew"));

  if (FactoryClass) {
    UFactory *Factory =
        NewObject<UFactory>(GetTransientPackage(), FactoryClass);
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools"));
    UObject *NewObj = AssetToolsModule.Get().CreateAsset(
        Name, DestFolder, ULevelSequence::StaticClass(), Factory);
    if (NewObj) {
      McpSafeAssetSave(NewObj);
      GCurrentSequencePath = FullPath;
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetStringField(TEXT("sequencePath"), FullPath);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleSequenceCreate: Created sequence, sending response "
                  "for RequestID=%s"),
             *RequestIdArg);
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("Sequence created"), Resp,
                                        FString());
    } else {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Error,
          TEXT("HandleSequenceCreate: Failed to create asset for RequestID=%s"),
          *RequestIdArg);
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                        TEXT("Failed to create sequence asset"),
                                        nullptr, TEXT("CREATE_ASSET_FAILED"));
    }
  } else {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("HandleSequenceCreate: Factory not found for RequestID=%s"),
           *RequestIdArg);
    Subsystem->SendAutomationResponse(
        Socket, RequestIdArg, false,
        TEXT("LevelSequenceFactoryNew class not found (Module not loaded?)"),
        nullptr, TEXT("FACTORY_NOT_AVAILABLE"));
  }
  return true;

#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_create requires editor build"), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetDisplayRate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_display_rate requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      FString FrameRateStr;
      double FrameRateVal = 0.0;

      FFrameRate NewRate;
      bool bRateFound = false;

      if (LocalPayload->TryGetStringField(TEXT("frameRate"), FrameRateStr)) {
        // Parse "30fps", "24000/1001", etc.
        // Simple parsing for standard rates
        if (FrameRateStr.EndsWith(TEXT("fps"))) {
          FrameRateStr.RemoveFromEnd(TEXT("fps"));
          NewRate = FFrameRate(FCString::Atoi(*FrameRateStr), 1);
          bRateFound = true;
        } else if (FrameRateStr.Contains(TEXT("/"))) {
          // Rational
          FString NumStr, DenomStr;
          if (FrameRateStr.Split(TEXT("/"), &NumStr, &DenomStr)) {
            NewRate =
                FFrameRate(FCString::Atoi(*NumStr), FCString::Atoi(*DenomStr));
            bRateFound = true;
          }
        } else {
          // Decimal string?
          if (FrameRateStr.IsNumeric()) {
            NewRate = FFrameRate(FCString::Atoi(*FrameRateStr), 1);
            bRateFound = true;
          }
        }
      } else if (LocalPayload->TryGetNumberField(TEXT("frameRate"),
                                                 FrameRateVal)) {
        NewRate = FFrameRate(FMath::RoundToInt(FrameRateVal), 1);
        bRateFound = true;
      }

      if (bRateFound) {
        MovieScene->SetDisplayRate(NewRate);
        MovieScene->Modify();

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("displayRate"),
                             NewRate.ToPrettyText().ToString());
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Display rate set"), Resp, FString());
        return true;
      }

      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Invalid frameRate format"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Invalid sequence type"), nullptr,
                         TEXT("INVALID_SEQUENCE"));
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_set_display_rate requires editor build"), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_properties requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;

  // Capture simple types. For JsonObject, we need to capture the data, not the
  // pointer if we want to be safe, but since we parsed it above, we should
  // capture the parsed values. Parsing logic happens above. We'll capture the
  // parsed variables. But wait, the parsing logic in the original code is
  // INSIDE the block I'm replacing (lines 176-185). I need to include the
  // parsing inside the Async task or move it out. I'll move the parsing INSIDE
  // the Async task, but I need to capture LocalPayload. LocalPayload is a
  // SharedPtr, so it's safe to capture.

  UMcpAutomationBridgeSubsystem *Subsystem = this;
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      bool bModified = false;
      double FrameRateValue = 0.0;
      double LengthInFramesValue = 0.0;
      double PlaybackStartValue = 0.0;
      double PlaybackEndValue = 0.0;

      const bool bHasFrameRate =
          LocalPayload->TryGetNumberField(TEXT("frameRate"), FrameRateValue);
      const bool bHasLengthInFrames = LocalPayload->TryGetNumberField(
          TEXT("lengthInFrames"), LengthInFramesValue);
      const bool bHasPlaybackStart = LocalPayload->TryGetNumberField(
          TEXT("playbackStart"), PlaybackStartValue);
      const bool bHasPlaybackEnd = LocalPayload->TryGetNumberField(
          TEXT("playbackEnd"), PlaybackEndValue);

      if (bHasFrameRate) {
        if (FrameRateValue <= 0.0) {
          Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                            TEXT("frameRate must be > 0"),
                                            nullptr, TEXT("INVALID_ARGUMENT"));
          return true;
        }
        const int32 Rounded =
            FMath::Clamp<int32>(FMath::RoundToInt(FrameRateValue), 1, 960);
        FFrameRate CurrentRate = MovieScene->GetDisplayRate();
        FFrameRate NewRate(Rounded, 1);
        if (NewRate != CurrentRate) {
          MovieScene->SetDisplayRate(NewRate);
          bModified = true;
        }
      }

      if (bHasPlaybackStart || bHasPlaybackEnd || bHasLengthInFrames) {
        TRange<FFrameNumber> ExistingRange = MovieScene->GetPlaybackRange();
        FFrameNumber StartFrame = ExistingRange.GetLowerBoundValue();
        FFrameNumber EndFrame = ExistingRange.GetUpperBoundValue();

        if (bHasPlaybackStart)
          StartFrame = FFrameNumber(static_cast<int32>(PlaybackStartValue));
        if (bHasPlaybackEnd)
          EndFrame = FFrameNumber(static_cast<int32>(PlaybackEndValue));
        else if (bHasLengthInFrames)
          EndFrame =
              StartFrame +
              FMath::Max<int32>(0, static_cast<int32>(LengthInFramesValue));

        if (EndFrame < StartFrame)
          EndFrame = StartFrame;
        MovieScene->SetPlaybackRange(
            TRange<FFrameNumber>(StartFrame, EndFrame));
        bModified = true;
      }

      if (bModified)
        MovieScene->Modify();

      FFrameRate FR = MovieScene->GetDisplayRate();
      TSharedPtr<FJsonObject> FrameRateObj = MakeShared<FJsonObject>();
      FrameRateObj->SetNumberField(TEXT("numerator"), FR.Numerator);
      FrameRateObj->SetNumberField(TEXT("denominator"), FR.Denominator);
      Resp->SetObjectField(TEXT("frameRate"), FrameRateObj);

      TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
      const double Start =
          static_cast<double>(Range.GetLowerBoundValue().Value);
      const double End = static_cast<double>(Range.GetUpperBoundValue().Value);
      Resp->SetNumberField(TEXT("playbackStart"), Start);
      Resp->SetNumberField(TEXT("playbackEnd"), End);
      Resp->SetNumberField(TEXT("duration"), End - Start);
      Resp->SetBoolField(TEXT("applied"), bModified);

      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("properties updated"), Resp,
                                        FString());
      return true;
    }
  }
  Resp->SetObjectField(TEXT("frameRate"), MakeShared<FJsonObject>());
  Resp->SetNumberField(TEXT("playbackStart"), 0.0);
  Resp->SetNumberField(TEXT("playbackEnd"), 0.0);
  Resp->SetNumberField(TEXT("duration"), 0.0);
  Resp->SetBoolField(TEXT("applied"), false);
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("sequence_set_properties is not available in this editor build or "
           "for this sequence type"),
      Resp, TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_properties requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceOpen(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_open requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleSequenceOpen: Opening sequence %s for RequestID=%s"),
         *SeqPath, *RequestIdArg);
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if MCP_HAS_LEVELSEQUENCE_EDITOR_SUBSYSTEM
  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (GEditor) {
      if (ULevelSequenceEditorSubsystem *LSES =
              GEditor->GetEditorSubsystem<ULevelSequenceEditorSubsystem>()) {
        if (UAssetEditorSubsystem *AssetEditorSS =
                GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
          AssetEditorSS->OpenEditorForAsset(LevelSeq);
          Resp->SetStringField(TEXT("sequencePath"), SeqPath);
          Resp->SetStringField(TEXT("message"), TEXT("Sequence opened"));
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("HandleSequenceOpen: Successfully opened in LSES, "
                      "sending response for RequestID=%s"),
                 *RequestIdArg);
          Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                            TEXT("Sequence opened"), Resp,
                                            FString());
          return true;
        }
      }
    }
  }
#endif

  if (GEditor) {
    if (UAssetEditorSubsystem *AssetEditorSS =
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
      AssetEditorSS->OpenEditorForAsset(SeqObj);
    }
  }
  Resp->SetStringField(TEXT("sequencePath"), SeqPath);
  Resp->SetStringField(TEXT("message"), TEXT("Sequence opened (asset editor)"));
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleSequenceOpen: Opened via AssetEditorSS, sending response "
              "for RequestID=%s"),
         *RequestIdArg);
  Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                    TEXT("Sequence opened"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_open requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddCamera(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_add_camera requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if MCP_HAS_EDITOR_ACTOR_SUBSYSTEM
  if (GEditor) {
    UClass *CameraClass = ACameraActor::StaticClass();
    AActor *Spawned = SpawnActorInActiveWorld<AActor>(
        CameraClass, FVector::ZeroVector, FRotator::ZeroRotator,
        TEXT("SequenceCamera"));
    if (Spawned) {
      // Fix for Issue #6: Auto-bind the camera to the sequence
      if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
        if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
          FGuid BindingGuid = MovieScene->AddPossessable(
              Spawned->GetActorLabel(), Spawned->GetClass());
          if (MovieScene->FindPossessable(BindingGuid)) {
            MovieScene->Modify();
            Resp->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
          }
        }
      }

      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("actorLabel"), Spawned->GetActorLabel());
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Camera actor spawned and bound to sequence"),
                             Resp, FString());
      return true;
    }
  }
  SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to add camera"),
                         nullptr, TEXT("ADD_CAMERA_FAILED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("UEditorActorSubsystem not available"), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_camera requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequencePlay(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No sequence selected or path provided"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::OpenLevelSequence(LevelSeq)) {
      ULevelSequenceEditorBlueprintLibrary::Play();
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("Sequence playing"), nullptr);
      return true;
    }
  }
  Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                    TEXT("Failed to open or play sequence"),
                                    nullptr, TEXT("EXECUTION_ERROR"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_play requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString ActorName;
  LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_add_actor requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  // Reuse multi-actor binding logic for a single actor by forwarding to
  // HandleSequenceAddActors with a one-element actorNames array and the
  // resolved sequence path. This ensures real LevelSequence bindings are
  // applied when supported by the editor build.
  TSharedPtr<FJsonObject> ForwardPayload = MakeShared<FJsonObject>();
  ForwardPayload->SetStringField(TEXT("path"), SeqPath);
  TArray<TSharedPtr<FJsonValue>> NamesArray;
  NamesArray.Add(MakeShared<FJsonValueString>(ActorName));
  ForwardPayload->SetArrayField(TEXT("actorNames"), NamesArray);

  return HandleSequenceAddActors(RequestId, ForwardPayload, Socket);
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_actor requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddActors(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
  if (!Arr || Arr->Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("actorNames required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_add_actors requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TArray<FString> Names;
  Names.Reserve(Arr->Num());
  for (const TSharedPtr<FJsonValue> &V : *Arr) {
    if (V.IsValid() && V->Type == EJson::String)
      Names.Add(V->AsString());
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }
  if (!GEditor) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Editor not available"), nullptr,
                                      TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

#if MCP_HAS_EDITOR_ACTOR_SUBSYSTEM
  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(Names.Num());
    for (const FString &Name : Names) {
      TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
      Item->SetStringField(TEXT("name"), Name);
      // Use robust actor lookup that checks label, name, and UAID
      AActor *Found = Subsystem->FindActorByName(Name);

      if (!Found) {
        Item->SetBoolField(TEXT("success"), false);
        Item->SetStringField(TEXT("error"), TEXT("Actor not found"));
      } else {
        if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
          UMovieScene *MovieScene = LevelSeq->GetMovieScene();
          if (MovieScene) {
            FGuid BindingGuid = MovieScene->AddPossessable(
                Found->GetActorLabel(), Found->GetClass());
            if (MovieScene->FindPossessable(BindingGuid)) {
              Item->SetBoolField(TEXT("success"), true);
              Item->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
              MovieScene->Modify();
            } else {
              Item->SetBoolField(TEXT("success"), false);
              Item->SetStringField(
                  TEXT("error"), TEXT("Failed to create possessable binding"));
            }
          } else {
            Item->SetBoolField(TEXT("success"), false);
            Item->SetStringField(TEXT("error"),
                                 TEXT("Sequence has no MovieScene"));
          }
        } else {
          Item->SetBoolField(TEXT("success"), false);
          Item->SetStringField(TEXT("error"),
                               TEXT("Sequence object is not a LevelSequence"));
        }
      }
      Results.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("results"), Results);
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                      TEXT("Actors processed"), Out, FString());
    return true;
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false, TEXT("EditorActorSubsystem not available"),
      nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
  return true;
#else
  Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                    TEXT("UEditorActorSubsystem not available"),
                                    nullptr, TEXT("NOT_AVAILABLE"));
#endif
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_actors requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddSpawnable(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString ClassName;
  LocalPayload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("className required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_add_spawnable_from_class requires a sequence path"),
        nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  UClass *ResolvedClass = nullptr;
  if (ClassName.StartsWith(TEXT("/")) || ClassName.Contains(TEXT("/"))) {
    if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(ClassName)) {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        ResolvedClass = BP->GeneratedClass;
      else if (UClass *C = Cast<UClass>(Loaded))
        ResolvedClass = C;
    }
  }
  if (!ResolvedClass)
    ResolvedClass = ResolveClassByName(ClassName);
  if (!ResolvedClass) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Class not found"),
                           nullptr, TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    UMovieScene *MovieScene = LevelSeq->GetMovieScene();
    if (MovieScene) {
      UObject *DefaultObject = ResolvedClass->GetDefaultObject();
      if (DefaultObject) {
        FGuid BindingGuid = MovieScene->AddSpawnable(ClassName, *DefaultObject);
        if (MovieScene->FindSpawnable(BindingGuid)) {
          MovieScene->Modify();
          TSharedPtr<FJsonObject> SpawnableResp = MakeShared<FJsonObject>();
          SpawnableResp->SetBoolField(TEXT("success"), true);
          SpawnableResp->SetStringField(TEXT("className"), ClassName);
          SpawnableResp->SetStringField(TEXT("bindingGuid"),
                                        BindingGuid.ToString());
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Spawnable added to sequence"),
                                 SpawnableResp, FString());
          return true;
        }
      }
    }
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create spawnable binding"), nullptr,
                           TEXT("SPAWNABLE_CREATION_FAILED"));
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Sequence object is not a LevelSequence"),
                         nullptr, TEXT("INVALID_SEQUENCE_TYPE"));
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_add_spawnable_from_class requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceRemoveActors(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
  if (!Arr || Arr->Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("actorNames required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_remove_actors requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

#if MCP_HAS_EDITOR_ACTOR_SUBSYSTEM
  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<TSharedPtr<FJsonValue>> Removed;
    int32 RemovedCount = 0;
    for (const TSharedPtr<FJsonValue> &V : *Arr) {
      if (!V.IsValid() || V->Type != EJson::String)
        continue;
      FString Name = V->AsString();
      TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
      Item->SetStringField(TEXT("name"), Name);

      if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
        UMovieScene *MovieScene = LevelSeq->GetMovieScene();
        if (MovieScene) {
          bool bRemoved = false;
          for (const FMovieSceneBinding &Binding :
               const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
            FString BindingName;
            if (FMovieScenePossessable *Possessable =
                    MovieScene->FindPossessable(Binding.GetObjectGuid())) {
              BindingName = Possessable->GetName();
            } else if (FMovieSceneSpawnable *Spawnable =
                           MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
              BindingName = Spawnable->GetName();
            }

            if (BindingName.Equals(Name, ESearchCase::IgnoreCase)) {
              MovieScene->RemovePossessable(Binding.GetObjectGuid());
              MovieScene->Modify();
              bRemoved = true;
              break;
            }
          }
          if (bRemoved) {
            Item->SetBoolField(TEXT("success"), true);
            Item->SetStringField(TEXT("status"), TEXT("Actor removed"));
            RemovedCount++;
          } else {
            Item->SetBoolField(TEXT("success"), false);
            Item->SetStringField(TEXT("error"),
                                 TEXT("Actor not found in sequence bindings"));
          }
        } else {
          Item->SetBoolField(TEXT("success"), false);
          Item->SetStringField(TEXT("error"),
                               TEXT("Sequence has no MovieScene"));
        }
      } else {
        Item->SetBoolField(TEXT("success"), false);
        Item->SetStringField(TEXT("error"),
                             TEXT("Sequence object is not a LevelSequence"));
      }
      Removed.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("removedActors"), Removed);
    Out->SetNumberField(TEXT("bindingsProcessed"), RemovedCount);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actors processed for removal"), Out,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("EditorActorSubsystem not available"), nullptr,
                         TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("UEditorActorSubsystem not available"), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_remove_actors requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceGetBindings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_get_bindings requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      TArray<TSharedPtr<FJsonValue>> BindingsArray;
      for (const FMovieSceneBinding &B :
           const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
        TSharedPtr<FJsonObject> Bobj = MakeShared<FJsonObject>();
        Bobj->SetStringField(TEXT("id"), B.GetObjectGuid().ToString());

        FString BindingName;
        if (FMovieScenePossessable *Possessable =
                MovieScene->FindPossessable(B.GetObjectGuid())) {
          BindingName = Possessable->GetName();
        } else if (FMovieSceneSpawnable *Spawnable =
                       MovieScene->FindSpawnable(B.GetObjectGuid())) {
          BindingName = Spawnable->GetName();
        }

        Bobj->SetStringField(TEXT("name"), BindingName);
        BindingsArray.Add(MakeShared<FJsonValueObject>(Bobj));
      }
      Resp->SetArrayField(TEXT("bindings"), BindingsArray);
      SendAutomationResponse(Socket, RequestId, true, TEXT("bindings listed"),
                             Resp, FString());
      return true;
    }
  }
  Resp->SetArrayField(TEXT("bindings"), TArray<TSharedPtr<FJsonValue>>());
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("bindings listed (empty)"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_get_bindings requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceGetProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_get_properties requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      FFrameRate FR = MovieScene->GetDisplayRate();
      TSharedPtr<FJsonObject> FrameRateObj = MakeShared<FJsonObject>();
      FrameRateObj->SetNumberField(TEXT("numerator"), FR.Numerator);
      FrameRateObj->SetNumberField(TEXT("denominator"), FR.Denominator);
      Resp->SetObjectField(TEXT("frameRate"), FrameRateObj);
      TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
      const double Start =
          static_cast<double>(Range.GetLowerBoundValue().Value);
      const double End = static_cast<double>(Range.GetUpperBoundValue().Value);
      Resp->SetNumberField(TEXT("playbackStart"), Start);
      Resp->SetNumberField(TEXT("playbackEnd"), End);
      Resp->SetNumberField(TEXT("duration"), End - Start);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("properties retrieved"), Resp, FString());
      return true;
    }
  }
  Resp->SetObjectField(TEXT("frameRate"), MakeShared<FJsonObject>());
  Resp->SetNumberField(TEXT("playbackStart"), 0.0);
  Resp->SetNumberField(TEXT("playbackEnd"), 0.0);
  Resp->SetNumberField(TEXT("duration"), 0.0);
  SendAutomationResponse(Socket, RequestId, true, TEXT("properties retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_get_properties requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetPlaybackSpeed(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  double Speed = 1.0;
  LocalPayload->TryGetNumberField(TEXT("speed"), Speed);
  if (Speed <= 0.0) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid speed (must be > 0)"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_playback_speed requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId; // Capture

  // Execute on Game Thread
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (GEditor) {
    if (UAssetEditorSubsystem *AssetEditorSS =
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
      IAssetEditorInstance *Editor =
          AssetEditorSS->FindEditorForAsset(SeqObj, false);
      // ILevelSequenceEditorToolkit inherits from IAssetEditorInstance.
      // Use static_cast since we verified the toolkit type through FindEditorForAsset.
      // Note: dynamic_cast doesn't work with /GR- compiler flag in UE 5.7+
      if (ILevelSequenceEditorToolkit *LSEditor =
              static_cast<ILevelSequenceEditorToolkit *>(Editor)) {
        if (LSEditor->GetSequencer().IsValid()) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("HandleSequenceSetPlaybackSpeed: Setting speed to %.2f"),
                 Speed);
          LSEditor->GetSequencer()->SetPlaybackSpeed(static_cast<float>(Speed));
          Subsystem->SendAutomationResponse(
              Socket, RequestIdArg, true,
              FString::Printf(TEXT("Playback speed set to %.2f"), Speed),
              nullptr);
          return true;
        } else {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
                 TEXT("HandleSequenceSetPlaybackSpeed: Sequencer invalid for "
                      "asset %s"),
                 *SeqObj->GetName());
        }
      }
    }
  }

  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("Sequence editor not open or interface unavailable"), nullptr,
      TEXT("EDITOR_NOT_OPEN"));
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_set_playback_speed requires editor build."), nullptr,
      TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequencePause(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_pause requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;

  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    // Ensure it's the active one
    if (ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence() ==
        LevelSeq) {
      ULevelSequenceEditorBlueprintLibrary::Pause();
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("Sequence paused"), nullptr);
      return true;
    }
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("Sequence not currently open in editor"), nullptr,
      TEXT("EXECUTION_ERROR"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_pause requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceStop(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_stop requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;

  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence() ==
        LevelSeq) {
      ULevelSequenceEditorBlueprintLibrary::Pause();

      FMovieSceneSequencePlaybackParams PlaybackParams;
      PlaybackParams.Frame = FFrameTime(0);
      PlaybackParams.UpdateMethod = EUpdatePositionMethod::Scrub;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
      // UE 5.4+: SetGlobalPosition available
      ULevelSequenceEditorBlueprintLibrary::SetGlobalPosition(PlaybackParams);
#else
      // UE 5.0-5.3 fallback - use SetCurrentTime (takes int32 frame number)
      ULevelSequenceEditorBlueprintLibrary::SetCurrentTime(0);
#endif

      Subsystem->SendAutomationResponse(
          Socket, RequestIdArg, true, TEXT("Sequence stopped (reset to start)"),
          nullptr);
      return true;
    }
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("Sequence not currently open in editor"), nullptr,
      TEXT("EXECUTION_ERROR"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_stop requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  TArray<TSharedPtr<FJsonValue>> SequencesArray;

  // Use Asset Registry to find all LevelSequence assets, not string matching
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(ULevelSequence::StaticClass()->GetClassPathName());
#else
  // UE 5.0: Use ClassNames instead of ClassPaths
  Filter.ClassNames.Add(ULevelSequence::StaticClass()->GetFName());
#endif
  Filter.bRecursiveClasses = true;
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(FName("/Game"));

  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> SeqObj = MakeShared<FJsonObject>();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    SeqObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
#else
    // UE 5.0: Construct object path manually
    SeqObj->SetStringField(TEXT("path"), FString::Printf(TEXT("%s.%s"), *Asset.PackageName.ToString(), *Asset.AssetName.ToString()));
#endif
    SeqObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
    SequencesArray.Add(MakeShared<FJsonValueObject>(SeqObj));
  }

  Resp->SetArrayField(TEXT("sequences"), SequencesArray);
  Resp->SetNumberField(TEXT("count"), SequencesArray.Num());
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Found %d sequences"), SequencesArray.Num()), Resp,
      FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_list requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceDuplicate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SourcePath;
  LocalPayload->TryGetStringField(TEXT("path"), SourcePath);
  FString DestinationPath;
  LocalPayload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_duplicate requires path and destinationPath"), nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve relative destination path (if just a name is provided)
  if (!DestinationPath.IsEmpty() && !DestinationPath.StartsWith(TEXT("/"))) {
    FString ParentPath = FPaths::GetPath(SourcePath);
    DestinationPath =
        FString::Printf(TEXT("%s/%s"), *ParentPath, *DestinationPath);
  }

#if WITH_EDITOR
  UObject *SourceSeq = UEditorAssetLibrary::LoadAsset(SourcePath);
  if (!SourceSeq) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source sequence not found: %s"), *SourcePath),
        nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
  UObject *DuplicatedSeq =
      UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath);
  if (DuplicatedSeq) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("sourcePath"), SourcePath);
    Resp->SetStringField(TEXT("destinationPath"), DestinationPath);
    Resp->SetStringField(TEXT("duplicatedPath"), DuplicatedSeq->GetPathName());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence duplicated successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to duplicate sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_duplicate requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceRename(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString Path;
  LocalPayload->TryGetStringField(TEXT("path"), Path);
  FString NewName;
  LocalPayload->TryGetStringField(TEXT("newName"), NewName);
  if (Path.IsEmpty() || NewName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_rename requires path and newName"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve relative new name to full path
  if (!NewName.IsEmpty() && !NewName.StartsWith(TEXT("/"))) {
    FString ParentPath = FPaths::GetPath(Path);
    NewName = FString::Printf(TEXT("%s/%s"), *ParentPath, *NewName);
  }

#if WITH_EDITOR
  if (UEditorAssetLibrary::RenameAsset(Path, NewName)) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("oldPath"), Path);
    Resp->SetStringField(TEXT("newName"), NewName);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence renamed successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to rename sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_rename requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceDelete(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString Path;
  LocalPayload->TryGetStringField(TEXT("path"), Path);
  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_delete requires path"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
#if WITH_EDITOR
  if (!UEditorAssetLibrary::DoesAssetExist(Path)) {
    // Idempotent success - if it's already gone, good.
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("deletedPath"), Path);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence deleted (or did not exist)"), Resp,
                           FString());
    return true;
  }

  if (UEditorAssetLibrary::DeleteAsset(Path)) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("deletedPath"), Path);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence deleted successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to delete sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_delete requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_get_metadata requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("path"), SeqPath);
  Resp->SetStringField(TEXT("name"), SeqObj->GetName());
  Resp->SetStringField(TEXT("class"), SeqObj->GetClass()->GetName());
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Sequence metadata retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_get_metadata requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddKeyframe(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_add_keyframe requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString BindingIdStr;
  LocalPayload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
  FString ActorName;
  LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
  FString PropertyName;
  LocalPayload->TryGetStringField(TEXT("property"), PropertyName);

  if (BindingIdStr.IsEmpty() && ActorName.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Either bindingId or actorName must be provided. bindingId is the "
             "GUID from add_actor/get_bindings. actorName is the label of an "
             "actor already bound to the sequence. Example: {\"actorName\": "
             "\"MySphere\", \"property\": \"Location\", \"frame\": 0, "
             "\"value\": {\"x\":0,\"y\":0,\"z\":0}}"),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double Frame = 0.0;
  if (!LocalPayload->TryGetNumberField(TEXT("frame"), Frame)) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("frame number is required. Example: "
                                "{\"frame\": 30} for keyframe at frame 30"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    UMovieScene *MovieScene = LevelSeq->GetMovieScene();
    if (MovieScene) {
      FGuid BindingGuid;
      if (!BindingIdStr.IsEmpty()) {
        FGuid::Parse(BindingIdStr, BindingGuid);
      } else if (!ActorName.IsEmpty()) {
        for (const FMovieSceneBinding &Binding :
             const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
          FString BindingName;
          if (FMovieScenePossessable *Possessable =
                  MovieScene->FindPossessable(Binding.GetObjectGuid())) {
            BindingName = Possessable->GetName();
          } else if (FMovieSceneSpawnable *Spawnable =
                         MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
            BindingName = Spawnable->GetName();
          }

          if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase)) {
            BindingGuid = Binding.GetObjectGuid();
            break;
          }
        }
      }

      if (!BindingGuid.IsValid()) {
        FString Target = !BindingIdStr.IsEmpty() ? BindingIdStr : ActorName;
        SendAutomationResponse(
            Socket, RequestId, false,
            FString::Printf(TEXT("Binding not found for '%s'. Ensure actor is "
                                 "bound to sequence."),
                            *Target),
            nullptr, TEXT("BINDING_NOT_FOUND"));
        return true;
      }

      FMovieSceneBinding *Binding = MovieScene->FindBinding(BindingGuid);
      if (!Binding) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Binding object not found in sequence"),
                               nullptr, TEXT("BINDING_NOT_FOUND"));
        return true;
      }

      if (PropertyName.Equals(TEXT("Transform"), ESearchCase::IgnoreCase)) {
        UMovieScene3DTransformTrack *Track =
            MovieScene->FindTrack<UMovieScene3DTransformTrack>(
                BindingGuid, FName("Transform"));
        if (!Track) {
          Track =
              MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
        }

        if (Track) {
          bool bSectionAdded = false;
          UMovieScene3DTransformSection *Section =
              Cast<UMovieScene3DTransformSection>(
                  Track->FindOrAddSection(0, bSectionAdded));
          if (Section) {
            FFrameRate TickResolution = MovieScene->GetTickResolution();
            FFrameRate DisplayRate = MovieScene->GetDisplayRate();
            FFrameNumber FrameNum = FFrameNumber(static_cast<int32>(Frame));
            FFrameNumber TickFrame =
                FFrameRate::TransformTime(FFrameTime(FrameNum), DisplayRate,
                                          TickResolution)
                    .FloorToFrame();

            bool bModified = false;
            const TSharedPtr<FJsonObject> *ValueObj = nullptr;

            FMovieSceneChannelProxy &Proxy = Section->GetChannelProxy();
            TArrayView<FMovieSceneDoubleChannel *> Channels =
                Proxy.GetChannels<FMovieSceneDoubleChannel>();

            if (LocalPayload->TryGetObjectField(TEXT("value"), ValueObj) &&
                ValueObj && Channels.Num() >= 9) {
              const TSharedPtr<FJsonObject> *LocObj = nullptr;
              if ((*ValueObj)->TryGetObjectField(TEXT("location"), LocObj)) {
                double X, Y, Z;
                if ((*LocObj)->TryGetNumberField(TEXT("x"), X)) {
                  Channels[0]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(X));
                  bModified = true;
                }
                if ((*LocObj)->TryGetNumberField(TEXT("y"), Y)) {
                  Channels[1]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Y));
                  bModified = true;
                }
                if ((*LocObj)->TryGetNumberField(TEXT("z"), Z)) {
                  Channels[2]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Z));
                  bModified = true;
                }
              }

              const TSharedPtr<FJsonObject> *RotObj = nullptr;
              if ((*ValueObj)->TryGetObjectField(TEXT("rotation"), RotObj)) {
                double P, Yaw, R;
                // UMovieScene3DTransformSection channel layout (standard UE order):
                // Channels 0-2: Location (X, Y, Z)
                // Channels 3-5: Rotation (Roll, Pitch, Yaw) - corresponds to FRotator order
                // Channels 6-8: Scale (X, Y, Z)
                if ((*RotObj)->TryGetNumberField(TEXT("roll"), R)) {
                  Channels[3]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(R));
                  bModified = true;
                }
                if ((*RotObj)->TryGetNumberField(TEXT("pitch"), P)) {
                  Channels[4]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(P));
                  bModified = true;
                }
                if ((*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw)) {
                  Channels[5]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Yaw));
                  bModified = true;
                }
              }

              const TSharedPtr<FJsonObject> *ScaleObj = nullptr;
              if ((*ValueObj)->TryGetObjectField(TEXT("scale"), ScaleObj)) {
                double X, Y, Z;
                if ((*ScaleObj)->TryGetNumberField(TEXT("x"), X)) {
                  Channels[6]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(X));
                  bModified = true;
                }
                if ((*ScaleObj)->TryGetNumberField(TEXT("y"), Y)) {
                  Channels[7]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Y));
                  bModified = true;
                }
                if ((*ScaleObj)->TryGetNumberField(TEXT("z"), Z)) {
                  Channels[8]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Z));
                  bModified = true;
                }
              }
            }

            if (bModified) {
              MovieScene->Modify();
              SendAutomationResponse(Socket, RequestId, true,
                                     TEXT("Keyframe added"), nullptr,
                                     FString());
              return true;
            }
          }
        }
      } else {
        // Try generic property tracks
        const TSharedPtr<FJsonValue> Val =
            LocalPayload->TryGetField(TEXT("value"));
        if (Val.IsValid() && Val->Type == EJson::Number) {
          UMovieSceneFloatTrack *Track =
              MovieScene->FindTrack<UMovieSceneFloatTrack>(
                  BindingGuid, FName(*PropertyName));
          if (!Track) {
            Track = MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingGuid);
            if (Track)
              Track->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
          }
          if (Track) {
            bool bSectionAdded = false;
            UMovieSceneFloatSection *Section = Cast<UMovieSceneFloatSection>(
                Track->FindOrAddSection(0, bSectionAdded));
            if (Section) {
              FFrameRate TickResolution = MovieScene->GetTickResolution();
              FFrameRate DisplayRate = MovieScene->GetDisplayRate();
              FFrameNumber FrameNum = FFrameNumber(static_cast<int32>(Frame));
              FFrameNumber TickFrame =
                  FFrameRate::TransformTime(FFrameTime(FrameNum), DisplayRate,
                                            TickResolution)
                      .GetFrame();

              FMovieSceneFloatChannel *Channel =
                  Section->GetChannelProxy()
                      .GetChannel<FMovieSceneFloatChannel>(0);
              if (Channel) {
                Channel->GetData().UpdateOrAddKey(
                    TickFrame, FMovieSceneFloatValue((float)Val->AsNumber()));
                MovieScene->Modify();
                SendAutomationResponse(Socket, RequestId, true,
                                       TEXT("Float Keyframe added"), nullptr);
                return true;
              }
            }
          }
        } else if (Val.IsValid() && Val->Type == EJson::Boolean) {
          UMovieSceneBoolTrack *Track =
              MovieScene->FindTrack<UMovieSceneBoolTrack>(BindingGuid,
                                                          FName(*PropertyName));
          if (!Track) {
            Track = MovieScene->AddTrack<UMovieSceneBoolTrack>(BindingGuid);
            if (Track)
              Track->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
          }
          if (Track) {
            bool bSectionAdded = false;
            UMovieSceneBoolSection *Section = Cast<UMovieSceneBoolSection>(
                Track->FindOrAddSection(0, bSectionAdded));
            if (Section) {
              FFrameRate TickResolution = MovieScene->GetTickResolution();
              FFrameRate DisplayRate = MovieScene->GetDisplayRate();
              FFrameNumber FrameNum = FFrameNumber(static_cast<int32>(Frame));
              FFrameNumber TickFrame =
                  FFrameRate::TransformTime(FFrameTime(FrameNum), DisplayRate,
                                            TickResolution)
                      .GetFrame();

              FMovieSceneBoolChannel *Channel =
                  Section->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(
                      0);
              if (Channel) {
                Channel->GetData().UpdateOrAddKey(TickFrame, Val->AsBool());
                MovieScene->Modify();
                SendAutomationResponse(Socket, RequestId, true,
                                       TEXT("Bool Keyframe added"), nullptr);
                return true;
              }
            }
          }
        }
      }

      SendAutomationResponse(
          Socket, RequestId, false,
          TEXT("Unsupported property or failed to create track"), nullptr,
          TEXT("UNSUPPORTED_PROPERTY"));
      return true;
    }
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Sequence object is not a LevelSequence"),
                         nullptr, TEXT("INVALID_SEQUENCE_TYPE"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_keyframe requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddSection(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_add_section requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  double StartFrame = 0.0, EndFrame = 100.0;
  Payload->TryGetNumberField(TEXT("startFrame"), StartFrame);
  Payload->TryGetNumberField(TEXT("endFrame"), EndFrame);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();

  // Find the track - either from master tracks or from actor binding
  UMovieSceneTrack *Track = nullptr;

  // First check master tracks
  for (UMovieSceneTrack *MasterTrack : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (MasterTrack &&
        (MasterTrack->GetName().Contains(TrackName) ||
         MasterTrack->GetDisplayName().ToString().Contains(TrackName))) {
      Track = MasterTrack;
      break;
    }
  }

  // If not found in master tracks, check bindings
  // Search all bindings if ActorName is empty, or filter by ActorName if
  // provided
  if (!Track) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      FString BindingName;
      if (FMovieScenePossessable *Possessable =
              MovieScene->FindPossessable(Binding.GetObjectGuid())) {
        BindingName = Possessable->GetName();
      } else if (FMovieSceneSpawnable *Spawnable =
                     MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
        BindingName = Spawnable->GetName();
      }

      // If ActorName is provided, filter by it; otherwise search all bindings
      if (ActorName.IsEmpty() || BindingName.Contains(ActorName)) {
        for (UMovieSceneTrack *BindingTrack : MCP_GET_BINDING_TRACKS(Binding)) {
          if (BindingTrack &&
              (BindingTrack->GetName().Contains(TrackName) ||
               BindingTrack->GetDisplayName().ToString().Contains(TrackName))) {
            Track = BindingTrack;
            break;
          }
        }
        if (Track)
          break;
      }
    }
  }

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  // Create the section
  UMovieSceneSection *NewSection = Track->CreateNewSection();
  if (NewSection) {
    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameNumber Start((int32)FMath::RoundToInt(StartFrame));
    FFrameNumber End((int32)FMath::RoundToInt(EndFrame));
    NewSection->SetRange(TRange<FFrameNumber>(Start, End));
    Track->AddSection(*NewSection);
    MovieScene->Modify();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("trackName"), Track->GetName());
    Resp->SetNumberField(TEXT("startFrame"), StartFrame);
    Resp->SetNumberField(TEXT("endFrame"), EndFrame);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Section added to track"), Resp);
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create section"), nullptr,
                           TEXT("SECTION_CREATION_FAILED"));
  }
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_section requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTickResolution(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ResolutionStr;
  Payload->TryGetStringField(TEXT("resolution"), ResolutionStr);

  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("path required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (Sequence && Sequence->GetMovieScene()) {
    // Get current tick resolution as fallback
    FFrameRate TickResolution = Sequence->GetMovieScene()->GetTickResolution();
    
    // Parse user-provided resolution string
    if (!ResolutionStr.IsEmpty()) {
      if (ResolutionStr.Contains(TEXT("24000")))
        TickResolution = FFrameRate(24000, 1);
      else if (ResolutionStr.Contains(TEXT("60000")))
        TickResolution = FFrameRate(60000, 1);
      else if (ResolutionStr.Contains(TEXT("/"))) {
        // Parse rational format "numerator/denominator"
        FString NumStr, DenomStr;
        if (ResolutionStr.Split(TEXT("/"), &NumStr, &DenomStr)) {
          int32 Num = FCString::Atoi(*NumStr);
          int32 Denom = FCString::Atoi(*DenomStr);
          if (Num > 0 && Denom > 0) {
            TickResolution = FFrameRate(Num, Denom);
          }
        }
      }
      else if (ResolutionStr.IsNumeric()) {
        // Parse simple integer format
        int32 Num = FCString::Atoi(*ResolutionStr);
        if (Num > 0) {
          TickResolution = FFrameRate(Num, 1);
        }
      }
      else {
        // Unrecognized format - log a warning but continue with current resolution
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("HandleSequenceSetTickResolution: Unrecognized resolution format '%s'. Using current resolution."),
               *ResolutionStr);
      }
    }

    Sequence->GetMovieScene()->SetTickResolutionDirectly(TickResolution);
    Sequence->GetMovieScene()->Modify();
    SendAutomationResponse(Socket, RequestId, true, TEXT("Tick resolution set"),
                           nullptr);
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("NOT_FOUND"));
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetViewRange(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double Start = 0;
  double End = 10;
  Payload->TryGetNumberField(TEXT("start"), Start);
  Payload->TryGetNumberField(TEXT("end"), End);
  FString SeqPath = ResolveSequencePath(Payload);

  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("path required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (Sequence && Sequence->GetMovieScene()) {
    Sequence->GetMovieScene()->SetViewRange(Start, End);
    Sequence->GetMovieScene()->Modify();
    SendAutomationResponse(Socket, RequestId, true, TEXT("View range set"),
                           nullptr);
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("NOT_FOUND"));
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTrackMuted(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  bool bMuted = true;
  Payload->TryGetBoolField(TEXT("muted"), bMuted);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *Track = nullptr;

  // Search master tracks and binding tracks
  for (UMovieSceneTrack *MasterTrack : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (MasterTrack && MasterTrack->GetName().Contains(TrackName)) {
      Track = MasterTrack;
      break;
    }
  }

  if (!Track) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      for (UMovieSceneTrack *BindingTrack : MCP_GET_BINDING_TRACKS(Binding)) {
        if (BindingTrack && BindingTrack->GetName().Contains(TrackName)) {
          Track = BindingTrack;
          break;
        }
      }
      if (Track)
        break;
    }
  }

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  // Set muted state via EvalOptions
  Track->SetEvalDisabled(bMuted);
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetBoolField(TEXT("muted"), bMuted);
  SendAutomationResponse(Socket, RequestId, true,
                         bMuted ? TEXT("Track muted") : TEXT("Track unmuted"),
                         Resp);
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_track_muted requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTrackSolo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Note: UE doesn't have a direct "solo" property on tracks, but we can
  // simulate it by muting all other tracks
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  bool bSolo = true;
  Payload->TryGetBoolField(TEXT("solo"), bSolo);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *SoloTrack = nullptr;

  // Find the track to solo
  TArray<UMovieSceneTrack *> AllTracks;
  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (Track) {
      AllTracks.Add(Track);
      if (Track->GetName().Contains(TrackName)) {
        SoloTrack = Track;
      }
    }
  }

  for (const FMovieSceneBinding &Binding :
       const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
      if (Track) {
        AllTracks.Add(Track);
        if (Track->GetName().Contains(TrackName)) {
          SoloTrack = Track;
        }
      }
    }
  }

  if (!SoloTrack) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  // If enabling solo, mute all other tracks; if disabling, unmute all
  for (UMovieSceneTrack *Track : AllTracks) {
    if (bSolo) {
      Track->SetEvalDisabled(Track != SoloTrack);
    } else {
      Track->SetEvalDisabled(false);
    }
  }
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("trackName"), SoloTrack->GetName());
  Resp->SetBoolField(TEXT("solo"), bSolo);
  Resp->SetStringField(TEXT("note"), TEXT("Solo is simulated by muting all other tracks. Unreal Engine does not have native track solo support."));
  SendAutomationResponse(
      Socket, RequestId, true,
      bSolo ? TEXT("Track solo enabled (simulated via muting other tracks)") : TEXT("Solo disabled (all tracks unmuted)"), Resp);
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_track_solo requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTrackLocked(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  bool bLocked = true;
  Payload->TryGetBoolField(TEXT("locked"), bLocked);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *Track = nullptr;

  // Search for track
  for (UMovieSceneTrack *MasterTrack : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (MasterTrack && MasterTrack->GetName().Contains(TrackName)) {
      Track = MasterTrack;
      break;
    }
  }

  if (!Track) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      for (UMovieSceneTrack *BindingTrack : MCP_GET_BINDING_TRACKS(Binding)) {
        if (BindingTrack && BindingTrack->GetName().Contains(TrackName)) {
          Track = BindingTrack;
          break;
        }
      }
      if (Track)
        break;
    }
  }

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  // Lock all sections in the track
  for (UMovieSceneSection *Section : Track->GetAllSections()) {
    if (Section) {
      Section->SetIsLocked(bLocked);
    }
  }
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetBoolField(TEXT("locked"), bLocked);
  SendAutomationResponse(
      Socket, RequestId, true,
      bLocked ? TEXT("Track locked") : TEXT("Track unlocked"), Resp);
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_set_track_locked requires editor build"), nullptr,
      TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceRemoveTrack(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  bool bRemoved = false;
  FString RemovedTrackName;

  // Try to remove from master tracks first
  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (Track && Track->GetName().Contains(TrackName)) {
      RemovedTrackName = Track->GetName();
      MovieScene->RemoveTrack(*Track);
      bRemoved = true;
      break;
    }
  }

  // If not found, try binding tracks
  if (!bRemoved) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
        if (Track && Track->GetName().Contains(TrackName)) {
          RemovedTrackName = Track->GetName();
          MovieScene->RemoveTrack(*Track);
          bRemoved = true;
          break;
        }
      }
      if (bRemoved)
        break;
    }
  }

  if (bRemoved) {
    MovieScene->Modify();
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("trackName"), RemovedTrackName);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Track removed"),
                           Resp);
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
  }
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_remove_track requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  // Also handle manage_sequence which acts as a dispatcher for sub-actions
  if (!Lower.StartsWith(TEXT("sequence_")) &&
      !Lower.Equals(TEXT("manage_sequence")))
    return false;

  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
  FString EffectiveAction = Lower;

  // If generic manage_sequence, extract the sub-action to determine behavior
  if (Lower.Equals(TEXT("manage_sequence"))) {
    FString Sub;
    if (LocalPayload->TryGetStringField(TEXT("subAction"), Sub) &&
        !Sub.IsEmpty()) {
      EffectiveAction = Sub.ToLower();
      // If subAction is just "create", map to "sequence_create" for consistency
      if (EffectiveAction == TEXT("create"))
        EffectiveAction = TEXT("sequence_create");
      else if (!EffectiveAction.StartsWith(TEXT("sequence_")))
        EffectiveAction = TEXT("sequence_") + EffectiveAction;
    }
  }

  if (EffectiveAction == TEXT("sequence_create"))
    return HandleSequenceCreate(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_display_rate"))
    return HandleSequenceSetDisplayRate(RequestId, LocalPayload,
                                        RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_properties"))
    return HandleSequenceSetProperties(RequestId, LocalPayload,
                                       RequestingSocket);
  if (EffectiveAction == TEXT("sequence_open"))
    return HandleSequenceOpen(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_camera"))
    return HandleSequenceAddCamera(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_play"))
    return HandleSequencePlay(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_actor"))
    return HandleSequenceAddActor(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_actors"))
    return HandleSequenceAddActors(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_spawnable_from_class"))
    return HandleSequenceAddSpawnable(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_remove_actors"))
    return HandleSequenceRemoveActors(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_get_bindings"))
    return HandleSequenceGetBindings(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_get_properties"))
    return HandleSequenceGetProperties(RequestId, LocalPayload,
                                       RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_playback_speed"))
    return HandleSequenceSetPlaybackSpeed(RequestId, LocalPayload,
                                          RequestingSocket);
  if (EffectiveAction == TEXT("sequence_pause"))
    return HandleSequencePause(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_stop"))
    return HandleSequenceStop(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_list"))
    return HandleSequenceList(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_duplicate"))
    return HandleSequenceDuplicate(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_rename"))
    return HandleSequenceRename(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_delete"))
    return HandleSequenceDelete(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_get_metadata"))
    return HandleSequenceGetMetadata(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_keyframe"))
    return HandleSequenceAddKeyframe(RequestId, LocalPayload, RequestingSocket);

  // New handlers
  if (EffectiveAction == TEXT("sequence_add_section"))
    return HandleSequenceAddSection(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_tick_resolution"))
    return HandleSequenceSetTickResolution(RequestId, LocalPayload,
                                           RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_view_range"))
    return HandleSequenceSetViewRange(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_track_muted"))
    return HandleSequenceSetTrackMuted(RequestId, LocalPayload,
                                       RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_track_solo"))
    return HandleSequenceSetTrackSolo(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_track_locked"))
    return HandleSequenceSetTrackLocked(RequestId, LocalPayload,
                                        RequestingSocket);
  if (EffectiveAction == TEXT("sequence_remove_track"))
    return HandleSequenceRemoveTrack(RequestId, LocalPayload, RequestingSocket);

  if (EffectiveAction == TEXT("sequence_list_track_types")) {
    // Discovery: list available track types
    TArray<TSharedPtr<FJsonValue>> Types;
    // Add common shortcuts first
    Types.Add(MakeShared<FJsonValueString>(TEXT("transform")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("3dtransform")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("audio")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("event")));

    // Discover all UMovieSceneTrack subclasses via reflection
    TSet<FString> AddedNames;
    AddedNames.Add(TEXT("transform"));
    AddedNames.Add(TEXT("3dtransform"));
    AddedNames.Add(TEXT("audio"));
    AddedNames.Add(TEXT("event"));

    for (TObjectIterator<UClass> It; It; ++It) {
      if (It->IsChildOf(UMovieSceneTrack::StaticClass()) &&
          !It->HasAnyClassFlags(CLASS_Abstract) &&
          !AddedNames.Contains(It->GetName())) {
        Types.Add(MakeShared<FJsonValueString>(It->GetName()));
        AddedNames.Add(It->GetName());
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetArrayField(TEXT("types"), Types);
    Resp->SetNumberField(TEXT("count"), Types.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Available track types"), Resp);
    return true;
  }

  if (EffectiveAction == TEXT("sequence_add_track")) {
    // add_track action: Add a track to a binding in a level sequence
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("sequence_add_track requires a sequence path"), nullptr,
          TEXT("INVALID_SEQUENCE"));
      return true;
    }

    ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Level sequence not found"), nullptr,
                             TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }

    UMovieScene *MovieScene = Sequence->GetMovieScene();
    if (!MovieScene) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("MovieScene not available"), nullptr,
                             TEXT("MOVIESCENE_UNAVAILABLE"));
      return true;
    }

    FString TrackType;
    LocalPayload->TryGetStringField(TEXT("trackType"), TrackType);
    if (TrackType.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("trackType required (e.g., Transform, Animation, Audio, Event)"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString TrackName;
    LocalPayload->TryGetStringField(TEXT("trackName"), TrackName);

    FString ActorName;
    LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);

    // Find or use master track if no actor specified
    FGuid BindingGuid;
    if (!ActorName.IsEmpty()) {
      // Find binding by actor name
      // Use const interface to avoid deprecation warning
      const UMovieScene *ConstMovieScene = MovieScene;
      for (const FMovieSceneBinding &Binding : ConstMovieScene->GetBindings()) {
        FString BindingName;
        if (FMovieScenePossessable *Possessable =
                MovieScene->FindPossessable(Binding.GetObjectGuid())) {
          BindingName = Possessable->GetName();
        } else if (FMovieSceneSpawnable *Spawnable =
                       MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
          BindingName = Spawnable->GetName();
        }

        if (BindingName.Contains(ActorName)) {
          BindingGuid = Binding.GetObjectGuid();
          break;
        }
      }
      if (!BindingGuid.IsValid()) {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Binding not found for actor: %s"),
                            *ActorName),
            nullptr, TEXT("BINDING_NOT_FOUND"));
        return true;
      }
    }

    // Add the track
    UMovieSceneTrack *NewTrack = nullptr;

    // Dynamic resolution with heuristics
    UClass *TrackClass = ResolveUClass(TrackType);

    // Try with common prefixes
    if (!TrackClass) {
      TrackClass = ResolveUClass(
          FString::Printf(TEXT("UMovieScene%sTrack"), *TrackType));
    }
    if (!TrackClass) {
      TrackClass =
          ResolveUClass(FString::Printf(TEXT("MovieScene%sTrack"), *TrackType));
    }
    // Try simple "U" prefix
    if (!TrackClass) {
      TrackClass = ResolveUClass(FString::Printf(TEXT("U%s"), *TrackType));
    }

    // Validate it's actually a track class
    if (TrackClass && TrackClass->IsChildOf(UMovieSceneTrack::StaticClass())) {
      if (BindingGuid.IsValid()) {
        NewTrack = MovieScene->AddTrack(TrackClass, BindingGuid);
      } else {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        NewTrack = MovieScene->AddTrack(TrackClass);
#else
        // UE 5.0: AddTrack always requires a binding GUID
        SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("Adding tracks without binding is not supported in UE 5.0. Please provide an actor or object binding."),
            TEXT("NOT_SUPPORTED"));
        return true;
#endif
      }
    } else if (TrackClass) {
      // Found a class but it's not a track
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Class '%s' is not a UMovieSceneTrack"),
                          *TrackClass->GetName()),
          TEXT("INVALID_CLASS_TYPE"));
      return true;
    }

    if (NewTrack) {
      Sequence->MarkPackageDirty();

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("sequencePath"), SeqPath);
      Resp->SetStringField(TEXT("trackType"), TrackType);
      Resp->SetStringField(TEXT("trackName"),
                           TrackName.IsEmpty() ? TrackType : TrackName);
      if (!ActorName.IsEmpty()) {
        Resp->SetStringField(TEXT("actorName"), ActorName);
        Resp->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
      }
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Track added successfully"), Resp, FString());
    } else {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Failed to add track of type: %s"), *TrackType),
          nullptr, TEXT("TRACK_CREATION_FAILED"));
    }
    return true;
  }

  // sequence_list_tracks: List all tracks for a sequence binding
  if (EffectiveAction == TEXT("sequence_list_tracks")) {
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("sequence_list_tracks requires a sequence path"), nullptr,
          TEXT("INVALID_SEQUENCE"));
      return true;
    }

#if WITH_EDITOR
    ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Level sequence not found"), nullptr,
                             TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }

    UMovieScene *MovieScene = Sequence->GetMovieScene();
    if (!MovieScene) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("MovieScene not available"), nullptr,
                             TEXT("MOVIESCENE_UNAVAILABLE"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> TracksArray;

    // Get Tracks (formerly GetMasterTracks)
    for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
      if (!Track)
        continue;
      TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
      TrackObj->SetStringField(TEXT("trackName"), Track->GetName());
      TrackObj->SetStringField(TEXT("trackType"), Track->GetClass()->GetName());
      TrackObj->SetStringField(TEXT("displayName"),
                               Track->GetDisplayName().ToString());
      TrackObj->SetBoolField(TEXT("isMasterTrack"), true);
      TrackObj->SetNumberField(TEXT("sectionCount"),
                               Track->GetAllSections().Num());
      TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
    }

    // Get tracks from bindings
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      FString BindingName;
      if (FMovieScenePossessable *Possessable =
              MovieScene->FindPossessable(Binding.GetObjectGuid())) {
        BindingName = Possessable->GetName();
      } else if (FMovieSceneSpawnable *Spawnable =
                     MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
        BindingName = Spawnable->GetName();
      }

      for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
        if (!Track)
          continue;
        TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
        TrackObj->SetStringField(TEXT("trackName"), Track->GetName());
        TrackObj->SetStringField(TEXT("trackType"),
                                 Track->GetClass()->GetName());
        TrackObj->SetStringField(TEXT("displayName"),
                                 Track->GetDisplayName().ToString());
        TrackObj->SetBoolField(TEXT("isMasterTrack"), false);
        TrackObj->SetStringField(TEXT("bindingName"), BindingName);
        TrackObj->SetStringField(TEXT("bindingGuid"),
                                 Binding.GetObjectGuid().ToString());
        TrackObj->SetNumberField(TEXT("sectionCount"),
                                 Track->GetAllSections().Num());
        TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetArrayField(TEXT("tracks"), TracksArray);
    Resp->SetNumberField(TEXT("trackCount"), TracksArray.Num());
    Resp->SetStringField(TEXT("sequencePath"), SeqPath);
    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Found %d tracks"), TracksArray.Num()), Resp,
        FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("sequence_list_tracks requires editor build"),
                           nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
  }

  // sequence_set_work_range: Set the work range of a sequence
  if (EffectiveAction == TEXT("sequence_set_work_range")) {
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("sequence_set_work_range requires a sequence path"), nullptr,
          TEXT("INVALID_SEQUENCE"));
      return true;
    }

#if WITH_EDITOR
    ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Level sequence not found"), nullptr,
                             TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }

    UMovieScene *MovieScene = Sequence->GetMovieScene();
    if (!MovieScene) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("MovieScene not available"), nullptr,
                             TEXT("MOVIESCENE_UNAVAILABLE"));
      return true;
    }

    double Start = 0.0, End = 0.0;
    LocalPayload->TryGetNumberField(TEXT("start"), Start);
    LocalPayload->TryGetNumberField(TEXT("end"), End);

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    // Round to int32 for FFrameNumber constructor
    FFrameNumber StartFrame(
        (int32)FMath::RoundToInt(Start * TickResolution.AsDecimal()));
    FFrameNumber EndFrame(
        (int32)FMath::RoundToInt(End * TickResolution.AsDecimal()));

    // SetWorkingRange expects seconds (double)
    MovieScene->SetWorkingRange(Start, End);
    MovieScene->Modify();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetNumberField(TEXT("startFrame"), StartFrame.Value);
    Resp->SetNumberField(TEXT("endFrame"), EndFrame.Value);
    Resp->SetStringField(TEXT("sequencePath"), SeqPath);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Work range set successfully"), Resp,
                           FString());
    return true;
#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("sequence_set_work_range requires editor build"), nullptr,
        TEXT("EDITOR_ONLY"));
    return true;
#endif
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Sequence action not implemented by plugin: %s"),
                      *Action),
      nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
}