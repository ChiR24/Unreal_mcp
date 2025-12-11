#include "LevelSequence.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneSequence.h"

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
#include "IAssetTools.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"

// Header checks removed causing issues with private headers

#if __has_include("LevelSequenceEditorSubsystem.h")
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

#if __has_include("Tracks/MovieScene3DTransformTrack.h")
#include "Tracks/MovieScene3DTransformTrack.h"
#endif
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
        FString Norm = Obj->GetPathName();
        if (Norm.Contains(TEXT("."))) {
          Norm = Norm.Left(Norm.Find(TEXT(".")));
        }
        return Norm;
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
      UEditorAssetLibrary::SaveAsset(FullPath);
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
    if (UEditorActorSubsystem *ActorSS =
            GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
      UClass *CameraClass = ACameraActor::StaticClass();
      AActor *Spawned = ActorSS->SpawnActorFromClass(
          CameraClass, FVector::ZeroVector, FRotator::ZeroRotator);
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
        SendAutomationResponse(
            Socket, RequestId, true,
            TEXT("Camera actor spawned and bound to sequence"), Resp,
            FString());
        return true;
      }
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
      AActor *Found = nullptr;
      TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
      for (AActor *A : AllActors) {
        if (A && A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase)) {
          Found = A;
          break;
        }
      }

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
      if (Editor && Editor->GetEditorName() == FName("LevelSequenceEditor")) {
        // We assume it implements ILevelSequenceEditorToolkit if the name
        // matches
        ILevelSequenceEditorToolkit *LSEditor =
            static_cast<ILevelSequenceEditorToolkit *>(Editor);
        if (LSEditor && LSEditor->GetSequencer().IsValid()) {
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
      ULevelSequenceEditorBlueprintLibrary::SetGlobalPosition(PlaybackParams);

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
  Filter.ClassPaths.Add(ULevelSequence::StaticClass()->GetClassPathName());
  Filter.bRecursiveClasses = true;
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(FName("/Game"));

  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> SeqObj = MakeShared<FJsonObject>();
    SeqObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
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
                // 0=Roll(X), 1=Pitch(Y), 2=Yaw(Z) in Transform Track channels
                // usually. Channels 3, 4, 5.
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

  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Sequence action not implemented by plugin: %s"),
                      *Action),
      nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
}