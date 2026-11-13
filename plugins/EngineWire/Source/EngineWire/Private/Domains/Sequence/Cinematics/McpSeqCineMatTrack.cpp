#include "Domains/Sequence/Cinematics/McpSeqCine.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#include "Domains/Sequence/McpSeqEditorSupport.h"

#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "Tracks/MovieSceneMaterialTrack.h"

namespace McpSequenceCinematics {
namespace {
bool ReadLinearColor(const TSharedPtr<FJsonObject> &Value,
                     FLinearColor &OutColor) {
  if (!Value.IsValid())
    return false;
  double Red = 0.0, Green = 0.0, Blue = 0.0, Alpha = 1.0;
  const bool bRgb = Value->TryGetNumberField(TEXT("r"), Red) &&
                    Value->TryGetNumberField(TEXT("g"), Green) &&
                    Value->TryGetNumberField(TEXT("b"), Blue);
  const bool bXyz = Value->TryGetNumberField(TEXT("x"), Red) &&
                    Value->TryGetNumberField(TEXT("y"), Green) &&
                    Value->TryGetNumberField(TEXT("z"), Blue);
  if (!bRgb && !bXyz)
    return false;
  if (!Value->TryGetNumberField(bRgb ? TEXT("a") : TEXT("w"), Alpha))
    Alpha = 1.0;
  OutColor = FLinearColor(Red, Green, Blue, Alpha);
  return FMath::IsFinite(OutColor.R) && FMath::IsFinite(OutColor.G) &&
         FMath::IsFinite(OutColor.B) && FMath::IsFinite(OutColor.A);
}

UPrimitiveComponent *ResolveMaterialComponent(
    ULevelSequence *Sequence, const FGuid &BindingGuid,
    const FString &ComponentName, FGuid &OutComponentGuid) {
  UWorld *World = McpHandlerUtils::GetEditorWorld();
  if (!Sequence || !Sequence->GetMovieScene() || !World)
    return nullptr;
  TArray<UObject *, TInlineAllocator<1>> BoundObjects;
  LocateBindingObjects(Sequence, BindingGuid, World, BoundObjects);
  UPrimitiveComponent *Component = nullptr;
  AActor *Owner = nullptr;
  for (UObject *Object : BoundObjects) {
    if (UPrimitiveComponent *Candidate = Cast<UPrimitiveComponent>(Object)) {
      if (ComponentName.IsEmpty() ||
          Candidate->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) {
        Component = Candidate;
        Owner = Candidate->GetOwner();
        break;
      }
    }
    if (AActor *Actor = Cast<AActor>(Object)) {
      TArray<UPrimitiveComponent *> Components;
      Actor->GetComponents<UPrimitiveComponent>(Components);
      for (UPrimitiveComponent *Candidate : Components) {
        if (Candidate &&
            (ComponentName.IsEmpty() ||
             Candidate->GetName().Equals(ComponentName,
                                         ESearchCase::IgnoreCase))) {
          Component = Candidate;
          Owner = Actor;
          break;
        }
      }
      if (Component)
        break;
    }
  }
  if (!Component)
    return nullptr;
  OutComponentGuid = FindExistingBinding(Sequence, Component, Owner);
  return Component;
}

// The binding's material track for slot MaterialIndex, or null.
UMovieSceneComponentMaterialTrack *FindSlotTrack(UMovieScene *MovieScene, const FGuid &ComponentGuid,
                                                 int32 MaterialIndex) {
  const FMovieSceneBinding *Binding = MovieScene->FindBinding(ComponentGuid);
  for (UMovieSceneTrack *Existing : Binding ? Binding->GetTracks() : TArray<UMovieSceneTrack *>()) {
    UMovieSceneComponentMaterialTrack *Track = Cast<UMovieSceneComponentMaterialTrack>(Existing);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    if (Track && Track->GetMaterialInfo().MaterialType == EComponentMaterialType::IndexedMaterial &&
        Track->GetMaterialInfo().MaterialSlotIndex == MaterialIndex)
#else
    if (Track && Track->GetMaterialIndex() == MaterialIndex)
#endif
      return Track;
  }
  return nullptr;
}

}

bool HandleAddMaterialParameterTrack(const TSharedPtr<FJsonObject> &Params,
                                     TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  FNewBinding NewBinding;
  if (!LoadSequenceAndBinding(Params, TEXT("add_material_parameter_track"), Sequence, Guid, OutResult, NewBinding))
    return true;
  const FString ParameterName = GetString(Params, TEXT("parameterName"));
  if (ParameterName.IsEmpty()) {
    OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                           TEXT("parameterName is required"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  double MaterialIndexValue = GetJsonNumberField(Params, TEXT("materialIndex"), 0.0);
  // The sequence dispatcher has refused one that is not finite, not whole or outside int32.
  if (MaterialIndexValue < 0.0) {
    OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                           TEXT("materialIndex must be a non-negative integer"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const int32 MaterialIndex = static_cast<int32>(MaterialIndexValue);
  const FString MaterialPath = GetString(Params, TEXT("materialPath"));
  UMaterialInterface *ExpectedMaterial =
      MaterialPath.IsEmpty()
          ? nullptr
          : LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
  if (!MaterialPath.IsEmpty() && !ExpectedMaterial) {
    OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                           TEXT("materialPath must reference a material"),
                           TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }
  FString ParameterType;
  double ScalarValue = 0.0;
  // FLinearColor's default constructor leaves its channels uninitialized, and
  // the only writer (ReadLinearColor) runs on the !bScalar branch, so the
  // compiler cannot correlate the write with the use below and warns C4701.
  // The use is in fact guarded, but leaving the one warning the plugin emits
  // standing would hide the next real one. Seeded to opaque black, matching
  // ReadLinearColor's own defaults (rgb 0, alpha 1).
  FLinearColor ColorValue(0.0f, 0.0f, 0.0f, 1.0f);
  const bool bScalar =
      Params->TryGetNumberField(TEXT("value"), ScalarValue) &&
      FMath::IsFinite(ScalarValue);
  if (bScalar) {
    ParameterType = TEXT("scalar");
  } else {
    const TSharedPtr<FJsonObject> *ColorObject = nullptr;
    if (!Params->TryGetObjectField(TEXT("value"), ColorObject) ||
        !ColorObject || !ReadLinearColor(*ColorObject, ColorValue)) {
      OutResult = MakeResult(
          false, TEXT("add_material_parameter_track"),
          TEXT("value must be a finite number or an {r,g,b,a} color object"),
          TEXT("MATERIAL_PARAMETER_VALUE_INVALID"));
      return true;
    }
    ParameterType = TEXT("color");
  }
  FGuid ComponentGuid;
  UPrimitiveComponent *Component = ResolveMaterialComponent(
      Sequence, Guid, GetString(Params, TEXT("componentName")), ComponentGuid);
  if (!Component) {
    OutResult = MakeResult(
        false, TEXT("add_material_parameter_track"),
        TEXT("bindingGuid must resolve to an actor or primitive component"),
        TEXT("MATERIAL_COMPONENT_REQUIRED"));
    return true;
  }
  UMaterialInterface *CurrentMaterial = Component->GetMaterial(MaterialIndex);
  if (!CurrentMaterial) {
    OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                           TEXT("materialIndex does not reference a material"),
                           TEXT("MATERIAL_SLOT_NOT_FOUND"));
    return true;
  }
  if (ExpectedMaterial && CurrentMaterial != ExpectedMaterial) {
    OutResult = MakeResult(
        false, TEXT("add_material_parameter_track"),
        TEXT("materialPath does not match the component material slot"),
        TEXT("MATERIAL_SLOT_MISMATCH"));
    return true;
  }
  const bool bCreatedComponentBinding = !ComponentGuid.IsValid();
  if (bCreatedComponentBinding && Component->GetOwner()) {
    ComponentGuid = BindComponentUnderActor(Sequence, Guid, Component, Component->GetOwner());
  }
  if (!ComponentGuid.IsValid()) {
    OutResult = MakeResult(
        false, TEXT("add_material_parameter_track"),
        TEXT("Failed to create a component binding for the material track"),
        TEXT("BINDING_CREATION_FAILED"));
    return true;
  }
  // The slot's own track when it has one: add_keyframe does not reach a material parameter, so a second key comes
  // here, and a track per call held one key each and never animated.
  UMovieSceneComponentMaterialTrack *Track =
      FindSlotTrack(Sequence->GetMovieScene(), ComponentGuid, MaterialIndex);
  const bool bCreatedTrack = !Track;
  if (bCreatedTrack) {
    Track = Cast<UMovieSceneComponentMaterialTrack>(AddTrackForBinding(
        Sequence->GetMovieScene(),
        UMovieSceneComponentMaterialTrack::StaticClass(), ComponentGuid));
  }
  if (!Track) {
    if (bCreatedComponentBinding) {
      Sequence->UnbindPossessableObjects(ComponentGuid);
      Sequence->GetMovieScene()->RemovePossessable(ComponentGuid);
    }
    OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                           TEXT("Failed to create material track"),
                           TEXT("TRACK_CREATION_FAILED"));
    return true;
  }
  if (bCreatedTrack) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    FComponentMaterialInfo Info;
    Info.MaterialSlotIndex = MaterialIndex;
    Info.MaterialType = EComponentMaterialType::IndexedMaterial;
    Track->SetMaterialInfo(Info);
#else
    Track->SetMaterialIndex(MaterialIndex);
#endif
  }
  const FFrameNumber Frame =
      GetFrame(Params, Sequence->GetMovieScene(), TEXT("startFrame"));
  if (bScalar)
    Track->AddScalarParameterKey(FName(*ParameterName), Frame,
                                 static_cast<float>(ScalarValue));
  else
    Track->AddColorParameterKey(FName(*ParameterName), Frame, ColorValue);
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
  NewBinding.Keep();
  if (!MaybeSaveSequence(Sequence, Params, OutResult))
    return true;
  OutResult = MakeResult(true, TEXT("add_material_parameter_track"),
                         TEXT("Material parameter track added"));
  OutResult->SetStringField(TEXT("bindingGuid"), ComponentGuid.ToString());
  OutResult->SetStringField(TEXT("componentName"), Component->GetName());
  OutResult->SetStringField(TEXT("parameterName"), ParameterName);
  OutResult->SetStringField(TEXT("parameterType"), ParameterType);
  if (!MaterialPath.IsEmpty())
    OutResult->SetStringField(TEXT("materialPath"), MaterialPath);
  return true;
}
}
