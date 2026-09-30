#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/PhysicsAsset.h"

namespace McpAnimationHandlers {
bool HandleAnimationSetupPhysicsSimulationAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString SkeletonPath;
    Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

    FString SkeletalMeshPath;
    Payload->TryGetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);

    // Support actorName parameter to find skeletal mesh from a spawned actor
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    const bool bSkeletonProvided = !SkeletonPath.IsEmpty();
    const bool bSkeletalMeshProvided = !SkeletalMeshPath.IsEmpty();
    const bool bActorProvided = !ActorName.IsEmpty();

    bool bSkeletonLoadFailed = false;
    bool bSkeletonMissingPreview = false;
    bool bSkeletalMeshLoadFailed = false;
    bool bSkeletalMeshTypeMismatch = false;
    FString FoundClassName;

    USkeletalMesh *TargetMesh = nullptr;

    if (!bSkeletonProvided && !bSkeletalMeshProvided && !bActorProvided) {
      Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("setup_physics_simulation requires skeletonPath, skeletalMeshPath, or actorName"));
      Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false, Message, Resp, ErrorCode);
      return true;
    }

    // If actorName provided, try to find the actor and get its skeletal mesh
    if (!bSkeletonProvided && !bSkeletalMeshProvided && bActorProvided) {
		AActor *FoundActor = Context.FindActorByName(ActorName);
	if (FoundActor) {
			// Try to get skeletal mesh component
        if (USkeletalMeshComponent *SkelComp =
                FoundActor->FindComponentByClass<USkeletalMeshComponent>()) {
#if ENGINE_MINOR_VERSION >= 1
          TargetMesh = SkelComp->GetSkeletalMeshAsset();
#else
          TargetMesh = SkelComp->SkeletalMesh;
#endif
			if (!TargetMesh) {
            Message =
                FString::Printf(TEXT("Actor '%s' has a SkeletalMeshComponent "
                                     "but no SkeletalMesh asset assigned."),
                                *FoundActor->GetName());
            ErrorCode = TEXT("ACTOR_SKELETAL_MESH_ASSET_NULL");
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"),
                   *Message);
          }
        } else {
          Message = FString::Printf(
              TEXT("Actor '%s' does not have a SkeletalMeshComponent."),
              *FoundActor->GetName());
          ErrorCode = TEXT("ACTOR_NO_SKELETAL_MESH_COMPONENT");
          UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *Message);
        }
      } else {
        Message = FString::Printf(TEXT("Actor '%s' not found."), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *Message);
      }

      if (!TargetMesh) {
        Resp->SetStringField(TEXT("actorName"), ActorName);
        Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false, Message, Resp, ErrorCode);
        return true;
      }
    }

    if (!TargetMesh && bSkeletalMeshProvided) {
      if (McpAssetExists(SkeletalMeshPath)) {
        UObject *Asset = McpLoadAsset(SkeletalMeshPath);
        TargetMesh = Cast<USkeletalMesh>(Asset);
        if (!TargetMesh && Asset) {
          bSkeletalMeshTypeMismatch = true;
          FoundClassName = Asset->GetClass()->GetName();
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("setup_physics_simulation: Asset %s is not a SkeletalMesh (Class: %s)"),
                 *SkeletalMeshPath, *FoundClassName);
        } else if (!Asset) {
          bSkeletalMeshLoadFailed = true;
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("setup_physics_simulation: failed to load skeletal mesh asset %s"),
                 *SkeletalMeshPath);
        }
      } else {
        bSkeletalMeshLoadFailed = true;
      }
    }

    USkeleton *TargetSkeleton = nullptr;
    if (!TargetMesh && bSkeletonProvided) {
      if (McpAssetExists(SkeletonPath)) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (TargetSkeleton) {
          TargetMesh = TargetSkeleton->GetPreviewMesh();
          if (!TargetMesh) {
            bSkeletonMissingPreview = true;
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                   TEXT("setup_physics_simulation: skeleton %s has no preview "
                        "mesh"),
                   *SkeletonPath);
          }
        } else {
          bSkeletonLoadFailed = true;
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("setup_physics_simulation: failed to load skeleton %s"),
                 *SkeletonPath);
        }
      } else {
        bSkeletonLoadFailed = true;
      }
    }

    if (!TargetSkeleton && TargetMesh) {
      TargetSkeleton = TargetMesh->GetSkeleton();
    }

    if (!TargetMesh) {
      if (bSkeletalMeshTypeMismatch) {
        Message = FString::Printf(
            TEXT("asset found but is not a SkeletalMesh: %s (is %s)"),
            *SkeletalMeshPath, *FoundClassName);
        ErrorCode = TEXT("TYPE_MISMATCH");
        Resp->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        Resp->SetStringField(TEXT("actualClass"), FoundClassName);
      } else if (bSkeletalMeshLoadFailed) {
        Message = FString::Printf(TEXT("asset not found: skeletal mesh %s"),
                                  *SkeletalMeshPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
      } else if (bSkeletonLoadFailed) {
        Message = FString::Printf(TEXT("asset not found: skeleton %s"),
                                  *SkeletonPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
      } else if (bSkeletonMissingPreview) {
        Message = FString::Printf(TEXT("asset not found: skeleton %s (no "
                                       "preview mesh for physics simulation)"),
                                  *SkeletonPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
      } else {
        Message = TEXT("asset not found: no valid skeletal mesh provided for "
                       "physics simulation setup");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }

      Resp->SetStringField(TEXT("error"), Message);
    } else {
      if (!TargetSkeleton && !SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      FString PhysicsAssetName;
      Payload->TryGetStringField(TEXT("physicsAssetName"), PhysicsAssetName);
      if (PhysicsAssetName.IsEmpty()) {
        PhysicsAssetName = TargetMesh->GetName() + TEXT("_Physics");
      }

      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Physics");
      }
      SavePath = SavePath.TrimStartAndEnd();

      if (!FPackageName::IsValidLongPackageName(SavePath)) {
        FString NormalizedPath;
        if (!FPackageName::TryConvertFilenameToLongPackageName(
                SavePath, NormalizedPath)) {
          Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("Invalid savePath for physics asset"));
          SavePath.Reset();
        } else {
          SavePath = NormalizedPath;
        }
      }

      if (!SavePath.IsEmpty()) {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        const FString PhysicsAssetObjectPath = FString::Printf(TEXT("%s/%s"), *SavePath, *PhysicsAssetName);

        if (McpAssetExists(PhysicsAssetObjectPath)) {
          bSuccess = true;
          Message = TEXT("Physics simulation already configured - existing asset reused");
          Resp->SetStringField(TEXT("physicsAssetPath"),
                               PhysicsAssetObjectPath);
          Resp->SetBoolField(TEXT("existingAsset"), true);
          Resp->SetStringField(TEXT("savePath"), SavePath);
          Resp->SetStringField(TEXT("meshPath"), TargetMesh->GetPathName());
          Resp->SetStringField(TEXT("skeletalMeshPath"), TargetMesh->GetPathName());
          if (TargetSkeleton) {
            Resp->SetStringField(TEXT("skeletonPath"),
                                 TargetSkeleton->GetPathName());
          }
          UPhysicsAsset* ExistingPhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, *PhysicsAssetObjectPath);
          if (ExistingPhysicsAsset) {
            McpHandlerUtils::AddVerification(Resp, ExistingPhysicsAsset);
          }
        } else {
          UPackage *Package = CreatePackage(*PhysicsAssetObjectPath);
          if (!Package) {
            Context.Fail(TEXT("PACKAGE_ERROR"), TEXT("Failed to create physics asset package"));
          } else {
            UPhysicsAsset *PhysicsAsset = NewObject<UPhysicsAsset>(
                Package, FName(*PhysicsAssetName),
                RF_Public | RF_Standalone | RF_Transactional);

            if (!PhysicsAsset) {
              Context.Fail(TEXT("ASSET_CREATION_FAILED"), TEXT("Failed to create physics asset"));
            } else {
              PhysicsAsset->SetPreviewMesh(TargetMesh);
              PhysicsAsset->UpdateBodySetupIndexMap();
              PhysicsAsset->UpdateBoundsBodiesArray();
              FAssetRegistryModule::AssetCreated(PhysicsAsset);
              Package->MarkPackageDirty();

              bool bAssignToMesh = false;
              Payload->TryGetBoolField(TEXT("assignToMesh"), bAssignToMesh);

              if (bAssignToMesh) {
                TargetMesh->Modify();
                TargetMesh->SetPhysicsAsset(PhysicsAsset);
                McpSafeOperations::McpSafeAssetSave(TargetMesh);
              }
              McpSafeOperations::McpSafeAssetSave(PhysicsAsset);

              Resp->SetStringField(TEXT("physicsAssetPath"),
                                   PhysicsAsset->GetPathName());
              Resp->SetBoolField(TEXT("assignedToMesh"), bAssignToMesh);
              Resp->SetBoolField(TEXT("existingAsset"), false);
              Resp->SetStringField(TEXT("savePath"), SavePath);
              Resp->SetStringField(TEXT("meshPath"), TargetMesh->GetPathName());
              Resp->SetStringField(TEXT("skeletalMeshPath"),
                                   TargetMesh->GetPathName());
              if (TargetSkeleton) {
                Resp->SetStringField(TEXT("skeletonPath"),
                                     TargetSkeleton->GetPathName());
              }
              Resp->SetNumberField(TEXT("bodyCount"),
                                   PhysicsAsset->SkeletalBodySetups.Num());
              Resp->SetNumberField(TEXT("constraintCount"),
                                   PhysicsAsset->ConstraintSetup.Num());
              McpHandlerUtils::AddVerification(Resp, PhysicsAsset);

              bSuccess = true;
              Message = TEXT("Physics simulation setup completed");
            }
          }
        }
      }
    }
    return false;
}
} // namespace McpAnimationHandlers
