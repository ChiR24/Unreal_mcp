#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class FProperty;
class UActorComponent;
class FMcpBridgeWebSocket;
class ULevelSequence;
class UMovieScene;
class UMovieSceneSection;
class UMovieSceneTrack;
class UEngineWireSubsystem;

namespace McpSequenceCinematics {
bool TryHandleCinematics(const FString &Action,
                         const TSharedPtr<FJsonObject> &Params,
                         TSharedPtr<FJsonObject> &OutResult);

TSharedPtr<FJsonObject> MakeResult(bool bSuccess, const FString &Action,
                                   const FString &Message,
                                   const FString &ErrorCode = FString());

bool HandleCreateMasterSequence(const TSharedPtr<FJsonObject> &Params,
                                TSharedPtr<FJsonObject> &OutResult);
bool HandleAddSubsequence(const TSharedPtr<FJsonObject> &Params,
                          TSharedPtr<FJsonObject> &OutResult);
bool HandleAddShotTrack(const TSharedPtr<FJsonObject> &Params,
                        TSharedPtr<FJsonObject> &OutResult);
bool HandleConfigureShotSettings(const TSharedPtr<FJsonObject> &Params,
                                 TSharedPtr<FJsonObject> &OutResult);
bool HandleCreateCineCameraActor(const TSharedPtr<FJsonObject> &Params,
                                 TSharedPtr<FJsonObject> &OutResult);
bool HandleConfigureCameraSettings(const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult);
bool HandleAddCameraCutTrack(const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult);
bool HandleAddCameraShakeTrack(const TSharedPtr<FJsonObject> &Params,
                               TSharedPtr<FJsonObject> &OutResult);
bool HandleConfigureCameraRigRail(const TSharedPtr<FJsonObject> &Params,
                                  TSharedPtr<FJsonObject> &OutResult);
bool HandleConfigureCameraRigCrane(const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult);
bool HandleAddFadeTrack(const TSharedPtr<FJsonObject> &Params,
                        TSharedPtr<FJsonObject> &OutResult);
bool HandleAddLevelVisibilityTrack(const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult);
bool HandleAddMaterialParameterTrack(const TSharedPtr<FJsonObject> &Params,
                                     TSharedPtr<FJsonObject> &OutResult);
bool HandleAddParticleTrack(const TSharedPtr<FJsonObject> &Params,
                            TSharedPtr<FJsonObject> &OutResult);
bool HandleAddSkeletalAnimationTrack(const TSharedPtr<FJsonObject> &Params,
                                     TSharedPtr<FJsonObject> &OutResult);
bool HandleAddTransformTrack(const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult);
bool HandleAddEventTrack(const TSharedPtr<FJsonObject> &Params,
                         TSharedPtr<FJsonObject> &OutResult);
bool HandleAddPropertyTrack(const TSharedPtr<FJsonObject> &Params,
                            TSharedPtr<FJsonObject> &OutResult);

FString GetString(const TSharedPtr<FJsonObject> &Params, const TCHAR *Name,
                  const TCHAR *Alias = nullptr);
FString GetSequencePath(const TSharedPtr<FJsonObject> &Params);
FFrameNumber GetFrame(const TSharedPtr<FJsonObject> &Params,
                      UMovieScene *MovieScene, const TCHAR *Name,
                      double DefaultValue = 0.0);
int32 GetDuration(const TSharedPtr<FJsonObject> &Params, UMovieScene *MovieScene,
                  int32 DefaultDuration = 100);
ULevelSequence *LoadSequence(const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult);
// LoadSequence for a handler that answers for itself: null once the loader's refusal is sent (its error and
// errorCode, else "sequencePath names no Level Sequence." and INVALID_SEQUENCE).
ULevelSequence *LoadSequenceOrSend(UEngineWireSubsystem &Bridge, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                   const FString &RequestId, const TSharedPtr<FJsonObject> &Params);
void SetSectionRange(UMovieScene *MovieScene, UMovieSceneSection *Section,
                     const TSharedPtr<FJsonObject> &Params,
                     int32 DefaultDuration = 100);
bool ReadBindingGuid(const TSharedPtr<FJsonObject> &Params, FGuid &OutGuid);
// False with OutResult set (BINDING_NOT_FOUND naming it) when bindingGuid parses but names no binding of MovieScene: a
// camera cut stores any id, and the cut bound no camera under a success reply.
bool CheckRequestedBinding(const TSharedPtr<FJsonObject> &Params, UMovieScene *MovieScene, const TCHAR *Action,
                           TSharedPtr<FJsonObject> &OutResult);
// A possessable's class default object, else a spawnable's template; null when neither resolves.
UObject *GetBindingTemplate(UMovieScene *MovieScene, const FGuid &Guid);
// InOutPath resolved on the binding's template; a component property is re-rooted to RootComponent, the path gaining
// "RootComponent.". Null with OutError when the binding has no such property.
FProperty *ResolveBoundPropertyPath(UMovieScene *MovieScene, const FGuid &Guid, FString &InOutPath, FString &OutError);
// Params[Field] (a finite number) written to Object's nested PropertyPath; recorded in Applied.
bool ApplyNumber(UObject *Object, const TSharedPtr<FJsonObject> &Params,
                 const TCHAR *Field, const TCHAR *PropertyPath, TArray<FString> &Applied);
AActor *ResolveActor(const TSharedPtr<FJsonObject> &Params);
FGuid ResolveOrCreateBinding(ULevelSequence *Sequence, AActor *Actor);
// A binding this call made for an actor the sequence did not bind yet. Unless the handler calls Keep() at its
// success, it is taken back out (its objects unbound, the possessable and its tracks removed) when this goes out of
// scope: every refusal after the binding was made used to leave it in the sequence.
struct FNewBinding {
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  FNewBinding() = default;
  FNewBinding(const FNewBinding &) = delete;
  FNewBinding &operator=(const FNewBinding &) = delete;
  ~FNewBinding();
  void Keep() { Sequence = nullptr; }
};
// ResolveOrCreateBinding, noting in New a binding it had to make.
FGuid BindActor(ULevelSequence *Sequence, AActor *Actor, FNewBinding &New);
// A new possessable for Component, a child of ParentGuid's binding, bound through Context (its actor, or the template
// it was found on). Callers look for an existing binding first.
FGuid BindComponentUnderActor(ULevelSequence *Sequence, const FGuid &ParentGuid, UActorComponent *Component,
                              UObject *Context);
// The possessable named like the actor's label, pointed at this actor with its tracks kept (McpSeqCineRebind.cpp):
// a sequence copied beside a copied level still binds the first level's actors. Invalid when none has that name.
FGuid RebindPossessableByName(ULevelSequence *Sequence, AActor *Actor);
// bindingGuid, else the actorName actor's binding (created when missing); invalid when neither resolves.
FGuid ResolveRequestBinding(const TSharedPtr<FJsonObject> &Params, ULevelSequence *Sequence, FNewBinding &New);
// LoadSequence plus ResolveRequestBinding; false with OutResult set (INVALID_ARGUMENT for a missing binding).
bool LoadSequenceAndBinding(const TSharedPtr<FJsonObject> &Params, const TCHAR *Action,
                            ULevelSequence *&OutSequence, FGuid &OutGuid,
                            TSharedPtr<FJsonObject> &OutResult, FNewBinding &New);
FGuid FindExistingBinding(ULevelSequence *Sequence, UObject *Object,
                          UObject *Context);
void LocateBindingObjects(
    ULevelSequence *Sequence, const FGuid &BindingGuid, UObject *Context,
    TArray<UObject *, TInlineAllocator<1>> &OutObjects);
UMovieSceneTrack *AddTrackForBinding(UMovieScene *MovieScene, UClass *TrackClass,
                                     const FGuid &BindingGuid);
void RemoveTrackAfterSectionFailure(UMovieScene *MovieScene,
                                    UMovieSceneTrack *Track,
                                    bool bTrackCreated);
// The binding's track of TrackClass (whose track name is TrackName when it is set), else a new one, as Sequencer adds
// them: bOutCreated says which. Defined in McpSeqCineBindingTracks.cpp.
UMovieSceneTrack *FindOrAddBindingTrack(UMovieScene *MovieScene, UClass *TrackClass, const FGuid &BindingGuid,
                                        bool &bOutCreated, const FName &TrackName = NAME_None);
// A new section added to Track; null (and the track removed again when this call created it) on failure.
UMovieSceneSection *AddTrackSection(UMovieScene *MovieScene, UMovieSceneTrack *Track, bool bTrackCreated);
bool MaybeSaveSequence(ULevelSequence *Sequence,
                       const TSharedPtr<FJsonObject> &Params,
                       TSharedPtr<FJsonObject> &OutResult);
FGuid ResolveParticleComponentBinding(ULevelSequence *Sequence,
                                      const FGuid &ActorGuid,
                                      TSharedPtr<FJsonObject> &OutDetails);
}
