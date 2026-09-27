#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "LevelSequence.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
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
ULevelSequence *LoadOrReply(UMcpAutomationBridgeSubsystem *Subsystem, const FString &RequestId,
                            TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject> &Payload,
                            const TCHAR *Action, UMovieScene *&OutMovieScene);
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

// First track whose name (or, optionally, display name) contains TrackName: the
// movie scene tracks first, then the tracks of every binding whose name contains
// BindingFilter (all bindings when the filter is empty).
inline UMovieSceneTrack *FindTrackByName(UMovieScene *MovieScene, const FString &TrackName,
                                         bool bMatchDisplayName = false,
                                         const FString &BindingFilter = FString()) {
  auto Matches = [&](UMovieSceneTrack *Track) {
    return Track && (Track->GetName().Contains(TrackName) ||
                     (bMatchDisplayName && Track->GetDisplayName().ToString().Contains(TrackName)));
  };
  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (Matches(Track)) {
      return Track;
    }
  }
  // The camera cut track is stored in its OWN UMovieScene member, not in the
  // Tracks array, so every lookup that walked only those two collections was
  // blind to it: removing, muting, soloing or locking a camera cut answered
  // TRACK_NOT_FOUND for a track the readback had just listed.
  if (UMovieSceneTrack *CameraCutTrack = MovieScene->GetCameraCutTrack()) {
    if (Matches(CameraCutTrack)) {
      return CameraCutTrack;
    }
  }
  for (const FMovieSceneBinding &Binding : const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    if (!BindingFilter.IsEmpty() && !GetBindingName(MovieScene, Binding.GetObjectGuid()).Contains(BindingFilter)) {
      continue;
    }
    for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
      if (Matches(Track)) {
        return Track;
      }
    }
  }
  return nullptr;
}

// Every track whose name contains TrackName (all tracks when it is empty),
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
    return Track && (TrackName.IsEmpty() || Track->GetName().Contains(TrackName));
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
}

namespace McpSequenceKeyframes {
FGuid ResolveBindingGuid(UMovieScene *MovieScene, const FString &BindingIdStr,
                         const FString &ActorName);
bool AddTransformKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                          FFrameNumber TickFrame,
                          const TSharedPtr<FJsonObject> &LocalPayload);
bool AddPropertyKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                         const FString &PropertyName, FFrameNumber TickFrame,
                         const TSharedPtr<FJsonObject> &LocalPayload,
                         FString &OutMessage);
}

namespace McpSequenceRanges {
bool HandleSetWorkRange(UMcpAutomationBridgeSubsystem *Subsystem,
                        const FString &RequestId,
                        const TSharedPtr<FJsonObject> &LocalPayload,
                        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}

namespace McpSequenceTracks {
bool HandleListTrackTypes(UMcpAutomationBridgeSubsystem *Subsystem,
                          const FString &RequestId,
                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleAddTrack(UMcpAutomationBridgeSubsystem *Subsystem,
                    const FString &RequestId,
                    const TSharedPtr<FJsonObject> &LocalPayload,
                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleListTracks(UMcpAutomationBridgeSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &LocalPayload,
                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleRemoveKeyframe(UMcpAutomationBridgeSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &LocalPayload,
                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleListTrackKeys(UMcpAutomationBridgeSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &LocalPayload,
                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
