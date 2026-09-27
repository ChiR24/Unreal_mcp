#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

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

namespace McpSequenceCinematics {
namespace {
UMovieSceneSection *CreateBoundSection(ULevelSequence *Sequence, UClass *TrackClass,
                                       const FGuid &BindingGuid,
                                       TSharedPtr<FJsonObject> &OutResult,
                                       const TCHAR *Action) {
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneSection *Section =
      AddTrackSection(MovieScene, AddTrackForBinding(MovieScene, TrackClass, BindingGuid), true);
  if (!Section) {
    OutResult = MakeResult(false, Action, TEXT("Failed to create track section"),
                           TEXT("SECTION_CREATION_FAILED"));
  }
  return Section;
}

bool BindingSupportsParticleActivation(UMovieScene *MovieScene,
                                       const FGuid &Guid) {
  const UObject *BoundTemplate = GetBindingTemplate(MovieScene, Guid);
  if (!BoundTemplate) return false;
  if (BoundTemplate->IsA<UFXSystemComponent>()) return true;
  return BoundTemplate->IsA<AEmitter>();
}

}

bool HandleAddFadeTrack(const TSharedPtr<FJsonObject> &Params,
                        TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneFadeTrack *Track = MovieScene->FindTrack<UMovieSceneFadeTrack>();
  const bool bCreatedTrack = !Track;
  if (!Track)
    Track = MovieScene->AddTrack<UMovieSceneFadeTrack>();
  UMovieSceneFadeSection *Section =
      Cast<UMovieSceneFadeSection>(AddTrackSection(MovieScene, Track, bCreatedTrack));
  if (!Section) {
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
}

bool HandleAddLevelVisibilityTrack(const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneLevelVisibilityTrack *Track =
      MovieScene->FindTrack<UMovieSceneLevelVisibilityTrack>();
  const bool bCreatedTrack = !Track;
  if (!Track)
    Track = MovieScene->AddTrack<UMovieSceneLevelVisibilityTrack>();
  UMovieSceneLevelVisibilitySection *Section =
      Cast<UMovieSceneLevelVisibilitySection>(AddTrackSection(MovieScene, Track, bCreatedTrack));
  if (!Section) {
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
}

bool HandleAddParticleTrack(const TSharedPtr<FJsonObject> &Params,
                            TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  if (!LoadSequenceAndBinding(Params, TEXT("add_particle_track"), Sequence, Guid,
                              OutResult))
    return true;
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  // Evaluator-compatible bindings (AEmitter, FX components) take the track directly;
  // an actor that merely owns a Niagara/particle component (ANiagaraActor, Blueprint
  // actors) gets it on a child component binding, which is what the evaluator resolves.
  FGuid TrackGuid = Guid;
  TSharedPtr<FJsonObject> BindingDetails;
  if (!BindingSupportsParticleActivation(MovieScene, Guid)) {
    TrackGuid = ResolveParticleComponentBinding(Sequence, Guid, BindingDetails);
  }
  if (!TrackGuid.IsValid()) {
    OutResult = MakeResult(
        false, TEXT("add_particle_track"),
        TEXT("The binding must reference an FX system component or actor (AEmitter, ANiagaraActor, or an actor owning a Niagara or particle component)"),
        TEXT("PARTICLE_BINDING_REQUIRED"));
    return true;
  }
  UMovieSceneParticleSection *Section = Cast<UMovieSceneParticleSection>(CreateBoundSection(
      Sequence, UMovieSceneParticleTrack::StaticClass(), TrackGuid, OutResult,
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
  OutResult->SetStringField(TEXT("bindingGuid"), TrackGuid.ToString());
  if (TrackGuid != Guid) {
    OutResult->SetStringField(TEXT("parentBindingGuid"), Guid.ToString());
    if (BindingDetails.IsValid()) {
      for (const auto &Pair : BindingDetails->Values) {
        OutResult->SetField(FString(*Pair.Key), Pair.Value);
      }
    }
  }
  OutResult->SetStringField(TEXT("particleAction"),
                            bActivate ? TEXT("activate") : TEXT("deactivate"));
  return true;
}
}
