#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#if WITH_EDITOR
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sections/MovieSceneFadeSection.h"
#include "Sections/MovieSceneLevelVisibilitySection.h"
#include "Sections/MovieSceneParticleSection.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Tracks/MovieSceneLevelVisibilityTrack.h"
#include "Tracks/MovieSceneParticleTrack.h"
#endif

namespace McpSequenceCinematics {
#if WITH_EDITOR
namespace {
UMovieSceneSection *CreateBoundSection(ULevelSequence *Sequence, UClass *TrackClass,
                                       const FGuid &BindingGuid,
                                       TSharedPtr<FJsonObject> &OutResult,
                                       const TCHAR *Action) {
  UMovieSceneTrack *Track =
      AddTrackForBinding(Sequence->GetMovieScene(), TrackClass, BindingGuid);
  UMovieSceneSection *Section = Track ? Track->CreateNewSection() : nullptr;
  if (Track && Section) Track->AddSection(*Section);
  if (!Section) {
    RemoveTrackAfterSectionFailure(Sequence->GetMovieScene(), Track, true);
    OutResult = MakeResult(false, Action, TEXT("Failed to create track section"),
                           TEXT("SECTION_CREATION_FAILED"));
  }
  return Section;
}

bool LoadSequenceAndBindingForAuxiliaryTrack(const TSharedPtr<FJsonObject> &Params,
                            const TCHAR *Action, ULevelSequence *&OutSequence,
                            FGuid &OutGuid, TSharedPtr<FJsonObject> &OutResult) {
  OutSequence = LoadSequence(Params, OutResult);
  if (!OutSequence) return false;
  if (!ReadBindingGuid(Params, OutGuid)) {
    OutResult = MakeResult(false, Action, TEXT("bindingGuid is required"),
                           TEXT("INVALID_ARGUMENT"));
    return false;
  }
  return true;
}

bool BindingSupportsParticleActivation(UMovieScene *MovieScene,
                                       const FGuid &Guid) {
  const FMovieScenePossessable *Possessable =
      MovieScene ? MovieScene->FindPossessable(Guid) : nullptr;
  const UClass *BoundClass =
      Possessable ? Possessable->GetPossessedObjectClass() : nullptr;
  const FMovieSceneSpawnable *Spawnable =
      MovieScene ? MovieScene->FindSpawnable(Guid) : nullptr;
  const UObject *BoundTemplate =
      BoundClass
          ? BoundClass->GetDefaultObject()
          : (Spawnable ? Spawnable->GetObjectTemplate() : nullptr);
  if (!BoundTemplate) return false;
  if (BoundTemplate->IsA<UFXSystemComponent>()) return true;
  return BoundTemplate->IsA<AEmitter>();
}

}
#endif

bool HandleAddFadeTrack(UMcpAutomationBridgeSubsystem *Self,
                        const TSharedPtr<FJsonObject> &Params,
                        TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneFadeTrack *Track = MovieScene->FindTrack<UMovieSceneFadeTrack>();
  const bool bCreatedTrack = !Track;
  if (!Track)
    Track = MovieScene->AddTrack<UMovieSceneFadeTrack>();
  UMovieSceneFadeSection *Section =
      Track ? Cast<UMovieSceneFadeSection>(Track->CreateNewSection()) : nullptr;
  if (Track && Section) Track->AddSection(*Section);
  if (!Section) {
    RemoveTrackAfterSectionFailure(MovieScene, Track, bCreatedTrack);
    OutResult = MakeResult(false, TEXT("add_fade_track"),
                           TEXT("Failed to create fade track section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(MovieScene, Section, Params, 100);
  double From = 0.0, To = 1.0;
  Params->TryGetNumberField(TEXT("from"), From);
  Params->TryGetNumberField(TEXT("to"), To);
  const FFrameNumber Start =
      GetFrame(Params, MovieScene, TEXT("startFrame"));
  Section->FloatCurve.AddCubicKey(Start, static_cast<float>(From));
  Section->FloatCurve.AddCubicKey(Start + GetDuration(Params, MovieScene, 100),
                                  static_cast<float>(To));
  MovieScene->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_fade_track"), TEXT("Fade track added"));
  return true;
#else
  OutResult = MakeResult(false, TEXT("add_fade_track"), TEXT("Editor build required"),
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool HandleAddLevelVisibilityTrack(UMcpAutomationBridgeSubsystem *Self,
                                   const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneLevelVisibilityTrack *Track =
      MovieScene->FindTrack<UMovieSceneLevelVisibilityTrack>();
  const bool bCreatedTrack = !Track;
  if (!Track)
    Track = MovieScene->AddTrack<UMovieSceneLevelVisibilityTrack>();
  UMovieSceneLevelVisibilitySection *Section =
      Track ? Cast<UMovieSceneLevelVisibilitySection>(Track->CreateNewSection()) : nullptr;
  if (Track && Section) Track->AddSection(*Section);
  if (!Section) {
    RemoveTrackAfterSectionFailure(MovieScene, Track, bCreatedTrack);
    OutResult = MakeResult(
        false, TEXT("add_level_visibility_track"),
        TEXT("Failed to create level visibility track section"),
        TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(MovieScene, Section, Params, 100);
  const FString Visibility = GetString(Params, TEXT("visibility"));
  Section->SetVisibility(Visibility.Equals(TEXT("hidden"), ESearchCase::IgnoreCase)
                             ? ELevelVisibility::Hidden
                             : ELevelVisibility::Visible);
  TArray<FName> LevelNames;
  const TArray<TSharedPtr<FJsonValue>> *Names = nullptr;
  if (Params->TryGetArrayField(TEXT("levelNames"), Names) && Names) {
    for (const TSharedPtr<FJsonValue> &Name : *Names) {
      LevelNames.Add(FName(*Name->AsString()));
    }
  }
  Section->SetLevelNames(LevelNames);
  MovieScene->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_level_visibility_track"),
                         TEXT("Level visibility track added"));
  return true;
#else
  OutResult = MakeResult(false, TEXT("add_level_visibility_track"),
                         TEXT("Editor build required"), TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool HandleAddParticleTrack(UMcpAutomationBridgeSubsystem *Self,
                            const TSharedPtr<FJsonObject> &Params,
                            TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  if (!LoadSequenceAndBindingForAuxiliaryTrack(Params, TEXT("add_particle_track"), Sequence, Guid,
                              OutResult))
    return true;
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  if (!BindingSupportsParticleActivation(MovieScene, Guid)) {
    OutResult = MakeResult(
        false, TEXT("add_particle_track"),
        TEXT("bindingGuid must reference an FX system component or actor"),
        TEXT("PARTICLE_BINDING_REQUIRED"));
    return true;
  }
  UMovieSceneParticleSection *Section = Cast<UMovieSceneParticleSection>(CreateBoundSection(
      Sequence, UMovieSceneParticleTrack::StaticClass(), Guid, OutResult,
      TEXT("add_particle_track")));
  if (!Section) return true;
  SetSectionRange(MovieScene, Section, Params, 100);
  bool bActivate = true;
  Params->TryGetBoolField(TEXT("activate"), bActivate);
  const EParticleKey Key =
      bActivate ? EParticleKey::Activate : EParticleKey::Deactivate;
  Section->ParticleKeys.GetData().UpdateOrAddKey(
      GetFrame(Params, MovieScene, TEXT("startFrame")),
      static_cast<uint8>(Key));
  MovieScene->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult =
      MakeResult(true, TEXT("add_particle_track"),
                 TEXT("Particle activation track added"));
  OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  OutResult->SetStringField(TEXT("particleAction"),
                            bActivate ? TEXT("activate") : TEXT("deactivate"));
  return true;
#else
  OutResult = MakeResult(false, TEXT("add_particle_track"), TEXT("Editor build required"),
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
}
