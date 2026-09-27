#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"
#include "PackageTools.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "MovieSceneCommonHelpers.h"
#include "UniversalObjectLocatorResolveParams.h"
#endif
#include "MovieSceneObjectBindingID.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "UObject/UnrealType.h"

namespace McpSequenceCinematics {
FString GetString(const TSharedPtr<FJsonObject> &Params, const TCHAR *Name,
                  const TCHAR *Alias) {
  return McpGetFirstStringField(Params, {Name, Alias ? Alias : Name});
}

FString GetSequencePath(const TSharedPtr<FJsonObject> &Params) {
  FString Path = GetString(Params, TEXT("sequencePath"), TEXT("path"));
  if (!Path.IsEmpty()) {
    return Path;
  }
  return McpSequence::ResolvePath(Params);
}

ULevelSequence *LoadSequence(const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult) {
  const FString SeqPath = GetSequencePath(Params);
  FString WritablePath;
  FString PathError;
  if (!McpSequencePathSecurity::ValidateWritableAssetPath(
          SeqPath, WritablePath, PathError)) {
    OutResult = MakeResult(false, TEXT("cinematics"), PathError,
                           TEXT("SEQUENCE_PATH_NOT_WRITABLE"));
    return nullptr;
  }
  ULevelSequence *Sequence =
      LoadObject<ULevelSequence>(nullptr, *WritablePath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    OutResult = MakeResult(false, TEXT("cinematics"),
                           TEXT("Valid sequencePath is required"),
                           TEXT("INVALID_SEQUENCE"));
    return nullptr;
  }
  FString FrameError;
  if (!McpSequenceFrameMath::ValidateCinematicFrameRequest(
          Params, Sequence->GetMovieScene(), FrameError)) {
    const FString Action = GetString(Params, TEXT("action"), TEXT("subAction"));
    OutResult = MakeResult(
        false, Action.IsEmpty() ? TEXT("cinematics") : Action, FrameError,
        TEXT("INVALID_ARGUMENT"));
    return nullptr;
  }

  bool bSave = false;
  UPackage *Package = Sequence->GetOutermost();
  if (Params.IsValid() && Params->TryGetBoolField(TEXT("save"), bSave) &&
      bSave && Package && Package->IsDirty() && !McpSafeAssetSave(Sequence)) {
    const FString Action = GetString(Params, TEXT("action"), TEXT("subAction"));
    OutResult = MakeResult(
        false, Action.IsEmpty() ? TEXT("cinematics") : Action,
        TEXT("Existing sequence changes could not be saved; no new mutation "
             "was applied"),
        TEXT("ASSET_PREFLIGHT_SAVE_FAILED"));
    OutResult->SetStringField(TEXT("sequencePath"), Sequence->GetPathName());
    return nullptr;
  }
  return Sequence;
}

void SetSectionRange(UMovieScene *MovieScene, UMovieSceneSection *Section,
                     const TSharedPtr<FJsonObject> &Params,
                     int32 DefaultDuration) {
  if (!Section) {
    return;
  }
  const FFrameNumber Start =
      GetFrame(Params, MovieScene, TEXT("startFrame"));
  const int32 Duration = GetDuration(Params, MovieScene, DefaultDuration);
  FFrameNumber End;
  FString Error;
  if (!McpSequenceFrameMath::TryAddFrames(Start, Duration, End, Error)) {
    return;
  }
  Section->SetRange(TRange<FFrameNumber>(Start, End));
  double RowIndex = 0.0;
  if (Params.IsValid() && Params->TryGetNumberField(TEXT("rowIndex"), RowIndex)) {
    Section->SetRowIndex(static_cast<int32>(FMath::RoundToInt(RowIndex)));
  }
}

bool ReadBindingGuid(const TSharedPtr<FJsonObject> &Params, FGuid &OutGuid) {
  FString BindingId = GetString(Params, TEXT("bindingGuid"), TEXT("bindingId"));
  return !BindingId.IsEmpty() && FGuid::Parse(BindingId, OutGuid);
}

AActor *ResolveActor(const TSharedPtr<FJsonObject> &Params) {
  FString ActorName = GetString(Params, TEXT("actorName"), TEXT("cameraName"));
  if (ActorName.IsEmpty()) {
    ActorName = GetString(Params, TEXT("actorPath"), TEXT("cameraActorPath"));
  }
  UObject *Object = ActorName.IsEmpty() ? nullptr : McpHandlerUtils::ResolveObjectFromPath(ActorName);
  return Cast<AActor>(Object);
}

UObject *GetBindingTemplate(UMovieScene *MovieScene, const FGuid &Guid) {
  if (!MovieScene) return nullptr;
  if (const FMovieScenePossessable *Possessable = MovieScene->FindPossessable(Guid)) {
    if (const UClass *BoundClass = Possessable->GetPossessedObjectClass()) {
      return BoundClass->GetDefaultObject();
    }
  }
  FMovieSceneSpawnable *Spawnable = MovieScene->FindSpawnable(Guid);
  return Spawnable ? Spawnable->GetObjectTemplate() : nullptr;
}

FGuid ResolveOrCreateBinding(ULevelSequence *Sequence, AActor *Actor) {
  if (!Sequence || !Actor || !Sequence->GetMovieScene()) {
    return FGuid();
  }
  const FGuid ExistingBinding =
      FindExistingBinding(Sequence, Actor, Actor->GetWorld());
  if (ExistingBinding.IsValid()) {
    return ExistingBinding;
  }
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  const FGuid BindingGuid =
      MovieScene->AddPossessable(Actor->GetActorLabel(), Actor->GetClass());
  if (BindingGuid.IsValid()) {
    Sequence->BindPossessableObject(BindingGuid, *Actor, Actor->GetWorld());
    MovieScene->Modify();
  }
  return BindingGuid;
}

FGuid ResolveRequestBinding(const TSharedPtr<FJsonObject> &Params, ULevelSequence *Sequence) {
  FGuid Guid;
  if (!ReadBindingGuid(Params, Guid))
    Guid = ResolveOrCreateBinding(Sequence, ResolveActor(Params));
  return Guid;
}

bool LoadSequenceAndBinding(const TSharedPtr<FJsonObject> &Params, const TCHAR *Action,
                            ULevelSequence *&OutSequence, FGuid &OutGuid,
                            TSharedPtr<FJsonObject> &OutResult) {
  OutSequence = LoadSequence(Params, OutResult);
  if (!OutSequence)
    return false;
  OutGuid = ResolveRequestBinding(Params, OutSequence);
  if (OutGuid.IsValid())
    return true;
  OutResult = MakeResult(false, Action, TEXT("actorName or bindingGuid is required"),
                         TEXT("INVALID_ARGUMENT"));
  return false;
}

FGuid FindExistingBinding(ULevelSequence *Sequence, UObject *Object,
                          UObject *Context) {
  if (!Sequence || !Object || !Context)
    return FGuid();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
  return Sequence->FindBindingFromObject(
      Object, MovieSceneHelpers::CreateTransientSharedPlaybackState(
                  Context, Sequence));
#else
  return Sequence->FindBindingFromObject(Object, Context);
#endif
}

void LocateBindingObjects(
    ULevelSequence *Sequence, const FGuid &BindingGuid, UObject *Context,
    TArray<UObject *, TInlineAllocator<1>> &OutObjects) {
  if (!Sequence || !Context)
    return;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
  Sequence->LocateBoundObjects(
      BindingGuid, UE::UniversalObjectLocator::FResolveParams(Context),
      MovieSceneHelpers::CreateTransientSharedPlaybackState(Context, Sequence),
      OutObjects);
#else
  Sequence->LocateBoundObjects(BindingGuid, Context, OutObjects);
#endif
}

UMovieSceneTrack *AddTrackForBinding(UMovieScene *MovieScene, UClass *TrackClass,
                                     const FGuid &BindingGuid) {
  if (!MovieScene || !TrackClass) {
    return nullptr;
  }
  return BindingGuid.IsValid() ? MovieScene->AddTrack(TrackClass, BindingGuid)
                               : MovieScene->AddTrack(TrackClass);
}

void RemoveTrackAfterSectionFailure(UMovieScene *MovieScene,
                                    UMovieSceneTrack *Track,
                                    bool bTrackCreated) {
  if (MovieScene && Track && bTrackCreated)
    MovieScene->RemoveTrack(*Track);
}

UMovieSceneSection *AddTrackSection(UMovieScene *MovieScene, UMovieSceneTrack *Track, bool bTrackCreated) {
  UMovieSceneSection *Section = Track ? Track->CreateNewSection() : nullptr;
  if (Section) {
    Track->AddSection(*Section);
  } else {
    RemoveTrackAfterSectionFailure(MovieScene, Track, bTrackCreated);
  }
  return Section;
}

bool MaybeSaveSequence(ULevelSequence *Sequence,
                       const TSharedPtr<FJsonObject> &Params,
                       TSharedPtr<FJsonObject> &OutResult) {
  bool bSave = false;
  if (!Sequence || !Params.IsValid() ||
      !Params->TryGetBoolField(TEXT("save"), bSave) || !bSave) {
    return true;
  }
  if (McpSafeAssetSave(Sequence)) {
    return true;
  }

  const FString SequencePath = Sequence->GetPathName();
  bool bRolledBack = false;
  FString RollbackError;
  UPackage *Package = Sequence->GetOutermost();
  if (Package) {
    FString PackageFilename;
    if (Package->HasAnyPackageFlags(PKG_NewlyCreated) &&
        FPackageName::TryConvertLongPackageNameToFilename(
            Package->GetName(), PackageFilename,
            FPackageName::GetAssetPackageExtension()) &&
        IFileManager::Get().FileExists(
            *FPaths::ConvertRelativePathToFull(PackageFilename))) {
      Package->ClearPackageFlags(PKG_NewlyCreated);
    }
    FText ReloadError;
    TArray<UPackage *> PackagesToReload{Package};
    bRolledBack = UPackageTools::ReloadPackages(
        PackagesToReload, ReloadError,
        EReloadPackagesInteractionMode::AssumePositive);
    RollbackError = ReloadError.ToString();
  }

  const FString Action = GetString(Params, TEXT("action"), TEXT("subAction"));
  OutResult = MakeResult(
      false, Action.IsEmpty() ? TEXT("cinematics") : Action,
      bRolledBack
          ? TEXT("The sequence could not be saved; the in-memory mutation was "
                 "rolled back")
          : TEXT("The sequence could not be saved and its in-memory mutation "
                 "could not be rolled back"),
      TEXT("ASSET_SAVE_FAILED"));
  OutResult->SetStringField(TEXT("sequencePath"), SequencePath);
  OutResult->SetBoolField(TEXT("rolledBack"), bRolledBack);
  if (!bRolledBack && !RollbackError.IsEmpty()) {
    OutResult->SetStringField(TEXT("rollbackError"), RollbackError);
  }
  return false;
}
}
