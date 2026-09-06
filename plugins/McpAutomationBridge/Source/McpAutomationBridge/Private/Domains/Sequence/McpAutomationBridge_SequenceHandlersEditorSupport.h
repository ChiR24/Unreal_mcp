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

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_GET_MOVIESCENE_TRACKS(MovieScene) (MovieScene)->GetTracks()
#else
#define MCP_GET_MOVIESCENE_TRACKS(MovieScene) (MovieScene)->GetMasterTracks()
#endif
#define MCP_GET_BINDING_TRACKS(Binding) (Binding).GetTracks()

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

#include "ScopedTransaction.h"
#if __has_include("Camera/CameraActor.h")
#include "Camera/CameraActor.h"
#endif
#endif

namespace McpSequence {
FString ResolvePath(const TSharedPtr<FJsonObject> &Payload);
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
}
