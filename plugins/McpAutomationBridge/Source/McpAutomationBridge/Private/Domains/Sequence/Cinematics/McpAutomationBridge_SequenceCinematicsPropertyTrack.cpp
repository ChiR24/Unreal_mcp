#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#if WITH_EDITOR
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneDoubleTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneIntegerTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "UObject/UnrealType.h"
#endif

namespace McpSequenceCinematics {
#if WITH_EDITOR
namespace {
FProperty *ResolveBoundPropertyPath(UMovieScene *MovieScene, const FGuid &Guid,
                                    const FString &Path, FString &OutError) {
  const FMovieScenePossessable *Possessable =
      MovieScene ? MovieScene->FindPossessable(Guid) : nullptr;
  const UClass *BoundClass =
      Possessable ? Possessable->GetPossessedObjectClass() : nullptr;
  FMovieSceneSpawnable *Spawnable =
      MovieScene ? MovieScene->FindSpawnable(Guid) : nullptr;
  UObject *DefaultObject =
      BoundClass ? BoundClass->GetDefaultObject()
                 : (Spawnable ? Spawnable->GetObjectTemplate() : nullptr);
  if (!DefaultObject) {
    OutError = TEXT("Binding has no resolvable object template");
    return nullptr;
  }
  void *Container = nullptr;
  return ResolveNestedPropertyPath(DefaultObject, Path, Container, OutError);
}

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
  } else if (const FNumericProperty *Numeric =
                 CastField<FNumericProperty>(Property);
             Numeric && Numeric->IsInteger()) {
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
#endif

bool HandleAddPropertyTrack(UMcpAutomationBridgeSubsystem *Self,
                            const TSharedPtr<FJsonObject> &Params,
                            TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  FGuid Guid;
  if (!ReadBindingGuid(Params, Guid)) {
    // Same contradiction as the bound-track loader: the record declares
    // actorName, so resolve the binding from it before refusing.
    if (AActor *BoundActor = ResolveActor(Params)) {
      Guid = ResolveOrCreateBinding(Sequence, BoundActor);
    }
    if (!Guid.IsValid()) {
      OutResult = MakeResult(false, TEXT("add_property_track"),
                             TEXT("actorName or bindingGuid is required"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }
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
      GetString(Params, TEXT("propertyType"), TEXT("type"));
  FString ResolvedType;
  UClass *TrackClass =
      ResolvePropertyTrackClass(RequestedType, Property, ResolvedType);
  if (!TrackClass) {
    OutResult = MakeResult(
        false, TEXT("add_property_track"),
        FString::Printf(TEXT("propertyType '%s' is incompatible with '%s'"),
                        *RequestedType, *PropertyPath),
        TEXT("PROPERTY_TYPE_MISMATCH"));
    return true;
  }
  UMovieScenePropertyTrack *Track = Cast<UMovieScenePropertyTrack>(
      AddTrackForBinding(Sequence->GetMovieScene(), TrackClass, Guid));
  if (!Track) {
    OutResult = MakeResult(false, TEXT("add_property_track"),
                           TEXT("Failed to create property track"),
                           TEXT("TRACK_CREATION_FAILED"));
    return true;
  }
  Track->SetPropertyNameAndPath(Property->GetFName(), PropertyPath);
  UMovieSceneSection *Section = Track->CreateNewSection();
  if (!Section) {
    RemoveTrackAfterSectionFailure(Sequence->GetMovieScene(), Track, true);
    OutResult = MakeResult(false, TEXT("add_property_track"),
                           TEXT("Failed to create property section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  Track->AddSection(*Section);
  SetSectionRange(Sequence->GetMovieScene(), Section, Params, 100);
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_property_track"),
                         TEXT("Property track added"));
  OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  OutResult->SetStringField(TEXT("propertyName"), Property->GetName());
  OutResult->SetStringField(TEXT("requestedPropertyName"), RequestedName);
  OutResult->SetStringField(TEXT("propertyPath"), PropertyPath);
  OutResult->SetStringField(TEXT("propertyType"), ResolvedType);
  return true;
#else
  OutResult = MakeResult(false, TEXT("add_property_track"),
                         TEXT("Editor build required"),
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
}
