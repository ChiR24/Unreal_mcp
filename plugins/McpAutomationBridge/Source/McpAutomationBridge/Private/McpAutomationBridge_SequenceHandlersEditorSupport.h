#pragma once

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "LevelSequence.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
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

#if __has_include("ScopedTransaction.h")
#include "ScopedTransaction.h"
#elif __has_include("Misc/ScopedTransaction.h")
#include "Misc/ScopedTransaction.h"
#else
#define MCP_NO_SCOPED_TRANSACTION 1
#endif
#if __has_include("Camera/CameraActor.h")
#include "Camera/CameraActor.h"
#endif
#endif

namespace McpSequence {
FString ResolvePath(const TSharedPtr<FJsonObject> &Payload);
}

namespace McpSequenceKeyframes {
FGuid ResolveBindingGuid(UMovieScene *MovieScene, const FString &BindingIdStr,
                         const FString &ActorName);
bool AddTransformKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                          double Frame,
                          const TSharedPtr<FJsonObject> &LocalPayload);
bool AddPropertyKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                         const FString &PropertyName, double Frame,
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
