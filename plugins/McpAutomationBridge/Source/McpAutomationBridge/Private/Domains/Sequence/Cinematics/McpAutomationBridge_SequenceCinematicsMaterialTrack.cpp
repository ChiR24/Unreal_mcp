#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#if WITH_EDITOR
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#endif

namespace McpSequenceCinematics {
#if WITH_EDITOR
namespace {
bool LoadMaterialTrackTarget(const TSharedPtr<FJsonObject> &Params,
                             ULevelSequence *&OutSequence, FGuid &OutGuid,
                             TSharedPtr<FJsonObject> &OutResult) {
  OutSequence = LoadSequence(Params, OutResult);
  if (!OutSequence)
    return false;
  if (ReadBindingGuid(Params, OutGuid))
    return true;
  OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                         TEXT("bindingGuid is required"),
                         TEXT("INVALID_ARGUMENT"));
  return false;
}

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
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

FGuid CreateMaterialComponentBinding(ULevelSequence *Sequence,
                                     const FGuid &ParentGuid,
                                     UPrimitiveComponent *Component) {
  if (!Sequence || !Sequence->GetMovieScene() || !Component ||
      !Component->GetOwner()) {
    return FGuid();
  }
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  const FGuid ComponentGuid =
      MovieScene->AddPossessable(Component->GetName(), Component->GetClass());
  FMovieScenePossessable *Possessable =
      MovieScene->FindPossessable(ComponentGuid);
  if (!Possessable) {
    MovieScene->RemovePossessable(ComponentGuid);
    return FGuid();
  }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Possessable->SetParent(ParentGuid, MovieScene);
#else
  Possessable->SetParent(ParentGuid);
#endif
  Sequence->BindPossessableObject(ComponentGuid, *Component,
                                  Component->GetOwner());
  return ComponentGuid;
}
}
#endif

bool HandleAddMaterialParameterTrack(UMcpAutomationBridgeSubsystem *Self,
                                     const TSharedPtr<FJsonObject> &Params,
                                     TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  if (!LoadMaterialTrackTarget(Params, Sequence, Guid, OutResult))
    return true;
  const FString ParameterName = GetString(Params, TEXT("parameterName"));
  if (ParameterName.IsEmpty()) {
    OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                           TEXT("parameterName is required"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  double MaterialIndexValue = 0.0;
  Params->TryGetNumberField(TEXT("materialIndex"), MaterialIndexValue);
  if (!FMath::IsFinite(MaterialIndexValue) || MaterialIndexValue < 0.0 ||
      MaterialIndexValue > MAX_int32) {
    OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                           TEXT("materialIndex must be a non-negative integer"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const int32 MaterialIndex = FMath::RoundToInt(MaterialIndexValue);
  if (!FMath::IsNearlyEqual(MaterialIndexValue,
                            static_cast<double>(MaterialIndex))) {
    OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                           TEXT("materialIndex must be a non-negative integer"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
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
          TEXT("value must be a finite number or an r/g/b/a color object"),
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
  if (bCreatedComponentBinding) {
    ComponentGuid =
        CreateMaterialComponentBinding(Sequence, Guid, Component);
  }
  if (!ComponentGuid.IsValid()) {
    OutResult = MakeResult(
        false, TEXT("add_material_parameter_track"),
        TEXT("Failed to create a component binding for the material track"),
        TEXT("BINDING_CREATION_FAILED"));
    return true;
  }
  UMovieSceneComponentMaterialTrack *Track =
      Cast<UMovieSceneComponentMaterialTrack>(AddTrackForBinding(
          Sequence->GetMovieScene(),
          UMovieSceneComponentMaterialTrack::StaticClass(), ComponentGuid));
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
  FComponentMaterialInfo Info;
  Info.MaterialSlotIndex = MaterialIndex;
  Info.MaterialType = EComponentMaterialType::IndexedMaterial;
  Track->SetMaterialInfo(Info);
#else
  Track->SetMaterialIndex(MaterialIndex);
#endif
  const FFrameNumber Frame =
      GetFrame(Params, Sequence->GetMovieScene(), TEXT("startFrame"));
  if (bScalar)
    Track->AddScalarParameterKey(FName(*ParameterName), Frame,
                                 static_cast<float>(ScalarValue));
  else
    Track->AddColorParameterKey(FName(*ParameterName), Frame, ColorValue);
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
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
#else
  OutResult = MakeResult(false, TEXT("add_material_parameter_track"),
                         TEXT("Editor build required"), TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
}
