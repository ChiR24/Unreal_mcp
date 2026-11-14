#include "Domains/Sequence/Cinematics/McpSeqCine.h"

#include "Domains/Sequence/McpSeqEditorSupport.h"

#include "Components/SceneComponent.h"
#include "Foundation/BridgeHelpers/Properties/McpNestedPropPath.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneDoubleTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneIntegerTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "UObject/UnrealType.h"

namespace McpSequenceCinematics {
FProperty *ResolveBoundPropertyPath(UMovieScene *MovieScene, const FGuid &Guid,
                                    FString &InOutPath, FString &OutError) {
  UObject *DefaultObject = GetBindingTemplate(MovieScene, Guid);
  if (!DefaultObject) {
    OutError = TEXT("Binding has no resolvable object template");
    return nullptr;
  }
  void *Container = nullptr;
  if (FProperty *Property =
          ResolveNestedPropertyPath(DefaultObject, InOutPath, Container, OutError)) {
    return Property;
  }
  // Component-owned properties (RelativeScale3D, bVisible, ...) live on the root
  // component, not the actor; re-root the path there so the track can bind it.
  const AActor *DefaultActor = Cast<AActor>(DefaultObject);
  USceneComponent *Root = DefaultActor ? DefaultActor->GetRootComponent() : nullptr;
  FString RootError;
  FProperty *RootProperty =
      Root ? ResolveNestedPropertyPath(Root, InOutPath, Container, RootError)
           : nullptr;
  if (RootProperty) {
    InOutPath = TEXT("RootComponent.") + InOutPath;
  }
  return RootProperty;
}

namespace {
bool IsTransformPropertyName(const FString &Name,
                             EMovieSceneTransformChannel &OutChannels) {
  auto Is = [&Name](const TCHAR *Candidate) {
    return Name.Equals(Candidate, ESearchCase::IgnoreCase);
  };
  if (Is(TEXT("Transform")) || Is(TEXT("RelativeTransform")) || Is(TEXT("ActorTransform"))) {
    OutChannels = EMovieSceneTransformChannel::AllTransform;
  } else if (Is(TEXT("Location")) || Is(TEXT("Translation")) ||
             Is(TEXT("RelativeLocation")) || Is(TEXT("ActorLocation"))) {
    OutChannels = EMovieSceneTransformChannel::Translation;
  } else if (Is(TEXT("Rotation")) || Is(TEXT("RelativeRotation")) ||
             Is(TEXT("ActorRotation"))) {
    OutChannels = EMovieSceneTransformChannel::Rotation;
  } else if (Is(TEXT("Scale")) || Is(TEXT("Scale3D")) ||
             Is(TEXT("RelativeScale3D")) || Is(TEXT("ActorScale3D"))) {
    OutChannels = EMovieSceneTransformChannel::Scale;
  } else {
    return false;
  }
  return true;
}

// Transform/Location/Rotation/Scale are not reflected FProperties on an actor; they
// are the 3D transform track with a channel mask. An existing transform
// track on the binding gains a new section rather than a duplicate track.
bool AddTransformPropertyTrack(ULevelSequence *Sequence, const FGuid &Guid,
                               const TSharedPtr<FJsonObject> &Params,
                               const FString &RequestedName,
                               EMovieSceneTransformChannel Channels,
                               TSharedPtr<FJsonObject> &OutResult, FNewBinding &NewBinding) {
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  bool bCreatedTrack = false;
  UMovieSceneTrack *Track =
      FindOrAddBindingTrack(MovieScene, UMovieScene3DTransformTrack::StaticClass(), Guid, bCreatedTrack);
  UMovieScene3DTransformSection *Section =
      Cast<UMovieScene3DTransformSection>(AddTrackSection(MovieScene, Track, bCreatedTrack));
  if (!Section) {
    OutResult = MakeResult(false, TEXT("add_property_track"),
                           TEXT("Failed to create transform section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  Section->SetMask(FMovieSceneTransformMask(Channels));
  SetSectionRange(MovieScene, Section, Params, 100);
  MovieScene->Modify();
  Sequence->MarkPackageDirty();
  NewBinding.Keep();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  const TCHAR *MaskName =
      Channels == EMovieSceneTransformChannel::AllTransform ? TEXT("Transform")
      : Channels == EMovieSceneTransformChannel::Translation ? TEXT("Location")
      : Channels == EMovieSceneTransformChannel::Rotation   ? TEXT("Rotation")
                                                             : TEXT("Scale");
  OutResult = MakeResult(true, TEXT("add_property_track"),
                         TEXT("Transform property track added"));
  OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  OutResult->SetStringField(TEXT("propertyName"), RequestedName);
  OutResult->SetStringField(TEXT("requestedPropertyName"), RequestedName);
  OutResult->SetStringField(TEXT("trackType"), TEXT("Transform"));
  OutResult->SetStringField(TEXT("channelMask"), MaskName);
  OutResult->SetBoolField(TEXT("trackCreated"), bCreatedTrack);
  return true;
}

// The integer properties the version's integer track writes: FIntProperty up to 5.6, whose int32 traits wrote four
// bytes at any integer property's offset; from 5.7 a variant over the signed 8- to 64-bit types that asserts on a
// property it cannot match. Bytes, enums and unsigned integers take none of them.
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 7
const TCHAR *const IntegerTrackTypes = TEXT("int8, int16, int32 or int64");
bool IsIntegerTrackProperty(const FProperty *Property) {
  return Property && (Property->IsA<FIntProperty>() || Property->IsA<FInt64Property>() ||
                      Property->IsA<FInt16Property>() || Property->IsA<FInt8Property>());
}
#else
const TCHAR *const IntegerTrackTypes = TEXT("int32");
bool IsIntegerTrackProperty(const FProperty *Property) { return Property && Property->IsA<FIntProperty>(); }
#endif

UClass *ResolvePropertyTrackClass(const FString &Requested,
                                  const FProperty *Property,
                                  FString &OutResolved) {
  UClass *TrackClass = nullptr;
  if (Property && Property->IsA<FBoolProperty>()) {
    OutResolved = TEXT("bool");
    TrackClass = UMovieSceneBoolTrack::StaticClass();
  } else if (Property && Property->IsA<FDoubleProperty>()) {
    OutResolved = TEXT("double");
    TrackClass = UMovieSceneDoubleTrack::StaticClass();
  } else if (Property && Property->IsA<FFloatProperty>()) {
    OutResolved = TEXT("float");
    TrackClass = UMovieSceneFloatTrack::StaticClass();
  } else if (IsIntegerTrackProperty(Property)) {
    OutResolved = TEXT("integer");
    TrackClass = UMovieSceneIntegerTrack::StaticClass();
  }
  if (!TrackClass || Requested.IsEmpty()) return TrackClass;
  const bool bIntegerAlias =
      OutResolved == TEXT("integer") &&
      Requested.Equals(TEXT("int"), ESearchCase::IgnoreCase);
  return bIntegerAlias ||
                 Requested.Equals(OutResolved, ESearchCase::IgnoreCase)
             ? TrackClass
             : nullptr;
}
}

bool HandleAddPropertyTrack(const TSharedPtr<FJsonObject> &Params,
                            TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  FNewBinding NewBinding;
  if (!LoadSequenceAndBinding(Params, TEXT("add_property_track"), Sequence, Guid, OutResult, NewBinding))
    return true;
  const FString RequestedName =
      GetString(Params, TEXT("propertyName"), TEXT("property"));
  if (RequestedName.IsEmpty()) {
    OutResult = MakeResult(false, TEXT("add_property_track"),
                           TEXT("propertyName is required"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // The record requires only `property`, so the documented single-field call
  // died on PROPERTY_PATH_REQUIRED. A bare property name IS its own one-segment
  // path; ResolveBoundPropertyPath below still rejects one that does not exist,
  // so an unresolvable name fails as a real lookup error, not a contract gap.
  FString PropertyPath = GetString(Params, TEXT("propertyPath"));
  if (PropertyPath.IsEmpty()) {
    PropertyPath = RequestedName;
  }
  EMovieSceneTransformChannel TransformChannels = EMovieSceneTransformChannel::None;
  if (IsTransformPropertyName(PropertyPath, TransformChannels)) {
    return AddTransformPropertyTrack(Sequence, Guid, Params, RequestedName,
                                     TransformChannels, OutResult, NewBinding);
  }
  FString PathError;
  FProperty *Property = ResolveBoundPropertyPath(
      Sequence->GetMovieScene(), Guid, PropertyPath, PathError);
  if (!Property) {
    OutResult = MakeResult(
        false, TEXT("add_property_track"),
        FString::Printf(TEXT("Invalid propertyPath '%s': %s"), *PropertyPath,
                        *PathError),
        TEXT("PROPERTY_NOT_FOUND"));
    return true;
  }
  const FString RequestedType =
      GetString(Params, TEXT("propertyType"));
  FString ResolvedType;
  UClass *TrackClass =
      ResolvePropertyTrackClass(RequestedType, Property, ResolvedType);
  if (!TrackClass) {
    OutResult = MakeResult(
        false, TEXT("add_property_track"),
        ResolvedType.IsEmpty()
            ? FString::Printf(TEXT("'%s' is a %s; a property track animates a float, double, bool or %s property"),
                              *PropertyPath, *Property->GetCPPType(), IntegerTrackTypes)
            : FString::Printf(TEXT("propertyType '%s' is incompatible with '%s'"), *RequestedType, *PropertyPath),
        TEXT("PROPERTY_TYPE_MISMATCH"));
    return true;
  }
  // The binding's track of this property path gains a section; another path of the same name gets its own track.
  bool bCreatedTrack = false;
  UMovieScenePropertyTrack *Track = Cast<UMovieScenePropertyTrack>(
      FindOrAddBindingTrack(Sequence->GetMovieScene(), TrackClass, Guid, bCreatedTrack, Property->GetFName()));
  if (Track && !bCreatedTrack && Track->GetPropertyPath().ToString() != PropertyPath) {
    Track = Cast<UMovieScenePropertyTrack>(AddTrackForBinding(Sequence->GetMovieScene(), TrackClass, Guid));
    bCreatedTrack = true;
  }
  if (!Track) {
    OutResult = MakeResult(false, TEXT("add_property_track"),
                           TEXT("Failed to create property track"),
                           TEXT("TRACK_CREATION_FAILED"));
    return true;
  }
  Track->SetPropertyNameAndPath(Property->GetFName(), PropertyPath);
  UMovieSceneSection *Section = AddTrackSection(Sequence->GetMovieScene(), Track, bCreatedTrack);
  if (!Section) {
    OutResult = MakeResult(false, TEXT("add_property_track"),
                           TEXT("Failed to create property section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(Sequence->GetMovieScene(), Section, Params, 100);
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
  NewBinding.Keep();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_property_track"),
                         TEXT("Property track added"));
  OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  OutResult->SetStringField(TEXT("propertyName"), Property->GetName());
  OutResult->SetStringField(TEXT("requestedPropertyName"), RequestedName);
  OutResult->SetStringField(TEXT("propertyPath"), PropertyPath);
  OutResult->SetStringField(TEXT("propertyType"), ResolvedType);
  return true;
}
}
