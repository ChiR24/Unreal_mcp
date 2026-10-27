#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "LevelSequence.h"
#include "Core/Module/EngineWireGlobals.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "UObject/UObjectIterator.h"

#define MCP_GET_BINDING_TRACKS(Binding) (Binding).GetTracks()

#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/EditorActorSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"


#if __has_include("ILevelSequenceEditorToolkit.h")
#include "ILevelSequenceEditorToolkit.h"
#endif

#include "ISequencer.h"
#include "MovieSceneSequencePlayer.h"

#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"

#include "Sections/MovieSceneBoolSection.h"
#include "Tracks/MovieSceneBoolTrack.h"

#include "Tracks/MovieScene3DTransformTrack.h"

#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneEventTrack.h"

#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneChannelProxy.h"

#include "ScopedTransaction.h"
#include "Camera/CameraActor.h"

namespace McpSequence {
FString ResolvePath(const TSharedPtr<FJsonObject> &Payload);
// The payload's level sequence and its movie scene; null after replying INVALID_SEQUENCE,
// SEQUENCE_NOT_FOUND or MOVIESCENE_UNAVAILABLE.
ULevelSequence *LoadOrReply(UEngineWireSubsystem *Subsystem, const FString &RequestId,
                            TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject> &Payload,
                            const TCHAR *Action, UMovieScene *&OutMovieScene);
// The reply for a trackName FindTrackByName did not resolve: TRACK_NOT_FOUND, or AMBIGUOUS_TRACK naming the tracks
// it fits (Ambiguous, as FindTrackByName lists them).
void ReplyTrackMiss(UEngineWireSubsystem *Subsystem, const FString &RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                    const FString &TrackName, const FString &Ambiguous);
// A new LevelSequence asset Folder/Name made by the editor's LevelSequenceFactoryNew; null when
// the factory is unavailable or AssetTools refuses.
ULevelSequence *CreateSequenceAsset(const FString &Name, const FString &Folder);
}

// Display name of a possessable or spawnable binding; empty when the guid is unknown.
inline FString GetBindingName(UMovieScene *MovieScene, const FGuid &Guid) {
  if (FMovieScenePossessable *Possessable = MovieScene->FindPossessable(Guid)) {
    return Possessable->GetName();
  }
  if (FMovieSceneSpawnable *Spawnable = MovieScene->FindSpawnable(Guid)) {
    return Spawnable->GetName();
  }
  return FString();
}

// A binding as reads report it: a component bound under its actor reads "Actor/Component", so the tracks of several
// actors' NiagaraComponent0 tell apart. GetBindingName stays the bare name that lookups match against.
inline FString GetBindingLabel(UMovieScene *MovieScene, const FGuid &Guid) {
  const FString Name = GetBindingName(MovieScene, Guid);
  const FMovieScenePossessable *Possessable = MovieScene->FindPossessable(Guid);
  const FString Parent = Possessable && Possessable->GetParent().IsValid() ? GetBindingName(MovieScene, Possessable->GetParent()) : FString();
  return Parent.IsEmpty() || Parent == Name ? Name : Parent + TEXT("/") + Name;
}

// The track TrackName names among the movie scene tracks and the tracks of every binding whose name contains
// BindingFilter (all bindings when the filter is empty): its own name (unique), else (when bMatchDisplayName) its
// display name, else a part of either, so Transform_1 never lands on Transform_10. A step several tracks fit names
// none of them: the first one met was taken, so "Transform" with two actors bound muted whichever came first.
// OutAmbiguous, when given, then lists them.
inline UMovieSceneTrack *FindTrackByName(UMovieScene *MovieScene, const FString &TrackName,
                                         bool bMatchDisplayName = false,
                                         const FString &BindingFilter = FString(),
                                         FString *OutAmbiguous = nullptr) {
  // The camera cut track is stored in its OWN UMovieScene member, not in the
  // Tracks array, so every lookup that walked only those two collections was
  // blind to it: removing, muting, soloing or locking a camera cut answered
  // TRACK_NOT_FOUND for a track the readback had just listed.
  TArray<UMovieSceneTrack *> Tracks(MCP_GET_MOVIESCENE_TRACKS(MovieScene));
  Tracks.Add(MovieScene->GetCameraCutTrack());
  for (const FMovieSceneBinding &Binding : const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    if (BindingFilter.IsEmpty() || GetBindingName(MovieScene, Binding.GetObjectGuid()).Contains(BindingFilter)) {
      Tracks.Append(MCP_GET_BINDING_TRACKS(Binding));
    }
  }
  for (int32 Step = 0; Step < 3; ++Step) {
    TArray<UMovieSceneTrack *> Found;
    for (UMovieSceneTrack *Track : Tracks) {
      const FString Name = Track ? Track->GetName() : FString();
      const FString Display = Track && bMatchDisplayName ? Track->GetDisplayName().ToString() : FString();
      if (Track && (Step == 0   ? Name.Equals(TrackName, ESearchCase::IgnoreCase)
                    : Step == 1 ? !Display.IsEmpty() && Display.Equals(TrackName, ESearchCase::IgnoreCase)
                                : Name.Contains(TrackName) || (!Display.IsEmpty() && Display.Contains(TrackName)))) {
        Found.Add(Track);
      }
    }
    if (Found.Num() == 1) {
      return Found[0];
    }
    for (int32 Index = 0; OutAmbiguous && Index < Found.Num(); ++Index) {
      *OutAmbiguous += FString::Printf(TEXT("%s%s (%s)"), Index ? TEXT(", ") : TEXT(""), *Found[Index]->GetName(),
                                       *Found[Index]->GetDisplayName().ToString());
    }
    if (Found.Num() > 1) {
      return nullptr;
    }
  }
  return nullptr;
}

// Every track whose name or display name contains TrackName (all tracks when it is empty; only those it names
// whole, when any),
// under the same name rule as FindTrackByName so a name that works for
// remove_track works for the key actions too. The movie scene tracks and the
// camera cut track are included only when no BindingId is given: a caller who
// scoped the call to one binding must not have the fade, audio and camera cut
// tracks swept up with it. Bindings come from GetBindings(), so spawnables are
// covered alongside possessables.
inline void CollectTracksByName(UMovieScene *MovieScene, const FString &TrackName,
                                const FString &BindingId,
                                TArray<UMovieSceneTrack *> &Out) {
  auto Matches = [&](UMovieSceneTrack *Track) {
    return Track && (TrackName.IsEmpty() || Track->GetName().Contains(TrackName) ||
                     Track->GetDisplayName().ToString().Contains(TrackName));
  };
  FGuid WantedBinding;
  const bool bHasBinding = !BindingId.IsEmpty();
  if (bHasBinding && !FGuid::Parse(BindingId, WantedBinding)) {
    return;
  }
  if (!bHasBinding) {
    for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
      if (Matches(Track)) {
        Out.Add(Track);
      }
    }
    if (UMovieSceneTrack *CameraCutTrack = MovieScene->GetCameraCutTrack()) {
      if (Matches(CameraCutTrack)) {
        Out.Add(CameraCutTrack);
      }
    }
  }
  for (const FMovieSceneBinding &Binding : const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    if (bHasBinding && Binding.GetObjectGuid() != WantedBinding) {
      continue;
    }
    for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
      if (Matches(Track)) {
        Out.Add(Track);
      }
    }
  }
  // A whole name keeps only the tracks of that name: Transform_1 swept up Transform_10 to _19 as well.
  auto Whole = [&TrackName](UMovieSceneTrack *Track) {
    return Track->GetName().Equals(TrackName, ESearchCase::IgnoreCase) ||
           Track->GetDisplayName().ToString().Equals(TrackName, ESearchCase::IgnoreCase);
  };
  if (!TrackName.IsEmpty() && Out.ContainsByPredicate(Whole)) {
    Out.RemoveAll([&Whole](UMovieSceneTrack *Track) { return !Whole(Track); });
  }
}

namespace McpSequenceKeyframes {
FGuid ResolveBindingGuid(UMovieScene *MovieScene, const FString &BindingIdStr,
                         const FString &ActorName);
// The payload's interpolation as a key mode: auto (the default, a smooth curve through the keys), linear or
// constant; false for any other name.
bool ReadKeyInterpolation(const TSharedPtr<FJsonObject> &Payload, ERichCurveInterpMode &OutMode);
// The section a key at TickFrame goes in: the track's section at frame 0, or a new one (bOutAdded), spanning the
// playback range and grown to reach the key; null when the track holds no section.
UMovieSceneSection *FindOrAddKeySection(UMovieScene *MovieScene, UMovieScenePropertyTrack *Track,
                                        FFrameNumber TickFrame, bool *bOutAdded = nullptr);
// A new transform section starts from Bound's relative transform; without Bound it keeps the section's own
// defaults (the origin, no rotation, unit scale).
bool AddTransformKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                          FFrameNumber TickFrame,
                          const TSharedPtr<FJsonObject> &LocalPayload,
                          const USceneComponent *Bound);
bool AddPropertyKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                         const FString &PropertyName, FFrameNumber TickFrame,
                         const TSharedPtr<FJsonObject> &LocalPayload,
                         FString &OutMessage);
}

namespace McpSequenceRanges {
bool HandleSetWorkRange(UEngineWireSubsystem *Subsystem,
                        const FString &RequestId,
                        const TSharedPtr<FJsonObject> &LocalPayload,
                        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}

namespace McpSequenceTracks {
bool HandleListTrackTypes(UEngineWireSubsystem *Subsystem,
                          const FString &RequestId,
                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleAddTrack(UEngineWireSubsystem *Subsystem,
                    const FString &RequestId,
                    const TSharedPtr<FJsonObject> &LocalPayload,
                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleListTracks(UEngineWireSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &LocalPayload,
                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleRemoveKeyframe(UEngineWireSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &LocalPayload,
                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleListTrackKeys(UEngineWireSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &LocalPayload,
                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
