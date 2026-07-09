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
bool TryHandleCinematics(UMcpAutomationBridgeSubsystem *Self,
                         const FString &Action,
                         const TSharedPtr<FJsonObject> &Params,
                         TSharedPtr<FJsonObject> &OutResult);

TSharedPtr<FJsonObject> MakeResult(bool bSuccess, const FString &Action,
                                   const FString &Message,
                                   const FString &ErrorCode = FString());

bool HandleCreateMasterSequence(UMcpAutomationBridgeSubsystem *Self,
                                const TSharedPtr<FJsonObject> &Params,
                                TSharedPtr<FJsonObject> &OutResult);
bool HandleAddSubsequence(UMcpAutomationBridgeSubsystem *Self,
                          const TSharedPtr<FJsonObject> &Params,
                          TSharedPtr<FJsonObject> &OutResult);
bool HandleAddShotTrack(UMcpAutomationBridgeSubsystem *Self,
                        const TSharedPtr<FJsonObject> &Params,
                        TSharedPtr<FJsonObject> &OutResult);
bool HandleConfigureShotSettings(UMcpAutomationBridgeSubsystem *Self,
                                 const TSharedPtr<FJsonObject> &Params,
                                 TSharedPtr<FJsonObject> &OutResult);
bool HandleCreateCineCameraActor(UMcpAutomationBridgeSubsystem *Self,
                                 const TSharedPtr<FJsonObject> &Params,
                                 TSharedPtr<FJsonObject> &OutResult);
bool HandleConfigureCameraSettings(UMcpAutomationBridgeSubsystem *Self,
                                   const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult);
bool HandleAddCameraCutTrack(UMcpAutomationBridgeSubsystem *Self,
                             const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult);
bool HandleAddCameraShakeTrack(UMcpAutomationBridgeSubsystem *Self,
                               const TSharedPtr<FJsonObject> &Params,
                               TSharedPtr<FJsonObject> &OutResult);
bool HandleConfigureCameraRigRail(UMcpAutomationBridgeSubsystem *Self,
                                  const TSharedPtr<FJsonObject> &Params,
                                  TSharedPtr<FJsonObject> &OutResult);
bool HandleConfigureCameraRigCrane(UMcpAutomationBridgeSubsystem *Self,
                                   const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult);
bool HandleAddFadeTrack(UMcpAutomationBridgeSubsystem *Self,
                        const TSharedPtr<FJsonObject> &Params,
                        TSharedPtr<FJsonObject> &OutResult);
bool HandleAddLevelVisibilityTrack(UMcpAutomationBridgeSubsystem *Self,
                                   const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult);
bool HandleAddMaterialParameterTrack(UMcpAutomationBridgeSubsystem *Self,
                                     const TSharedPtr<FJsonObject> &Params,
                                     TSharedPtr<FJsonObject> &OutResult);
bool HandleAddParticleTrack(UMcpAutomationBridgeSubsystem *Self,
                            const TSharedPtr<FJsonObject> &Params,
                            TSharedPtr<FJsonObject> &OutResult);
bool HandleAddSkeletalAnimationTrack(UMcpAutomationBridgeSubsystem *Self,
                                     const TSharedPtr<FJsonObject> &Params,
                                     TSharedPtr<FJsonObject> &OutResult);
bool HandleAddTransformTrack(UMcpAutomationBridgeSubsystem *Self,
                             const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult);
bool HandleAddEventTrack(UMcpAutomationBridgeSubsystem *Self,
                         const TSharedPtr<FJsonObject> &Params,
                         TSharedPtr<FJsonObject> &OutResult);
bool HandleAddPropertyTrack(UMcpAutomationBridgeSubsystem *Self,
                            const TSharedPtr<FJsonObject> &Params,
                            TSharedPtr<FJsonObject> &OutResult);

#if WITH_EDITOR
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
AActor *ResolveActor(const TSharedPtr<FJsonObject> &Params);
FGuid ResolveOrCreateBinding(ULevelSequence *Sequence, AActor *Actor);
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
bool MaybeSaveSequence(ULevelSequence *Sequence,
                       const TSharedPtr<FJsonObject> &Params,
                       TSharedPtr<FJsonObject> &OutResult);
#endif
}
