#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class ULevelSequence;
class UMcpAutomationBridgeSubsystem;
class UMovieScene;
class UMovieSceneSection;
class UMovieSceneTrack;

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
void SetSectionRange(UMovieScene *MovieScene, UMovieSceneSection *Section,
                     const TSharedPtr<FJsonObject> &Params,
                     int32 DefaultDuration = 100);
bool ReadBindingGuid(const TSharedPtr<FJsonObject> &Params, FGuid &OutGuid);
// A possessable's class default object, else a spawnable's template; null when neither resolves.
UObject *GetBindingTemplate(UMovieScene *MovieScene, const FGuid &Guid);
// Params[Field] (a finite number) written to Object's nested PropertyPath; recorded in Applied.
bool ApplyNumber(UObject *Object, const TSharedPtr<FJsonObject> &Params,
                 const TCHAR *Field, const TCHAR *PropertyPath, TArray<FString> &Applied);
AActor *ResolveActor(const TSharedPtr<FJsonObject> &Params);
FGuid ResolveOrCreateBinding(ULevelSequence *Sequence, AActor *Actor);
// bindingGuid, else the actorName actor's binding (created when missing); invalid when neither resolves.
FGuid ResolveRequestBinding(const TSharedPtr<FJsonObject> &Params, ULevelSequence *Sequence);
// LoadSequence plus ResolveRequestBinding; false with OutResult set (INVALID_ARGUMENT for a missing binding).
bool LoadSequenceAndBinding(const TSharedPtr<FJsonObject> &Params, const TCHAR *Action,
                            ULevelSequence *&OutSequence, FGuid &OutGuid,
                            TSharedPtr<FJsonObject> &OutResult);
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
// A new section added to Track; null (and the track removed again when this call created it) on failure.
UMovieSceneSection *AddTrackSection(UMovieScene *MovieScene, UMovieSceneTrack *Track, bool bTrackCreated);
bool MaybeSaveSequence(ULevelSequence *Sequence,
                       const TSharedPtr<FJsonObject> &Params,
                       TSharedPtr<FJsonObject> &OutResult);
FGuid ResolveParticleComponentBinding(ULevelSequence *Sequence,
                                      const FGuid &ActorGuid,
                                      TSharedPtr<FJsonObject> &OutDetails);
}
