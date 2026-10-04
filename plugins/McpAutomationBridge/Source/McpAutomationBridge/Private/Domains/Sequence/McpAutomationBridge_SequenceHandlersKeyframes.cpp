#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"
#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

namespace {
// Location, Translation, Rotation and Scale key that part of the Transform track.
const TCHAR *TransformPartFor(const FString &PropertyName) {
  if (PropertyName.Equals(TEXT("Location"), ESearchCase::IgnoreCase) ||
      PropertyName.Equals(TEXT("Translation"), ESearchCase::IgnoreCase)) {
    return TEXT("location");
  }
  if (PropertyName.Equals(TEXT("Rotation"), ESearchCase::IgnoreCase)) {
    return TEXT("rotation");
  }
  return PropertyName.Equals(TEXT("Scale"), ESearchCase::IgnoreCase) ? TEXT("scale") : nullptr;
}

void WrapTransformPart(const TSharedPtr<FJsonObject> &Key, const TCHAR *Part) {
  const TSharedPtr<FJsonObject> *ValueObj = nullptr;
  if (Key->TryGetObjectField(TEXT("value"), ValueObj) && ValueObj && ValueObj->IsValid()) {
    TSharedPtr<FJsonObject> TransformValue = MakeShared<FJsonObject>();
    TransformValue->SetObjectField(Part, *ValueObj);
    Key->SetObjectField(TEXT("value"), TransformValue);
  }
}

// The keys a call writes: each keys entry {frame, value, interpolation} (an entry without interpolation takes the
// call's), or else the call's own frame and value. Every key's frame and interpolation are checked before any is
// written.
bool ReadKeys(const TSharedPtr<FJsonObject> &Payload, TArray<TSharedPtr<FJsonObject>> &OutKeys, FString &OutError) {
  const TArray<TSharedPtr<FJsonValue>> *Entries = nullptr;
  const bool bBatch = Payload->TryGetArrayField(TEXT("keys"), Entries) && Entries;
  if (!bBatch) {
    OutKeys.Add(Payload);
  } else {
    for (const TSharedPtr<FJsonValue> &Entry : *Entries) {
      const TSharedPtr<FJsonObject> *EntryObj = nullptr;
      if (!Entry.IsValid() || !Entry->TryGetObject(EntryObj) || !EntryObj || !EntryObj->IsValid()) {
        OutError = TEXT("each keys entry is an object {frame, value, interpolation}");
        return false;
      }
      TSharedPtr<FJsonObject> Key = MakeShared<FJsonObject>();
      for (const TCHAR *Field : {TEXT("frame"), TEXT("value"), TEXT("interpolation")}) {
        TSharedPtr<FJsonValue> Value = (*EntryObj)->TryGetField(Field);
        if (!Value.IsValid() && FCString::Strcmp(Field, TEXT("interpolation")) == 0) {
          Value = Payload->TryGetField(Field);
        }
        if (Value.IsValid()) {
          Key->SetField(Field, Value);
        }
      }
      OutKeys.Add(Key);
    }
  }
  if (OutKeys.Num() == 0) {
    OutError = TEXT("keys needs at least one {frame, value} entry");
    return false;
  }
  for (int32 Index = 0; Index < OutKeys.Num(); ++Index) {
    const FString Where = bBatch ? FString::Printf(TEXT("keys[%d]: "), Index) : FString();
    double Frame = 0.0;
    ERichCurveInterpMode Interpolation = RCIM_Cubic;
    if (!OutKeys[Index]->TryGetNumberField(TEXT("frame"), Frame)) {
      OutError = Where + TEXT("frame number is required. Example: {\"frame\": 30} for keyframe at frame 30, or "
                              "keys: [{\"frame\": 0, \"value\": ...}, {\"frame\": 30, \"value\": ...}] for several");
      return false;
    }
    if (!McpSequenceKeyframes::ReadKeyInterpolation(OutKeys[Index], Interpolation)) {
      OutError = Where + TEXT("interpolation must be auto, linear or constant");
      return false;
    }
  }
  return true;
}
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddKeyframe(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
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

  TArray<TSharedPtr<FJsonObject>> Keys;
  FString KeyError;
  if (!ReadKeys(LocalPayload, Keys, KeyError)) {
    SendAutomationResponse(Socket, RequestId, false, KeyError, nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const bool bBatch = LocalPayload->HasField(TEXT("keys"));
  if (const TCHAR *Part = TransformPartFor(PropertyName)) {
    for (const TSharedPtr<FJsonObject> &Key : Keys) {
      WrapTransformPart(Key, Part);
    }
    PropertyName = TEXT("Transform");
  }

  UObject *SeqObj = McpLoadAsset(SeqPath);
  ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj);
  UMovieScene *MovieScene = LevelSeq ? LevelSeq->GetMovieScene() : nullptr;
  if (!MovieScene) {
    SendAutomationResponse(Socket, RequestId, false,
                           SeqObj ? TEXT("Sequence object is not a LevelSequence") : TEXT("Sequence not found"),
                           nullptr, SeqObj ? TEXT("INVALID_SEQUENCE_TYPE") : TEXT("INVALID_SEQUENCE"));
    return true;
  }
  FGuid BindingGuid = McpSequenceKeyframes::ResolveBindingGuid(MovieScene, BindingIdStr, ActorName);
  if (!BindingGuid.IsValid() || !MovieScene->FindBinding(BindingGuid)) {
    FString Target = !BindingIdStr.IsEmpty() ? BindingIdStr : ActorName;
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Binding not found for '%s'. Ensure actor is bound to sequence."), *Target),
        nullptr, TEXT("BINDING_NOT_FOUND"));
    return true;
  }

  const bool bTransform = PropertyName.Equals(TEXT("Transform"), ESearchCase::IgnoreCase);
  const USceneComponent *BoundRoot = nullptr;
  if (bTransform) {
    TArray<UObject *, TInlineAllocator<1>> Bound;
    McpSequenceCinematics::LocateBindingObjects(LevelSeq, BindingGuid, GEditor ? GEditor->GetEditorWorldContext().World() : nullptr, Bound);
    const AActor *BoundActor = Bound.Num() > 0 ? Cast<AActor>(Bound[0]) : nullptr;
    BoundRoot = BoundActor ? BoundActor->GetRootComponent() : (Bound.Num() > 0 ? Cast<USceneComponent>(Bound[0]) : nullptr);
  }

  bool bRangeExtended = false;
  FString SuccessMessage = TEXT("Keyframe added");
  for (int32 Index = 0; Index < Keys.Num(); ++Index) {
    const FString Where = bBatch ? FString::Printf(TEXT("keys[%d]: "), Index) : FString();
    double Frame = 0.0;
    Keys[Index]->TryGetNumberField(TEXT("frame"), Frame);
    FFrameNumber TickFrame;
    FString FrameError;
    if (!McpSequenceFrameMath::TryTransformFrameFloor(Frame, MovieScene->GetDisplayRate(),
                                                      MovieScene->GetTickResolution(), TickFrame, FrameError)) {
      SendAutomationResponse(Socket, RequestId, false, Where + FrameError, nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    // A key outside the playback range is silently dead: it never evaluates and nothing said so. Sequencer's own
    // editor auto-expands here, so do the same and report it rather than accepting a key that cannot play.
    const TRange<FFrameNumber> Playback = MovieScene->GetPlaybackRange();
    if (!Playback.Contains(TickFrame)) {
      MovieScene->Modify();
      MovieScene->SetPlaybackRange(TRange<FFrameNumber>::Hull(Playback, TRange<FFrameNumber>(TickFrame, TickFrame + 1)), false);
      MovieScene->MarkPackageDirty();
      bRangeExtended = true;
    }
    const bool bAdded =
        bTransform ? McpSequenceKeyframes::AddTransformKeyframe(MovieScene, BindingGuid, TickFrame, Keys[Index], BoundRoot)
                   : McpSequenceKeyframes::AddPropertyKeyframe(MovieScene, BindingGuid, PropertyName, TickFrame,
                                                               Keys[Index], SuccessMessage);
    if (!bAdded) {
      SendAutomationResponse(
          Socket, RequestId, false,
          Where + (Index > 0 ? FString::Printf(TEXT("(the %d keys before it were written) "), Index) : FString()) +
              TEXT("Unsupported property or failed to create track. Supported "
                   "properties: transform (location+rotation+scale in one value "
                   "object; lookAt there needs location beside it), "
                   "location/translation, rotation, scale, Visibility "
                   "(true or false), or a float or bool property — e.g. "
                   "{\"property\": \"transform\", \"value\": {\"location\": "
                   "{\"x\":0,\"y\":0,\"z\":0}, \"rotation\": {\"pitch\":0,\"yaw\":0,"
                   "\"roll\":0}, \"scale\": {\"x\":1,\"y\":1,\"z\":1}}}."),
          nullptr, TEXT("UNSUPPORTED_PROPERTY"));
      return true;
    }
  }
  if (bBatch) {
    SuccessMessage = FString::Printf(TEXT("%d keys added"), Keys.Num());
  }
  if (bRangeExtended) {
    SuccessMessage += bBatch ? TEXT("; playback range extended to include them") : TEXT("; playback range extended to include it");
  }
  SendAutomationResponse(Socket, RequestId, true, SuccessMessage, nullptr, FString());
  return true;
}
