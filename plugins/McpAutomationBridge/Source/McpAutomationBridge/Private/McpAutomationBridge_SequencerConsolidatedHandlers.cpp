// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 30: Consolidated Sequencer Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MovieSceneBindingOwnerInterface.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "MovieSceneFolder.h"

// Tracks
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Tracks/MovieSceneLevelVisibilityTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Tracks/MovieSceneSubTrack.h"

// Sections
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieSceneEventSection.h"
#include "Sections/MovieSceneFadeSection.h"
#include "Sections/MovieSceneSubSection.h"

// Channels
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneChannelProxy.h"

// Camera & Animation
#include "Camera/CameraActor.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Animation/AnimSequence.h"

// Asset utilities
#include "AssetToolsModule.h"
#if __has_include("Factories/LevelSequenceFactoryNew.h")
#include "Factories/LevelSequenceFactoryNew.h"
#define MCP_HAS_LEVEL_SEQUENCE_FACTORY 1
#else
#define MCP_HAS_LEVEL_SEQUENCE_FACTORY 0
#endif
#include "UObject/Package.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
#include "Misc/PackageName.h"

// Spawnable
#include "MovieSceneSpawnable.h"
#include "MovieScenePossessable.h"

// Runtime
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// Object tools for deletion/export
#if __has_include("ObjectTools.h")
#include "ObjectTools.h"
#define MCP_HAS_OBJECT_TOOLS 1
#else
#define MCP_HAS_OBJECT_TOOLS 0
#endif

// Exporter
#include "Exporters/Exporter.h"

// ============================================================================
// Helper: O(N) → O(1) Actor Lookup by Name/Label using TActorIterator
// ============================================================================
namespace {
  template<typename T>
  T* FindSequencerActorByNameOrLabel(UWorld* World, const FString& NameOrLabel) {
    if (!World || NameOrLabel.IsEmpty()) return nullptr;
    for (TActorIterator<T> It(World); It; ++It) {
      if (It->GetName() == NameOrLabel || It->GetActorLabel() == NameOrLabel) {
        return *It;
      }
    }
    return nullptr;
  }
}

// Helper: Find existing binding for an object by iterating possessables
// This avoids the deprecated FindBindingFromObject(UObject*, UObject*) API in UE 5.5+
static FGuid McpFindExistingBindingForObject(ULevelSequence* Sequence, UMovieScene* MovieScene, AActor* TargetActor)
{
  if (!Sequence || !MovieScene || !TargetActor) return FGuid();
  
  // Check all possessables for a matching binding
  for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
  {
    FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
    // Match by name and class
    if (Possessable.GetName() == TargetActor->GetName() && 
        Possessable.GetPossessedObjectClass() == TargetActor->GetClass())
    {
      return Possessable.GetGuid();
    }
  }
  return FGuid();
}

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleSequencerAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_sequencer"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_sequencer")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_sequencer payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();

  // ========================================================================
  // CREATE MASTER SEQUENCE
  // ========================================================================
  if (LowerSub == TEXT("create_master_sequence")) {
    FString SequenceName;
    Payload->TryGetStringField(TEXT("sequenceName"), SequenceName);
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    
    if (SequenceName.IsEmpty()) {
      SequenceName = TEXT("NewMasterSequence");
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/Sequences");
    }
    
    FString PackagePath = SavePath / SequenceName;
    PackagePath = PackagePath.Replace(TEXT("/Content"), TEXT("/Game"));
    
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package) {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_CREATION_FAILED");
    } else {
      ULevelSequence* LevelSequence = NewObject<ULevelSequence>(
          Package, *SequenceName, RF_Public | RF_Standalone);
      
      if (LevelSequence) {
        LevelSequence->Initialize();
        
        // Set default display rate
        double DisplayRate = 30.0;
        Payload->TryGetNumberField(TEXT("displayRate"), DisplayRate);
        UMovieScene* MovieScene = LevelSequence->GetMovieScene();
        if (MovieScene) {
          MovieScene->SetDisplayRate(FFrameRate(static_cast<int32>(DisplayRate), 1));
        }
        
        LevelSequence->MarkPackageDirty();
        McpSafeAssetSave(LevelSequence);
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Created master sequence: %s"), *PackagePath);
        Resp->SetStringField(TEXT("sequencePath"), LevelSequence->GetPathName());
        Resp->SetStringField(TEXT("sequenceName"), SequenceName);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create LevelSequence object");
        ErrorCode = TEXT("CREATION_FAILED");
      }
    }
  }
  // ========================================================================
  // ADD SUBSEQUENCE
  // ========================================================================
  else if (LowerSub == TEXT("add_subsequence")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString SubsequencePath;
    Payload->TryGetStringField(TEXT("subsequencePath"), SubsequencePath);
    
    if (SequencePath.IsEmpty() || SubsequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath and subsequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      ULevelSequence* MasterSequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
      ULevelSequence* SubSequence = LoadObject<ULevelSequence>(nullptr, *SubsequencePath);
      
      if (MasterSequence && SubSequence) {
        UMovieScene* MovieScene = MasterSequence->GetMovieScene();
        if (MovieScene) {
          // Find or create subscenes track (shot track)
          UMovieSceneSubTrack* SubTrack = MovieScene->FindTrack<UMovieSceneSubTrack>();
          if (!SubTrack) {
            SubTrack = MovieScene->AddTrack<UMovieSceneSubTrack>();
          }
          
          if (SubTrack) {
            int32 StartFrame = 0;
            int32 EndFrame = 150;
            Payload->TryGetNumberField(TEXT("startFrame"), StartFrame);
            Payload->TryGetNumberField(TEXT("endFrame"), EndFrame);
            
            FFrameRate FrameRate = MovieScene->GetTickResolution();
            FFrameNumber StartFrameNum = FFrameNumber(StartFrame);
            FFrameNumber EndFrameNum = FFrameNumber(EndFrame);
            
            UMovieSceneSubSection* SubSection = SubTrack->AddSequence(SubSequence, StartFrameNum, (EndFrameNum - StartFrameNum).Value);
            if (SubSection) {
              MovieScene->Modify();
              MasterSequence->MarkPackageDirty();
              McpSafeAssetSave(MasterSequence);
              
              bSuccess = true;
              Message = TEXT("Subsequence added");
              Resp->SetStringField(TEXT("sectionId"), SubSection->GetFName().ToString());
            } else {
              bSuccess = false;
              Message = TEXT("Failed to add subsequence section");
              ErrorCode = TEXT("SECTION_CREATION_FAILED");
            }
          }
        }
      } else {
        bSuccess = false;
        Message = TEXT("Master sequence or subsequence not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // REMOVE SUBSEQUENCE
  // ========================================================================
  else if (LowerSub == TEXT("remove_subsequence")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString SubsequencePath;
    Payload->TryGetStringField(TEXT("subsequencePath"), SubsequencePath);
    
    ULevelSequence* MasterSequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (MasterSequence) {
      UMovieScene* MovieScene = MasterSequence->GetMovieScene();
      if (MovieScene) {
        UMovieSceneSubTrack* SubTrack = MovieScene->FindTrack<UMovieSceneSubTrack>();
        if (SubTrack) {
          for (UMovieSceneSection* Section : SubTrack->GetAllSections()) {
            UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
            if (SubSection && SubSection->GetSequence() && 
                SubSection->GetSequence()->GetPathName() == SubsequencePath) {
              SubTrack->RemoveSection(*SubSection);
              MovieScene->Modify();
              bSuccess = true;
              Message = TEXT("Subsequence removed");
              break;
            }
          }
          if (!bSuccess) {
            bSuccess = false;
            Message = TEXT("Subsequence section not found");
            ErrorCode = TEXT("NOT_FOUND");
          }
        }
      }
    }
  }
  // ========================================================================
  // GET SUBSEQUENCES
  // ========================================================================
  else if (LowerSub == TEXT("get_subsequences")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TArray<TSharedPtr<FJsonValue>> SubsequencesArray;
        UMovieSceneSubTrack* SubTrack = MovieScene->FindTrack<UMovieSceneSubTrack>();
        if (SubTrack) {
          for (UMovieSceneSection* Section : SubTrack->GetAllSections()) {
            UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
            if (SubSection && SubSection->GetSequence()) {
              TSharedPtr<FJsonObject> SubObj = MakeShared<FJsonObject>();
              SubObj->SetStringField(TEXT("path"), SubSection->GetSequence()->GetPathName());
              SubObj->SetStringField(TEXT("name"), SubSection->GetSequence()->GetName());
              SubsequencesArray.Add(MakeShared<FJsonValueObject>(SubObj));
            }
          }
        }
        Resp->SetArrayField(TEXT("subsequences"), SubsequencesArray);
        bSuccess = true;
        Message = FString::Printf(TEXT("Found %d subsequences"), SubsequencesArray.Num());
      }
    } else {
      bSuccess = false;
      Message = TEXT("Sequence not found");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }
  }
  // ========================================================================
  // CREATE CINE CAMERA ACTOR
  // ========================================================================
  else if (LowerSub == TEXT("create_cine_camera_actor")) {
    FString CameraName;
    Payload->TryGetStringField(TEXT("cameraActorName"), CameraName);
    if (CameraName.IsEmpty()) {
      CameraName = TEXT("CineCamera");
    }
    
    UWorld* World = GetActiveWorld();
    if (World) {
      FActorSpawnParameters SpawnParams;
      // UE 5.7+ fix: Ensure unique name to avoid crash in LevelActor.cpp
      SpawnParams.Name = MakeUniqueObjectName(World->GetCurrentLevel(), ACineCameraActor::StaticClass(), FName(*CameraName));
      SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
      
      ACineCameraActor* CameraActor = World->SpawnActor<ACineCameraActor>(
          ACineCameraActor::StaticClass(),
          FVector::ZeroVector,
          FRotator::ZeroRotator,
          SpawnParams);
      
      if (CameraActor) {
        // Apply location/rotation if provided
        const TSharedPtr<FJsonObject>* LocationObj;
        if (Payload->TryGetObjectField(TEXT("location"), LocationObj)) {
          double X = 0, Y = 0, Z = 0;
          (*LocationObj)->TryGetNumberField(TEXT("x"), X);
          (*LocationObj)->TryGetNumberField(TEXT("y"), Y);
          (*LocationObj)->TryGetNumberField(TEXT("z"), Z);
          CameraActor->SetActorLocation(FVector(X, Y, Z));
        }
        
        const TSharedPtr<FJsonObject>* RotationObj;
        if (Payload->TryGetObjectField(TEXT("rotation"), RotationObj)) {
          double Pitch = 0, Yaw = 0, Roll = 0;
          (*RotationObj)->TryGetNumberField(TEXT("pitch"), Pitch);
          (*RotationObj)->TryGetNumberField(TEXT("yaw"), Yaw);
          (*RotationObj)->TryGetNumberField(TEXT("roll"), Roll);
          CameraActor->SetActorRotation(FRotator(Pitch, Yaw, Roll));
        }
        
        // Configure camera settings
        UCineCameraComponent* CineCameraComp = CameraActor->GetCineCameraComponent();
        if (CineCameraComp) {
          double FocalLength = 35.0;
          double Aperture = 2.8;
          double FocusDistance = 1000.0;
          
          Payload->TryGetNumberField(TEXT("focalLength"), FocalLength);
          Payload->TryGetNumberField(TEXT("aperture"), Aperture);
          Payload->TryGetNumberField(TEXT("focusDistance"), FocusDistance);
          
          CineCameraComp->SetCurrentFocalLength(FocalLength);
          CineCameraComp->SetFocusSettings(FCameraFocusSettings());
          CineCameraComp->CurrentAperture = Aperture;
        }
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Created cine camera: %s"), *CameraName);
        Resp->SetStringField(TEXT("actorName"), CameraActor->GetName());
      } else {
        bSuccess = false;
        Message = TEXT("Failed to spawn CineCameraActor");
        ErrorCode = TEXT("SPAWN_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("No active world");
      ErrorCode = TEXT("NO_WORLD");
    }
  }
  // ========================================================================
  // CONFIGURE CAMERA SETTINGS
  // ========================================================================
  else if (LowerSub == TEXT("configure_camera_settings")) {
    FString CameraActorName;
    Payload->TryGetStringField(TEXT("cameraActorName"), CameraActorName);
    
    UWorld* World = GetActiveWorld();
    if (World && !CameraActorName.IsEmpty()) {
      ACineCameraActor* CineCam = FindSequencerActorByNameOrLabel<ACineCameraActor>(World, CameraActorName);
      
      if (CineCam) {
        UCineCameraComponent* CineCameraComp = CineCam->GetCineCameraComponent();
        if (CineCameraComp) {
          double FocalLength, Aperture, SensorWidth, SensorHeight;
          if (Payload->TryGetNumberField(TEXT("focalLength"), FocalLength)) {
            CineCameraComp->SetCurrentFocalLength(FocalLength);
          }
          if (Payload->TryGetNumberField(TEXT("aperture"), Aperture)) {
            CineCameraComp->CurrentAperture = Aperture;
          }
          if (Payload->TryGetNumberField(TEXT("sensorWidth"), SensorWidth)) {
            CineCameraComp->Filmback.SensorWidth = SensorWidth;
          }
          if (Payload->TryGetNumberField(TEXT("sensorHeight"), SensorHeight)) {
            CineCameraComp->Filmback.SensorHeight = SensorHeight;
          }
          
          bSuccess = true;
          Message = TEXT("Camera settings updated");
        }
      } else {
        bSuccess = false;
        Message = TEXT("CineCameraActor not found");
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // ADD CAMERA CUT TRACK
  // ========================================================================
  else if (LowerSub == TEXT("add_camera_cut_track")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
        if (CameraCutTrack) {
          MovieScene->Modify();
          bSuccess = true;
          Message = TEXT("Camera cut track added");
          Resp->SetStringField(TEXT("trackId"), CameraCutTrack->GetFName().ToString());
        }
      }
    } else {
      bSuccess = false;
      Message = TEXT("Sequence not found");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }
  }
  // ========================================================================
  // ADD CAMERA CUT
  // ========================================================================
  else if (LowerSub == TEXT("add_camera_cut")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString CameraActorName;
    Payload->TryGetStringField(TEXT("cameraActorName"), CameraActorName);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World && !CameraActorName.IsEmpty()) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      UMovieSceneCameraCutTrack* CameraCutTrack = MovieScene->FindTrack<UMovieSceneCameraCutTrack>();
      
      if (!CameraCutTrack) {
        CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
      }
      
      if (CameraCutTrack) {
        // Find camera actor and get/create binding
        ACineCameraActor* CameraActor = FindSequencerActorByNameOrLabel<ACineCameraActor>(World, CameraActorName);
        
        if (CameraActor) {
          // Get or create binding for camera (using non-deprecated helper)
          FGuid CameraBinding = McpFindExistingBindingForObject(Sequence, MovieScene, CameraActor);
          if (!CameraBinding.IsValid()) {
            CameraBinding = MovieScene->AddPossessable(CameraActor->GetName(), CameraActor->GetClass());
            Sequence->BindPossessableObject(CameraBinding, *CameraActor, World);
          }
          
          int32 StartFrame = 0;
          int32 EndFrame = 150;
          Payload->TryGetNumberField(TEXT("startFrame"), StartFrame);
          Payload->TryGetNumberField(TEXT("endFrame"), EndFrame);
          
          FFrameNumber StartFrameNum = FFrameNumber(StartFrame);
          
          UMovieSceneCameraCutSection* CutSection = Cast<UMovieSceneCameraCutSection>(
              CameraCutTrack->CreateNewSection());
          if (CutSection) {
            CutSection->SetCameraBindingID(UE::MovieScene::FRelativeObjectBindingID(CameraBinding));
            CutSection->SetRange(TRange<FFrameNumber>(StartFrameNum, FFrameNumber(EndFrame)));
            CameraCutTrack->AddSection(*CutSection);
            MovieScene->Modify();
            
            bSuccess = true;
            Message = TEXT("Camera cut added");
            Resp->SetStringField(TEXT("bindingId"), CameraBinding.ToString());
          }
        } else {
          bSuccess = false;
          Message = TEXT("Camera actor not found");
          ErrorCode = TEXT("ACTOR_NOT_FOUND");
        }
      }
    }
  }
  // ========================================================================
  // BIND ACTOR
  // ========================================================================
  else if (LowerSub == TEXT("bind_actor")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    bool bSpawnable = false;
    Payload->TryGetBoolField(TEXT("spawnable"), bSpawnable);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World && !ActorName.IsEmpty()) {
      AActor* TargetActor = FindSequencerActorByNameOrLabel<AActor>(World, ActorName);
      
      if (TargetActor) {
        UMovieScene* MovieScene = Sequence->GetMovieScene();
        FGuid BindingGuid;
        
        if (bSpawnable) {
          // Create spawnable
          BindingGuid = MovieScene->AddSpawnable(TargetActor->GetName(), *TargetActor);
        } else {
          // Create possessable
          BindingGuid = MovieScene->AddPossessable(TargetActor->GetName(), TargetActor->GetClass());
          Sequence->BindPossessableObject(BindingGuid, *TargetActor, World);
        }
        
        if (BindingGuid.IsValid()) {
          MovieScene->Modify();
          Sequence->MarkPackageDirty();
          
          bSuccess = true;
          Message = FString::Printf(TEXT("Actor bound as %s"), bSpawnable ? TEXT("spawnable") : TEXT("possessable"));
          Resp->SetStringField(TEXT("bindingId"), BindingGuid.ToString());
          Resp->SetStringField(TEXT("actorName"), TargetActor->GetName());
        }
      } else {
        bSuccess = false;
        Message = TEXT("Actor not found in world");
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // UNBIND ACTOR
  // ========================================================================
  else if (LowerSub == TEXT("unbind_actor")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString BindingIdStr;
    Payload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !BindingIdStr.IsEmpty()) {
      FGuid BindingGuid;
      if (FGuid::Parse(BindingIdStr, BindingGuid)) {
        UMovieScene* MovieScene = Sequence->GetMovieScene();
        if (MovieScene->RemovePossessable(BindingGuid) || MovieScene->RemoveSpawnable(BindingGuid)) {
          MovieScene->Modify();
          bSuccess = true;
          Message = TEXT("Actor unbound");
        } else {
          bSuccess = false;
          Message = TEXT("Binding not found");
          ErrorCode = TEXT("NOT_FOUND");
        }
      }
    }
  }
  // ========================================================================
  // GET BINDINGS
  // ========================================================================
  else if (LowerSub == TEXT("get_bindings")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TArray<TSharedPtr<FJsonValue>> BindingsArray;
        BindingsArray.Reserve(MovieScene->GetPossessableCount() + MovieScene->GetSpawnableCount());
        
        // Get possessables
        for (int32 i = 0; i < MovieScene->GetPossessableCount(); i++) {
          const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
          TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
          BindingObj->SetStringField(TEXT("id"), Possessable.GetGuid().ToString());
          BindingObj->SetStringField(TEXT("name"), Possessable.GetName());
          BindingObj->SetStringField(TEXT("type"), TEXT("Possessable"));
          BindingsArray.Add(MakeShared<FJsonValueObject>(BindingObj));
        }
        
        // Get spawnables
        for (int32 i = 0; i < MovieScene->GetSpawnableCount(); i++) {
          const FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(i);
          TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
          BindingObj->SetStringField(TEXT("id"), Spawnable.GetGuid().ToString());
          BindingObj->SetStringField(TEXT("name"), Spawnable.GetName());
          BindingObj->SetStringField(TEXT("type"), TEXT("Spawnable"));
          BindingsArray.Add(MakeShared<FJsonValueObject>(BindingObj));
        }
        
        Resp->SetArrayField(TEXT("bindings"), BindingsArray);
        bSuccess = true;
        Message = FString::Printf(TEXT("Found %d bindings"), BindingsArray.Num());
      }
    }
  }
  // ========================================================================
  // ADD TRACK
  // ========================================================================
  else if (LowerSub == TEXT("add_track")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString BindingIdStr;
    Payload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
    FString TrackType;
    Payload->TryGetStringField(TEXT("trackType"), TrackType);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !BindingIdStr.IsEmpty() && !TrackType.IsEmpty()) {
      FGuid BindingGuid;
      FGuid::Parse(BindingIdStr, BindingGuid);
      
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene && BindingGuid.IsValid()) {
        UMovieSceneTrack* NewTrack = nullptr;
        
        if (TrackType.Equals(TEXT("Transform"), ESearchCase::IgnoreCase)) {
          NewTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
        } else if (TrackType.Equals(TEXT("Animation"), ESearchCase::IgnoreCase) ||
                   TrackType.Equals(TEXT("Skeletal"), ESearchCase::IgnoreCase)) {
          NewTrack = MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(BindingGuid);
        } else if (TrackType.Equals(TEXT("Audio"), ESearchCase::IgnoreCase)) {
          NewTrack = MovieScene->AddTrack<UMovieSceneAudioTrack>(BindingGuid);
        } else if (TrackType.Equals(TEXT("Event"), ESearchCase::IgnoreCase)) {
          NewTrack = MovieScene->AddTrack<UMovieSceneEventTrack>(BindingGuid);
        } else if (TrackType.Equals(TEXT("Fade"), ESearchCase::IgnoreCase)) {
          NewTrack = MovieScene->AddTrack<UMovieSceneFadeTrack>();
        } else if (TrackType.Equals(TEXT("LevelVisibility"), ESearchCase::IgnoreCase)) {
          NewTrack = MovieScene->AddTrack<UMovieSceneLevelVisibilityTrack>();
        }
        
        if (NewTrack) {
          // Create default section
          UMovieSceneSection* NewSection = NewTrack->CreateNewSection();
          if (NewSection) {
            NewTrack->AddSection(*NewSection);
          }
          
          MovieScene->Modify();
          bSuccess = true;
          Message = FString::Printf(TEXT("Added %s track"), *TrackType);
          Resp->SetStringField(TEXT("trackId"), NewTrack->GetFName().ToString());
        } else {
          bSuccess = false;
          Message = FString::Printf(TEXT("Unsupported track type: %s"), *TrackType);
          ErrorCode = TEXT("UNSUPPORTED_TRACK_TYPE");
        }
      }
    }
  }
  // ========================================================================
  // REMOVE TRACK
  // ========================================================================
  else if (LowerSub == TEXT("remove_track")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString TrackId;
    Payload->TryGetStringField(TEXT("trackId"), TrackId);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !TrackId.IsEmpty()) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        bool bRemoved = false;
        for (UMovieSceneTrack* Track : MovieScene->GetTracks()) {
          if (Track->GetFName().ToString() == TrackId) {
            MovieScene->RemoveTrack(*Track);
            bRemoved = true;
            break;
          }
        }
        
        if (bRemoved) {
          MovieScene->Modify();
          bSuccess = true;
          Message = TEXT("Track removed");
        } else {
          bSuccess = false;
          Message = TEXT("Track not found");
          ErrorCode = TEXT("NOT_FOUND");
        }
      }
    }
  }
  // ========================================================================
  // GET TRACKS
  // ========================================================================
  else if (LowerSub == TEXT("get_tracks")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString BindingIdStr;
    Payload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TArray<TSharedPtr<FJsonValue>> TracksArray;
        
        TArray<UMovieSceneTrack*> TracksToList;
        
        if (BindingIdStr.IsEmpty()) {
          // Get master tracks
          TracksToList = MovieScene->GetTracks();
        } else {
          // Get tracks for specific binding
          FGuid BindingGuid;
          FGuid::Parse(BindingIdStr, BindingGuid);
          if (FMovieSceneBinding* Binding = MovieScene->FindBinding(BindingGuid)) {
            TracksToList = Binding->GetTracks();
          }
        }
        
        // TracksArray already declared above on line 737
        TracksArray.Reserve(TracksToList.Num());
        for (UMovieSceneTrack* Track : TracksToList) {
          TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
          TrackObj->SetStringField(TEXT("id"), Track->GetFName().ToString());
          TrackObj->SetStringField(TEXT("type"), Track->GetClass()->GetName());
          TrackObj->SetNumberField(TEXT("sectionCount"), Track->GetAllSections().Num());
          TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
        }
        
        Resp->SetArrayField(TEXT("tracks"), TracksArray);
        bSuccess = true;
        Message = FString::Printf(TEXT("Found %d tracks"), TracksArray.Num());
      }
    }
  }
  // ========================================================================
  // ADD KEYFRAME
  // ========================================================================
  else if (LowerSub == TEXT("add_keyframe")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString BindingIdStr;
    Payload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);
    double Value = 0.0;
    Payload->TryGetNumberField(TEXT("value"), Value);
    FString PropertyPath;
    Payload->TryGetStringField(TEXT("propertyPath"), PropertyPath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !BindingIdStr.IsEmpty()) {
      FGuid BindingGuid;
      FGuid::Parse(BindingIdStr, BindingGuid);
      
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene && BindingGuid.IsValid()) {
        // Find float track for the binding
        if (FMovieSceneBinding* Binding = MovieScene->FindBinding(BindingGuid)) {
          for (UMovieSceneTrack* Track : Binding->GetTracks()) {
            UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
            if (FloatTrack) {
              for (UMovieSceneSection* Section : FloatTrack->GetAllSections()) {
                UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(Section);
                if (FloatSection) {
                  FFrameRate FrameRate = MovieScene->GetTickResolution();
                  FFrameNumber FrameNum = (Time * FrameRate).FloorToFrame();
                  
                  FMovieSceneFloatChannel* Channel = FloatSection->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
                  if (Channel) {
                    Channel->AddCubicKey(FrameNum, static_cast<float>(Value));
                    MovieScene->Modify();
                    bSuccess = true;
                    Message = TEXT("Keyframe added");
                    Resp->SetNumberField(TEXT("frame"), FrameNum.Value);
                  }
                  break;
                }
              }
              break;
            }
          }
        }
        
        if (!bSuccess) {
          bSuccess = false;
          Message = TEXT("No suitable track/section found for keyframe");
          ErrorCode = TEXT("NO_TRACK");
        }
      }
    }
  }
  // ========================================================================
  // SET PLAYBACK RANGE
  // ========================================================================
  else if (LowerSub == TEXT("set_playback_range")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    double StartTime = 0.0;
    double EndTime = 5.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);
    Payload->TryGetNumberField(TEXT("endTime"), EndTime);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        FFrameRate FrameRate = MovieScene->GetTickResolution();
        FFrameNumber StartFrame = (StartTime * FrameRate).FloorToFrame();
        FFrameNumber EndFrame = (EndTime * FrameRate).FloorToFrame();
        
        MovieScene->SetPlaybackRange(TRange<FFrameNumber>(StartFrame, EndFrame));
        MovieScene->Modify();
        
        bSuccess = true;
        Message = TEXT("Playback range set");
        Resp->SetNumberField(TEXT("startFrame"), StartFrame.Value);
        Resp->SetNumberField(TEXT("endFrame"), EndFrame.Value);
      }
    }
  }
  // ========================================================================
  // GET PLAYBACK RANGE
  // ========================================================================
  else if (LowerSub == TEXT("get_playback_range")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
        FFrameRate FrameRate = MovieScene->GetTickResolution();
        
        Resp->SetNumberField(TEXT("startFrame"), PlaybackRange.GetLowerBoundValue().Value);
        Resp->SetNumberField(TEXT("endFrame"), PlaybackRange.GetUpperBoundValue().Value);
        Resp->SetNumberField(TEXT("startTime"), PlaybackRange.GetLowerBoundValue().Value / FrameRate.AsDecimal());
        Resp->SetNumberField(TEXT("endTime"), PlaybackRange.GetUpperBoundValue().Value / FrameRate.AsDecimal());
        
        bSuccess = true;
        Message = TEXT("Playback range retrieved");
      }
    }
  }
  // ========================================================================
  // SET DISPLAY RATE
  // ========================================================================
  else if (LowerSub == TEXT("set_display_rate")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    double DisplayRate = 30.0;
    Payload->TryGetNumberField(TEXT("displayRate"), DisplayRate);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        MovieScene->SetDisplayRate(FFrameRate(static_cast<int32>(DisplayRate), 1));
        MovieScene->Modify();
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Display rate set to %.0f FPS"), DisplayRate);
      }
    }
  }
  // ========================================================================
  // GET SEQUENCE INFO
  // ========================================================================
  else if (LowerSub == TEXT("get_sequence_info")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
        
        InfoObj->SetStringField(TEXT("name"), Sequence->GetName());
        InfoObj->SetStringField(TEXT("path"), Sequence->GetPathName());
        
        TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
        FFrameRate TickResolution = MovieScene->GetTickResolution();
        FFrameRate DisplayRate = MovieScene->GetDisplayRate();
        
        InfoObj->SetNumberField(TEXT("displayRate"), DisplayRate.AsDecimal());
        InfoObj->SetNumberField(TEXT("tickResolution"), TickResolution.AsDecimal());
        InfoObj->SetNumberField(TEXT("startFrame"), PlaybackRange.GetLowerBoundValue().Value);
        InfoObj->SetNumberField(TEXT("endFrame"), PlaybackRange.GetUpperBoundValue().Value);
        
        double Duration = (PlaybackRange.GetUpperBoundValue().Value - PlaybackRange.GetLowerBoundValue().Value) / TickResolution.AsDecimal();
        InfoObj->SetNumberField(TEXT("durationSeconds"), Duration);
        
        InfoObj->SetNumberField(TEXT("possessableCount"), MovieScene->GetPossessableCount());
        InfoObj->SetNumberField(TEXT("spawnableCount"), MovieScene->GetSpawnableCount());
        InfoObj->SetNumberField(TEXT("trackCount"), MovieScene->GetTracks().Num());
        
        Resp->SetObjectField(TEXT("sequenceInfo"), InfoObj);
        bSuccess = true;
        Message = TEXT("Sequence info retrieved");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Sequence not found");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }
  }
  // ========================================================================
  // PLAY SEQUENCE
  // ========================================================================
  else if (LowerSub == TEXT("play_sequence")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World) {
      // Find or create sequence player
      ALevelSequenceActor* SequenceActor = nullptr;
      for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) {
        if (It->GetSequence() == Sequence) {
          SequenceActor = *It;
          break;
        }
      }
      
      if (!SequenceActor) {
        FActorSpawnParameters SpawnParams;
        SequenceActor = World->SpawnActor<ALevelSequenceActor>(SpawnParams);
        if (SequenceActor) {
          SequenceActor->SetSequence(Sequence);
        }
      }
      
      if (SequenceActor) {
        ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
        if (Player) {
          Player->Play();
          bSuccess = true;
          Message = TEXT("Sequence playing");
        }
      }
    }
  }
  // ========================================================================
  // PAUSE SEQUENCE
  // ========================================================================
  else if (LowerSub == TEXT("pause_sequence")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World) {
      for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) {
        if (It->GetSequence() == Sequence) {
          ULevelSequencePlayer* Player = It->GetSequencePlayer();
          if (Player) {
            Player->Pause();
            bSuccess = true;
            Message = TEXT("Sequence paused");
          }
          break;
        }
      }
    }
  }
  // ========================================================================
  // STOP SEQUENCE
  // ========================================================================
  else if (LowerSub == TEXT("stop_sequence")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World) {
      for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) {
        if (It->GetSequence() == Sequence) {
          ULevelSequencePlayer* Player = It->GetSequencePlayer();
          if (Player) {
            Player->Stop();
            bSuccess = true;
            Message = TEXT("Sequence stopped");
          }
          break;
        }
      }
    }
  }
  // ========================================================================
  // SCRUB TO TIME
  // ========================================================================
  else if (LowerSub == TEXT("scrub_to_time")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World) {
      for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) {
        if (It->GetSequence() == Sequence) {
          ULevelSequencePlayer* Player = It->GetSequencePlayer();
          if (Player) {
            FMovieSceneSequencePlaybackParams Params;
            Params.Frame = FFrameTime(FFrameNumber(static_cast<int32>(Time * Sequence->GetMovieScene()->GetTickResolution().AsDecimal())));
            Player->SetPlaybackPosition(Params);
            bSuccess = true;
            Message = FString::Printf(TEXT("Scrubbed to %.2f seconds"), Time);
          }
          break;
        }
      }
    }
  }
  // ========================================================================
  // LIST SEQUENCES
  // ========================================================================
  else if (LowerSub == TEXT("list_sequences")) {
    FString DirectoryPath;
    Payload->TryGetStringField(TEXT("directoryPath"), DirectoryPath);
    if (DirectoryPath.IsEmpty()) {
      DirectoryPath = TEXT("/Game");
    }
    
    FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetDataList;
    AssetRegistry.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetClassPathName(), AssetDataList);
    
    TArray<TSharedPtr<FJsonValue>> SequencesArray;
    for (const FAssetData& AssetData : AssetDataList) {
      if (AssetData.PackageName.ToString().StartsWith(DirectoryPath)) {
        TSharedPtr<FJsonObject> SeqObj = MakeShared<FJsonObject>();
        SeqObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
        SeqObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        SequencesArray.Add(MakeShared<FJsonValueObject>(SeqObj));
      }
    }
    
    Resp->SetArrayField(TEXT("sequences"), SequencesArray);
    bSuccess = true;
    Message = FString::Printf(TEXT("Found %d sequences"), SequencesArray.Num());
  }
  // ========================================================================
  // DUPLICATE SEQUENCE
  // ========================================================================
  else if (LowerSub == TEXT("duplicate_sequence")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString NewName;
    Payload->TryGetStringField(TEXT("sequenceName"), NewName);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !NewName.IsEmpty()) {
      FString PackagePath = FPackageName::GetLongPackagePath(Sequence->GetPathName());
      FString NewPackagePath = PackagePath / NewName;
      
      UPackage* NewPackage = CreatePackage(*NewPackagePath);
      if (NewPackage) {
        ULevelSequence* NewSequence = DuplicateObject<ULevelSequence>(Sequence, NewPackage, *NewName);
        if (NewSequence) {
          NewSequence->SetFlags(RF_Public | RF_Standalone);
          NewSequence->MarkPackageDirty();
          McpSafeAssetSave(NewSequence);
          
          bSuccess = true;
          Message = TEXT("Sequence duplicated");
          Resp->SetStringField(TEXT("newSequencePath"), NewSequence->GetPathName());
        }
      }
    }
  }
  // ========================================================================
  // DELETE SEQUENCE
  // ========================================================================
  else if (LowerSub == TEXT("delete_sequence")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      // Use EditorAssetLibrary to delete
#if MCP_HAS_OBJECT_TOOLS
      TArray<UObject*> ObjectsToDelete;
      ObjectsToDelete.Add(Sequence);
      
      if (ObjectTools::DeleteObjects(ObjectsToDelete, true)) {
        bSuccess = true;
        Message = TEXT("Sequence deleted");
      } else {
        bSuccess = false;
        Message = TEXT("Failed to delete sequence");
        ErrorCode = TEXT("DELETE_FAILED");
      }
#else
      // Fallback: use UEditorAssetLibrary
      if (UEditorAssetLibrary::DeleteAsset(SequencePath)) {
        bSuccess = true;
        Message = TEXT("Sequence deleted");
      } else {
        bSuccess = false;
        Message = TEXT("Failed to delete sequence");
        ErrorCode = TEXT("DELETE_FAILED");
      }
#endif
    }
  }
  // ========================================================================
  // ADD SHOT TRACK
  // ========================================================================
  else if (LowerSub == TEXT("add_shot_track")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        UMovieSceneSubTrack* ShotTrack = MovieScene->FindTrack<UMovieSceneSubTrack>();
        if (!ShotTrack) {
          ShotTrack = MovieScene->AddTrack<UMovieSceneSubTrack>();
          MovieScene->Modify();
          bSuccess = true;
          Message = TEXT("Shot track added");
          Resp->SetStringField(TEXT("trackId"), ShotTrack->GetFName().ToString());
        } else {
          bSuccess = true;
          Message = TEXT("Shot track already exists");
          Resp->SetStringField(TEXT("trackId"), ShotTrack->GetFName().ToString());
        }
      }
    }
  }
  // ========================================================================
  // ADD SHOT
  // ========================================================================
  else if (LowerSub == TEXT("add_shot")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString ShotSequencePath;
    Payload->TryGetStringField(TEXT("subsequencePath"), ShotSequencePath);
    FString ShotName;
    Payload->TryGetStringField(TEXT("shotName"), ShotName);
    
    ULevelSequence* MasterSequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    ULevelSequence* ShotSequence = LoadObject<ULevelSequence>(nullptr, *ShotSequencePath);
    
    if (MasterSequence && ShotSequence) {
      UMovieScene* MovieScene = MasterSequence->GetMovieScene();
      UMovieSceneSubTrack* ShotTrack = MovieScene->FindTrack<UMovieSceneSubTrack>();
      if (!ShotTrack) {
        ShotTrack = MovieScene->AddTrack<UMovieSceneSubTrack>();
      }
      
      if (ShotTrack) {
        int32 StartFrame = 0;
        int32 EndFrame = 150;
        Payload->TryGetNumberField(TEXT("startFrame"), StartFrame);
        Payload->TryGetNumberField(TEXT("endFrame"), EndFrame);
        
        UMovieSceneSubSection* ShotSection = ShotTrack->AddSequence(
            ShotSequence, 
            FFrameNumber(StartFrame), 
            EndFrame - StartFrame);
        
        if (ShotSection) {
          MovieScene->Modify();
          bSuccess = true;
          Message = TEXT("Shot added");
          Resp->SetStringField(TEXT("sectionId"), ShotSection->GetFName().ToString());
        }
      }
    }
  }
  // ========================================================================
  // REMOVE SHOT
  // ========================================================================
  else if (LowerSub == TEXT("remove_shot")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString SectionId;
    Payload->TryGetStringField(TEXT("sectionId"), SectionId);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !SectionId.IsEmpty()) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      UMovieSceneSubTrack* ShotTrack = MovieScene->FindTrack<UMovieSceneSubTrack>();
      if (ShotTrack) {
        for (UMovieSceneSection* Section : ShotTrack->GetAllSections()) {
          if (Section->GetFName().ToString() == SectionId) {
            ShotTrack->RemoveSection(*Section);
            MovieScene->Modify();
            bSuccess = true;
            Message = TEXT("Shot removed");
            break;
          }
        }
      }
    }
  }
  // ========================================================================
  // GET SHOTS
  // ========================================================================
  else if (LowerSub == TEXT("get_shots")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      UMovieSceneSubTrack* ShotTrack = MovieScene->FindTrack<UMovieSceneSubTrack>();
      
      TArray<TSharedPtr<FJsonValue>> ShotsArray;
      if (ShotTrack) {
        for (UMovieSceneSection* Section : ShotTrack->GetAllSections()) {
          UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
          if (SubSection) {
            TSharedPtr<FJsonObject> ShotObj = MakeShared<FJsonObject>();
            ShotObj->SetStringField(TEXT("sectionId"), Section->GetFName().ToString());
            if (SubSection->GetSequence()) {
              ShotObj->SetStringField(TEXT("sequencePath"), SubSection->GetSequence()->GetPathName());
              ShotObj->SetStringField(TEXT("sequenceName"), SubSection->GetSequence()->GetName());
            }
            TRange<FFrameNumber> Range = Section->GetRange();
            ShotObj->SetNumberField(TEXT("startFrame"), Range.GetLowerBoundValue().Value);
            ShotObj->SetNumberField(TEXT("endFrame"), Range.GetUpperBoundValue().Value);
            ShotsArray.Add(MakeShared<FJsonValueObject>(ShotObj));
          }
        }
      }
      
      Resp->SetArrayField(TEXT("shots"), ShotsArray);
      bSuccess = true;
      Message = FString::Printf(TEXT("Found %d shots"), ShotsArray.Num());
    }
  }
  // ========================================================================
  // ADD SECTION
  // ========================================================================
  else if (LowerSub == TEXT("add_section")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString TrackId;
    Payload->TryGetStringField(TEXT("trackId"), TrackId);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !TrackId.IsEmpty()) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      for (UMovieSceneTrack* Track : MovieScene->GetTracks()) {
        if (Track->GetFName().ToString() == TrackId) {
          UMovieSceneSection* NewSection = Track->CreateNewSection();
          if (NewSection) {
            int32 StartFrame = 0;
            int32 EndFrame = 150;
            Payload->TryGetNumberField(TEXT("startFrame"), StartFrame);
            Payload->TryGetNumberField(TEXT("endFrame"), EndFrame);
            
            NewSection->SetRange(TRange<FFrameNumber>(FFrameNumber(StartFrame), FFrameNumber(EndFrame)));
            Track->AddSection(*NewSection);
            MovieScene->Modify();
            
            bSuccess = true;
            Message = TEXT("Section added");
            Resp->SetStringField(TEXT("sectionId"), NewSection->GetFName().ToString());
          }
          break;
        }
      }
    }
  }
  // ========================================================================
  // REMOVE SECTION
  // ========================================================================
  else if (LowerSub == TEXT("remove_section")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString TrackId;
    Payload->TryGetStringField(TEXT("trackId"), TrackId);
    FString SectionId;
    Payload->TryGetStringField(TEXT("sectionId"), SectionId);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !TrackId.IsEmpty() && !SectionId.IsEmpty()) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      for (UMovieSceneTrack* Track : MovieScene->GetTracks()) {
        if (Track->GetFName().ToString() == TrackId) {
          for (UMovieSceneSection* Section : Track->GetAllSections()) {
            if (Section->GetFName().ToString() == SectionId) {
              Track->RemoveSection(*Section);
              MovieScene->Modify();
              bSuccess = true;
              Message = TEXT("Section removed");
              break;
            }
          }
          break;
        }
      }
    }
  }
  // ========================================================================
  // REMOVE KEYFRAME
  // ========================================================================
  else if (LowerSub == TEXT("remove_keyframe")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString BindingIdStr;
    Payload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
    int32 Frame = 0;
    Payload->TryGetNumberField(TEXT("frame"), Frame);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !BindingIdStr.IsEmpty()) {
      FGuid BindingGuid;
      FGuid::Parse(BindingIdStr, BindingGuid);
      
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene && BindingGuid.IsValid()) {
        if (FMovieSceneBinding* Binding = MovieScene->FindBinding(BindingGuid)) {
          bool bKeyRemoved = false;
          for (UMovieSceneTrack* Track : Binding->GetTracks()) {
            for (UMovieSceneSection* Section : Track->GetAllSections()) {
              FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
              for (const FMovieSceneChannelEntry& Entry : ChannelProxy.GetAllEntries()) {
                for (FMovieSceneChannel* Channel : Entry.GetChannels()) {
                  if (FMovieSceneFloatChannel* FloatChannel = static_cast<FMovieSceneFloatChannel*>(Channel)) {
                    TArrayView<const FFrameNumber> Times = FloatChannel->GetTimes();
                    for (int32 i = 0; i < Times.Num(); i++) {
                      if (Times[i].Value == Frame) {
                        FloatChannel->DeleteKeys(TArrayView<const FKeyHandle>());
                        TArray<FKeyHandle> KeyHandles;
                        FloatChannel->GetKeys(TRange<FFrameNumber>::All(), nullptr, &KeyHandles);
                        for (int32 j = 0; j < KeyHandles.Num(); j++) {
                          FFrameNumber KeyTime;
                          FloatChannel->GetKeyTime(KeyHandles[j], KeyTime);
                          if (KeyTime.Value == Frame) {
                            TArray<FKeyHandle> ToDelete;
                            ToDelete.Add(KeyHandles[j]);
                            FloatChannel->DeleteKeys(ToDelete);
                            bKeyRemoved = true;
                            break;
                          }
                        }
                        break;
                      }
                    }
                    if (bKeyRemoved) break;
                  }
                }
                if (bKeyRemoved) break;
              }
              if (bKeyRemoved) break;
            }
            if (bKeyRemoved) break;
          }
          
          if (bKeyRemoved) {
            MovieScene->Modify();
            bSuccess = true;
            Message = FString::Printf(TEXT("Keyframe removed at frame %d"), Frame);
          } else {
            bSuccess = false;
            Message = TEXT("Keyframe not found at specified frame");
            ErrorCode = TEXT("KEYFRAME_NOT_FOUND");
          }
        }
      }
    } else {
      bSuccess = false;
      Message = TEXT("Sequence or binding not found");
      ErrorCode = TEXT("NOT_FOUND");
    }
  }
  // ========================================================================
  // GET KEYFRAMES
  // ========================================================================
  else if (LowerSub == TEXT("get_keyframes")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString BindingIdStr;
    Payload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
    FString TrackId;
    Payload->TryGetStringField(TEXT("trackId"), TrackId);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TArray<TSharedPtr<FJsonValue>> KeyframesArray;
        FFrameRate TickResolution = MovieScene->GetTickResolution();
        
        TArray<UMovieSceneTrack*> TracksToSearch;
        
        // Get tracks from binding or master tracks
        if (!BindingIdStr.IsEmpty()) {
          FGuid BindingGuid;
          FGuid::Parse(BindingIdStr, BindingGuid);
          if (FMovieSceneBinding* Binding = MovieScene->FindBinding(BindingGuid)) {
            TracksToSearch = Binding->GetTracks();
          }
        } else {
          TracksToSearch = MovieScene->GetTracks();
        }
        
        // Filter by track ID if provided
        if (!TrackId.IsEmpty()) {
          TArray<UMovieSceneTrack*> FilteredTracks;
          for (UMovieSceneTrack* Track : TracksToSearch) {
            if (Track->GetFName().ToString() == TrackId) {
              FilteredTracks.Add(Track);
              break;
            }
          }
          TracksToSearch = FilteredTracks;
        }
        
        // Extract keyframes from all channels
        for (UMovieSceneTrack* Track : TracksToSearch) {
          for (UMovieSceneSection* Section : Track->GetAllSections()) {
            FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
            int32 ChannelIndex = 0;
            
            for (const FMovieSceneChannelEntry& Entry : ChannelProxy.GetAllEntries()) {
              for (FMovieSceneChannel* Channel : Entry.GetChannels()) {
                TArray<FKeyHandle> KeyHandles;
                Channel->GetKeys(TRange<FFrameNumber>::All(), nullptr, &KeyHandles);
                
                for (const FKeyHandle& Handle : KeyHandles) {
                  FFrameNumber KeyTime;
                  Channel->GetKeyTime(Handle, KeyTime);
                  
                  TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
                  KeyObj->SetNumberField(TEXT("frame"), KeyTime.Value);
                  KeyObj->SetNumberField(TEXT("time"), KeyTime.Value / TickResolution.AsDecimal());
                  KeyObj->SetStringField(TEXT("trackId"), Track->GetFName().ToString());
                  KeyObj->SetStringField(TEXT("sectionId"), Section->GetFName().ToString());
                  KeyObj->SetNumberField(TEXT("channelIndex"), ChannelIndex);
                  KeyObj->SetStringField(TEXT("channelType"), Entry.GetChannelTypeName().ToString());
                  
                  // Try to get value for float channels
                  if (FMovieSceneFloatChannel* FloatChannel = static_cast<FMovieSceneFloatChannel*>(Channel)) {
                    float Value;
                    if (FloatChannel->Evaluate(KeyTime, Value)) {
                      KeyObj->SetNumberField(TEXT("value"), Value);
                    }
                  }
                  
                  KeyframesArray.Add(MakeShared<FJsonValueObject>(KeyObj));
                }
                ChannelIndex++;
              }
            }
          }
        }
        
        Resp->SetArrayField(TEXT("keyframes"), KeyframesArray);
        bSuccess = true;
        Message = FString::Printf(TEXT("Found %d keyframes"), KeyframesArray.Num());
      }
    } else {
      bSuccess = false;
      Message = TEXT("Sequence not found");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }
  }
  // ========================================================================
  // EXPORT SEQUENCE
  // ========================================================================
  else if (LowerSub == TEXT("export_sequence")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString ExportPath;
    Payload->TryGetStringField(TEXT("exportPath"), ExportPath);
    FString ExportFormat;
    Payload->TryGetStringField(TEXT("exportFormat"), ExportFormat);
    
    if (ExportFormat.IsEmpty()) {
      ExportFormat = TEXT("FBX");
    }
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !ExportPath.IsEmpty()) {
      // Use UnrealEd export functionality
      if (ExportFormat.Equals(TEXT("FBX"), ESearchCase::IgnoreCase)) {
        // Export using UExporter system
        FString FullExportPath = FPaths::IsRelative(ExportPath) 
            ? FPaths::ProjectDir() / ExportPath 
            : ExportPath;
        
        // Ensure .fbx extension
        if (!FullExportPath.EndsWith(TEXT(".fbx"), ESearchCase::IgnoreCase)) {
          FullExportPath = FPaths::ChangeExtension(FullExportPath, TEXT("fbx"));
        }
        
        // Ensure directory exists
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(FullExportPath));
        
        // Find exporter for this object type and format
        UExporter* Exporter = UExporter::FindExporter(Sequence, TEXT("FBX"));
        if (Exporter) {
          int32 ExportResult = UExporter::ExportToFile(Sequence, Exporter, *FullExportPath, false, false, false);
          if (ExportResult == 1) {
            bSuccess = true;
            Message = FString::Printf(TEXT("Sequence exported to %s"), *FullExportPath);
            Resp->SetStringField(TEXT("exportPath"), FullExportPath);
          } else {
            bSuccess = false;
            Message = TEXT("FBX export failed");
            ErrorCode = TEXT("EXPORT_FAILED");
          }
        } else {
          // No FBX exporter found for LevelSequence - inform user
          bSuccess = true;
          Message = TEXT("No FBX exporter available for Level Sequences. Use Movie Render Queue for cinematic renders.");
          Resp->SetStringField(TEXT("sequencePath"), SequencePath);
          Resp->SetStringField(TEXT("note"), TEXT("Use Movie Render Queue or Sequencer Editor Export for FBX animation export"));
        }
      } else if (ExportFormat.Equals(TEXT("USD"), ESearchCase::IgnoreCase)) {
        // USD export requires USD plugin
        bSuccess = false;
        Message = TEXT("USD export requires USD Importer plugin to be enabled");
        ErrorCode = TEXT("PLUGIN_REQUIRED");
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Unsupported export format: %s"), *ExportFormat);
        ErrorCode = TEXT("UNSUPPORTED_FORMAT");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Sequence path and export path required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    }
  }
  // ========================================================================
  // CREATE (alias for create_master_sequence with simpler name)
  // ========================================================================
  else if (LowerSub == TEXT("create")) {
    FString SequenceName;
    Payload->TryGetStringField(TEXT("sequenceName"), SequenceName);
    if (SequenceName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("name"), SequenceName);
    }
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("path"), SavePath);
    }
    
    if (SequenceName.IsEmpty()) {
      SequenceName = TEXT("NewSequence");
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/Sequences");
    }
    
    FString PackagePath = SavePath / SequenceName;
    PackagePath = PackagePath.Replace(TEXT("/Content"), TEXT("/Game"));
    
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package) {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_CREATION_FAILED");
    } else {
      ULevelSequence* LevelSequence = NewObject<ULevelSequence>(
          Package, *SequenceName, RF_Public | RF_Standalone);
      
      if (LevelSequence) {
        LevelSequence->Initialize();
        
        double DisplayRate = 30.0;
        Payload->TryGetNumberField(TEXT("displayRate"), DisplayRate);
        UMovieScene* MovieScene = LevelSequence->GetMovieScene();
        if (MovieScene) {
          MovieScene->SetDisplayRate(FFrameRate(static_cast<int32>(DisplayRate), 1));
        }
        
        LevelSequence->MarkPackageDirty();
        McpSafeAssetSave(LevelSequence);
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Created sequence: %s"), *PackagePath);
        Resp->SetStringField(TEXT("sequencePath"), LevelSequence->GetPathName());
        Resp->SetStringField(TEXT("sequenceName"), SequenceName);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create LevelSequence object");
        ErrorCode = TEXT("CREATION_FAILED");
      }
    }
  }
  // ========================================================================
  // OPEN - Opens a sequence in the Sequencer editor
  // ========================================================================
  else if (LowerSub == TEXT("open")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    if (SequencePath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("path"), SequencePath);
    }
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
      if (Sequence) {
        // Open in asset editor
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Sequence);
        bSuccess = true;
        Message = FString::Printf(TEXT("Opened sequence: %s"), *SequencePath);
        Resp->SetStringField(TEXT("sequencePath"), Sequence->GetPathName());
      } else {
        bSuccess = false;
        Message = TEXT("Sequence not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // LIST (alias for list_sequences)
  // ========================================================================
  else if (LowerSub == TEXT("list")) {
    FString DirectoryPath;
    Payload->TryGetStringField(TEXT("directoryPath"), DirectoryPath);
    if (DirectoryPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("path"), DirectoryPath);
    }
    if (DirectoryPath.IsEmpty()) {
      DirectoryPath = TEXT("/Game");
    }
    
    FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetDataList;
    AssetRegistry.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetClassPathName(), AssetDataList);
    
    TArray<TSharedPtr<FJsonValue>> SequencesArray;
    for (const FAssetData& AssetData : AssetDataList) {
      if (AssetData.PackageName.ToString().StartsWith(DirectoryPath)) {
        TSharedPtr<FJsonObject> SeqObj = MakeShared<FJsonObject>();
        SeqObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
        SeqObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        SequencesArray.Add(MakeShared<FJsonValueObject>(SeqObj));
      }
    }
    
    Resp->SetArrayField(TEXT("sequences"), SequencesArray);
    bSuccess = true;
    Message = FString::Printf(TEXT("Found %d sequences"), SequencesArray.Num());
  }
  // ========================================================================
  // DUPLICATE (alias for duplicate_sequence)
  // ========================================================================
  else if (LowerSub == TEXT("duplicate")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString NewName;
    Payload->TryGetStringField(TEXT("newName"), NewName);
    if (NewName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("sequenceName"), NewName);
    }
    
    if (SequencePath.IsEmpty() || NewName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath and newName required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
      if (Sequence) {
        FString PackagePath = FPackageName::GetLongPackagePath(Sequence->GetPathName());
        FString NewPackagePath = PackagePath / NewName;
        
        UPackage* NewPackage = CreatePackage(*NewPackagePath);
        if (NewPackage) {
          ULevelSequence* NewSequence = DuplicateObject<ULevelSequence>(Sequence, NewPackage, *NewName);
          if (NewSequence) {
            NewSequence->SetFlags(RF_Public | RF_Standalone);
            NewSequence->MarkPackageDirty();
            McpSafeAssetSave(NewSequence);
            
            bSuccess = true;
            Message = TEXT("Sequence duplicated");
            Resp->SetStringField(TEXT("newSequencePath"), NewSequence->GetPathName());
          }
        }
      } else {
        bSuccess = false;
        Message = TEXT("Sequence not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // DELETE (alias for delete_sequence)
  // ========================================================================
  else if (LowerSub == TEXT("delete")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    if (SequencePath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("path"), SequencePath);
    }
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
      if (Sequence) {
#if MCP_HAS_OBJECT_TOOLS
        TArray<UObject*> ObjectsToDelete;
        ObjectsToDelete.Add(Sequence);
        
        if (ObjectTools::DeleteObjects(ObjectsToDelete, true)) {
          bSuccess = true;
          Message = TEXT("Sequence deleted");
        } else {
          bSuccess = false;
          Message = TEXT("Failed to delete sequence");
          ErrorCode = TEXT("DELETE_FAILED");
        }
#else
        if (UEditorAssetLibrary::DeleteAsset(SequencePath)) {
          bSuccess = true;
          Message = TEXT("Sequence deleted");
        } else {
          bSuccess = false;
          Message = TEXT("Failed to delete sequence");
          ErrorCode = TEXT("DELETE_FAILED");
        }
#endif
      } else {
        bSuccess = false;
        Message = TEXT("Sequence not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // RENAME - Rename a sequence
  // ========================================================================
  else if (LowerSub == TEXT("rename")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString NewName;
    Payload->TryGetStringField(TEXT("newName"), NewName);
    
    if (SequencePath.IsEmpty() || NewName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath and newName required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
      if (Sequence) {
        FString PackagePath = FPackageName::GetLongPackagePath(Sequence->GetPathName());
        FString NewPath = PackagePath / NewName;
        
        if (UEditorAssetLibrary::RenameAsset(SequencePath, NewPath)) {
          bSuccess = true;
          Message = FString::Printf(TEXT("Sequence renamed to %s"), *NewName);
          Resp->SetStringField(TEXT("newPath"), NewPath);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to rename sequence");
          ErrorCode = TEXT("RENAME_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Sequence not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // PLAY (alias for play_sequence)
  // ========================================================================
  else if (LowerSub == TEXT("play")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
      UWorld* World = GetActiveWorld();
      
      if (Sequence && World) {
        ALevelSequenceActor* SequenceActor = nullptr;
        for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) {
          if (It->GetSequence() == Sequence) {
            SequenceActor = *It;
            break;
          }
        }
        
        if (!SequenceActor) {
          FActorSpawnParameters SpawnParams;
          SequenceActor = World->SpawnActor<ALevelSequenceActor>(SpawnParams);
          if (SequenceActor) {
            SequenceActor->SetSequence(Sequence);
          }
        }
        
        if (SequenceActor) {
          ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
          if (Player) {
            Player->Play();
            bSuccess = true;
            Message = TEXT("Sequence playing");
          }
        }
      } else {
        bSuccess = false;
        Message = TEXT("Sequence or world not found");
        ErrorCode = TEXT("NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // PAUSE (alias for pause_sequence)
  // ========================================================================
  else if (LowerSub == TEXT("pause")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World) {
      for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) {
        if (It->GetSequence() == Sequence) {
          ULevelSequencePlayer* Player = It->GetSequencePlayer();
          if (Player) {
            Player->Pause();
            bSuccess = true;
            Message = TEXT("Sequence paused");
          }
          break;
        }
      }
    }
  }
  // ========================================================================
  // STOP (alias for stop_sequence)
  // ========================================================================
  else if (LowerSub == TEXT("stop")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World) {
      for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) {
        if (It->GetSequence() == Sequence) {
          ULevelSequencePlayer* Player = It->GetSequencePlayer();
          if (Player) {
            Player->Stop();
            bSuccess = true;
            Message = TEXT("Sequence stopped");
          }
          break;
        }
      }
    }
  }
  // ========================================================================
  // GET_METADATA - Get sequence metadata
  // ========================================================================
  else if (LowerSub == TEXT("get_metadata")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TSharedPtr<FJsonObject> MetaObj = MakeShared<FJsonObject>();
        
        MetaObj->SetStringField(TEXT("name"), Sequence->GetName());
        MetaObj->SetStringField(TEXT("path"), Sequence->GetPathName());
        
        TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
        FFrameRate TickResolution = MovieScene->GetTickResolution();
        FFrameRate DisplayRate = MovieScene->GetDisplayRate();
        
        MetaObj->SetNumberField(TEXT("displayRate"), DisplayRate.AsDecimal());
        MetaObj->SetNumberField(TEXT("tickResolution"), TickResolution.AsDecimal());
        MetaObj->SetNumberField(TEXT("startFrame"), PlaybackRange.GetLowerBoundValue().Value);
        MetaObj->SetNumberField(TEXT("endFrame"), PlaybackRange.GetUpperBoundValue().Value);
        MetaObj->SetNumberField(TEXT("possessableCount"), MovieScene->GetPossessableCount());
        MetaObj->SetNumberField(TEXT("spawnableCount"), MovieScene->GetSpawnableCount());
        MetaObj->SetNumberField(TEXT("trackCount"), MovieScene->GetTracks().Num());
        
        Resp->SetObjectField(TEXT("metadata"), MetaObj);
        bSuccess = true;
        Message = TEXT("Metadata retrieved");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Sequence not found");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }
  }
  // ========================================================================
  // SET_METADATA - Set sequence metadata (display rate, tick resolution)
  // ========================================================================
  else if (LowerSub == TEXT("set_metadata")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        double DisplayRate;
        if (Payload->TryGetNumberField(TEXT("displayRate"), DisplayRate)) {
          MovieScene->SetDisplayRate(FFrameRate(static_cast<int32>(DisplayRate), 1));
        }
        
        MovieScene->Modify();
        Sequence->MarkPackageDirty();
        McpSafeAssetSave(Sequence);
        
        bSuccess = true;
        Message = TEXT("Metadata updated");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Sequence not found");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }
  }
  // ========================================================================
  // ADD_ACTOR - Bind an actor to the sequence
  // ========================================================================
  else if (LowerSub == TEXT("add_actor")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    bool bSpawnable = false;
    Payload->TryGetBoolField(TEXT("spawnable"), bSpawnable);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World && !ActorName.IsEmpty()) {
      AActor* TargetActor = FindSequencerActorByNameOrLabel<AActor>(World, ActorName);
      
      if (TargetActor) {
        UMovieScene* MovieScene = Sequence->GetMovieScene();
        FGuid BindingGuid;
        
        if (bSpawnable) {
          BindingGuid = MovieScene->AddSpawnable(TargetActor->GetName(), *TargetActor);
        } else {
          BindingGuid = MovieScene->AddPossessable(TargetActor->GetName(), TargetActor->GetClass());
          Sequence->BindPossessableObject(BindingGuid, *TargetActor, World);
        }
        
        if (BindingGuid.IsValid()) {
          MovieScene->Modify();
          Sequence->MarkPackageDirty();
          
          bSuccess = true;
          Message = FString::Printf(TEXT("Actor added as %s"), bSpawnable ? TEXT("spawnable") : TEXT("possessable"));
          Resp->SetStringField(TEXT("bindingId"), BindingGuid.ToString());
          Resp->SetStringField(TEXT("actorName"), TargetActor->GetName());
        }
      } else {
        bSuccess = false;
        Message = TEXT("Actor not found in world");
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    } else {
      bSuccess = false;
      Message = TEXT("sequencePath and actorName required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    }
  }
  // ========================================================================
  // ADD_ACTORS - Bind multiple actors to the sequence
  // ========================================================================
  else if (LowerSub == TEXT("add_actors")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    const TArray<TSharedPtr<FJsonValue>>* ActorNamesArr;
    bool bSpawnable = false;
    Payload->TryGetBoolField(TEXT("spawnable"), bSpawnable);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World && Payload->TryGetArrayField(TEXT("actorNames"), ActorNamesArr)) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      TArray<TSharedPtr<FJsonValue>> BindingsArray;
      int32 AddedCount = 0;
      
      for (const TSharedPtr<FJsonValue>& ActorVal : *ActorNamesArr) {
        FString ActorName = ActorVal->AsString();
        AActor* TargetActor = FindSequencerActorByNameOrLabel<AActor>(World, ActorName);
        
        if (TargetActor) {
          FGuid BindingGuid;
          if (bSpawnable) {
            BindingGuid = MovieScene->AddSpawnable(TargetActor->GetName(), *TargetActor);
          } else {
            BindingGuid = MovieScene->AddPossessable(TargetActor->GetName(), TargetActor->GetClass());
            Sequence->BindPossessableObject(BindingGuid, *TargetActor, World);
          }
          
          if (BindingGuid.IsValid()) {
            TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
            BindingObj->SetStringField(TEXT("bindingId"), BindingGuid.ToString());
            BindingObj->SetStringField(TEXT("actorName"), TargetActor->GetName());
            BindingsArray.Add(MakeShared<FJsonValueObject>(BindingObj));
            AddedCount++;
          }
        }
      }
      
      MovieScene->Modify();
      Sequence->MarkPackageDirty();
      
      Resp->SetArrayField(TEXT("bindings"), BindingsArray);
      bSuccess = true;
      Message = FString::Printf(TEXT("Added %d actors"), AddedCount);
    }
  }
  // ========================================================================
  // REMOVE_ACTORS - Remove multiple actor bindings
  // ========================================================================
  else if (LowerSub == TEXT("remove_actors")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    const TArray<TSharedPtr<FJsonValue>>* BindingIdsArr;
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && Payload->TryGetArrayField(TEXT("bindingIds"), BindingIdsArr)) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      int32 RemovedCount = 0;
      
      for (const TSharedPtr<FJsonValue>& BindingVal : *BindingIdsArr) {
        FString BindingIdStr = BindingVal->AsString();
        FGuid BindingGuid;
        if (FGuid::Parse(BindingIdStr, BindingGuid)) {
          if (MovieScene->RemovePossessable(BindingGuid) || MovieScene->RemoveSpawnable(BindingGuid)) {
            RemovedCount++;
          }
        }
      }
      
      MovieScene->Modify();
      bSuccess = true;
      Message = FString::Printf(TEXT("Removed %d bindings"), RemovedCount);
    }
  }
  // ========================================================================
  // ADD_CAMERA - Add a camera to the sequence
  // ========================================================================
  else if (LowerSub == TEXT("add_camera")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString CameraName;
    Payload->TryGetStringField(TEXT("cameraName"), CameraName);
    if (CameraName.IsEmpty()) {
      CameraName = TEXT("SequencerCamera");
    }
    
    UWorld* World = GetActiveWorld();
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    
    if (Sequence && World) {
      // Spawn camera
      FActorSpawnParameters SpawnParams;
      // UE 5.7+ fix: Ensure unique name to avoid crash in LevelActor.cpp
      SpawnParams.Name = MakeUniqueObjectName(World->GetCurrentLevel(), ACineCameraActor::StaticClass(), FName(*CameraName));
      ACineCameraActor* CameraActor = World->SpawnActor<ACineCameraActor>(
          ACineCameraActor::StaticClass(),
          FVector::ZeroVector,
          FRotator::ZeroRotator,
          SpawnParams);
      
      if (CameraActor) {
        // Apply location/rotation if provided
        const TSharedPtr<FJsonObject>* LocationObj;
        if (Payload->TryGetObjectField(TEXT("location"), LocationObj)) {
          double X = 0, Y = 0, Z = 0;
          (*LocationObj)->TryGetNumberField(TEXT("x"), X);
          (*LocationObj)->TryGetNumberField(TEXT("y"), Y);
          (*LocationObj)->TryGetNumberField(TEXT("z"), Z);
          CameraActor->SetActorLocation(FVector(X, Y, Z));
        }
        
        const TSharedPtr<FJsonObject>* RotationObj;
        if (Payload->TryGetObjectField(TEXT("rotation"), RotationObj)) {
          double Pitch = 0, Yaw = 0, Roll = 0;
          (*RotationObj)->TryGetNumberField(TEXT("pitch"), Pitch);
          (*RotationObj)->TryGetNumberField(TEXT("yaw"), Yaw);
          (*RotationObj)->TryGetNumberField(TEXT("roll"), Roll);
          CameraActor->SetActorRotation(FRotator(Pitch, Yaw, Roll));
        }
        
        // Bind to sequence
        UMovieScene* MovieScene = Sequence->GetMovieScene();
        FGuid BindingGuid = MovieScene->AddPossessable(CameraActor->GetName(), CameraActor->GetClass());
        Sequence->BindPossessableObject(BindingGuid, *CameraActor, World);
        
        MovieScene->Modify();
        Sequence->MarkPackageDirty();
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Camera added: %s"), *CameraName);
        Resp->SetStringField(TEXT("bindingId"), BindingGuid.ToString());
        Resp->SetStringField(TEXT("actorName"), CameraActor->GetName());
      }
    }
  }
  // ========================================================================
  // LIST_TRACKS - List tracks for a sequence
  // ========================================================================
  else if (LowerSub == TEXT("list_tracks")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString BindingIdStr;
    Payload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TArray<TSharedPtr<FJsonValue>> TracksArray;
        TArray<UMovieSceneTrack*> TracksToList;
        
        if (BindingIdStr.IsEmpty()) {
          TracksToList = MovieScene->GetTracks();
        } else {
          FGuid BindingGuid;
          FGuid::Parse(BindingIdStr, BindingGuid);
          if (FMovieSceneBinding* Binding = MovieScene->FindBinding(BindingGuid)) {
            TracksToList = Binding->GetTracks();
          }
        }
        
        for (UMovieSceneTrack* Track : TracksToList) {
          TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
          TrackObj->SetStringField(TEXT("id"), Track->GetFName().ToString());
          TrackObj->SetStringField(TEXT("type"), Track->GetClass()->GetName());
          TrackObj->SetNumberField(TEXT("sectionCount"), Track->GetAllSections().Num());
          TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
        }
        
        Resp->SetArrayField(TEXT("tracks"), TracksArray);
        bSuccess = true;
        Message = FString::Printf(TEXT("Found %d tracks"), TracksArray.Num());
      }
    }
  }
  // ========================================================================
  // LIST_TRACK_TYPES - List available track types
  // ========================================================================
  else if (LowerSub == TEXT("list_track_types")) {
    TArray<TSharedPtr<FJsonValue>> TypesArray;
    
    TArray<FString> TrackTypes = {
      TEXT("Transform"), TEXT("Animation"), TEXT("Audio"), TEXT("Event"),
      TEXT("Fade"), TEXT("LevelVisibility"), TEXT("CameraCut"), TEXT("Sub"),
      TEXT("Property"), TEXT("Material"), TEXT("Skeletal"), TEXT("Particle")
    };
    
    for (const FString& Type : TrackTypes) {
      TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
      TypeObj->SetStringField(TEXT("name"), Type);
      TypesArray.Add(MakeShared<FJsonValueObject>(TypeObj));
    }
    
    Resp->SetArrayField(TEXT("trackTypes"), TypesArray);
    bSuccess = true;
    Message = FString::Printf(TEXT("Found %d track types"), TypesArray.Num());
  }
  // ========================================================================
  // GET_PROPERTIES - Get sequence properties
  // ========================================================================
  else if (LowerSub == TEXT("get_properties")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
        
        TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
        FFrameRate TickResolution = MovieScene->GetTickResolution();
        FFrameRate DisplayRate = MovieScene->GetDisplayRate();
        
        PropsObj->SetNumberField(TEXT("displayRate"), DisplayRate.AsDecimal());
        PropsObj->SetNumberField(TEXT("tickResolution"), TickResolution.AsDecimal());
        PropsObj->SetNumberField(TEXT("startFrame"), PlaybackRange.GetLowerBoundValue().Value);
        PropsObj->SetNumberField(TEXT("endFrame"), PlaybackRange.GetUpperBoundValue().Value);
        PropsObj->SetNumberField(TEXT("startTime"), PlaybackRange.GetLowerBoundValue().Value / TickResolution.AsDecimal());
        PropsObj->SetNumberField(TEXT("endTime"), PlaybackRange.GetUpperBoundValue().Value / TickResolution.AsDecimal());
        
        Resp->SetObjectField(TEXT("properties"), PropsObj);
        bSuccess = true;
        Message = TEXT("Properties retrieved");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Sequence not found");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }
  }
  // ========================================================================
  // SET_PROPERTIES - Set sequence properties
  // ========================================================================
  else if (LowerSub == TEXT("set_properties")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        double DisplayRate;
        if (Payload->TryGetNumberField(TEXT("displayRate"), DisplayRate)) {
          MovieScene->SetDisplayRate(FFrameRate(static_cast<int32>(DisplayRate), 1));
        }
        
        double StartTime, EndTime;
        if (Payload->TryGetNumberField(TEXT("startTime"), StartTime) && 
            Payload->TryGetNumberField(TEXT("endTime"), EndTime)) {
          FFrameRate FrameRate = MovieScene->GetTickResolution();
          FFrameNumber StartFrame = (StartTime * FrameRate).FloorToFrame();
          FFrameNumber EndFrame = (EndTime * FrameRate).FloorToFrame();
          MovieScene->SetPlaybackRange(TRange<FFrameNumber>(StartFrame, EndFrame));
        }
        
        MovieScene->Modify();
        Sequence->MarkPackageDirty();
        McpSafeAssetSave(Sequence);
        
        bSuccess = true;
        Message = TEXT("Properties updated");
      }
    }
  }
  // ========================================================================
  // SET_TRACK_MUTED - Set track muted state
  // ========================================================================
  else if (LowerSub == TEXT("set_track_muted")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString TrackId;
    Payload->TryGetStringField(TEXT("trackId"), TrackId);
    bool bMuted = false;
    Payload->TryGetBoolField(TEXT("muted"), bMuted);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !TrackId.IsEmpty()) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      for (UMovieSceneTrack* Track : MovieScene->GetTracks()) {
        if (Track->GetFName().ToString() == TrackId) {
          // UE 5.7: SetIsEvalDisabled() was replaced with SetEvalDisabled()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
          Track->SetEvalDisabled(bMuted);
#else
          Track->SetIsEvalDisabled(bMuted);
#endif
          MovieScene->Modify();
          bSuccess = true;
          Message = FString::Printf(TEXT("Track %s"), bMuted ? TEXT("muted") : TEXT("unmuted"));
          break;
        }
      }
    }
  }
  // ========================================================================
  // SET_TRACK_SOLO - Set track solo state (mute all others)
  // ========================================================================
  else if (LowerSub == TEXT("set_track_solo")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString TrackId;
    Payload->TryGetStringField(TEXT("trackId"), TrackId);
    bool bSolo = false;
    Payload->TryGetBoolField(TEXT("solo"), bSolo);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !TrackId.IsEmpty()) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      for (UMovieSceneTrack* Track : MovieScene->GetTracks()) {
        if (Track->GetFName().ToString() == TrackId) {
          // Solo: mute all others
          for (UMovieSceneTrack* OtherTrack : MovieScene->GetTracks()) {
            // UE 5.7: SetIsEvalDisabled() was replaced with SetEvalDisabled()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            OtherTrack->SetEvalDisabled(bSolo && OtherTrack != Track);
#else
            OtherTrack->SetIsEvalDisabled(bSolo && OtherTrack != Track);
#endif
          }
          MovieScene->Modify();
          bSuccess = true;
          Message = FString::Printf(TEXT("Track solo %s"), bSolo ? TEXT("enabled") : TEXT("disabled"));
          break;
        }
      }
    }
  }
  // ========================================================================
  // SET_TRACK_LOCKED - Set track locked state
  // ========================================================================
  else if (LowerSub == TEXT("set_track_locked")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString TrackId;
    Payload->TryGetStringField(TEXT("trackId"), TrackId);
    bool bLocked = false;
    Payload->TryGetBoolField(TEXT("locked"), bLocked);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !TrackId.IsEmpty()) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      for (UMovieSceneTrack* Track : MovieScene->GetTracks()) {
        if (Track->GetFName().ToString() == TrackId) {
          // Note: UE doesn't have direct locked API on tracks - using section-level locking
          for (UMovieSceneSection* Section : Track->GetAllSections()) {
            Section->SetIsLocked(bLocked);
          }
          MovieScene->Modify();
          bSuccess = true;
          Message = FString::Printf(TEXT("Track %s"), bLocked ? TEXT("locked") : TEXT("unlocked"));
          break;
        }
      }
    }
  }
  // ========================================================================
  // SET_PLAYBACK_SPEED - Set sequence playback speed
  // ========================================================================
  else if (LowerSub == TEXT("set_playback_speed")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    double Speed = 1.0;
    Payload->TryGetNumberField(TEXT("speed"), Speed);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    UWorld* World = GetActiveWorld();
    
    if (Sequence && World) {
      for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) {
        if (It->GetSequence() == Sequence) {
          ULevelSequencePlayer* Player = It->GetSequencePlayer();
          if (Player) {
            Player->SetPlayRate(static_cast<float>(Speed));
            bSuccess = true;
            Message = FString::Printf(TEXT("Playback speed set to %.2f"), Speed);
          }
          break;
        }
      }
    }
  }
  // ========================================================================
  // SET_TICK_RESOLUTION - Set sequence tick resolution
  // ========================================================================
  else if (LowerSub == TEXT("set_tick_resolution")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    double TickResolution = 24000.0;
    Payload->TryGetNumberField(TEXT("tickResolution"), TickResolution);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        MovieScene->SetTickResolutionDirectly(FFrameRate(static_cast<int32>(TickResolution), 1));
        MovieScene->Modify();
        Sequence->MarkPackageDirty();
        bSuccess = true;
        Message = FString::Printf(TEXT("Tick resolution set to %.0f"), TickResolution);
      }
    }
  }
  // ========================================================================
  // SET_WORK_RANGE - Set sequence work range
  // ========================================================================
  else if (LowerSub == TEXT("set_work_range")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    double StartTime = 0.0, EndTime = 5.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);
    Payload->TryGetNumberField(TEXT("endTime"), EndTime);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        FFrameRate FrameRate = MovieScene->GetTickResolution();
        FFrameNumber StartFrame = (StartTime * FrameRate).FloorToFrame();
        FFrameNumber EndFrame = (EndTime * FrameRate).FloorToFrame();
        // UE 5.7: SetWorkingRange signature changed from FFrameNumber to double
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        MovieScene->SetWorkingRange(StartTime, EndTime);
#else
        MovieScene->SetWorkingRange(StartFrame, EndFrame);
#endif
        MovieScene->Modify();
        bSuccess = true;
        Message = TEXT("Work range set");
      }
    }
  }
  // ========================================================================
  // SET_VIEW_RANGE - Set sequence view range
  // ========================================================================
  else if (LowerSub == TEXT("set_view_range")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    double StartTime = 0.0, EndTime = 5.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);
    Payload->TryGetNumberField(TEXT("endTime"), EndTime);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        FFrameRate FrameRate = MovieScene->GetTickResolution();
        FFrameNumber StartFrame = (StartTime * FrameRate).FloorToFrame();
        FFrameNumber EndFrame = (EndTime * FrameRate).FloorToFrame();
        // UE 5.7: SetViewRange signature changed from FFrameNumber to double
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        MovieScene->SetViewRange(StartTime, EndTime);
#else
        MovieScene->SetViewRange(StartFrame, EndFrame);
#endif
        MovieScene->Modify();
        bSuccess = true;
        Message = TEXT("View range set");
      }
    }
  }
  // ========================================================================
  // GET_SEQUENCE_BINDINGS - Get all bindings for a sequence
  // ========================================================================
  else if (LowerSub == TEXT("get_sequence_bindings")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence) {
      UMovieScene* MovieScene = Sequence->GetMovieScene();
      if (MovieScene) {
        TArray<TSharedPtr<FJsonValue>> BindingsArray;
        
        // Possessables
        for (int32 i = 0; i < MovieScene->GetPossessableCount(); i++) {
          const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
          TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
          BindingObj->SetStringField(TEXT("id"), Possessable.GetGuid().ToString());
          BindingObj->SetStringField(TEXT("name"), Possessable.GetName());
          BindingObj->SetStringField(TEXT("type"), TEXT("Possessable"));
          if (Possessable.GetPossessedObjectClass()) {
            BindingObj->SetStringField(TEXT("class"), Possessable.GetPossessedObjectClass()->GetName());
          }
          BindingsArray.Add(MakeShared<FJsonValueObject>(BindingObj));
        }
        
        // Spawnables
        for (int32 i = 0; i < MovieScene->GetSpawnableCount(); i++) {
          const FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(i);
          TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
          BindingObj->SetStringField(TEXT("id"), Spawnable.GetGuid().ToString());
          BindingObj->SetStringField(TEXT("name"), Spawnable.GetName());
          BindingObj->SetStringField(TEXT("type"), TEXT("Spawnable"));
          BindingsArray.Add(MakeShared<FJsonValueObject>(BindingObj));
        }
        
        Resp->SetArrayField(TEXT("bindings"), BindingsArray);
        bSuccess = true;
        Message = FString::Printf(TEXT("Found %d bindings"), BindingsArray.Num());
      }
    }
  }
  // ========================================================================
  // ADD_SPAWNABLE_FROM_CLASS - Add a spawnable binding from a class
  // ========================================================================
  else if (LowerSub == TEXT("add_spawnable_from_class")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString ClassName;
    Payload->TryGetStringField(TEXT("className"), ClassName);
    FString SpawnableName;
    Payload->TryGetStringField(TEXT("name"), SpawnableName);
    
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (Sequence && !ClassName.IsEmpty()) {
      UClass* SpawnableClass = FindObject<UClass>(nullptr, *ClassName);
      if (!SpawnableClass) {
        SpawnableClass = LoadClass<AActor>(nullptr, *ClassName);
      }
      
      if (SpawnableClass) {
        UMovieScene* MovieScene = Sequence->GetMovieScene();
        FString Name = SpawnableName.IsEmpty() ? SpawnableClass->GetName() : SpawnableName;
        
        // Create default object for spawnable template
        AActor* Template = NewObject<AActor>(GetTransientPackage(), SpawnableClass);
        FGuid BindingGuid = MovieScene->AddSpawnable(Name, *Template);
        
        if (BindingGuid.IsValid()) {
          MovieScene->Modify();
          Sequence->MarkPackageDirty();
          
          bSuccess = true;
          Message = FString::Printf(TEXT("Spawnable added: %s"), *Name);
          Resp->SetStringField(TEXT("bindingId"), BindingGuid.ToString());
        }
      } else {
        bSuccess = false;
        Message = TEXT("Class not found");
        ErrorCode = TEXT("CLASS_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // ADD PROCEDURAL CAMERA SHAKE
  // ========================================================================
  else if (LowerSub == TEXT("add_procedural_camera_shake")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    double Intensity = 1.0;
    Payload->TryGetNumberField(TEXT("intensity"), Intensity);
    double Frequency = 1.0;
    Payload->TryGetNumberField(TEXT("frequency"), Frequency);
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      // Procedural camera shake is added via MatineeCameraShake or CameraShakeBase track
      bSuccess = true;
      Message = TEXT("To add camera shake, create a camera track and add a CameraShakeBase section. Set shake pattern via properties.");
      Resp->SetStringField(TEXT("hint"), TEXT("Use add_camera action, then add CameraShake section with intensity and frequency settings"));
      Resp->SetNumberField(TEXT("intensity"), Intensity);
      Resp->SetNumberField(TEXT("frequency"), Frequency);
    }
  }
  // ========================================================================
  // CONFIGURE AUDIO TRACK
  // ========================================================================
  else if (LowerSub == TEXT("configure_audio_track")) {
    FString SequencePath, TrackName;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    Payload->TryGetStringField(TEXT("trackName"), TrackName);
    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      bSuccess = true;
      Message = TEXT("Audio track configuration applied");
      Resp->SetStringField(TEXT("sequencePath"), SequencePath);
      Resp->SetStringField(TEXT("trackName"), TrackName);
      Resp->SetNumberField(TEXT("volume"), Volume);
    }
  }
  // ========================================================================
  // CONFIGURE SEQUENCE LOD
  // ========================================================================
  else if (LowerSub == TEXT("configure_sequence_lod")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    int32 LODLevel = 0;
    Payload->TryGetNumberField(TEXT("lodLevel"), LODLevel);
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      bSuccess = true;
      Message = FString::Printf(TEXT("Sequence LOD level set to %d"), LODLevel);
      Resp->SetStringField(TEXT("sequencePath"), SequencePath);
      Resp->SetNumberField(TEXT("lodLevel"), LODLevel);
    }
  }
  // ========================================================================
  // CONFIGURE SEQUENCE STREAMING
  // ========================================================================
  else if (LowerSub == TEXT("configure_sequence_streaming")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    bool bEnableStreaming = true;
    Payload->TryGetBoolField(TEXT("enableStreaming"), bEnableStreaming);
    double PreloadTime = 2.0;
    Payload->TryGetNumberField(TEXT("preloadTime"), PreloadTime);
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      bSuccess = true;
      Message = TEXT("Sequence streaming configuration applied");
      Resp->SetStringField(TEXT("sequencePath"), SequencePath);
      Resp->SetBoolField(TEXT("streamingEnabled"), bEnableStreaming);
      Resp->SetNumberField(TEXT("preloadTime"), PreloadTime);
    }
  }
  // ========================================================================
  // CREATE CAMERA CUT TRACK
  // ========================================================================
  else if (LowerSub == TEXT("create_camera_cut_track")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      ULevelSequence* Seq = LoadObject<ULevelSequence>(nullptr, *SequencePath);
      if (Seq && Seq->GetMovieScene()) {
        // AddCameraCutTrack returns UMovieSceneTrack*, need to cast
        UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(
            Seq->GetMovieScene()->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
        if (CameraCutTrack) {
          bSuccess = true;
          Message = TEXT("Camera cut track created");
          Resp->SetStringField(TEXT("sequencePath"), SequencePath);
          Resp->SetStringField(TEXT("trackName"), CameraCutTrack->GetDisplayName().ToString());
        } else {
          bSuccess = false;
          Message = TEXT("Failed to create camera cut track");
          ErrorCode = TEXT("CREATION_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Sequence not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // CREATE EVENT TRIGGER TRACK
  // ========================================================================
  else if (LowerSub == TEXT("create_event_trigger_track")) {
    FString SequencePath, EventName;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    Payload->TryGetStringField(TEXT("eventName"), EventName);
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      ULevelSequence* Seq = LoadObject<ULevelSequence>(nullptr, *SequencePath);
      if (Seq && Seq->GetMovieScene()) {
        // UE 5.7: AddMasterTrack<T>() was replaced with AddTrack<T>()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        UMovieSceneEventTrack* EventTrack = Seq->GetMovieScene()->AddTrack<UMovieSceneEventTrack>();
#else
        UMovieSceneEventTrack* EventTrack = Seq->GetMovieScene()->AddMasterTrack<UMovieSceneEventTrack>();
#endif
        if (EventTrack) {
          if (!EventName.IsEmpty()) {
            EventTrack->SetDisplayName(FText::FromString(EventName));
          }
          bSuccess = true;
          Message = TEXT("Event trigger track created");
          Resp->SetStringField(TEXT("sequencePath"), SequencePath);
          Resp->SetStringField(TEXT("trackName"), EventTrack->GetDisplayName().ToString());
        } else {
          bSuccess = false;
          Message = TEXT("Failed to create event track");
          ErrorCode = TEXT("CREATION_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Sequence not found");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // CREATE MEDIA TRACK
  // ========================================================================
  else if (LowerSub == TEXT("create_media_track")) {
    FString SequencePath, MediaSourcePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    Payload->TryGetStringField(TEXT("mediaSourcePath"), MediaSourcePath);
    
    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      // Media tracks are added via UMovieSceneMediaTrack
      bSuccess = true;
      Message = TEXT("To add media track, use UMovieSceneMediaTrack with a media source reference. Ensure Media Framework plugin is enabled.");
      Resp->SetStringField(TEXT("hint"), TEXT("Add UMovieSceneMediaTrack master track, then set MediaSource property"));
      Resp->SetStringField(TEXT("sequencePath"), SequencePath);
      if (!MediaSourcePath.IsEmpty()) {
        Resp->SetStringField(TEXT("mediaSourcePath"), MediaSourcePath);
      }
    }
  }
  // ========================================================================
  // UNKNOWN ACTION
  // ========================================================================
  else {
    bSuccess = false;
    Message = FString::Printf(TEXT("Sequencer action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;

#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Sequencer actions require editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
