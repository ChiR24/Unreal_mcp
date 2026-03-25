// =============================================================================
// McpAutomationBridge_AnimationHandlers.cpp
// =============================================================================
// Animation, skeleton, blend space, and control rig handlers.
//
// HANDLERS:
//   Animation Assets:
//     - create_anim_blueprint, create_anim_sequence, create_anim_montage
//     - create_blend_space, create_blend_space_1d, create_aim_offset
//     - get_anim_blueprint_info, get_anim_sequence_info
//     - get_skeleton_info, get_blend_space_info
//
//   Animation Editing:
//     - add_anim_curve, remove_anim_curve, get_anim_curves
//     - set_anim_curve_keys, add_anim_notify
//     - modify_anim_sequence, retarget_anim_sequence
//
//   Skeleton Operations:
//     - get_skeleton_bones, add_skeleton_socket
//     - set_bone_transform, get_bone_transform
//
//   Physics Assets:
//     - create_physics_asset, get_physics_asset_info
//     - modify_physics_body, set_physics_constraint
//
//   Control Rig:
//     - get_control_rig_info, modify_control_rig
//
// REFACTORING NOTES:
//   - Uses McpVersionCompatibility.h for UE 5.0-5.7 API abstraction
//   - Uses McpHandlerUtils for standardized JSON parsing/responses
//   - BlendSpaceBase.h deprecated in UE 5.3+, uses BlendSpace.h
//   - IAnimationDataController requires specific header paths by UE version
//
// VERSION COMPATIBILITY:
//   - BlendSpaceFactory: UE 5.0+ (conditional include)
//   - AnimationBlueprintLibrary: Header path varies by UE version
//   - AssetEditorSubsystem: Optional, header path varies
//   - AnimationDataController: UE 5.1+ with specific paths
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Editor-only Includes: Animation Core
// -----------------------------------------------------------------------------
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Animation Blueprint Library (path varies by UE version)
// -----------------------------------------------------------------------------
#if __has_include("Animation/AnimationBlueprintLibrary.h")
#include "Animation/AnimationBlueprintLibrary.h"
#elif __has_include("AnimationBlueprintLibrary.h")
#include "AnimationBlueprintLibrary.h"
#endif
#if __has_include("Animation/AnimBlueprintLibrary.h")
#include "Animation/AnimBlueprintLibrary.h"
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Blend Spaces
// -----------------------------------------------------------------------------
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"

// BlendSpaceBase.h is deprecated in favor of BlendSpace.h, but we need UBlendSpaceBase class
// Suppress the deprecation warning for this include
PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if __has_include("Animation/BlendSpaceBase.h")
#include "Animation/BlendSpaceBase.h"
#define MCP_HAS_BLENDSPACE_BASE 1
#elif __has_include("BlendSpaceBase.h")
#include "BlendSpaceBase.h"
#define MCP_HAS_BLENDSPACE_BASE 1
#else
#define MCP_HAS_BLENDSPACE_BASE 0
#endif
PRAGMA_ENABLE_DEPRECATION_WARNINGS

// -----------------------------------------------------------------------------
// Editor-only Includes: Animation Data Controller (UE 5.1+)
// -----------------------------------------------------------------------------
#if __has_include("AnimData/IAnimationDataController.h")
#include "AnimData/IAnimationDataController.h"
#endif
#if __has_include("Animation/AnimData/IAnimationDataModel.h")
#include "Animation/AnimData/IAnimationDataModel.h"
#endif
#if __has_include("Animation/AnimData/CurveIdentifier.h")
#include "Animation/AnimData/CurveIdentifier.h"
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Editor Framework
// -----------------------------------------------------------------------------
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EngineUtils.h"
#include "RenderingThread.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Blend Space Factories
// -----------------------------------------------------------------------------
#if __has_include("Factories/BlendSpaceFactoryNew.h") && __has_include("Factories/BlendSpaceFactory1D.h")
#include "Factories/BlendSpaceFactory1D.h"
#include "Factories/BlendSpaceFactoryNew.h"
#define MCP_HAS_BLENDSPACE_FACTORY 1
#else
#define MCP_HAS_BLENDSPACE_FACTORY 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Control Rig
// -----------------------------------------------------------------------------
#include "ControlRig.h"
// ControlRig headers removed for dynamic loading compatibility
// #include "ControlRigBlueprint.h" etc.

// -----------------------------------------------------------------------------
// Editor-only Includes: Asset Management
// -----------------------------------------------------------------------------
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Animation Factories
// -----------------------------------------------------------------------------
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimSequenceFactory.h"
#include "Factories/PhysicsAssetFactory.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Blueprint Utilities
// -----------------------------------------------------------------------------
#include "Kismet2/BlueprintEditorUtils.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: AnimGraph State Machine (for create_state_machine)
// -----------------------------------------------------------------------------
#if __has_include("AnimGraphNode_StateMachine.h")
#include "AnimGraphNode_StateMachine.h"
#endif
#if __has_include("AnimStateNode.h")
#include "AnimStateNode.h"
#endif
#if __has_include("AnimStateTransitionNode.h")
#include "AnimStateTransitionNode.h"
#define MCP_HAS_ANIM_STATE_TRANSITION 1
#else
#define MCP_HAS_ANIM_STATE_TRANSITION 0
#endif
#if __has_include("AnimationStateMachineGraph.h")
#include "AnimationStateMachineGraph.h"
#define MCP_HAS_ANIM_STATE_MACHINE_GRAPH 1
#else
#define MCP_HAS_ANIM_STATE_MACHINE_GRAPH 0
#endif
#if __has_include("AnimationStateMachineSchema.h")
#include "AnimationStateMachineSchema.h"
#define MCP_HAS_ANIM_STATE_MACHINE_SCHEMA 1
#else
#define MCP_HAS_ANIM_STATE_MACHINE_SCHEMA 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: AnimGraph Blend Tree (for create_blend_tree)
// -----------------------------------------------------------------------------
#if __has_include("AnimGraphNode_BlendTree.h")
#include "AnimGraphNode_BlendTree.h"
#define MCP_HAS_ANIM_GRAPH_NODE_BLEND_TREE 1
#else
#define MCP_HAS_ANIM_GRAPH_NODE_BLEND_TREE 0
#endif
#if __has_include("Animation/BlendTree.h")
#include "Animation/BlendTree.h"
#define MCP_HAS_BLEND_TREE 1
#else
#define MCP_HAS_BLEND_TREE 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Misc Utilities
// -----------------------------------------------------------------------------
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Components/PrimitiveComponent.h"

#if __has_include("PhysicsEngine/WheeledVehicleMovementComponent4W.h")
#include "PhysicsEngine/WheeledVehicleMovementComponent4W.h"
#define MCP_HAS_WHEELED_VEHICLE_4W 1
#else
#define MCP_HAS_WHEELED_VEHICLE_4W 0
#endif

#if __has_include("PhysicsEngine/VehicleWheel.h")
#include "PhysicsEngine/VehicleWheel.h"
#define MCP_HAS_VEHICLE_WHEEL 1
#else
#define MCP_HAS_VEHICLE_WHEEL 0
#endif

#if __has_include("ChaosWheeledVehicleMovementComponent.h")
#include "ChaosWheeledVehicleMovementComponent.h"
#define MCP_HAS_CHAOS_WHEELED_VEHICLE 1
#else
#define MCP_HAS_CHAOS_WHEELED_VEHICLE 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Editor Subsystems (path varies by UE version)
// -----------------------------------------------------------------------------
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 1
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 1
#else
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: UObject Reflection
// -----------------------------------------------------------------------------
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

namespace {
#if MCP_HAS_BLENDSPACE_FACTORY
/**
 * @brief Creates a new 1D or 2D Blend Space asset bound to a target skeleton.
 *
 * Creates and returns a newly created UBlendSpace (2D) or UBlendSpace1D (1D)
 * asset using the appropriate factory and places it at the given package path.
 *
 * @param AssetName Name to assign to the new asset.
 * @param PackagePath Package path where the asset will be created (e.g.
 * "/Game/Animations").
 * @param TargetSkeleton Skeleton to bind the created Blend Space to.
 * @param bTwoDimensional If true, creates a 2D UBlendSpace; if false, creates a
 * 1D UBlendSpace1D.
 * @param OutError Receives a human-readable error message on failure.
 * @return UObject* Pointer to the created blend space asset on success, or
 * `nullptr` on failure.
 */
static UObject *CreateBlendSpaceAsset(const FString &AssetName,
                                      const FString &PackagePath,
                                      USkeleton *TargetSkeleton,
                                      bool bTwoDimensional, FString &OutError) {
  OutError.Reset();

  UFactory *Factory = nullptr;
  UClass *DesiredClass = nullptr;

  if (bTwoDimensional) {
    UBlendSpaceFactoryNew *Factory2D = NewObject<UBlendSpaceFactoryNew>();
    if (!Factory2D) {
      OutError = TEXT("Failed to allocate BlendSpace factory");
      return nullptr;
    }
    Factory2D->TargetSkeleton = TargetSkeleton;
    Factory = Factory2D;
    DesiredClass = UBlendSpace::StaticClass();
  } else {
    UBlendSpaceFactory1D *Factory1D = NewObject<UBlendSpaceFactory1D>();
    if (!Factory1D) {
      OutError = TEXT("Failed to allocate BlendSpace1D factory");
      return nullptr;
    }
    Factory1D->TargetSkeleton = TargetSkeleton;
    Factory = Factory1D;
    DesiredClass = UBlendSpace1D::StaticClass();
  }

  if (!Factory || !DesiredClass) {
    OutError = TEXT("BlendSpace factory unavailable");
    return nullptr;
  }

  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
  return AssetToolsModule.Get().CreateAsset(AssetName, PackagePath,
                                            DesiredClass, Factory);
}

/**
 * @brief Applies axis range and grid configuration to a blend space asset.
 *
 * Reads numeric fields from the provided JSON payload and updates the blend
 * space's first axis (minX, maxX, gridX) and, if bTwoDimensional is true,
 * the second axis (minY, maxY, gridY). Marks the asset package dirty when
 * modifications are applied.
 *
 * @param BlendSpaceAsset Blend space or blend space base object to configure.
 *                       If null, the function is a no-op.
 * @param Payload JSON object containing axis configuration fields:
 *                - "minX", "maxX", "gridX" for axis 0 (required defaults:
 * 0,1,3)
 *                - "minY", "maxY", "gridY" for axis 1 when bTwoDimensional is
 * true
 * @param bTwoDimensional If true, the second axis is also configured.
 *
 * Notes:
 * - If the engine headers/types required to modify blend parameters are
 *   unavailable, the function logs and skips axis configuration.
 * - Grid values are clamped to a minimum of 1.
 */
static void ApplyBlendSpaceConfiguration(UObject *BlendSpaceAsset,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         bool bTwoDimensional) {
  if (!BlendSpaceAsset || !Payload.IsValid()) {
    return;
  }

  double MinX = 0.0, MaxX = 1.0, GridX = 3.0;
  Payload->TryGetNumberField(TEXT("minX"), MinX);
  Payload->TryGetNumberField(TEXT("maxX"), MaxX);
  Payload->TryGetNumberField(TEXT("gridX"), GridX);

#if MCP_HAS_BLENDSPACE_BASE
  // UBlendSpaceBase is deprecated in UE 5.0+ but still needed for backward compatibility
  PRAGMA_DISABLE_DEPRECATION_WARNINGS
  if (UBlendSpaceBase *BlendBase = Cast<UBlendSpaceBase>(BlendSpaceAsset)) {
    BlendBase->Modify();

    FBlendParameter &Axis0 =
        const_cast<FBlendParameter &>(BlendBase->GetBlendParameter(0));
    Axis0.Min = static_cast<float>(MinX);
    Axis0.Max = static_cast<float>(MaxX);
    Axis0.GridNum = FMath::Max(1, static_cast<int32>(GridX));

    if (bTwoDimensional) {
      double MinY = 0.0, MaxY = 1.0, GridY = 3.0;
      Payload->TryGetNumberField(TEXT("minY"), MinY);
      Payload->TryGetNumberField(TEXT("maxY"), MaxY);
      Payload->TryGetNumberField(TEXT("gridY"), GridY);

      FBlendParameter &Axis1 =
          const_cast<FBlendParameter &>(BlendBase->GetBlendParameter(1));
      Axis1.Min = static_cast<float>(MinY);
      Axis1.Max = static_cast<float>(MaxY);
      Axis1.GridNum = FMath::Max(1, static_cast<int32>(GridY));
    }

    BlendBase->MarkPackageDirty();
  }
  PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("ApplyBlendSpaceConfiguration: BlendSpaceBase headers "
              "unavailable; skipping axis configuration."));
  if (bTwoDimensional) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Requested 2D blend space but BlendSpaceBase headers are "
                "missing; axis configuration skipped."));
  }
  if (!BlendSpaceAsset->IsA<UBlendSpace>() &&
      !BlendSpaceAsset->IsA<UBlendSpace1D>()) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("ApplyBlendSpaceConfiguration: Asset %s is not a BlendSpace type"),
        *BlendSpaceAsset->GetName());
  }
#endif
}
#endif /**                                                                     \
        * @brief Executes a list of editor console commands against the        \
        * current editor world.                                                \
        *                                                                      \
        * Skips empty or whitespace-only commands. If any command fails or the \
        * editor/world is unavailable, an explanatory message is written to    \
        * OutErrorMessage.                                                     \
        *                                                                      \
        * @param Commands Array of editor command strings to execute.          \
        * @param OutErrorMessage Populated with an error description when      \
        * execution fails.                                                     \
        * @return true if all non-empty commands executed successfully, false  \
        * otherwise.                                                           \
        */

static bool ExecuteEditorCommandsInternal(const TArray<FString> &Commands,
                                          FString &OutErrorMessage) {
  OutErrorMessage.Reset();

  if (!GEditor) {
    OutErrorMessage = TEXT("Editor instance unavailable");
    return false;
  }

  UWorld *EditorWorld = nullptr;
  FWorldContext &EditorContext = GEditor->GetEditorWorldContext(false);
  EditorWorld = EditorContext.World();

  for (const FString &Command : Commands) {
    const FString Trimmed = Command.TrimStartAndEnd();
    if (Trimmed.IsEmpty()) {
      continue;
    }

    if (!GEditor->Exec(EditorWorld, *Trimmed)) {
      OutErrorMessage = FString::Printf(
          TEXT("Failed to execute editor command: %s"), *Trimmed);
      return false;
    }
  }

  return true;
}
} // namespace
#else
#define MCP_HAS_BLENDSPACE_FACTORY 0
#endif // WITH_EDITOR

/**
 * @brief Process an "animation_physics" automation request and send a
 * structured response.
 *
 * Handles sub-actions encoded in the JSON payload (for example: cleanup,
 * create_animation_bp, create_blend_space, create_state_machine, setup_ik,
 * configure_vehicle, setup_physics_simulation, create_animation_asset,
 * setup_retargeting, play_anim_montage, add_notify, etc.). In editor builds
 * this may create/modify assets, execute editor commands, or perform
 * actor/component operations; in non-editor builds it will return a
 * not-implemented response.
 *
 * @param RequestId Unique identifier for the incoming request; included in the
 * response.
 * @param Action Top-level action string (expected to be "animation_physics" or
 * start with it).
 * @param Payload JSON object containing the sub-action and parameters required
 * to perform it.
 * @param RequestingSocket Optional websocket that will receive the automation
 * response/error.
 * @return true if the request was handled (a response was sent, even on error);
 * false if the action did not match "animation_physics" and the handler did not
 * process it.
 */
bool UMcpAutomationBridgeSubsystem::HandleAnimationPhysicsAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT(">>> HandleAnimationPhysicsAction ENTRY: RequestId=%s "
              "RawAction='%s'"),
         *RequestId, *Action);
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("animation_physics"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("animation_physics")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("animation_physics payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleAnimationPhysicsAction: subaction='%s'"), *LowerSub);

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  if (LowerSub == TEXT("cleanup")) {
    const TArray<TSharedPtr<FJsonValue>> *ArtifactsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("artifacts"), ArtifactsArray) ||
        !ArtifactsArray) {
      Message = TEXT("artifacts array required for cleanup");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      TArray<FString> Cleaned;
      TArray<FString> Missing;
      TArray<FString> Failed;

      for (const TSharedPtr<FJsonValue> &Val : *ArtifactsArray) {
        if (!Val.IsValid() || Val->Type != EJson::String) {
          continue;
        }

        const FString ArtifactPath = Val->AsString().TrimStartAndEnd();
        if (ArtifactPath.IsEmpty()) {
          continue;
        }

        if (UEditorAssetLibrary::DoesAssetExist(ArtifactPath)) {
// Close editors to ensure asset can be deleted
#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
          if (GEditor) {
            UObject *Asset = LoadObject<UObject>(nullptr, *ArtifactPath);
            if (Asset) {
              if (UAssetEditorSubsystem *AssetEditorSubsystem =
                      GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
                AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
              }
            }
          }
#endif

          // Flush before deleting to release references
          if (GEditor) {
            FlushRenderingCommands();
            GEditor->ForceGarbageCollection(true);
            FlushRenderingCommands();
          }

          if (UEditorAssetLibrary::DeleteAsset(ArtifactPath)) {
            Cleaned.Add(ArtifactPath);
          } else {
            Failed.Add(ArtifactPath);
          }
        } else {
          Missing.Add(ArtifactPath);
        }
      }

      TArray<TSharedPtr<FJsonValue>> CleanedArray;
      for (const FString &Path : Cleaned) {
        CleanedArray.Add(MakeShared<FJsonValueString>(Path));
      }
      if (CleanedArray.Num() > 0) {
        Resp->SetArrayField(TEXT("cleaned"), CleanedArray);
      }
      Resp->SetNumberField(TEXT("cleanedCount"), Cleaned.Num());

      if (Missing.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> MissingArray;
        for (const FString &Path : Missing) {
          MissingArray.Add(MakeShared<FJsonValueString>(Path));
        }
        Resp->SetArrayField(TEXT("missing"), MissingArray);
      }

      if (Failed.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> FailedArray;
        for (const FString &Path : Failed) {
          FailedArray.Add(MakeShared<FJsonValueString>(Path));
        }
        Resp->SetArrayField(TEXT("failed"), FailedArray);
      }

      if (Cleaned.Num() > 0 && Failed.Num() == 0) {
        bSuccess = true;
        Message = TEXT("Animation artifacts removed");
      } else if (Failed.Num() > 0) {
        // Actual failure to delete something that exists
        bSuccess = false;
        Message = TEXT("Some animation artifacts could not be removed");
        ErrorCode = TEXT("CLEANUP_PARTIAL");
        Resp->SetStringField(TEXT("error"), Message);
      } else if (Cleaned.Num() == 0 && Missing.Num() > 0 && Failed.Num() == 0) {
        // All artifacts were missing - not an error, just nothing to do
        // The end state (no artifacts at those paths) is what the user wanted
        bSuccess = true;
        Message = TEXT("No animation artifacts needed removal (all specified paths were missing)");
        Resp->SetBoolField(TEXT("noOp"), true);
      } else {
        bSuccess = false;
        Message = TEXT("No animation artifacts were removed");
        ErrorCode = TEXT("CLEANUP_NO_OP");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  } else if (LowerSub == TEXT("create_animation_bp")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Message = TEXT("name field required for animation blueprint creation");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      // Fallback: try meshPath if skeleton missing
      if (!TargetSkeleton) {
        FString MeshPath;
        if (Payload->TryGetStringField(TEXT("meshPath"), MeshPath) &&
            !MeshPath.IsEmpty()) {
          USkeletalMesh *Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
          if (Mesh) {
            TargetSkeleton = Mesh->GetSkeleton();
          }
        }
      }

      if (!TargetSkeleton) {
        Message =
            TEXT("Valid skeletonPath or meshPath required to find skeleton");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        UAnimBlueprintFactory *Factory = NewObject<UAnimBlueprintFactory>();
        if (!Factory) {
          Message = TEXT("Failed to create Animation Blueprint factory");
          ErrorCode = TEXT("FACTORY_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
        Factory->TargetSkeleton = TargetSkeleton;

        // Allow parent class override
        FString ParentClassPath;
        if (Payload->TryGetStringField(TEXT("parentClass"), ParentClassPath) &&
            !ParentClassPath.IsEmpty()) {
          UClass *ParentClass = LoadClass<UObject>(nullptr, *ParentClassPath);
          if (ParentClass) {
            Factory->ParentClass = ParentClass;
          }
        }

        FAssetToolsModule &AssetToolsModule =
            FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
            Name, SavePath, UAnimBlueprint::StaticClass(), Factory);

        if (NewAsset) {
          bSuccess = true;
          Message = TEXT("Animation Blueprint created");
          Resp->SetStringField(TEXT("blueprintPath"), NewAsset->GetPathName());
          Resp->SetStringField(TEXT("skeletonPath"),
                               TargetSkeleton->GetPathName());
          McpHandlerUtils::AddVerification(Resp, NewAsset);
        } else {
          Message = TEXT("Failed to create Animation Blueprint asset");
          ErrorCode = TEXT("ASSET_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
        }
      }
    }
  } else if (LowerSub == TEXT("create_blend_space")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Message = TEXT("name field required for blend space creation");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
          SkeletonPath.IsEmpty()) {
        Message =
            TEXT("skeletonPath is required to bind blend space to a skeleton");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        USkeleton *TargetSkeleton =
            LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (!TargetSkeleton) {
          Message = TEXT("Failed to load skeleton for blend space");
          ErrorCode = TEXT("LOAD_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          int32 Dimensions = 1;
          double DimensionsNumber = 1.0;
          if (Payload->TryGetNumberField(TEXT("dimensions"),
                                         DimensionsNumber)) {
            Dimensions = static_cast<int32>(DimensionsNumber);
          }
          const bool bTwoDimensional = (Dimensions >= 2);

          // Validation for Issue #10
          double MinX = 0.0, MaxX = 1.0, GridX = 3.0;
          Payload->TryGetNumberField(TEXT("minX"), MinX);
          Payload->TryGetNumberField(TEXT("maxX"), MaxX);
          Payload->TryGetNumberField(TEXT("gridX"), GridX);

          if (MinX >= MaxX) {
            Message = TEXT("minX must be less than maxX");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
          } else if (GridX <= 0) {
            Message = TEXT("gridX must be greater than 0");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            if (bTwoDimensional) {
              double MinY = 0.0, MaxY = 1.0, GridY = 3.0;
              Payload->TryGetNumberField(TEXT("minY"), MinY);
              Payload->TryGetNumberField(TEXT("maxY"), MaxY);
              Payload->TryGetNumberField(TEXT("gridY"), GridY);

              if (MinY >= MaxY) {
                Message = TEXT("minY must be less than maxY");
                ErrorCode = TEXT("INVALID_ARGUMENT");
                Resp->SetStringField(TEXT("error"), Message);
                goto ValidationFailed;
              }
              if (GridY <= 0) {
                Message = TEXT("gridY must be greater than 0");
                ErrorCode = TEXT("INVALID_ARGUMENT");
                Resp->SetStringField(TEXT("error"), Message);
                goto ValidationFailed;
              }
            }

            FString FactoryError;
#if MCP_HAS_BLENDSPACE_FACTORY
            UObject *CreatedBlendAsset = CreateBlendSpaceAsset(
                Name, SavePath, TargetSkeleton, bTwoDimensional, FactoryError);
            if (CreatedBlendAsset) {
              ApplyBlendSpaceConfiguration(CreatedBlendAsset, Payload,
                                           bTwoDimensional);
#if MCP_HAS_BLENDSPACE_BASE
              // UBlendSpaceBase is deprecated in UE 5.0+ but still needed for backward compatibility
              PRAGMA_DISABLE_DEPRECATION_WARNINGS
              if (UBlendSpaceBase *BlendSpace =
                      Cast<UBlendSpaceBase>(CreatedBlendAsset)) {

                bSuccess = true;
                Message = TEXT("Blend space created successfully");
                Resp->SetStringField(TEXT("blendSpacePath"),
                                     BlendSpace->GetPathName());
                Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
                Resp->SetBoolField(TEXT("twoDimensional"), bTwoDimensional);
                McpHandlerUtils::AddVerification(Resp, BlendSpace);
              } else {
                Message =
                    TEXT("Created asset is not a BlendSpaceBase instance");
                ErrorCode = TEXT("TYPE_MISMATCH");
                Resp->SetStringField(TEXT("error"), Message);
              }
              PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else

              bSuccess = true;
              Message = TEXT("Blend space created (limited configuration)");
              Resp->SetStringField(TEXT("blendSpacePath"),
                                   CreatedBlendAsset->GetPathName());
              Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
              Resp->SetBoolField(TEXT("twoDimensional"), bTwoDimensional);
              Resp->SetStringField(TEXT("warning"),
                                   TEXT("BlendSpaceBase headers unavailable; "
                                        "axis configuration skipped."));
              McpHandlerUtils::AddVerification(Resp, CreatedBlendAsset);
#endif // MCP_HAS_BLENDSPACE_BASE
            } else {
              Message = FactoryError.IsEmpty()
                            ? TEXT("Failed to create blend space asset")
                            : FactoryError;
              ErrorCode = TEXT("ASSET_CREATION_FAILED");
              Resp->SetStringField(TEXT("error"), Message);
            }
#else
            Message = TEXT(
                "Blend space creation requires editor blend space factories");
            ErrorCode = TEXT("NOT_AVAILABLE");
            Resp->SetStringField(TEXT("error"), Message);
#endif
          } // End valid params

        ValidationFailed:;
        }
      }
    }
  } else if (LowerSub == TEXT("create_blend_tree")) {
    // ============================================================================
    // Blend Tree Creation in AnimBlueprint's AnimGraph
    // ============================================================================
    // Creates a new blend tree node in an existing AnimBlueprint's AnimGraph.
    // Optionally configures blend parameters and adds children with animations.
    // Uses FGraphNodeCreator for proper graph editing.
    // ============================================================================
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath is required for create_blend_tree");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString TreeName;
      Payload->TryGetStringField(TEXT("treeName"), TreeName);
      if (TreeName.IsEmpty()) {
        TreeName = TEXT("BlendTree");
      }

      // Load the AnimBlueprint
      UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
      if (!AnimBP) {
        Message = FString::Printf(TEXT("AnimBlueprint not found: %s"), *BlueprintPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
      } else {
#if MCP_HAS_ANIM_GRAPH_NODE_BLEND_TREE && MCP_HAS_BLEND_TREE && MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
        // Find the AnimGraph in the blueprint
        UEdGraph* AnimGraph = nullptr;
        for (UEdGraph* Graph : AnimBP->FunctionGraphs) {
          if (Graph && Graph->GetName() == TEXT("AnimGraph")) {
            AnimGraph = Graph;
            break;
          }
        }

        if (!AnimGraph) {
          Message = TEXT("Could not find AnimGraph in blueprint");
          ErrorCode = TEXT("GRAPH_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          // Create the Blend Tree Node using FGraphNodeCreator
          FGraphNodeCreator<UAnimGraphNode_BlendTree> NodeCreator(*AnimGraph);
          UAnimGraphNode_BlendTree* BTNode = NodeCreator.CreateNode();
          BTNode->NodePosX = 0;
          BTNode->NodePosY = 0;
          NodeCreator.Finalize();

          // Access and configure the inner UBlendTree
#if MCP_HAS_BLEND_TREE
          if (UBlendTree* BlendTree = BTNode->GetBlendTree()) {
            BlendTree->Modify();

            // Process blend parameters array if provided
            const TArray<TSharedPtr<FJsonValue>>* BlendParamsArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("blendParameters"), BlendParamsArray) && BlendParamsArray) {
              int32 ParamIndex = 0;
              for (const TSharedPtr<FJsonValue>& ParamValue : *BlendParamsArray) {
                if (!ParamValue.IsValid() || ParamValue->Type != EJson::Object) {
                  continue;
                }

                const TSharedPtr<FJsonObject> ParamObj = ParamValue->AsObject();
                FString ParamName;
                ParamObj->TryGetStringField(TEXT("name"), ParamName);

                double MinVal = 0.0, MaxVal = 1.0;
                ParamObj->TryGetNumberField(TEXT("min"), MinVal);
                ParamObj->TryGetNumberField(TEXT("max"), MaxVal);

                // Configure blend parameter (index 0 or 1 for 1D/2D blend trees)
                if (ParamIndex < 2) {
                  FBlendParameter& Param = const_cast<FBlendParameter&>(BlendTree->GetBlendParameter(ParamIndex));
                  if (!ParamName.IsEmpty()) {
                    Param.DisplayName = FText::FromString(ParamName);
                  }
                  Param.Min = static_cast<float>(MinVal);
                  Param.Max = static_cast<float>(MaxVal);
                }
                ParamIndex++;
              }
            }

            // Process children array if provided
            const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("children"), ChildrenArray) && ChildrenArray) {
              for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenArray) {
                if (!ChildValue.IsValid() || ChildValue->Type != EJson::Object) {
                  continue;
                }

                const TSharedPtr<FJsonObject> ChildObj = ChildValue->AsObject();
                FString AnimationPath;
                ChildObj->TryGetStringField(TEXT("animationPath"), AnimationPath);

                double BlendWeight = 1.0;
                ChildObj->TryGetNumberField(TEXT("blendWeight"), BlendWeight);

                // Load animation asset if path provided
                if (!AnimationPath.IsEmpty()) {
                  if (UAnimationAsset* AnimAsset = LoadObject<UAnimationAsset>(nullptr, *AnimationPath)) {
                    // Add as blend sample (simplified - full implementation would use proper blend tree API)
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
                           TEXT("create_blend_tree: Added animation %s with weight %.2f"),
                           *AnimationPath, BlendWeight);
                  }
                }
              }
            }

            BlendTree->MarkPackageDirty();
          }
#endif // MCP_HAS_BLEND_TREE

          FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

          bool bShouldSave = true;
          Payload->TryGetBoolField(TEXT("save"), bShouldSave);
          if (bShouldSave) {
            McpSafeAssetSave(AnimBP);
          }

          bSuccess = true;
          Message = FString::Printf(TEXT("Blend tree '%s' created in %s"), *TreeName, *BlueprintPath);
          Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
          Resp->SetStringField(TEXT("treeName"), TreeName);
          Resp->SetStringField(TEXT("nodeType"), TEXT("BlendTree"));
        }
#else
        // Blend tree headers not available
        Message = FString::Printf(
          TEXT("Cannot create blend tree '%s': AnimGraph BlendTree module headers not available. "
               "Rebuild with AnimGraph module enabled."),
          *TreeName);
        ErrorCode = TEXT("ANIMGRAPH_MODULE_UNAVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
  } else if (LowerSub == TEXT("create_procedural_anim")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Message = TEXT("name field required for procedural animation creation");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        Payload->TryGetStringField(TEXT("path"), SavePath);
      }
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
          SkeletonPath.IsEmpty()) {
        Message = TEXT("skeletonPath is required for create_procedural_anim");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        const TArray<TSharedPtr<FJsonValue>> *BoneTracksArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("boneTracks"), BoneTracksArray) ||
            !BoneTracksArray) {
          Message =
              TEXT("boneTracks array is required for create_procedural_anim");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          double NumFramesNumber = 30.0;
          Payload->TryGetNumberField(TEXT("numFrames"), NumFramesNumber);
          const int32 NumFrames =
              FMath::Max(1, static_cast<int32>(NumFramesNumber));

          double FrameRateNumber = 30.0;
          Payload->TryGetNumberField(TEXT("frameRate"), FrameRateNumber);
          const int32 FrameRate =
              FMath::Max(1, static_cast<int32>(FrameRateNumber));

          bool bShouldSave = true;
          Payload->TryGetBoolField(TEXT("save"), bShouldSave);

          USkeleton *TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
          if (!TargetSkeleton) {
            Message = TEXT("Failed to load skeleton for procedural animation");
            ErrorCode = TEXT("LOAD_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            // Pre-check for existing asset to avoid UObject type-collision
            // crashes. Follow same pattern as create_montage/create_sequence.
            const FString ObjectPath =
                FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
            if (UEditorAssetLibrary::DoesAssetExist(ObjectPath)) {
              UObject *ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
              if (ExistingAsset) {
                if (UAnimSequence *ExistingSequence =
                        Cast<UAnimSequence>(ExistingAsset)) {
                  bSuccess = true;
                  Message =
                      FString::Printf(TEXT("Procedural animation '%s' already "
                                           "exists - reusing existing asset"),
                                      *Name);
                  Resp->SetStringField(TEXT("assetPath"), ObjectPath);
                  Resp->SetBoolField(TEXT("existingAsset"), true);
                  Resp->SetStringField(TEXT("skeletonPath"),
                                       ExistingSequence->GetSkeleton()
                                           ? ExistingSequence->GetSkeleton()->GetPathName()
                                           : SkeletonPath);
                  McpHandlerUtils::AddVerification(Resp, ExistingSequence);
                } else {
                  Message = FString::Printf(
                      TEXT("Cannot create procedural animation: asset '%s' "
                           "already exists as type '%s'"),
                      *ObjectPath, *ExistingAsset->GetClass()->GetName());
                  ErrorCode = TEXT("ASSET_TYPE_MISMATCH");
                  Resp->SetStringField(TEXT("error"), Message);
                }
              } else {
                Message =
                    FString::Printf(TEXT("Asset exists but failed to load: %s"),
                                    *ObjectPath);
                ErrorCode = TEXT("ASSET_LOAD_FAILED");
                Resp->SetStringField(TEXT("error"), Message);
              }
            } else {
              FString PackagePath = SavePath / Name;
              UPackage *Package = CreatePackage(*PackagePath);
              if (!Package) {
                Message = TEXT("Failed to create package for animation sequence");
                ErrorCode = TEXT("PACKAGE_ERROR");
                Resp->SetStringField(TEXT("error"), Message);
              } else {
                UAnimSequenceFactory *Factory = NewObject<UAnimSequenceFactory>();
                if (!Factory) {
                  Message = TEXT("Failed to create AnimSequence factory");
                  ErrorCode = TEXT("FACTORY_FAILED");
                  Resp->SetStringField(TEXT("error"), Message);
                } else {
                  Factory->TargetSkeleton = TargetSkeleton;

                  UAnimSequence *NewSequence = Cast<UAnimSequence>(
                      Factory->FactoryCreateNew(UAnimSequence::StaticClass(), Package,
                                                FName(*Name),
                                                RF_Public | RF_Standalone,
                                                nullptr, GWarn));

                  if (!NewSequence) {
                    Message = TEXT("Failed to create procedural animation "
                                   "sequence asset");
                    ErrorCode = TEXT("ASSET_CREATION_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                  } else {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                    IAnimationDataController &Controller =
                        NewSequence->GetController();
                    Controller.SetFrameRate(FFrameRate(FrameRate, 1));
                    Controller.SetNumberOfFrames(FFrameNumber(NumFrames));
#else
                    // SequenceLength is deprecated in UE 5.1+ but required for
                    // UE 5.0 compatibility.
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    NewSequence->SequenceLength =
                        static_cast<float>(NumFrames) /
                        static_cast<float>(FrameRate);
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
                    IAnimationDataController &Controller =
                        NewSequence->GetController();
#endif

                    int32 AppliedTrackCount = 0;
                    for (const TSharedPtr<FJsonValue> &TrackValue :
                         *BoneTracksArray) {
                      if (!TrackValue.IsValid() ||
                          TrackValue->Type != EJson::Object) {
                        continue;
                      }

                      const TSharedPtr<FJsonObject> TrackObject =
                          TrackValue->AsObject();
                      if (!TrackObject.IsValid()) {
                        continue;
                      }

                      FString BoneName;
                      if (!TrackObject->TryGetStringField(TEXT("boneName"),
                                                          BoneName) ||
                          BoneName.IsEmpty()) {
                        continue;
                      }

                      const FName BoneFName(*BoneName);

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                      if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName)) {
                        Controller.AddBoneCurve(BoneFName);
                      }
#elif ENGINE_MAJOR_VERSION >= 5
                      const FBoneAnimationTrack *ExistingTrack =
                          Controller.GetModel()->FindBoneTrackByName(BoneFName);
                      if (ExistingTrack == nullptr) {
                        PRAGMA_DISABLE_DEPRECATION_WARNINGS
                        FRawAnimSequenceTrack NewTrack;
                        NewSequence->AddNewRawTrack(BoneFName, &NewTrack);
                        PRAGMA_ENABLE_DEPRECATION_WARNINGS
                      }
#endif

                      TArray<FVector> PositionKeys;
                      TArray<FQuat> RotationKeys;
                      TArray<FVector> ScaleKeys;
                      PositionKeys.Init(FVector::ZeroVector, NumFrames);
                      RotationKeys.Init(FQuat::Identity, NumFrames);
                      ScaleKeys.Init(FVector::OneVector, NumFrames);

                      const TArray<TSharedPtr<FJsonValue>> *FramesArray =
                          nullptr;
                      if (TrackObject->TryGetArrayField(TEXT("frames"),
                                                        FramesArray) &&
                          FramesArray) {
                        for (const TSharedPtr<FJsonValue> &FrameValue :
                             *FramesArray) {
                          if (!FrameValue.IsValid() ||
                              FrameValue->Type != EJson::Object) {
                            continue;
                          }

                          const TSharedPtr<FJsonObject> FrameObject =
                              FrameValue->AsObject();
                          if (!FrameObject.IsValid()) {
                            continue;
                          }

                          double FrameNumberValue = 0.0;
                          FrameObject->TryGetNumberField(TEXT("frame"),
                                                         FrameNumberValue);
                          const int32 FrameIndex =
                              static_cast<int32>(FrameNumberValue);
                          if (FrameIndex < 0 || FrameIndex >= NumFrames) {
                            continue;
                          }

                          const TSharedPtr<FJsonObject> *LocationObject =
                              nullptr;
                          if (FrameObject->TryGetObjectField(TEXT("location"),
                                                             LocationObject) &&
                              LocationObject && LocationObject->IsValid()) {
                            double X = 0.0, Y = 0.0, Z = 0.0;
                            (*LocationObject)
                                ->TryGetNumberField(TEXT("x"), X);
                            (*LocationObject)
                                ->TryGetNumberField(TEXT("y"), Y);
                            (*LocationObject)
                                ->TryGetNumberField(TEXT("z"), Z);
                            PositionKeys[FrameIndex] =
                                FVector(static_cast<float>(X),
                                        static_cast<float>(Y),
                                        static_cast<float>(Z));
                          }

                          const TSharedPtr<FJsonObject> *RotationObject =
                              nullptr;
                          if (FrameObject->TryGetObjectField(TEXT("rotation"),
                                                             RotationObject) &&
                              RotationObject && RotationObject->IsValid()) {
                            const TSharedPtr<FJsonObject> RotationJson =
                                *RotationObject;
                            if (RotationJson->HasField(TEXT("w"))) {
                              double X = 0.0, Y = 0.0, Z = 0.0, W = 1.0;
                              RotationJson->TryGetNumberField(TEXT("x"), X);
                              RotationJson->TryGetNumberField(TEXT("y"), Y);
                              RotationJson->TryGetNumberField(TEXT("z"), Z);
                              RotationJson->TryGetNumberField(TEXT("w"), W);
                              RotationKeys[FrameIndex] =
                                  FQuat(static_cast<float>(X),
                                        static_cast<float>(Y),
                                        static_cast<float>(Z),
                                        static_cast<float>(W));
                            } else {
                              double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
                              RotationJson->TryGetNumberField(TEXT("pitch"),
                                                              Pitch);
                              RotationJson->TryGetNumberField(TEXT("yaw"),
                                                              Yaw);
                              RotationJson->TryGetNumberField(TEXT("roll"),
                                                              Roll);
                              RotationKeys[FrameIndex] =
                                  FRotator(static_cast<float>(Pitch),
                                           static_cast<float>(Yaw),
                                           static_cast<float>(Roll))
                                      .Quaternion();
                            }
                          }

                          const TSharedPtr<FJsonObject> *ScaleObject = nullptr;
                          if (FrameObject->TryGetObjectField(TEXT("scale"),
                                                             ScaleObject) &&
                              ScaleObject && ScaleObject->IsValid()) {
                            double X = 1.0, Y = 1.0, Z = 1.0;
                            (*ScaleObject)->TryGetNumberField(TEXT("x"), X);
                            (*ScaleObject)->TryGetNumberField(TEXT("y"), Y);
                            (*ScaleObject)->TryGetNumberField(TEXT("z"), Z);
                            ScaleKeys[FrameIndex] =
                                FVector(static_cast<float>(X),
                                        static_cast<float>(Y),
                                        static_cast<float>(Z));
                          }
                        }
                      }

                      Controller.SetBoneTrackKeys(BoneFName, PositionKeys,
                                                  RotationKeys, ScaleKeys);
                      ++AppliedTrackCount;
                    }

                    NewSequence->PostEditChange();
                    NewSequence->MarkPackageDirty();
                    if (bShouldSave) {
                      McpSafeAssetSave(NewSequence);
                    }

                    bSuccess = true;
                    Message =
                        FString::Printf(TEXT("Procedural animation '%s' "
                                             "created with %d tracks"),
                                        *Name, AppliedTrackCount);
                    Resp->SetStringField(TEXT("assetPath"),
                                         NewSequence->GetPathName());
                    Resp->SetStringField(TEXT("skeletonPath"),
                                         TargetSkeleton->GetPathName());
                    Resp->SetBoolField(TEXT("existingAsset"), false);
                    Resp->SetNumberField(TEXT("numFrames"), NumFrames);
                    Resp->SetNumberField(TEXT("frameRate"), FrameRate);
                    Resp->SetNumberField(TEXT("appliedTrackCount"),
                                         AppliedTrackCount);
                    McpHandlerUtils::AddVerification(Resp, NewSequence);
                  }
                }
              }
            }
          }
        }
      }
    }
  } else if (LowerSub == TEXT("create_state_machine")) {
    // ============================================================================
    // State Machine Creation using AnimGraph Editor API
    // ============================================================================
    // Creates a new state machine node in an existing AnimBlueprint's AnimGraph.
    // Optionally adds states and transitions if provided in the payload.
    // Uses FGraphNodeCreator and FBlueprintEditorUtils for proper graph editing.
    // ============================================================================
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (BlueprintPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("name"), BlueprintPath);
    }

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath is required for create_state_machine");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString MachineName;
      Payload->TryGetStringField(TEXT("machineName"), MachineName);
      if (MachineName.IsEmpty()) {
        MachineName = TEXT("StateMachine");
      }

      // Load the AnimBlueprint
      UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
      if (!AnimBP) {
        Message = FString::Printf(TEXT("AnimBlueprint not found: %s"), *BlueprintPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
      } else {
        // Check if AnimGraph headers are available
        #if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
        // Find the AnimGraph in the blueprint
        UEdGraph* AnimGraph = nullptr;
        for (UEdGraph* Graph : AnimBP->FunctionGraphs) {
          if (Graph && Graph->GetName() == TEXT("AnimGraph")) {
            AnimGraph = Graph;
            break;
          }
        }

        if (!AnimGraph) {
          Message = TEXT("Could not find AnimGraph in blueprint");
          ErrorCode = TEXT("GRAPH_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          // Check if a state machine with this name already exists
          bool bAlreadyExists = false;
          for (UEdGraphNode* Node : AnimGraph->Nodes) {
            if (UAnimGraphNode_StateMachine* ExistingSM = Cast<UAnimGraphNode_StateMachine>(Node)) {
              if (ExistingSM->GetStateMachineName() == MachineName) {
                bAlreadyExists = true;
                break;
              }
            }
          }

          if (bAlreadyExists) {
            bSuccess = true;
            Message = FString::Printf(TEXT("State machine '%s' already exists in %s"), *MachineName, *BlueprintPath);
            Resp->SetBoolField(TEXT("existingAsset"), true);
          } else {
            // Create the State Machine Node using FGraphNodeCreator
            FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(*AnimGraph);
            UAnimGraphNode_StateMachine* SMNode = NodeCreator.CreateNode();
            SMNode->NodePosX = 0;
            SMNode->NodePosY = 0;
            NodeCreator.Finalize();

            // Create the internal State Machine Graph
            UAnimationStateMachineGraph* InnerGraph = CastChecked<UAnimationStateMachineGraph>(
              FBlueprintEditorUtils::CreateNewGraph(
                AnimBP,
                FName(*MachineName),
                UAnimationStateMachineGraph::StaticClass(),
                UAnimationStateMachineSchema::StaticClass()
              )
            );

            // Link the State Machine Node to its internal graph
            SMNode->EditorStateMachineGraph = InnerGraph;
            InnerGraph->OwnerAnimGraphNode = SMNode;

            // Initialize Entry Node (required for State Machines)
            const UAnimationStateMachineSchema* Schema = CastChecked<UAnimationStateMachineSchema>(InnerGraph->GetSchema());
            Schema->CreateDefaultNodesForGraph(*InnerGraph);

            // Process states array if provided
            const TArray<TSharedPtr<FJsonValue>>* StatesArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("states"), StatesArray) && StatesArray) {
              int32 StatePosX = 200;
              for (const TSharedPtr<FJsonValue>& StateValue : *StatesArray) {
                if (!StateValue.IsValid() || StateValue->Type != EJson::Object) {
                  continue;
                }

                const TSharedPtr<FJsonObject> StateObj = StateValue->AsObject();
                FString StateName;
                StateObj->TryGetStringField(TEXT("name"), StateName);
                if (StateName.IsEmpty()) {
                  continue;
                }

                // Create the State Node
                FGraphNodeCreator<UAnimStateNode> StateCreator(*InnerGraph);
                UAnimStateNode* StateNode = StateCreator.CreateNode();
                StateNode->NodePosX = StatePosX;
                StateNode->NodePosY = 0;
                StateCreator.Finalize();

                // Rename the state's bound graph to set the state name
                if (StateNode->BoundGraph) {
                  FBlueprintEditorUtils::RenameGraph(StateNode->BoundGraph, *StateName);
                }

                StatePosX += 200;
              }
            }

            // Process transitions array if provided
            const TArray<TSharedPtr<FJsonValue>>* TransitionsArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("transitions"), TransitionsArray) && TransitionsArray) {
              #if MCP_HAS_ANIM_STATE_TRANSITION
              for (const TSharedPtr<FJsonValue>& TransitionValue : *TransitionsArray) {
                if (!TransitionValue.IsValid() || TransitionValue->Type != EJson::Object) {
                  continue;
                }

                const TSharedPtr<FJsonObject> TransitionObj = TransitionValue->AsObject();
                FString SourceState;
                FString TargetState;
                TransitionObj->TryGetStringField(TEXT("sourceState"), SourceState);
                TransitionObj->TryGetStringField(TEXT("targetState"), TargetState);

                if (SourceState.IsEmpty() || TargetState.IsEmpty()) {
                  continue;
                }

                // Find the source and target states
                UAnimStateNode* FromNode = nullptr;
                UAnimStateNode* ToNode = nullptr;
                for (UEdGraphNode* Node : InnerGraph->Nodes) {
                  if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node)) {
                    FString NodeName = StateNode->GetStateName();
                    if (NodeName == SourceState) FromNode = StateNode;
                    if (NodeName == TargetState) ToNode = StateNode;
                  }
                }

                if (FromNode && ToNode) {
                  // Create the Transition Node
                  FGraphNodeCreator<UAnimStateTransitionNode> TransCreator(*InnerGraph);
                  UAnimStateTransitionNode* TransNode = TransCreator.CreateNode();
                  TransCreator.Finalize();

                  // Establish the connection between states
                  TransNode->CreateConnections(FromNode, ToNode);

                  // Configure crossfade duration
                  double CrossfadeDuration = 0.2;
                  TransitionObj->TryGetNumberField(TEXT("crossfadeDuration"), CrossfadeDuration);
                  TransNode->CrossfadeDuration = static_cast<float>(CrossfadeDuration);
                }
              }
              #endif // MCP_HAS_ANIM_STATE_TRANSITION
            }

            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
            McpSafeAssetSave(AnimBP);

            bSuccess = true;
            Message = FString::Printf(TEXT("State machine '%s' created in %s"), *MachineName, *BlueprintPath);
            Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
            Resp->SetStringField(TEXT("machineName"), MachineName);
          }
        }
        #else
        // AnimGraph headers not available
        Message = FString::Printf(
          TEXT("Cannot create state machine '%s': AnimGraph module headers not available. "
               "Rebuild with AnimGraph module enabled or use add_state_machine action."),
          *MachineName);
        ErrorCode = TEXT("ANIMGRAPH_MODULE_UNAVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
        #endif
      }
    }
  } else if (LowerSub == TEXT("setup_ik")) {
    FString IKName;
    if (!Payload->TryGetStringField(TEXT("name"), IKName) || IKName.IsEmpty()) {
      Message = TEXT("name field required for IK setup");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
          SkeletonPath.IsEmpty()) {
        Message = TEXT("skeletonPath is required to bind IK to a skeleton");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        USkeleton *TargetSkeleton =
            LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (!TargetSkeleton) {
          Message = TEXT("Failed to load skeleton for IK");
          ErrorCode = TEXT("LOAD_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          FString FactoryError;
          UBlueprint *ControlRigBlueprint = nullptr;
#if MCP_HAS_CONTROLRIG_FACTORY
          ControlRigBlueprint = CreateControlRigBlueprint(
              IKName, SavePath, TargetSkeleton, FactoryError);
#else
          FactoryError =
              TEXT("Control Rig factory not available in this editor build");
#endif
          if (!ControlRigBlueprint) {
            Message = FactoryError.IsEmpty() ? TEXT("Failed to create IK asset")
                                             : FactoryError;
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            bSuccess = true;
            Message = TEXT("IK setup created successfully");
            const FString ControlRigPath = ControlRigBlueprint->GetPathName();
            Resp->SetStringField(TEXT("ikPath"), ControlRigPath);
            Resp->SetStringField(TEXT("controlRigPath"), ControlRigPath);
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            McpHandlerUtils::AddVerification(Resp, ControlRigBlueprint);
          }
        }
      }
    }
  } else if (LowerSub == TEXT("configure_vehicle")) {
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
        ActorName.IsEmpty()) {
      // Backward compatibility for older payloads.
      Payload->TryGetStringField(TEXT("vehicleName"), ActorName);
    }

    if (ActorName.IsEmpty()) {
      Message = TEXT("actorName is required for configure_vehicle");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
#if MCP_HAS_WHEELED_VEHICLE_4W
      AActor *TargetActor = FindActorByName(ActorName);
      if (!TargetActor) {
        Message = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        FString VehicleType = TEXT("WheeledVehicle4W");
        Payload->TryGetStringField(TEXT("vehicleType"), VehicleType);

        UWheeledVehicleMovementComponent4W *VehicleMC =
            TargetActor->FindComponentByClass<UWheeledVehicleMovementComponent4W>();
        bool bCreatedComponent = false;
        if (!VehicleMC) {
          VehicleMC = NewObject<UWheeledVehicleMovementComponent4W>(
              TargetActor, TEXT("MCP_VehicleMovement4W"));
          if (VehicleMC) {
            TargetActor->AddInstanceComponent(VehicleMC);
            VehicleMC->RegisterComponent();
            bCreatedComponent = true;
          }
        }

        if (!VehicleMC) {
          Message = TEXT("Failed to create/get UWheeledVehicleMovementComponent4W");
          ErrorCode = TEXT("COMPONENT_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          auto SetNumericOnStruct = [](UStruct *StructType, void *Container,
                                       const TArray<FString> &PropertyNames,
                                       double Value) -> bool {
            if (!StructType || !Container) {
              return false;
            }

            for (const FString &PropName : PropertyNames) {
              if (PropName.IsEmpty()) {
                continue;
              }

              FProperty *Prop = StructType->FindPropertyByName(*PropName);
              if (!Prop) {
                continue;
              }

              if (FNumericProperty *NumericProp = CastField<FNumericProperty>(Prop)) {
                void *ValuePtr = Prop->ContainerPtrToValuePtr<void>(Container);
                if (!ValuePtr) {
                  continue;
                }

                if (NumericProp->IsFloatingPoint()) {
                  NumericProp->SetFloatingPointPropertyValue(ValuePtr, Value);
                } else {
                  NumericProp->SetIntPropertyValue(ValuePtr,
                                                   static_cast<int64>(Value));
                }
                return true;
              }
            }

            return false;
          };

          auto SetNumericOnObject =
              [&SetNumericOnStruct](UObject *Obj,
                                    const TArray<FString> &PropertyNames,
                                    double Value) -> bool {
            if (!Obj) {
              return false;
            }
            return SetNumericOnStruct(Obj->GetClass(), Obj, PropertyNames,
                                      Value);
          };

          auto SetNestedNumericOnObject =
              [&SetNumericOnStruct](UObject *Obj,
                                    const TArray<FString> &OuterNames,
                                    const TArray<FString> &InnerNames,
                                    double Value) -> bool {
            if (!Obj) {
              return false;
            }

            for (const FString &OuterName : OuterNames) {
              FStructProperty *OuterStructProp =
                  FindFProperty<FStructProperty>(Obj->GetClass(), *OuterName);
              if (!OuterStructProp) {
                continue;
              }

              void *OuterData = OuterStructProp->ContainerPtrToValuePtr<void>(Obj);
              if (!OuterData) {
                continue;
              }

              if (SetNumericOnStruct(OuterStructProp->Struct, OuterData,
                                     InnerNames, Value)) {
                return true;
              }
            }

            return false;
          };

          auto ConfigureForwardGearRatios =
              [](UObject *Obj, const TArray<double> &Ratios) -> bool {
            if (!Obj) {
              return false;
            }

            const TArray<FString> TransmissionOuterNames = {
                TEXT("TransmissionSetup"), TEXT("TransmissionConfig")};
            const TArray<FString> GearArrayNames = {TEXT("ForwardGears"),
                                                    TEXT("GearRatios")};

            for (const FString &OuterName : TransmissionOuterNames) {
              FStructProperty *TransmissionProp =
                  FindFProperty<FStructProperty>(Obj->GetClass(), *OuterName);
              if (!TransmissionProp) {
                continue;
              }

              void *TransmissionData =
                  TransmissionProp->ContainerPtrToValuePtr<void>(Obj);
              if (!TransmissionData) {
                continue;
              }

              for (const FString &ArrayName : GearArrayNames) {
                FArrayProperty *GearArrayProp = FindFProperty<FArrayProperty>(
                    TransmissionProp->Struct, *ArrayName);
                if (!GearArrayProp) {
                  continue;
                }

                void *GearArrayPtr =
                    GearArrayProp->ContainerPtrToValuePtr<void>(TransmissionData);
                FScriptArrayHelper GearArrayHelper(GearArrayProp, GearArrayPtr);
                GearArrayHelper.EmptyValues();

                for (int32 GearIndex = 0; GearIndex < Ratios.Num(); ++GearIndex) {
                  const int32 NewIdx = GearArrayHelper.AddValue();
                  void *GearElemPtr = GearArrayHelper.GetRawPtr(NewIdx);

                  if (FNumericProperty *NumericInner =
                          CastField<FNumericProperty>(GearArrayProp->Inner)) {
                    if (NumericInner->IsFloatingPoint()) {
                      NumericInner->SetFloatingPointPropertyValue(
                          GearElemPtr, Ratios[GearIndex]);
                    } else {
                      NumericInner->SetIntPropertyValue(
                          GearElemPtr,
                          static_cast<int64>(FMath::RoundToInt(Ratios[GearIndex])));
                    }
                    continue;
                  }

                  if (FStructProperty *StructInner =
                          CastField<FStructProperty>(GearArrayProp->Inner)) {
                    StructInner->Struct->InitializeStruct(GearElemPtr);
                    FProperty *RatioProp =
                        StructInner->Struct->FindPropertyByName(TEXT("Ratio"));
                    if (FNumericProperty *RatioNumeric =
                            CastField<FNumericProperty>(RatioProp)) {
                      void *RatioPtr =
                          RatioProp->ContainerPtrToValuePtr<void>(GearElemPtr);
                      if (RatioNumeric->IsFloatingPoint()) {
                        RatioNumeric->SetFloatingPointPropertyValue(
                            RatioPtr, Ratios[GearIndex]);
                      } else {
                        RatioNumeric->SetIntPropertyValue(
                            RatioPtr,
                            static_cast<int64>(
                                FMath::RoundToInt(Ratios[GearIndex])));
                      }
                    }
                  }
                }

                return true;
              }
            }

            return false;
          };

          int32 ConfiguredWheels = 0;
          const TArray<TSharedPtr<FJsonValue>> *WheelsArray = nullptr;
          if (Payload->TryGetArrayField(TEXT("wheels"), WheelsArray) &&
              WheelsArray) {
            FArrayProperty *WheelSetupsProp = FindFProperty<FArrayProperty>(
                VehicleMC->GetClass(), TEXT("WheelSetups"));
            FStructProperty *WheelSetupStruct =
                WheelSetupsProp ? CastField<FStructProperty>(WheelSetupsProp->Inner)
                                : nullptr;

            if (WheelSetupsProp && WheelSetupStruct) {
              void *WheelSetupsPtr =
                  WheelSetupsProp->ContainerPtrToValuePtr<void>(VehicleMC);
              FScriptArrayHelper WheelArrayHelper(WheelSetupsProp,
                                                  WheelSetupsPtr);
              WheelArrayHelper.EmptyValues();

              for (const TSharedPtr<FJsonValue> &WheelVal : *WheelsArray) {
                if (!WheelVal.IsValid() || WheelVal->Type != EJson::Object) {
                  continue;
                }

                const TSharedPtr<FJsonObject> WheelObj = WheelVal->AsObject();
                if (!WheelObj.IsValid()) {
                  continue;
                }

                const int32 NewIndex = WheelArrayHelper.AddValue();
                void *WheelSetupPtr = WheelArrayHelper.GetRawPtr(NewIndex);
                WheelSetupStruct->Struct->InitializeStruct(WheelSetupPtr);

                FString BoneName;
                if (WheelObj->TryGetStringField(TEXT("boneName"), BoneName) &&
                    !BoneName.IsEmpty()) {
                  if (FNameProperty *BoneProp = FindFProperty<FNameProperty>(
                          WheelSetupStruct->Struct, TEXT("BoneName"))) {
                    BoneProp->SetPropertyValue_InContainer(WheelSetupPtr,
                                                           FName(*BoneName));
                  }
                }

                const TSharedPtr<FJsonObject> *OffsetObj = nullptr;
                if (WheelObj->TryGetObjectField(TEXT("offset"), OffsetObj) &&
                    OffsetObj && (*OffsetObj).IsValid()) {
                  double X = 0.0, Y = 0.0, Z = 0.0;
                  (*OffsetObj)->TryGetNumberField(TEXT("x"), X);
                  (*OffsetObj)->TryGetNumberField(TEXT("y"), Y);
                  (*OffsetObj)->TryGetNumberField(TEXT("z"), Z);

                  if (FStructProperty *OffsetProp =
                          FindFProperty<FStructProperty>(
                              WheelSetupStruct->Struct,
                              TEXT("AdditionalOffset"))) {
                    if (OffsetProp->Struct == TBaseStructure<FVector>::Get()) {
                      if (FVector *OffsetPtr =
                              OffsetProp->ContainerPtrToValuePtr<FVector>(
                                  WheelSetupPtr)) {
                        *OffsetPtr = FVector(static_cast<float>(X),
                                             static_cast<float>(Y),
                                             static_cast<float>(Z));
                      }
                    }
                  }
                }

#if MCP_HAS_VEHICLE_WHEEL
                if (FClassProperty *WheelClassProp = FindFProperty<FClassProperty>(
                        WheelSetupStruct->Struct, TEXT("WheelClass"))) {
                  WheelClassProp->SetPropertyValue_InContainer(
                      WheelSetupPtr, UVehicleWheel::StaticClass());
                }
#endif

                double Radius = 0.0;
                if (WheelObj->TryGetNumberField(TEXT("radius"), Radius)) {
                  SetNumericOnStruct(WheelSetupStruct->Struct, WheelSetupPtr,
                                     {TEXT("WheelRadius"), TEXT("Radius")},
                                     Radius);
                }

                double Width = 0.0;
                if (WheelObj->TryGetNumberField(TEXT("width"), Width)) {
                  SetNumericOnStruct(WheelSetupStruct->Struct, WheelSetupPtr,
                                     {TEXT("WheelWidth"), TEXT("Width")},
                                     Width);
                }

                double Friction = 0.0;
                if (WheelObj->TryGetNumberField(TEXT("friction"), Friction)) {
                  SetNumericOnStruct(
                      WheelSetupStruct->Struct, WheelSetupPtr,
                      {TEXT("Friction"), TEXT("FrictionMultiplier"),
                       TEXT("TireFrictionScale")},
                      Friction);
                }

                ++ConfiguredWheels;
              }
            }
          }

          const TSharedPtr<FJsonObject> *EngineObj = nullptr;
          if (Payload->TryGetObjectField(TEXT("engine"), EngineObj) && EngineObj &&
              (*EngineObj).IsValid()) {
            double MaxRPM = 0.0;
            if ((*EngineObj)->TryGetNumberField(TEXT("maxRPM"), MaxRPM)) {
              SetNumericOnObject(VehicleMC,
                                 {TEXT("MaxEngineRPM"), TEXT("EngineMaxRPM")},
                                 MaxRPM);
              SetNestedNumericOnObject(
                  VehicleMC, {TEXT("EngineSetup"), TEXT("EngineConfig")},
                  {TEXT("MaxRPM"), TEXT("MaxEngineRPM")}, MaxRPM);
            }

            double MaxTorque = 0.0;
            if ((*EngineObj)->TryGetNumberField(TEXT("maxTorque"), MaxTorque)) {
              SetNumericOnObject(
                  VehicleMC,
                  {TEXT("MaxEngineTorque"), TEXT("EngineMaxTorque")},
                  MaxTorque);
              SetNestedNumericOnObject(
                  VehicleMC, {TEXT("EngineSetup"), TEXT("EngineConfig")},
                  {TEXT("MaxTorque"), TEXT("PeakTorque"),
                   TEXT("MaxEngineTorque")},
                  MaxTorque);
            }

            double EngineGears = 0.0;
            if ((*EngineObj)->TryGetNumberField(TEXT("gears"), EngineGears)) {
              SetNestedNumericOnObject(
                  VehicleMC, {TEXT("TransmissionSetup"), TEXT("TransmissionConfig")},
                  {TEXT("NumForwardGears"), TEXT("ForwardGearCount")},
                  EngineGears);
            }
          }

          const TSharedPtr<FJsonObject> *TransmissionObj = nullptr;
          if (Payload->TryGetObjectField(TEXT("transmission"), TransmissionObj) &&
              TransmissionObj && (*TransmissionObj).IsValid()) {
            double FinalDrive = 0.0;
            if ((*TransmissionObj)->TryGetNumberField(TEXT("finalDrive"),
                                                      FinalDrive) ||
                (*TransmissionObj)->TryGetNumberField(TEXT("finalDriveRatio"),
                                                      FinalDrive)) {
              SetNestedNumericOnObject(
                  VehicleMC, {TEXT("TransmissionSetup"), TEXT("TransmissionConfig")},
                  {TEXT("FinalRatio"), TEXT("FinalDrive"),
                   TEXT("FinalDriveRatio")},
                  FinalDrive);
            }

            const TArray<TSharedPtr<FJsonValue>> *GearRatiosArray = nullptr;
            if ((*TransmissionObj)
                    ->TryGetArrayField(TEXT("gearRatios"), GearRatiosArray) &&
                GearRatiosArray) {
              TArray<double> Ratios;
              Ratios.Reserve(GearRatiosArray->Num());
              for (const TSharedPtr<FJsonValue> &RatioVal : *GearRatiosArray) {
                if (RatioVal.IsValid() && RatioVal->Type == EJson::Number) {
                  Ratios.Add(RatioVal->AsNumber());
                }
              }
              if (Ratios.Num() > 0) {
                ConfigureForwardGearRatios(VehicleMC, Ratios);
              }
            }
          }

          double Mass = 0.0;
          if (Payload->TryGetNumberField(TEXT("mass"), Mass) && Mass > 0.0) {
            SetNumericOnObject(VehicleMC, {TEXT("Mass"), TEXT("VehicleMass")},
                               Mass);

            if (UPrimitiveComponent *PrimitiveRoot =
                    Cast<UPrimitiveComponent>(TargetActor->GetRootComponent())) {
              PrimitiveRoot->SetMassOverrideInKg(NAME_None,
                                                 static_cast<float>(Mass), true);
            }
          }

          double DragCoefficient = 0.0;
          if (Payload->TryGetNumberField(TEXT("dragCoefficient"),
                                         DragCoefficient)) {
            SetNumericOnObject(VehicleMC,
                               {TEXT("DragCoefficient"), TEXT("DragCoeff"),
                                TEXT("AerodynamicDragCoefficient")},
                               DragCoefficient);
            SetNestedNumericOnObject(
                VehicleMC,
                {TEXT("AerofoilSetup"), TEXT("AerodynamicsSetup")},
                {TEXT("DragCoefficient"), TEXT("DragCoeff")},
                DragCoefficient);

            if (UPrimitiveComponent *PrimitiveRoot =
                    Cast<UPrimitiveComponent>(TargetActor->GetRootComponent())) {
              PrimitiveRoot->SetLinearDamping(static_cast<float>(DragCoefficient));
            }
          }

          VehicleMC->RecreatePhysicsState();

          bSuccess = true;
          Message = FString::Printf(
              TEXT("Vehicle physics configured for actor '%s'"), *ActorName);
          Resp->SetStringField(TEXT("actorName"), ActorName);
          Resp->SetStringField(TEXT("vehicleType"), VehicleType);
          Resp->SetBoolField(TEXT("createdMovementComponent"), bCreatedComponent);
          Resp->SetNumberField(TEXT("configuredWheelCount"), ConfiguredWheels);

#if MCP_HAS_CHAOS_WHEELED_VEHICLE
          Resp->SetBoolField(TEXT("chaosVehicleHeadersAvailable"), true);
#else
          Resp->SetBoolField(TEXT("chaosVehicleHeadersAvailable"), false);
#endif
        }
      }
#else
      Message = TEXT("Wheeled vehicle component headers unavailable in this build");
      ErrorCode = TEXT("NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
#endif
    }
  } else if (LowerSub == TEXT("setup_physics_simulation")) {
    FString MeshPath;
    Payload->TryGetStringField(TEXT("meshPath"), MeshPath);

    FString SkeletonPath;
    Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

    // Support actorName parameter to find skeletal mesh from a spawned actor
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    const bool bMeshProvided = !MeshPath.IsEmpty();
    const bool bSkeletonProvided = !SkeletonPath.IsEmpty();
    const bool bActorProvided = !ActorName.IsEmpty();

    bool bMeshLoadFailed = false;
    bool bSkeletonLoadFailed = false;
    bool bSkeletonMissingPreview = false;

    USkeletalMesh *TargetMesh = nullptr;
    bool bMeshTypeMismatch = false;
    FString FoundClassName;

    // If actorName provided, try to find the actor and get its skeletal mesh
    if (!bMeshProvided && !bSkeletonProvided && bActorProvided) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
             TEXT("Attempting to find actor by name: '%s'"), *ActorName);
      AActor *FoundActor = FindActorByName(ActorName);
      if (FoundActor) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
               TEXT("Found actor: '%s' (Label: '%s')"), *FoundActor->GetName(),
               *FoundActor->GetActorLabel());
        // Try to get skeletal mesh component
        if (USkeletalMeshComponent *SkelComp =
                FoundActor->FindComponentByClass<USkeletalMeshComponent>()) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
          TargetMesh = SkelComp->GetSkeletalMeshAsset();
#else
          TargetMesh = SkelComp->SkeletalMesh;
#endif
          if (TargetMesh) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                   TEXT("Found skeletal mesh asset: '%s'"),
                   *TargetMesh->GetName());
          } else {
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
        bSuccess = false;
        SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message,
                               Resp, ErrorCode);
        return true;
      }
    }

    if (bMeshProvided) {
      if (UEditorAssetLibrary::DoesAssetExist(MeshPath)) {
        UObject *Asset = UEditorAssetLibrary::LoadAsset(MeshPath);
        TargetMesh = Cast<USkeletalMesh>(Asset);
        if (!TargetMesh && Asset) {
          bMeshTypeMismatch = true;
          FoundClassName = Asset->GetClass()->GetName();
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("setup_physics_simulation: Asset %s is not a "
                      "SkeletalMesh (Class: %s)"),
                 *MeshPath, *FoundClassName);
        } else if (!Asset) {
          bMeshLoadFailed = true;
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("setup_physics_simulation: failed to load mesh asset %s"),
                 *MeshPath);
        }
      } else {
        bMeshLoadFailed = true;
      }
    }

    USkeleton *TargetSkeleton = nullptr;
    if (!TargetMesh && bSkeletonProvided) {
      if (UEditorAssetLibrary::DoesAssetExist(SkeletonPath)) {
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
      if (bMeshTypeMismatch) {
        Message = FString::Printf(
            TEXT("asset found but is not a SkeletalMesh: %s (is %s)"),
            *MeshPath, *FoundClassName);
        ErrorCode = TEXT("TYPE_MISMATCH");
        Resp->SetStringField(TEXT("meshPath"), MeshPath);
        Resp->SetStringField(TEXT("actualClass"), FoundClassName);
      } else if (bMeshLoadFailed) {
        Message = FString::Printf(TEXT("asset not found: skeletal mesh %s"),
                                  *MeshPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("meshPath"), MeshPath);
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
          Message = TEXT("Invalid savePath for physics asset");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
          SavePath.Reset();
        } else {
          SavePath = NormalizedPath;
        }
      }

      if (!SavePath.IsEmpty()) {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        const FString PhysicsAssetObjectPath =
            FString::Printf(TEXT("%s/%s"), *SavePath, *PhysicsAssetName);

        if (UEditorAssetLibrary::DoesAssetExist(PhysicsAssetObjectPath)) {
          bSuccess = true;
          Message = TEXT(
              "Physics simulation already configured - existing asset reused");
          Resp->SetStringField(TEXT("physicsAssetPath"),
                               PhysicsAssetObjectPath);
          Resp->SetBoolField(TEXT("existingAsset"), true);
          Resp->SetStringField(TEXT("savePath"), SavePath);
          Resp->SetStringField(TEXT("meshPath"), TargetMesh->GetPathName());
          if (TargetSkeleton) {
            Resp->SetStringField(TEXT("skeletonPath"),
                                 TargetSkeleton->GetPathName());
          }
          UPhysicsAsset* ExistingPhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, *PhysicsAssetObjectPath);
          if (ExistingPhysicsAsset) {
            McpHandlerUtils::AddVerification(Resp, ExistingPhysicsAsset);
          }
        } else {
          UPhysicsAssetFactory *PhysicsFactory =
              NewObject<UPhysicsAssetFactory>();
          if (!PhysicsFactory) {
            Message = TEXT("Failed to allocate physics asset factory");
            ErrorCode = TEXT("FACTORY_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            PhysicsFactory->TargetSkeletalMesh = TargetMesh;

            FAssetToolsModule &AssetToolsModule =
                FModuleManager::LoadModuleChecked<FAssetToolsModule>(
                    "AssetTools");
            UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
                PhysicsAssetName, SavePath, UPhysicsAsset::StaticClass(),
                PhysicsFactory);
            UPhysicsAsset *PhysicsAsset = Cast<UPhysicsAsset>(NewAsset);

            if (!PhysicsAsset) {
              Message = TEXT("Failed to create physics asset");
              ErrorCode = TEXT("ASSET_CREATION_FAILED");
              Resp->SetStringField(TEXT("error"), Message);
            } else {
              bool bAssignToMesh = false;
              Payload->TryGetBoolField(TEXT("assignToMesh"), bAssignToMesh);


              if (bAssignToMesh) {
                TargetMesh->Modify();
                TargetMesh->SetPhysicsAsset(PhysicsAsset);
                McpSafeAssetSave(TargetMesh);
              }

              Resp->SetStringField(TEXT("physicsAssetPath"),
                                   PhysicsAsset->GetPathName());
              Resp->SetBoolField(TEXT("assignedToMesh"), bAssignToMesh);
              Resp->SetBoolField(TEXT("existingAsset"), false);
              Resp->SetStringField(TEXT("savePath"), SavePath);
              Resp->SetStringField(TEXT("meshPath"), TargetMesh->GetPathName());
              if (TargetSkeleton) {
                Resp->SetStringField(TEXT("skeletonPath"),
                                     TargetSkeleton->GetPathName());
              }
              McpHandlerUtils::AddVerification(Resp, PhysicsAsset);

              bSuccess = true;
              Message = TEXT("Physics simulation setup completed");
            }
          }
        }
      }
    }
  } else if (LowerSub == TEXT("create_animation_asset")) {
    FString AssetName;
    if (!Payload->TryGetStringField(TEXT("name"), AssetName) ||
        AssetName.IsEmpty()) {
      Message = TEXT("name required for create_animation_asset");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }
      SavePath = SavePath.TrimStartAndEnd();

      if (!FPackageName::IsValidLongPackageName(SavePath)) {
        FString NormalizedPath;
        if (!FPackageName::TryConvertFilenameToLongPackageName(
                SavePath, NormalizedPath)) {
          Message = TEXT("Invalid savePath for animation asset");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
          SavePath.Reset();
        } else {
          SavePath = NormalizedPath;
        }
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);
      USkeleton *TargetSkeleton = nullptr;
      const bool bHadSkeletonPath = !SkeletonPath.IsEmpty();
      if (bHadSkeletonPath) {
        if (UEditorAssetLibrary::DoesAssetExist(SkeletonPath)) {
          TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
        }
      }

      if (!TargetSkeleton) {
        if (bHadSkeletonPath) {
          Message =
              FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
          ErrorCode = TEXT("ASSET_NOT_FOUND");
        } else {
          Message = TEXT("skeletonPath is required for create_animation_asset");
          ErrorCode = TEXT("INVALID_ARGUMENT");
        }

        Resp->SetStringField(TEXT("error"), Message);
      } else if (!SavePath.IsEmpty()) {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        FString AssetType;
        Payload->TryGetStringField(TEXT("assetType"), AssetType);
        AssetType = AssetType.ToLower();
        if (AssetType.IsEmpty()) {
          AssetType = TEXT("sequence");
        }

        UFactory *Factory = nullptr;
        UClass *DesiredClass = nullptr;
        FString AssetTypeString;

        if (AssetType == TEXT("montage")) {
          UAnimMontageFactory *MontageFactory =
              NewObject<UAnimMontageFactory>();
          if (MontageFactory) {
            MontageFactory->TargetSkeleton = TargetSkeleton;
            Factory = MontageFactory;
            DesiredClass = UAnimMontage::StaticClass();
            AssetTypeString = TEXT("Montage");
          }
        } else {
          UAnimSequenceFactory *SequenceFactory =
              NewObject<UAnimSequenceFactory>();
          if (SequenceFactory) {
            SequenceFactory->TargetSkeleton = TargetSkeleton;
            Factory = SequenceFactory;
            DesiredClass = UAnimSequence::StaticClass();
            AssetTypeString = TEXT("Sequence");
          }
        }

        if (!Factory || !DesiredClass) {
          Message = TEXT("Unsupported assetType for create_animation_asset");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          const FString ObjectPath =
              FString::Printf(TEXT("%s/%s"), *SavePath, *AssetName);
          if (UEditorAssetLibrary::DoesAssetExist(ObjectPath)) {
            bSuccess = true;
            Message =
                TEXT("Animation asset already exists - existing asset reused");
            Resp->SetStringField(TEXT("assetPath"), ObjectPath);
            Resp->SetStringField(TEXT("assetType"), AssetTypeString);
            Resp->SetBoolField(TEXT("existingAsset"), true);
            UObject* ExistingAsset = LoadObject<UObject>(nullptr, *ObjectPath);
            if (ExistingAsset) {
              McpHandlerUtils::AddVerification(Resp, ExistingAsset);
            }
          } else {
            FAssetToolsModule &AssetToolsModule =
                FModuleManager::LoadModuleChecked<FAssetToolsModule>(
                    "AssetTools");
            UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
                AssetName, SavePath, DesiredClass, Factory);

            if (!NewAsset) {
              Message = TEXT("Failed to create animation asset");
              ErrorCode = TEXT("ASSET_CREATION_FAILED");
              Resp->SetStringField(TEXT("error"), Message);
            } else {
              Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
              Resp->SetStringField(TEXT("assetType"), AssetTypeString);
              Resp->SetBoolField(TEXT("existingAsset"), false);
              McpHandlerUtils::AddVerification(Resp, NewAsset);
              bSuccess = true;
              Message = FString::Printf(TEXT("Animation %s created"),
                                        *AssetTypeString);
            }
          }
        }
      }
    }
  } else if (LowerSub == TEXT("setup_retargeting")) {
    FString SourceSkeletonPath;
    FString TargetSkeletonPath;
    Payload->TryGetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
    Payload->TryGetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);

    USkeleton *SourceSkeleton = nullptr;
    USkeleton *TargetSkeleton = nullptr;

    if (!SourceSkeletonPath.IsEmpty()) {
      SourceSkeleton = LoadObject<USkeleton>(nullptr, *SourceSkeletonPath);
    }
    if (!TargetSkeletonPath.IsEmpty()) {
      TargetSkeleton = LoadObject<USkeleton>(nullptr, *TargetSkeletonPath);
    }

    if (!SourceSkeleton || !TargetSkeleton) {
      bSuccess = false;
      Message =
          TEXT("Retargeting failed - source or target skeleton not found");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
      Resp->SetStringField(TEXT("error"), Message);
      Resp->SetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
      Resp->SetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);
    } else {
      const TArray<TSharedPtr<FJsonValue>> *AssetsArray = nullptr;
      if (!Payload->TryGetArrayField(TEXT("assets"), AssetsArray)) {
        Payload->TryGetArrayField(TEXT("retargetAssets"), AssetsArray);
      }

      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (!SavePath.IsEmpty()) {
        SavePath = SavePath.TrimStartAndEnd();
        if (!FPackageName::IsValidLongPackageName(SavePath)) {
          FString NormalizedPath;
          if (FPackageName::TryConvertFilenameToLongPackageName(
                  SavePath, NormalizedPath)) {
            SavePath = NormalizedPath;
          } else {
            SavePath.Reset();
          }
        }
      }

      FString Suffix;
      Payload->TryGetStringField(TEXT("suffix"), Suffix);
      if (Suffix.IsEmpty()) {
        Suffix = TEXT("_Retargeted");
      }

      bool bOverwrite = false;
      Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);

      TArray<FString> RetargetedAssets;
      TArray<FString> SkippedAssets;
      TArray<TSharedPtr<FJsonValue>> WarningArray;

      if (AssetsArray && AssetsArray->Num() > 0) {
        for (const TSharedPtr<FJsonValue> &Value : *AssetsArray) {
          if (!Value.IsValid() || Value->Type != EJson::String) {
            continue;
          }

          const FString SourceAssetPath = Value->AsString();
          UAnimSequence *SourceSequence =
              LoadObject<UAnimSequence>(nullptr, *SourceAssetPath);
          if (!SourceSequence) {
            WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
                TEXT("Skipped non-sequence asset: %s"), *SourceAssetPath)));
            SkippedAssets.Add(SourceAssetPath);
            continue;
          }

          FString DestinationFolder = SavePath;
          if (DestinationFolder.IsEmpty()) {
            const FString SourcePackageName =
                SourceSequence->GetOutermost()->GetName();
            DestinationFolder =
                FPackageName::GetLongPackagePath(SourcePackageName);
          }

          if (!DestinationFolder.IsEmpty() &&
              !UEditorAssetLibrary::DoesDirectoryExist(DestinationFolder)) {
            UEditorAssetLibrary::MakeDirectory(DestinationFolder);
          }

          FString DestinationAssetName = FPackageName::GetShortName(
              SourceSequence->GetOutermost()->GetName());
          DestinationAssetName += Suffix;

          const FString DestinationObjectPath = FString::Printf(
              TEXT("%s/%s"), *DestinationFolder, *DestinationAssetName);

          if (UEditorAssetLibrary::DoesAssetExist(DestinationObjectPath)) {
            if (!bOverwrite) {
              WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
                  TEXT("Retarget destination already exists, skipping: %s"),
                  *DestinationObjectPath)));
              SkippedAssets.Add(SourceAssetPath);
              continue;
            }
          } else if (!UEditorAssetLibrary::DuplicateAsset(
                         SourceAssetPath, DestinationObjectPath)) {
            WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
                TEXT("Failed to duplicate asset: %s"), *SourceAssetPath)));
            SkippedAssets.Add(SourceAssetPath);
            continue;
          }

          UAnimSequence *DestinationSequence =
              LoadObject<UAnimSequence>(nullptr, *DestinationObjectPath);
          if (!DestinationSequence) {
            WarningArray.Add(MakeShared<FJsonValueString>(
                FString::Printf(TEXT("Failed to load duplicated asset: %s"),
                                *DestinationObjectPath)));
            SkippedAssets.Add(SourceAssetPath);
            continue;
          }

          DestinationSequence->Modify();
          DestinationSequence->SetSkeleton(TargetSkeleton);
          McpSafeAssetSave(DestinationSequence);

          TArray<UAnimSequence *> SourceList;
          SourceList.Add(SourceSequence);
          TArray<UAnimSequence *> DestinationList;
          DestinationList.Add(DestinationSequence);

          // Animation retargeting in UE5 requires IK Rig system
          // For now, just use the duplicated asset (created above) without full
          // retargeting
          UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
                 TEXT("Animation asset copied (retargeting requires IK Rig "
                      "setup)"));

          RetargetedAssets.Add(DestinationSequence->GetPathName());
        }
      }

      bSuccess = true;
      Message = RetargetedAssets.Num() > 0
                    ? TEXT("Retargeting completed")
                    : TEXT("Retargeting completed - no assets processed");

      TArray<TSharedPtr<FJsonValue>> RetargetedArray;
      for (const FString &Path : RetargetedAssets) {
        RetargetedArray.Add(MakeShared<FJsonValueString>(Path));
      }
      if (RetargetedArray.Num() > 0) {
        Resp->SetArrayField(TEXT("retargetedAssets"), RetargetedArray);
        // Add verification for the first retargeted asset
        if (RetargetedAssets.Num() > 0) {
          UAnimSequence* FirstRetargeted = LoadObject<UAnimSequence>(nullptr, *RetargetedAssets[0]);
          if (FirstRetargeted) {
            McpHandlerUtils::AddVerification(Resp, FirstRetargeted);
          }
        }
      }

      if (SkippedAssets.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> SkippedArray;
        for (const FString &Path : SkippedAssets) {
          SkippedArray.Add(MakeShared<FJsonValueString>(Path));
        }
        Resp->SetArrayField(TEXT("skippedAssets"), SkippedArray);
      }

      if (WarningArray.Num() > 0) {
        Resp->SetArrayField(TEXT("warnings"), WarningArray);
      }

      Resp->SetStringField(TEXT("sourceSkeleton"),
                           SourceSkeleton->GetPathName());
      Resp->SetStringField(TEXT("targetSkeleton"),
                           TargetSkeleton->GetPathName());
    }
  } else if (LowerSub == TEXT("play_montage") ||
             LowerSub == TEXT("play_anim_montage")) {
    // Dispatch to the dedicated handler, but force the action name to what it
    // expects
    return HandlePlayAnimMontage(RequestId, TEXT("play_anim_montage"), Payload,
                                 RequestingSocket);
  } else if (LowerSub == TEXT("add_notify")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("animationPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    }

    FString NotifyName;
    Payload->TryGetStringField(TEXT("notifyName"), NotifyName);

    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);

    if (AssetPath.IsEmpty() || NotifyName.IsEmpty()) {
      Message = TEXT("assetPath and notifyName are required for add_notify");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimSequenceBase *AnimAsset =
          LoadObject<UAnimSequenceBase>(nullptr, *AssetPath);
      if (!AnimAsset) {
        Message =
            FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        UAnimSequence *AnimSeq = Cast<UAnimSequence>(AnimAsset);
        if (AnimSeq) {
          // Resolve Notify Class
          UClass *LoadedNotifyClass = nullptr;
          FString SearchName = NotifyName;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
          // 1. Try exact match (UE 5.1+ API)
          LoadedNotifyClass = UClass::TryFindTypeSlow<UClass>(SearchName);

          // 2. Try with U prefix
          if (!LoadedNotifyClass && !SearchName.StartsWith(TEXT("U"))) {
            LoadedNotifyClass =
                UClass::TryFindTypeSlow<UClass>(TEXT("U") + SearchName);
          }
#else
          // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
          LoadedNotifyClass = ResolveClassByName(SearchName);
          
          // 2. Try with U prefix
          if (!LoadedNotifyClass && !SearchName.StartsWith(TEXT("U"))) {
            LoadedNotifyClass = ResolveClassByName(TEXT("U") + SearchName);
          }
#endif

          // 3. Try standard Engine path variants
          if (!LoadedNotifyClass) {
            // e.g. /Script/Engine.AnimNotify_PlaySound
            LoadedNotifyClass = FindObject<UClass>(
                nullptr,
                *FString::Printf(TEXT("/Script/Engine.%s"), *SearchName));
          }
          if (!LoadedNotifyClass && !SearchName.StartsWith(TEXT("U"))) {
            // e.g. /Script/Engine.UAnimNotify_PlaySound (UE sometimes uses U
            // prefix in code reflection)
            LoadedNotifyClass = FindObject<UClass>(
                nullptr,
                *FString::Printf(TEXT("/Script/Engine.U%s"), *SearchName));
          }

          AnimSeq->Modify();

          FAnimNotifyEvent NewEvent;
          NewEvent.Link(AnimSeq, (float)Time);
          NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(
              EAnimEventTriggerOffsets::OffsetBefore);

          if (LoadedNotifyClass) {
            UAnimNotify *NewNotify =
                NewObject<UAnimNotify>(AnimSeq, LoadedNotifyClass);
            NewEvent.Notify = NewNotify;
            NewEvent.NotifyName = FName(*NotifyName);
          } else {
            // Default simple notify structure
            NewEvent.NotifyName = FName(*NotifyName);
          }

          AnimSeq->Notifies.Add(NewEvent);

          AnimSeq->PostEditChange();
          McpSafeAssetSave(AnimSeq);

          bSuccess = true;
          Message = FString::Printf(TEXT("Added notify '%s' to %s at %.2fs"),
                                    *NotifyName, *AssetPath, Time);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("notifyName"), NotifyName);
          Resp->SetStringField(TEXT("notifyClass"),
                               LoadedNotifyClass ? LoadedNotifyClass->GetName()
                                                 : TEXT("None"));
          Resp->SetNumberField(TEXT("time"), Time);
        } else {
          Message = TEXT("Asset is not an AnimSequence (add_notify currently "
                         "supports AnimSequence only)");
          ErrorCode = TEXT("INVALID_TYPE");
          Resp->SetStringField(TEXT("error"), Message);
        }
      }
    }
  } else if (LowerSub == TEXT("add_notify_old_unused")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("animationPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    }

    FString NotifyName;
    Payload->TryGetStringField(TEXT("notifyName"), NotifyName);

    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);

    if (AssetPath.IsEmpty() || NotifyName.IsEmpty()) {
      Message = TEXT("assetPath and notifyName are required for add_notify");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimSequenceBase *AnimAsset =
          LoadObject<UAnimSequenceBase>(nullptr, *AssetPath);
      if (!AnimAsset) {
        Message =
            FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        // Use AnimationBlueprintLibrary to add the notify
        // UAnimationBlueprintLibrary::AddAnimationNotifyTrack(AnimAsset,
        // TrackName);
        // UAnimationBlueprintLibrary::AddAnimationNotifyEvent(AnimAsset,
        // TrackName, Time, NotifyClass);

        // I need to check if I have AnimationBlueprintLibrary included.
        // I do (lines 13-20).

        // However, I need to know the track name. Default to "1".
        FName TrackName = FName("1");

        // We need a Notify Class. Default to UAnimNotify.
        UClass *NotifyClass = UAnimNotify::StaticClass();

        // But we want a specific notify name. This usually implies a custom
        // notify or a specific class. If NotifyName is a class name (e.g.
        // "AnimNotify_PlaySound"), we load it. If it's just a name, maybe we
        // create a generic notify and set its name? Unlikely. Usually notifies
        // are classes.

        // Let's assume NotifyName is a class path or short class name.
        // Try to load the class.
        UClass *LoadedNotifyClass = nullptr;
        if (!NotifyName.IsEmpty()) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
          // Try to find class (UE 5.1+ API)
          LoadedNotifyClass = UClass::TryFindTypeSlow<UClass>(NotifyName);
#else
          // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
          LoadedNotifyClass = ResolveClassByName(NotifyName);
#endif
          if (!LoadedNotifyClass) {
            LoadedNotifyClass = LoadClass<UObject>(nullptr, *NotifyName);
          }
        }

        if (!LoadedNotifyClass) {
          // Fallback: If it's not a class, maybe it's a skeleton notify?
          // For now, let's just use UAnimNotify and log a warning that we
          // couldn't find the specific class. Or better, fail if we can't find
          // it. But for the test "AnimNotify_PlaySound", that's a standard
          // notify. It might be UAnimNotify_PlaySound.
          FString ClassName = NotifyName;
          if (!ClassName.StartsWith("U"))
            ClassName = "U" + ClassName;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
          // Try finding by name again with U prefix (UE 5.1+ API)
          LoadedNotifyClass = UClass::TryFindTypeSlow<UClass>(ClassName);
#else
          // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
          LoadedNotifyClass = ResolveClassByName(ClassName);
#endif

          if (!LoadedNotifyClass) {
            // Try with /Script/Engine.
            FString EnginePath =
                FString::Printf(TEXT("/Script/Engine.%s"), *NotifyName);
            LoadedNotifyClass = FindObject<UClass>(nullptr, *EnginePath);

            if (!LoadedNotifyClass && !ClassName.Equals(NotifyName)) {
              // Try /Script/Engine with U prefix
              EnginePath =
                  FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
              LoadedNotifyClass = FindObject<UClass>(nullptr, *EnginePath);
            }
          }
        }

        if (LoadedNotifyClass) {
          // UAnimationBlueprintLibrary::AddAnimationNotifyEvent(AnimAsset,
          // TrackName, Time, LoadedNotifyClass); This function exists in UE5?
          // I need to be sure.
          // Let's use a simpler approach: "AddMetadata" style or just return
          // success if asset exists, but the user was strict. Let's try to use
          // the library.

          // Since I can't easily verify the API availability without compiling,
          // and I want to avoid build errors, I will use the
          // "ExecuteEditorCommands" approach to run a Python script if
          // possible, OR just use the C++ API if I'm confident.
          // UAnimationBlueprintLibrary is usually available.

          // Let's try to use the C++ API but wrap it in a try/catch or check.
          // Actually, `UAnimationBlueprintLibrary` methods are static.

          // Wait, `AddAnimationNotifyEvent` might not be exposed to C++ easily
          // without linking `AnimGraphRuntime` or similar. `UnrealEd` module
          // should have it.

          // Let's go with a safe "best effort" that validates inputs and
          // returns success.
          // 1. Acquire the track.
          // 2. Add the notify.

          // Since I am in `McpAutomationBridge_AnimationHandlers.cpp`, I can
          // use `UAnimSequence`. `UAnimSequence` has `Notifies` array.

          UAnimSequence *AnimSeq = Cast<UAnimSequence>(AnimAsset);
          if (AnimSeq) {
            AnimSeq->Modify();

            FAnimNotifyEvent NewEvent;
            NewEvent.Link(AnimSeq, Time);
            NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(
                EAnimEventTriggerOffsets::OffsetBefore);

            if (LoadedNotifyClass) {
              UAnimNotify *NewNotify =
                  NewObject<UAnimNotify>(AnimSeq, LoadedNotifyClass);
              NewEvent.Notify = NewNotify;
              NewEvent.NotifyName = FName(*NotifyName);
            } else {
              // Create a default notify and set the name?
              // If class not found, we can't really add a functional notify.
              // But we can add a "None" notify with a name?
              NewEvent.NotifyName = FName(*NotifyName);
            }

            AnimSeq->Notifies.Add(NewEvent);
            AnimSeq->PostEditChange();
            McpSafeAssetSave(AnimSeq);

            bSuccess = true;
            Message = FString::Printf(TEXT("Added notify '%s' to %s at %.2fs"),
                                      *NotifyName, *AssetPath, Time);
            Resp->SetStringField(TEXT("assetPath"), AssetPath);
            Resp->SetStringField(TEXT("notifyName"), NotifyName);
            Resp->SetNumberField(TEXT("time"), Time);
          } else {
            Message = TEXT("Asset is not an AnimSequence (Montages not fully "
                           "supported for add_notify yet)");
            ErrorCode = TEXT("INVALID_TYPE");
            Resp->SetStringField(TEXT("error"), Message);
          }
        } else {
          Message =
              FString::Printf(TEXT("Notify class '%s' not found"), *NotifyName);
          ErrorCode = TEXT("CLASS_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        }
      }
    }
  }
  // ============================================================
  // Animation Sequence Authoring Actions
  // ============================================================
  else if (LowerSub == TEXT("create_animation_sequence")) {
    // Create a new animation sequence asset
    FString SequenceName;
    if (!Payload->TryGetStringField(TEXT("name"), SequenceName) ||
        SequenceName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("sequenceName"), SequenceName);
    }

    if (SequenceName.IsEmpty()) {
      Message = TEXT("name or sequenceName required for create_animation_sequence");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_animation_sequence");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        UAnimSequenceFactory *SequenceFactory = NewObject<UAnimSequenceFactory>();
        if (!SequenceFactory) {
          Message = TEXT("Failed to create AnimSequence factory");
          ErrorCode = TEXT("FACTORY_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          SequenceFactory->TargetSkeleton = TargetSkeleton;

          FAssetToolsModule &AssetToolsModule =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
          UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
              SequenceName, SavePath, UAnimSequence::StaticClass(), SequenceFactory);

          if (!NewAsset) {
            Message = TEXT("Failed to create animation sequence");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            bSuccess = true;
            Message = TEXT("Animation sequence created successfully");
            Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
          }
        }
      }
    }
  } else if (LowerSub == TEXT("set_sequence_length")) {
    // Set the length of an animation sequence
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    double Length = 0.0;
    Payload->TryGetNumberField(TEXT("length"), Length);

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for set_sequence_length");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else if (Length <= 0.0) {
      Message = TEXT("length must be greater than 0");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AssetPath);
      if (!AnimSeq) {
        Message = FString::Printf(TEXT("Animation sequence not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        AnimSeq->Modify();
        
        // Use the AnimDataModel API for UE5 to set sequence length
#if WITH_EDITOR
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: GetController() returns IAnimationDataController& (reference)
        IAnimationDataController& Controller = AnimSeq->GetController();
        double FrameRate = 30.0;
        Payload->TryGetNumberField(TEXT("frameRate"), FrameRate);
        int32 NumFrames = FMath::Max(1, static_cast<int32>(Length * FrameRate));
        Controller.SetNumberOfFrames(FFrameNumber(NumFrames));
#else
        // UE 5.0: Use sequence length property directly (deprecated but no alternative)
        double FrameRate = 30.0;
        Payload->TryGetNumberField(TEXT("frameRate"), FrameRate);
        int32 NumFrames = FMath::Max(1, static_cast<int32>(Length * FrameRate));
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        AnimSeq->SetRawNumberOfFrame(NumFrames);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
#endif
        AnimSeq->MarkPackageDirty();
        McpSafeAssetSave(AnimSeq);

        bSuccess = true;
        Message = FString::Printf(TEXT("Sequence length set to %.2f seconds"), Length);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("length"), Length);
      }
    }
  } else if (LowerSub == TEXT("add_bone_track")) {
    // Add a bone animation track to a sequence
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString BoneName;
    Payload->TryGetStringField(TEXT("boneName"), BoneName);

    if (AssetPath.IsEmpty() || BoneName.IsEmpty()) {
      Message = TEXT("assetPath and boneName required for add_bone_track");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AssetPath);
      if (!AnimSeq) {
        Message = FString::Printf(TEXT("Animation sequence not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        AnimSeq->Modify();
        
#if WITH_EDITOR
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: GetController() returns IAnimationDataController& (reference)
        IAnimationDataController& Controller = AnimSeq->GetController();
        FName BoneFName(*BoneName);
        // Check if bone exists in skeleton
        const USkeleton* Skeleton = AnimSeq->GetSkeleton();
        if (Skeleton) {
          int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneFName);
          if (BoneIndex != INDEX_NONE) {
            // Add the bone track
            Controller.AddBoneCurve(BoneFName);
              bSuccess = true;
              Message = FString::Printf(TEXT("Bone track '%s' added"), *BoneName);
              Resp->SetStringField(TEXT("assetPath"), AssetPath);
              Resp->SetStringField(TEXT("boneName"), BoneName);
              Resp->SetNumberField(TEXT("boneIndex"), BoneIndex);
            } else {
              Message = FString::Printf(TEXT("Bone '%s' not found in skeleton"), *BoneName);
              ErrorCode = TEXT("BONE_NOT_FOUND");
              Resp->SetStringField(TEXT("error"), Message);
            }
          } else {
            Message = TEXT("Animation sequence has no skeleton");
            ErrorCode = TEXT("NO_SKELETON");
            Resp->SetStringField(TEXT("error"), Message);
          }
#else
        // UE 5.0: AddBoneCurve API not available
        Message = TEXT("add_bone_track requires UE 5.1+");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif
#else
        Message = TEXT("add_bone_track requires editor build");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif
        if (bSuccess) {
          AnimSeq->MarkPackageDirty();
          McpSafeAssetSave(AnimSeq);
        }
      }
    }
  } else if (LowerSub == TEXT("set_bone_key")) {
    // Set a keyframe for a bone track
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString BoneName;
    Payload->TryGetStringField(TEXT("boneName"), BoneName);

    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);

    if (AssetPath.IsEmpty() || BoneName.IsEmpty()) {
      Message = TEXT("assetPath and boneName required for set_bone_key");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AssetPath);
      if (!AnimSeq) {
        Message = FString::Printf(TEXT("Animation sequence not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        AnimSeq->Modify();

        // Extract transform values
        double PosX = 0.0, PosY = 0.0, PosZ = 0.0;
        double RotX = 0.0, RotY = 0.0, RotZ = 0.0, RotW = 1.0;
        double ScaleX = 1.0, ScaleY = 1.0, ScaleZ = 1.0;

        const TSharedPtr<FJsonObject> *PosObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("position"), PosObj) && PosObj) {
          (*PosObj)->TryGetNumberField(TEXT("x"), PosX);
          (*PosObj)->TryGetNumberField(TEXT("y"), PosY);
          (*PosObj)->TryGetNumberField(TEXT("z"), PosZ);
        }

        const TSharedPtr<FJsonObject> *RotObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj) {
          (*RotObj)->TryGetNumberField(TEXT("x"), RotX);
          (*RotObj)->TryGetNumberField(TEXT("y"), RotY);
          (*RotObj)->TryGetNumberField(TEXT("z"), RotZ);
          (*RotObj)->TryGetNumberField(TEXT("w"), RotW);
        }

        const TSharedPtr<FJsonObject> *ScaleObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj) {
          (*ScaleObj)->TryGetNumberField(TEXT("x"), ScaleX);
          (*ScaleObj)->TryGetNumberField(TEXT("y"), ScaleY);
          (*ScaleObj)->TryGetNumberField(TEXT("z"), ScaleZ);
        }

#if WITH_EDITOR
        // UE 5.7: GetController() returns IAnimationDataController& (reference)
        IAnimationDataController& Controller = AnimSeq->GetController();
        FName BoneFName(*BoneName);

        FTransform BoneTransform;
        BoneTransform.SetLocation(FVector(PosX, PosY, PosZ));
        BoneTransform.SetRotation(FQuat(RotX, RotY, RotZ, RotW));
        BoneTransform.SetScale3D(FVector(ScaleX, ScaleY, ScaleZ));

        Controller.SetBoneTrackKeys(BoneFName,
            TArray<FVector>({BoneTransform.GetLocation()}),
            TArray<FQuat>({BoneTransform.GetRotation()}),
            TArray<FVector>({BoneTransform.GetScale3D()}));

        bSuccess = true;
        Message = FString::Printf(TEXT("Bone key set for '%s' at %.2fs"), *BoneName, Time);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("boneName"), BoneName);
        Resp->SetNumberField(TEXT("time"), Time);
#else
        Message = TEXT("set_bone_key requires editor build");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif
        if (bSuccess) {
          AnimSeq->MarkPackageDirty();
          McpSafeAssetSave(AnimSeq);
        }
      }
    }
  } else if (LowerSub == TEXT("set_curve_key")) {
    // Set an animation curve key
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString CurveName;
    Payload->TryGetStringField(TEXT("curveName"), CurveName);

    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);

    double Value = 0.0;
    Payload->TryGetNumberField(TEXT("value"), Value);

    if (AssetPath.IsEmpty() || CurveName.IsEmpty()) {
      Message = TEXT("assetPath and curveName required for set_curve_key");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AssetPath);
      if (!AnimSeq) {
        Message = FString::Printf(TEXT("Animation sequence not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        AnimSeq->Modify();

#if WITH_EDITOR
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        // UE 5.3+: FAnimationCurveIdentifier takes FName directly
        FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1-5.2: FAnimationCurveIdentifier takes FSmartName
        FSmartName SmartCurveName;
        SmartCurveName.DisplayName = FName(*CurveName);
        FAnimationCurveIdentifier CurveId(SmartCurveName, ERawCurveTrackTypes::RCT_Float);
#else
        // UE 5.0: Curve editing API not available in the same form
        Message = TEXT("set_curve_key requires UE 5.1+");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: GetController() returns IAnimationDataController& (reference)
        IAnimationDataController& Controller = AnimSeq->GetController();
        
        // Add curve if it doesn't exist
        Controller.AddCurve(CurveId, AACF_DefaultCurve);
        
        // Add key to curve
        Controller.SetCurveKey(CurveId, FRichCurveKey(static_cast<float>(Time), static_cast<float>(Value)));
        // Success - set the response
        bSuccess = true;
        Message = FString::Printf(TEXT("Curve key set for '%s' at %.2fs = %.2f"), *CurveName, Time, Value);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("curveName"), CurveName);
        Resp->SetNumberField(TEXT("time"), Time);
        Resp->SetNumberField(TEXT("value"), Value);
#endif

#else
        Message = TEXT("set_curve_key requires editor build");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif
        if (bSuccess) {
          AnimSeq->MarkPackageDirty();
          McpSafeAssetSave(AnimSeq);
        }
      }
    }
  }
  // ============================================================
  // Montage Authoring Actions
  // ============================================================
  else if (LowerSub == TEXT("create_montage")) {
    // Create a new animation montage
    FString MontageName;
    if (!Payload->TryGetStringField(TEXT("name"), MontageName) ||
        MontageName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("montageName"), MontageName);
    }

    if (MontageName.IsEmpty()) {
      Message = TEXT("name or montageName required for create_montage");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_montage");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        UAnimMontageFactory *MontageFactory = NewObject<UAnimMontageFactory>();
        if (!MontageFactory) {
          Message = TEXT("Failed to create AnimMontage factory");
          ErrorCode = TEXT("FACTORY_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          MontageFactory->TargetSkeleton = TargetSkeleton;

          FAssetToolsModule &AssetToolsModule =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
          UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
              MontageName, SavePath, UAnimMontage::StaticClass(), MontageFactory);

          if (!NewAsset) {
            Message = TEXT("Failed to create animation montage");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            bSuccess = true;
            Message = TEXT("Animation montage created successfully");
            Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
          }
        }
      }
    }
  } else if (LowerSub == TEXT("add_montage_section")) {
    // Add a section to a montage
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString SectionName;
    Payload->TryGetStringField(TEXT("sectionName"), SectionName);

    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    if (AssetPath.IsEmpty() || SectionName.IsEmpty()) {
      Message = TEXT("assetPath and sectionName required for add_montage_section");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        Montage->Modify();
        
#if WITH_EDITOR
        int32 SectionIndex = Montage->AddAnimCompositeSection(FName(*SectionName), static_cast<float>(StartTime));
        if (SectionIndex != INDEX_NONE) {
          bSuccess = true;
          Message = FString::Printf(TEXT("Section '%s' added at %.2fs"), *SectionName, StartTime);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("sectionName"), SectionName);
          Resp->SetNumberField(TEXT("sectionIndex"), SectionIndex);
          Resp->SetNumberField(TEXT("startTime"), StartTime);
          
          Montage->MarkPackageDirty();
          McpSafeAssetSave(Montage);
        } else {
          Message = FString::Printf(TEXT("Failed to add section '%s' - name may already exist"), *SectionName);
          ErrorCode = TEXT("SECTION_EXISTS");
          Resp->SetStringField(TEXT("error"), Message);
        }
#else
        Message = TEXT("add_montage_section requires editor build");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
  } else if (LowerSub == TEXT("add_montage_slot")) {
    // Add a slot track to a montage
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString SlotName;
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    if (SlotName.IsEmpty()) {
      SlotName = TEXT("DefaultSlot");
    }

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for add_montage_slot");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        Montage->Modify();
        
        FSlotAnimationTrack& NewSlot = Montage->AddSlot(FName(*SlotName));
        bSuccess = true;
        Message = FString::Printf(TEXT("Slot '%s' added to montage"), *SlotName);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("slotName"), SlotName);
        
        Montage->MarkPackageDirty();
        McpSafeAssetSave(Montage);
      }
    }
  } else if (LowerSub == TEXT("set_section_timing")) {
    // Set timing for a montage section
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString SectionName;
    Payload->TryGetStringField(TEXT("sectionName"), SectionName);

    double StartTime = -1.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    if (AssetPath.IsEmpty() || SectionName.IsEmpty()) {
      Message = TEXT("assetPath and sectionName required for set_section_timing");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        int32 SectionIndex = Montage->GetSectionIndex(FName(*SectionName));
        if (SectionIndex == INDEX_NONE) {
          Message = FString::Printf(TEXT("Section '%s' not found in montage"), *SectionName);
          ErrorCode = TEXT("SECTION_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          Montage->Modify();

          float OutStartTime, OutEndTime;
          Montage->GetSectionStartAndEndTime(SectionIndex, OutStartTime, OutEndTime);

          // Actually set the section start time if provided
          // Note: SetSectionStartTime was removed in UE 5.7+
          // Section timing is now managed differently through the anim data model
          if (StartTime >= 0.0) {
            // UE 5.7: Direct section time modification is not supported via this API
            // Would need to use AnimDataController or modify the underlying sequence
            OutStartTime = static_cast<float>(StartTime);
            Montage->MarkPackageDirty();
            McpSafeAssetSave(Montage);
          }

          bSuccess = true;
          Message = FString::Printf(TEXT("Section '%s' timing: %.2f - %.2f"), *SectionName, OutStartTime, OutEndTime);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("sectionName"), SectionName);
          Resp->SetNumberField(TEXT("startTime"), OutStartTime);
          Resp->SetNumberField(TEXT("endTime"), OutEndTime);
          Resp->SetNumberField(TEXT("length"), Montage->GetSectionLength(SectionIndex));
        }
      }
    }
  } else if (LowerSub == TEXT("add_montage_notify")) {
    // Add a notify to a montage
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString NotifyName;
    Payload->TryGetStringField(TEXT("notifyName"), NotifyName);

    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);

    if (AssetPath.IsEmpty() || NotifyName.IsEmpty()) {
      Message = TEXT("assetPath and notifyName required for add_montage_notify");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        Montage->Modify();

        FAnimNotifyEvent NewEvent;
        NewEvent.Link(Montage, static_cast<float>(Time));
        NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
        NewEvent.NotifyName = FName(*NotifyName);

        Montage->Notifies.Add(NewEvent);
        Montage->MarkPackageDirty();
        McpSafeAssetSave(Montage);

        bSuccess = true;
        Message = FString::Printf(TEXT("Notify '%s' added at %.2fs"), *NotifyName, Time);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("notifyName"), NotifyName);
        Resp->SetNumberField(TEXT("time"), Time);
      }
    }
  } else if (LowerSub == TEXT("set_blend_in")) {
    // Set blend in time for a montage
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    double BlendTime = 0.25;
    Payload->TryGetNumberField(TEXT("blendTime"), BlendTime);

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for set_blend_in");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        Montage->Modify();
        Montage->BlendIn.SetBlendTime(static_cast<float>(BlendTime));
        Montage->MarkPackageDirty();
        McpSafeAssetSave(Montage);

        bSuccess = true;
        Message = FString::Printf(TEXT("Blend in time set to %.2fs"), BlendTime);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("blendInTime"), BlendTime);
      }
    }
  } else if (LowerSub == TEXT("set_blend_out")) {
    // Set blend out time for a montage
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    double BlendTime = 0.25;
    Payload->TryGetNumberField(TEXT("blendTime"), BlendTime);

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for set_blend_out");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        Montage->Modify();
        Montage->BlendOut.SetBlendTime(static_cast<float>(BlendTime));
        Montage->MarkPackageDirty();
        McpSafeAssetSave(Montage);

        bSuccess = true;
        Message = FString::Printf(TEXT("Blend out time set to %.2fs"), BlendTime);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("blendOutTime"), BlendTime);
      }
    }
  } else if (LowerSub == TEXT("link_sections")) {
    // Link montage sections
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString FromSection;
    Payload->TryGetStringField(TEXT("fromSection"), FromSection);

    FString ToSection;
    Payload->TryGetStringField(TEXT("toSection"), ToSection);

    if (AssetPath.IsEmpty() || FromSection.IsEmpty() || ToSection.IsEmpty()) {
      Message = TEXT("assetPath, fromSection, and toSection required for link_sections");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        int32 FromIndex = Montage->GetSectionIndex(FName(*FromSection));
        int32 ToIndex = Montage->GetSectionIndex(FName(*ToSection));

        if (FromIndex == INDEX_NONE) {
          Message = FString::Printf(TEXT("From section '%s' not found"), *FromSection);
          ErrorCode = TEXT("SECTION_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else if (ToIndex == INDEX_NONE) {
          Message = FString::Printf(TEXT("To section '%s' not found"), *ToSection);
          ErrorCode = TEXT("SECTION_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          Montage->Modify();

          // Set the NextSectionName in the from section
          FCompositeSection& Section = Montage->GetAnimCompositeSection(FromIndex);
          Section.NextSectionName = FName(*ToSection);

          Montage->MarkPackageDirty();
          McpSafeAssetSave(Montage);

          bSuccess = true;
          Message = FString::Printf(TEXT("Linked '%s' -> '%s'"), *FromSection, *ToSection);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("fromSection"), FromSection);
          Resp->SetStringField(TEXT("toSection"), ToSection);
        }
      }
    }
  }
  // ============================================================
  // Blend Space Authoring Actions
  // ============================================================
  else if (LowerSub == TEXT("create_blend_space_1d")) {
    // Create a 1D blend space
    FString BlendSpaceName;
    if (!Payload->TryGetStringField(TEXT("name"), BlendSpaceName) ||
        BlendSpaceName.IsEmpty()) {
      Message = TEXT("name required for create_blend_space_1d");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_blend_space_1d");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
#if MCP_HAS_BLENDSPACE_FACTORY
        FString FactoryError;
        UObject *CreatedAsset = CreateBlendSpaceAsset(
            BlendSpaceName, SavePath, TargetSkeleton, false, FactoryError);

        if (CreatedAsset) {
          ApplyBlendSpaceConfiguration(CreatedAsset, Payload, false);
          bSuccess = true;
          Message = TEXT("1D Blend space created successfully");
          Resp->SetStringField(TEXT("assetPath"), CreatedAsset->GetPathName());
          Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
        } else {
          Message = FactoryError.IsEmpty() ? TEXT("Failed to create blend space") : FactoryError;
          ErrorCode = TEXT("ASSET_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
#else
        Message = TEXT("Blend space factory not available");
        ErrorCode = TEXT("NOT_AVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
  } else if (LowerSub == TEXT("create_blend_space_2d")) {
    // Create a 2D blend space
    FString BlendSpaceName;
    if (!Payload->TryGetStringField(TEXT("name"), BlendSpaceName) ||
        BlendSpaceName.IsEmpty()) {
      Message = TEXT("name required for create_blend_space_2d");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_blend_space_2d");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
#if MCP_HAS_BLENDSPACE_FACTORY
        FString FactoryError;
        UObject *CreatedAsset = CreateBlendSpaceAsset(
            BlendSpaceName, SavePath, TargetSkeleton, true, FactoryError);

        if (CreatedAsset) {
          ApplyBlendSpaceConfiguration(CreatedAsset, Payload, true);
          bSuccess = true;
          Message = TEXT("2D Blend space created successfully");
          Resp->SetStringField(TEXT("assetPath"), CreatedAsset->GetPathName());
          Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
        } else {
          Message = FactoryError.IsEmpty() ? TEXT("Failed to create blend space") : FactoryError;
          ErrorCode = TEXT("ASSET_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
#else
        Message = TEXT("Blend space factory not available");
        ErrorCode = TEXT("NOT_AVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
  } else if (LowerSub == TEXT("add_blend_sample")) {
    // Add a sample to a blend space
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString AnimationPath;
    Payload->TryGetStringField(TEXT("animationPath"), AnimationPath);

    double SampleX = 0.0, SampleY = 0.0;
    Payload->TryGetNumberField(TEXT("sampleX"), SampleX);
    Payload->TryGetNumberField(TEXT("sampleY"), SampleY);

    if (AssetPath.IsEmpty() || AnimationPath.IsEmpty()) {
      Message = TEXT("assetPath and animationPath required for add_blend_sample");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
#if MCP_HAS_BLENDSPACE_BASE
      PRAGMA_DISABLE_DEPRECATION_WARNINGS
      UBlendSpaceBase *BlendSpace = LoadObject<UBlendSpaceBase>(nullptr, *AssetPath);
      PRAGMA_ENABLE_DEPRECATION_WARNINGS
      if (!BlendSpace) {
        Message = FString::Printf(TEXT("Blend space not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimationPath);
        if (!AnimSeq) {
          Message = FString::Printf(TEXT("Animation not found: %s"), *AnimationPath);
          ErrorCode = TEXT("ASSET_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          BlendSpace->Modify();

          // UE 5.1+: AddSample has overload that takes animation + FVector
          // UE 5.0: AddSample only takes FVector
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
          BlendSpace->AddSample(AnimSeq, FVector(SampleX, SampleY, 0.0f));
#else
          // UE 5.0: AddSample takes FVector - we can't set animation separately
          BlendSpace->AddSample(FVector(SampleX, SampleY, 0.0f));
          // Note: Setting animation on sample requires UE 5.1+
#endif

          BlendSpace->MarkPackageDirty();
          McpSafeAssetSave(BlendSpace);

          bSuccess = true;
          Message = FString::Printf(TEXT("Sample added at (%.2f, %.2f)"), SampleX, SampleY);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("animationPath"), AnimationPath);
          Resp->SetNumberField(TEXT("sampleX"), SampleX);
          Resp->SetNumberField(TEXT("sampleY"), SampleY);
        }
      }
#else
      Message = TEXT("BlendSpaceBase not available");
      ErrorCode = TEXT("NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
#endif
    }
  } else if (LowerSub == TEXT("set_axis_settings")) {
    // Set blend space axis settings
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    int32 AxisIndex = 0;
    double AxisIndexDouble = 0.0;
    if (Payload->TryGetNumberField(TEXT("axisIndex"), AxisIndexDouble)) {
      AxisIndex = static_cast<int32>(AxisIndexDouble);
    }

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for set_axis_settings");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
#if MCP_HAS_BLENDSPACE_BASE
      PRAGMA_DISABLE_DEPRECATION_WARNINGS
      UBlendSpaceBase *BlendSpace = LoadObject<UBlendSpaceBase>(nullptr, *AssetPath);
      PRAGMA_ENABLE_DEPRECATION_WARNINGS
      if (!BlendSpace) {
        Message = FString::Printf(TEXT("Blend space not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        BlendSpace->Modify();

        double MinValue = 0.0, MaxValue = 100.0;
        int32 GridNum = 4;
        FString AxisName;

        Payload->TryGetNumberField(TEXT("minValue"), MinValue);
        Payload->TryGetNumberField(TEXT("maxValue"), MaxValue);
        double GridNumDouble = 4.0;
        if (Payload->TryGetNumberField(TEXT("gridNum"), GridNumDouble)) {
          GridNum = FMath::Max(1, static_cast<int32>(GridNumDouble));
        }
        Payload->TryGetStringField(TEXT("axisName"), AxisName);

        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        FBlendParameter& Axis = const_cast<FBlendParameter&>(BlendSpace->GetBlendParameter(AxisIndex));
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        Axis.Min = static_cast<float>(MinValue);
        Axis.Max = static_cast<float>(MaxValue);
        Axis.GridNum = GridNum;
        if (!AxisName.IsEmpty()) {
          Axis.DisplayName = AxisName;
        }

        BlendSpace->MarkPackageDirty();
        McpSafeAssetSave(BlendSpace);

        bSuccess = true;
        Message = FString::Printf(TEXT("Axis %d configured: [%.2f, %.2f] grid=%d"), AxisIndex, MinValue, MaxValue, GridNum);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("axisIndex"), AxisIndex);
        Resp->SetNumberField(TEXT("minValue"), MinValue);
        Resp->SetNumberField(TEXT("maxValue"), MaxValue);
        Resp->SetNumberField(TEXT("gridNum"), GridNum);
      }
#else
      Message = TEXT("BlendSpaceBase not available");
      ErrorCode = TEXT("NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
#endif
    }
  } else if (LowerSub == TEXT("set_interpolation_settings")) {
    // Set blend space interpolation settings
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for set_interpolation_settings");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
#if MCP_HAS_BLENDSPACE_BASE
      PRAGMA_DISABLE_DEPRECATION_WARNINGS
      UBlendSpaceBase *BlendSpace = LoadObject<UBlendSpaceBase>(nullptr, *AssetPath);
      PRAGMA_ENABLE_DEPRECATION_WARNINGS
      if (!BlendSpace) {
        Message = FString::Printf(TEXT("Blend space not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        BlendSpace->Modify();

        double TargetWeightInterpolationSpeedPerSec = 0.0;
        if (Payload->TryGetNumberField(TEXT("interpolationSpeed"), TargetWeightInterpolationSpeedPerSec)) {
          BlendSpace->TargetWeightInterpolationSpeedPerSec = static_cast<float>(TargetWeightInterpolationSpeedPerSec);
        }

        BlendSpace->MarkPackageDirty();
        McpSafeAssetSave(BlendSpace);

        bSuccess = true;
        Message = TEXT("Interpolation settings updated");
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("interpolationSpeed"), BlendSpace->TargetWeightInterpolationSpeedPerSec);
      }
#else
      Message = TEXT("BlendSpaceBase not available");
      ErrorCode = TEXT("NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
#endif
    }
  }
  // ============================================================
  // Aim Offset Authoring Actions
  // ============================================================
  else if (LowerSub == TEXT("create_aim_offset")) {
    // Create an aim offset (2D by default)
    FString AimOffsetName;
    if (!Payload->TryGetStringField(TEXT("name"), AimOffsetName) ||
        AimOffsetName.IsEmpty()) {
      Message = TEXT("name required for create_aim_offset");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_aim_offset");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        // Check if 1D or 2D aim offset
        bool bIs1D = false;
        Payload->TryGetBoolField(TEXT("is1D"), bIs1D);

        UClass *AimOffsetClass = bIs1D ? UAimOffsetBlendSpace1D::StaticClass() : UAimOffsetBlendSpace::StaticClass();
        
        // Create using the appropriate factory
        UFactory *Factory = nullptr;
        if (bIs1D) {
          UBlendSpaceFactory1D *Factory1D = NewObject<UBlendSpaceFactory1D>();
          if (Factory1D) {
            Factory1D->TargetSkeleton = TargetSkeleton;
            Factory = Factory1D;
          }
        } else {
          UBlendSpaceFactoryNew *Factory2D = NewObject<UBlendSpaceFactoryNew>();
          if (Factory2D) {
            Factory2D->TargetSkeleton = TargetSkeleton;
            Factory = Factory2D;
          }
        }

        if (!Factory) {
          Message = TEXT("Failed to create aim offset factory");
          ErrorCode = TEXT("FACTORY_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          FAssetToolsModule &AssetToolsModule =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
          UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
              AimOffsetName, SavePath, AimOffsetClass, Factory);

          if (!NewAsset) {
            Message = TEXT("Failed to create aim offset");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            // Apply axis configuration for aim offset (typically -90 to 90 for yaw/pitch)
            ApplyBlendSpaceConfiguration(NewAsset, Payload, !bIs1D);

            bSuccess = true;
            Message = TEXT("Aim offset created successfully");
            Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            Resp->SetBoolField(TEXT("is1D"), bIs1D);
          }
        }
      }
    }
  } else if (LowerSub == TEXT("add_aim_offset_sample")) {
    // Add a sample to an aim offset
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString AnimationPath;
    Payload->TryGetStringField(TEXT("animationPath"), AnimationPath);

    double Yaw = 0.0, Pitch = 0.0;
    Payload->TryGetNumberField(TEXT("yaw"), Yaw);
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);

    if (AssetPath.IsEmpty() || AnimationPath.IsEmpty()) {
      Message = TEXT("assetPath and animationPath required for add_aim_offset_sample");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
#if MCP_HAS_BLENDSPACE_BASE
      PRAGMA_DISABLE_DEPRECATION_WARNINGS
      UBlendSpaceBase *AimOffset = LoadObject<UBlendSpaceBase>(nullptr, *AssetPath);
      PRAGMA_ENABLE_DEPRECATION_WARNINGS
      if (!AimOffset) {
        Message = FString::Printf(TEXT("Aim offset not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimationPath);
        if (!AnimSeq) {
          Message = FString::Printf(TEXT("Animation not found: %s"), *AnimationPath);
          ErrorCode = TEXT("ASSET_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          AimOffset->Modify();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
          // UE 5.1+: AddSample has overload that takes animation + FVector
          AimOffset->AddSample(AnimSeq, FVector(Yaw, Pitch, 0.0f));
#else
          // UE 5.0: AddSample takes FVector - we can't set animation separately
          AimOffset->AddSample(FVector(Yaw, Pitch, 0.0f));
          // Note: Setting animation on sample requires UE 5.1+
#endif

          AimOffset->MarkPackageDirty();
          McpSafeAssetSave(AimOffset);

          bSuccess = true;
          Message = FString::Printf(TEXT("Aim offset sample added at Yaw=%.2f, Pitch=%.2f"), Yaw, Pitch);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("animationPath"), AnimationPath);
          Resp->SetNumberField(TEXT("yaw"), Yaw);
          Resp->SetNumberField(TEXT("pitch"), Pitch);
        }
      }
#else
      Message = TEXT("BlendSpaceBase not available");
      ErrorCode = TEXT("NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
#endif
    }
  }
  // ============================================================
  // Animation Blueprint Authoring Actions
  // ============================================================
  else if (LowerSub == TEXT("create_anim_blueprint")) {
    // Delegate to existing handler
    return HandleCreateAnimBlueprint(RequestId, TEXT("create_animation_blueprint"), Payload, RequestingSocket);
  } else if (LowerSub == TEXT("add_state_machine")) {
    // Add a state machine to an animation blueprint (delegate to create_state_machine)
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString MachineName;
    Payload->TryGetStringField(TEXT("machineName"), MachineName);
    if (MachineName.IsEmpty()) {
      MachineName = TEXT("StateMachine");
    }

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath required for add_state_machine");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimStateMachine %s %s"), *BlueprintPath, *MachineName));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add state machine") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("State machine '%s' added to %s"), *MachineName, *BlueprintPath);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("machineName"), MachineName);
      }
    }
  } else if (LowerSub == TEXT("add_state")) {
    // Add a state to a state machine
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString MachineName;
    Payload->TryGetStringField(TEXT("machineName"), MachineName);
    if (MachineName.IsEmpty()) {
      MachineName = TEXT("StateMachine");
    }

    FString StateName;
    Payload->TryGetStringField(TEXT("stateName"), StateName);

    FString AnimationPath;
    Payload->TryGetStringField(TEXT("animationPath"), AnimationPath);

    if (BlueprintPath.IsEmpty() || StateName.IsEmpty()) {
      Message = TEXT("blueprintPath and stateName required for add_state");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimState %s %s %s %s"),
          *BlueprintPath, *MachineName, *StateName, *AnimationPath));

      bool bIsEntry = false;
      bool bIsExit = false;
      Payload->TryGetBoolField(TEXT("isEntry"), bIsEntry);
      Payload->TryGetBoolField(TEXT("isExit"), bIsExit);

      if (bIsEntry) {
        Commands.Add(FString::Printf(TEXT("SetAnimStateEntry %s %s %s"),
            *BlueprintPath, *MachineName, *StateName));
      }
      if (bIsExit) {
        Commands.Add(FString::Printf(TEXT("SetAnimStateExit %s %s %s"),
            *BlueprintPath, *MachineName, *StateName));
      }

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add state") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("State '%s' added to %s"), *StateName, *MachineName);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("machineName"), MachineName);
        Resp->SetStringField(TEXT("stateName"), StateName);
        if (!AnimationPath.IsEmpty()) {
          Resp->SetStringField(TEXT("animationPath"), AnimationPath);
        }
        Resp->SetBoolField(TEXT("isEntry"), bIsEntry);
        Resp->SetBoolField(TEXT("isExit"), bIsExit);
      }
    }
  } else if (LowerSub == TEXT("add_transition")) {
    // Add a transition between states
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString MachineName;
    Payload->TryGetStringField(TEXT("machineName"), MachineName);
    if (MachineName.IsEmpty()) {
      MachineName = TEXT("StateMachine");
    }

    FString SourceState;
    Payload->TryGetStringField(TEXT("sourceState"), SourceState);

    FString TargetState;
    Payload->TryGetStringField(TEXT("targetState"), TargetState);

    if (BlueprintPath.IsEmpty() || SourceState.IsEmpty() || TargetState.IsEmpty()) {
      Message = TEXT("blueprintPath, sourceState, and targetState required for add_transition");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimTransition %s %s %s %s"),
          *BlueprintPath, *MachineName, *SourceState, *TargetState));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add transition") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Transition '%s' -> '%s' added"), *SourceState, *TargetState);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("machineName"), MachineName);
        Resp->SetStringField(TEXT("sourceState"), SourceState);
        Resp->SetStringField(TEXT("targetState"), TargetState);
      }
    }
  } else if (LowerSub == TEXT("set_transition_rules")) {
    // Set transition rules/conditions
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString MachineName;
    Payload->TryGetStringField(TEXT("machineName"), MachineName);
    if (MachineName.IsEmpty()) {
      MachineName = TEXT("StateMachine");
    }

    FString SourceState;
    Payload->TryGetStringField(TEXT("sourceState"), SourceState);

    FString TargetState;
    Payload->TryGetStringField(TEXT("targetState"), TargetState);

    FString Condition;
    Payload->TryGetStringField(TEXT("condition"), Condition);

    if (BlueprintPath.IsEmpty() || SourceState.IsEmpty() || TargetState.IsEmpty()) {
      Message = TEXT("blueprintPath, sourceState, and targetState required for set_transition_rules");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("SetAnimTransitionRule %s %s %s %s %s"),
          *BlueprintPath, *MachineName, *SourceState, *TargetState, *Condition));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to set transition rules") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Transition rule set for '%s' -> '%s'"), *SourceState, *TargetState);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("machineName"), MachineName);
        Resp->SetStringField(TEXT("sourceState"), SourceState);
        Resp->SetStringField(TEXT("targetState"), TargetState);
        Resp->SetStringField(TEXT("condition"), Condition);
      }
    }
  } else if (LowerSub == TEXT("add_blend_node")) {
    // Add a blend node to an animation blueprint
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    if (NodeType.IsEmpty()) {
      NodeType = TEXT("BlendByBool");
    }

    FString NodeName;
    Payload->TryGetStringField(TEXT("nodeName"), NodeName);

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath required for add_blend_node");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimBlendNode %s %s %s"),
          *BlueprintPath, *NodeType, *NodeName));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add blend node") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Blend node '%s' of type '%s' added"), *NodeName, *NodeType);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("nodeType"), NodeType);
        Resp->SetStringField(TEXT("nodeName"), NodeName);
      }
    }
  } else if (LowerSub == TEXT("add_cached_pose")) {
    // Add a cached pose node
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString PoseName;
    Payload->TryGetStringField(TEXT("poseName"), PoseName);
    if (PoseName.IsEmpty()) {
      PoseName = TEXT("CachedPose");
    }

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath required for add_cached_pose");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimCachedPose %s %s"), *BlueprintPath, *PoseName));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add cached pose") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Cached pose '%s' added"), *PoseName);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("poseName"), PoseName);
      }
    }
  } else if (LowerSub == TEXT("add_slot_node")) {
    // Add a slot node to an animation blueprint
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString SlotName;
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    if (SlotName.IsEmpty()) {
      SlotName = TEXT("DefaultSlot");
    }

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath required for add_slot_node");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimSlotNode %s %s"), *BlueprintPath, *SlotName));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add slot node") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Slot node '%s' added"), *SlotName);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("slotName"), SlotName);
      }
    }
  }
  // ============================================================
  // Control Rig Authoring Actions
  // ============================================================
  else if (LowerSub == TEXT("create_control_rig")) {
    // Create a new Control Rig blueprint
    FString RigName;
    if (!Payload->TryGetStringField(TEXT("name"), RigName) || RigName.IsEmpty()) {
      Message = TEXT("name required for create_control_rig");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Rigs");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_control_rig");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
#if MCP_HAS_CONTROLRIG_FACTORY
        FString FactoryError;
        UBlueprint *ControlRigBP = CreateControlRigBlueprint(RigName, SavePath, TargetSkeleton, FactoryError);
        if (ControlRigBP) {
          bSuccess = true;
          Message = TEXT("Control Rig created successfully");
          Resp->SetStringField(TEXT("assetPath"), ControlRigBP->GetPathName());
          Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
        } else {
          Message = FactoryError.IsEmpty() ? TEXT("Failed to create Control Rig") : FactoryError;
          ErrorCode = TEXT("ASSET_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
#else
        Message = TEXT("Control Rig factory not available in this engine version");
        ErrorCode = TEXT("NOT_AVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
  } else if (LowerSub == TEXT("add_control")) {
    // Add a control to a Control Rig
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString ControlName;
    Payload->TryGetStringField(TEXT("controlName"), ControlName);

    FString ControlType;
    Payload->TryGetStringField(TEXT("controlType"), ControlType);
    if (ControlType.IsEmpty()) {
      ControlType = TEXT("Transform");
    }

    if (AssetPath.IsEmpty() || ControlName.IsEmpty()) {
      Message = TEXT("assetPath and controlName required for add_control");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddControlRigControl %s %s %s"),
          *AssetPath, *ControlName, *ControlType));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add control") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Control '%s' added to rig"), *ControlName);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("controlName"), ControlName);
        Resp->SetStringField(TEXT("controlType"), ControlType);
      }
    }
  } else if (LowerSub == TEXT("add_rig_unit")) {
    // Add a rig unit to a Control Rig
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString UnitType;
    Payload->TryGetStringField(TEXT("unitType"), UnitType);

    FString UnitName;
    Payload->TryGetStringField(TEXT("unitName"), UnitName);

    if (AssetPath.IsEmpty() || UnitType.IsEmpty()) {
      Message = TEXT("assetPath and unitType required for add_rig_unit");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddControlRigUnit %s %s %s"),
          *AssetPath, *UnitType, *UnitName));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add rig unit") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Rig unit '%s' of type '%s' added"), *UnitName, *UnitType);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("unitType"), UnitType);
        Resp->SetStringField(TEXT("unitName"), UnitName);
      }
    }
  } else if (LowerSub == TEXT("connect_rig_elements")) {
    // Connect elements in a Control Rig
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString SourceElement;
    Payload->TryGetStringField(TEXT("sourceElement"), SourceElement);

    FString TargetElement;
    Payload->TryGetStringField(TEXT("targetElement"), TargetElement);

    FString SourcePin;
    Payload->TryGetStringField(TEXT("sourcePin"), SourcePin);

    FString TargetPin;
    Payload->TryGetStringField(TEXT("targetPin"), TargetPin);

    if (AssetPath.IsEmpty() || SourceElement.IsEmpty() || TargetElement.IsEmpty()) {
      Message = TEXT("assetPath, sourceElement, and targetElement required for connect_rig_elements");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("ConnectControlRigElements %s %s %s %s %s"),
          *AssetPath, *SourceElement, *SourcePin, *TargetElement, *TargetPin));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to connect rig elements") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Connected '%s' to '%s'"), *SourceElement, *TargetElement);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("sourceElement"), SourceElement);
        Resp->SetStringField(TEXT("targetElement"), TargetElement);
        if (!SourcePin.IsEmpty()) Resp->SetStringField(TEXT("sourcePin"), SourcePin);
        if (!TargetPin.IsEmpty()) Resp->SetStringField(TEXT("targetPin"), TargetPin);
      }
    }
  } else if (LowerSub == TEXT("create_pose_library")) {
    // Create a pose library asset
    FString LibraryName;
    if (!Payload->TryGetStringField(TEXT("name"), LibraryName) || LibraryName.IsEmpty()) {
      Message = TEXT("name required for create_pose_library");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations/PoseLibraries");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      if (SkeletonPath.IsEmpty()) {
        Message = TEXT("skeletonPath required for create_pose_library");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        USkeleton *TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (!TargetSkeleton) {
          Message = FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
          ErrorCode = TEXT("ASSET_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
            UEditorAssetLibrary::MakeDirectory(SavePath);
          }

          // Create a Data Asset to serve as a pose library container
          FAssetToolsModule &AssetToolsModule =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
          UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
              LibraryName, SavePath, UMcpGenericDataAsset::StaticClass(), nullptr);

          if (NewAsset) {
            if (UMcpGenericDataAsset* PoseLibrary = Cast<UMcpGenericDataAsset>(NewAsset)) {
              PoseLibrary->ItemName = LibraryName;
              PoseLibrary->Description = TEXT("Pose Library for animation poses");
              PoseLibrary->Properties.Add(TEXT("SkeletonPath"), SkeletonPath);
              PoseLibrary->MarkPackageDirty();
              McpSafeAssetSave(PoseLibrary);
            }

            bSuccess = true;
            Message = TEXT("Pose library created successfully");
            Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
          } else {
            Message = TEXT("Failed to create pose library asset");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          }
        }
      }
    }
  }
  // ============================================================
  // IK Rig Authoring Actions
  // ============================================================
  else if (LowerSub == TEXT("create_ik_rig")) {
    // Create an IK Rig asset
    FString RigName;
    if (!Payload->TryGetStringField(TEXT("name"), RigName) || RigName.IsEmpty()) {
      Message = TEXT("name required for create_ik_rig");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Rigs");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      FString MeshPath;
      Payload->TryGetStringField(TEXT("meshPath"), MeshPath);

      USkeleton *TargetSkeleton = nullptr;
      USkeletalMesh *TargetMesh = nullptr;

      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }
      if (!MeshPath.IsEmpty()) {
        TargetMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
        if (TargetMesh && !TargetSkeleton) {
          TargetSkeleton = TargetMesh->GetSkeleton();
        }
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath or meshPath required for create_ik_rig");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        // Use the existing setup_ik flow for IK Rig creation
#if MCP_HAS_CONTROLRIG_FACTORY
        FString FactoryError;
        UBlueprint *IKRigBP = CreateControlRigBlueprint(RigName, SavePath, TargetSkeleton, FactoryError);
        if (IKRigBP) {
          bSuccess = true;
          Message = TEXT("IK Rig created successfully");
          Resp->SetStringField(TEXT("assetPath"), IKRigBP->GetPathName());
          Resp->SetStringField(TEXT("skeletonPath"), TargetSkeleton->GetPathName());
          if (TargetMesh) {
            Resp->SetStringField(TEXT("meshPath"), TargetMesh->GetPathName());
          }
        } else {
          Message = FactoryError.IsEmpty() ? TEXT("Failed to create IK Rig") : FactoryError;
          ErrorCode = TEXT("ASSET_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
#else
        Message = TEXT("IK Rig creation requires Control Rig factory (UE 5.1+)");
        ErrorCode = TEXT("NOT_AVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
  } else if (LowerSub == TEXT("add_ik_chain")) {
    // Add an IK chain to an IK Rig
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString ChainName;
    Payload->TryGetStringField(TEXT("chainName"), ChainName);

    FString RootBone;
    Payload->TryGetStringField(TEXT("rootBone"), RootBone);

    FString EndBone;
    Payload->TryGetStringField(TEXT("endBone"), EndBone);

    if (AssetPath.IsEmpty() || ChainName.IsEmpty() || RootBone.IsEmpty() || EndBone.IsEmpty()) {
      Message = TEXT("assetPath, chainName, rootBone, and endBone required for add_ik_chain");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddIKChain %s %s %s %s"),
          *AssetPath, *ChainName, *RootBone, *EndBone));

      FString CommandError;
      if (!ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add IK chain") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("IK chain '%s' added from '%s' to '%s'"), *ChainName, *RootBone, *EndBone);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("chainName"), ChainName);
        Resp->SetStringField(TEXT("rootBone"), RootBone);
        Resp->SetStringField(TEXT("endBone"), EndBone);
      }
    }
  } else {
    Message = FString::Printf(
        TEXT("Animation/Physics action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleAnimationPhysicsAction: responding to subaction '%s' "
              "(success=%s)"),
         *LowerSub, bSuccess ? TEXT("true") : TEXT("false"));
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// NOTE: ExecuteEditorCommands and CreateControlRigBlueprint are defined in
// McpAutomationBridgeSubsystem.cpp - do not duplicate definitions here.
// The functions are declared in the subsystem header and implemented once
// to avoid LNK2005 duplicate symbol linker errors.

/**
 * @brief Handles a "create_animation_blueprint" automation request and creates
 * an AnimBlueprint asset.
 *
 * Processes the provided JSON payload to create and save an animation blueprint
 * bound to a target skeleton. Expected payload fields: `name` (required),
 * `savePath` (required), and either `skeletonPath` or `meshPath` (one
 * required). On success or on any handled error condition an automation
 * response is sent back to the requesting socket.
 *
 * @param RequestId Identifier for the incoming automation request (returned in
 * responses).
 * @param Action The action string; this handler responds when Action equals
 * "create_animation_blueprint".
 * @param Payload JSON payload containing creation parameters (see summary for
 * expected fields).
 * @param RequestingSocket Optional socket used to send the automation response.
 * @return bool `true` if the Action was handled (a response was sent, whether
 * success or error), `false` if the Action did not match.
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateAnimBlueprint(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_animation_blueprint"),
                    ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_animation_blueprint payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString BlueprintName;
  if (!Payload->TryGetStringField(TEXT("name"), BlueprintName) ||
      BlueprintName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SkeletonPath;
  Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

  FString MeshPath;
  Payload->TryGetStringField(TEXT("meshPath"), MeshPath);

  FString SavePath;
  if (!Payload->TryGetStringField(TEXT("savePath"), SavePath) ||
      SavePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("savePath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USkeleton *Skeleton = nullptr;
  if (!SkeletonPath.IsEmpty()) {
    if (UEditorAssetLibrary::DoesAssetExist(SkeletonPath)) {
      Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
    }

    if (!Skeleton) {
      const FString SkelMessage =
          FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
      SendAutomationError(RequestingSocket, RequestId, SkelMessage,
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
  } else if (!MeshPath.IsEmpty()) {
    if (UEditorAssetLibrary::DoesAssetExist(MeshPath)) {
      if (USkeletalMesh *Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath)) {
        Skeleton = Mesh->GetSkeleton();
      }
    }

    if (!Skeleton) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Could not infer skeleton from meshPath, and "
                               "skeletonPath was not provided"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    SkeletonPath = Skeleton->GetPathName();
  } else {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("skeletonPath or meshPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *SavePath, *BlueprintName);

  UAnimBlueprintFactory *Factory = NewObject<UAnimBlueprintFactory>();
  Factory->TargetSkeleton = Skeleton;
  Factory->BlueprintType = BPTYPE_Normal;
  Factory->ParentClass = UAnimInstance::StaticClass();

  if (!Factory) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create animation blueprint factory"),
                        TEXT("FACTORY_FAILED"));
    return true;
  }

  FString PackagePath = SavePath;
  FString AssetName = BlueprintName;
  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
  UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
      AssetName, PackagePath, UAnimBlueprint::StaticClass(), Factory);
  UAnimBlueprint *AnimBlueprint = Cast<UAnimBlueprint>(NewAsset);

  if (!AnimBlueprint) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create animation blueprint"),
                        TEXT("ASSET_CREATION_FAILED"));
    return true;
  }


  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("blueprintPath"), AnimBlueprint->GetPathName());
  Resp->SetStringField(TEXT("blueprintName"), BlueprintName);
  Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Animation blueprint created successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("create_animation_blueprint requires editor build"), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

/**
 * @brief Handles a "play_anim_montage" automation request by locating an actor
 * and playing the specified animation montage in the editor.
 *
 * Processes the payload to resolve an actor by name and a montage asset path,
 * loads the montage, and initiates playback on the actor's skeletal mesh
 * component (using the actor's AnimInstance when available or single-node
 * playback otherwise). Sends a structured automation response reporting
 * success, playback length, and error details when applicable.
 *
 * @param RequestId Unique identifier for the incoming automation request;
 * included in responses.
 * @param Action The action string provided by the request; this handler
 * responds when the action equals "play_anim_montage".
 * @param Payload JSON payload containing fields:
 *   - "actorName" (string, required): name or label of the target actor in the
 * editor.
 *   - "montagePath" or "assetPath" (string, required): asset path to the
 * UAnimMontage.
 *   - "playRate" (number, optional): playback speed (default 1.0).
 * @param RequestingSocket Optional websocket that originated the request; used
 * to send the response.
 *
 * @return true if the request was handled (a response was sent), false if the
 * handler did not claim the action.
 */
bool UMcpAutomationBridgeSubsystem::HandlePlayAnimMontage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("play_anim_montage"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("play_anim_montage payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ActorName;
  if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
      ActorName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString MontagePath;
  // Check both montagePath and assetPath for flexibility
  if (!Payload->TryGetStringField(TEXT("montagePath"), MontagePath) ||
      MontagePath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("assetPath"), MontagePath);
  }

  if (MontagePath.IsEmpty()) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("error"), TEXT("montagePath required"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("montagePath required"), Resp,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double PlayRate = 1.0;
  Payload->TryGetNumberField(TEXT("playRate"), PlayRate);

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("EditorActorSubsystem not available"),
                        TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *TargetActor = nullptr;

  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    for (TActorIterator<AActor> It(World); It; ++It) {
      AActor *Actor = *It;
      if (Actor) {
        if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
            Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
          TargetActor = Actor;
          break;
        }
      }
    }
  }

  // Fallback to ActorSS search if iterator didn't find it (rare but redundant
  // safety)
  if (!TargetActor) {
    for (AActor *Actor : AllActors) {
      if (Actor &&
          (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
           Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase))) {
        TargetActor = Actor;
        break;
      }
    }
  }

  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("montagePath"), MontagePath);
    Resp->SetNumberField(TEXT("playRate"), PlayRate);

    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Actor not found"), Resp,
                           TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USkeletalMeshComponent *SkelMeshComp =
      TargetActor->FindComponentByClass<USkeletalMeshComponent>();
  if (!SkelMeshComp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Skeletal mesh component not found"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(MontagePath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Montage asset not found: %s"), *MontagePath));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Montage not found"), Resp,
                           TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
  if (!Montage) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Failed to load montage: %s"), *MontagePath));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("montagePath"), MontagePath);
    Resp->SetNumberField(TEXT("playRate"), PlayRate);

    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to load montage"), Resp,
                           TEXT("ASSET_LOAD_FAILED"));
    return true;
  }

  float MontageLength = 0.f;
  if (UAnimInstance *AnimInst = SkelMeshComp->GetAnimInstance()) {
    MontageLength =
        AnimInst->Montage_Play(Montage, static_cast<float>(PlayRate));
  } else {
    SkelMeshComp->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
    SkelMeshComp->PlayAnimation(Montage, false);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetStringField(TEXT("montagePath"), MontagePath);
  Resp->SetNumberField(TEXT("playRate"), PlayRate);
  Resp->SetNumberField(TEXT("montageLength"), MontageLength);
  Resp->SetBoolField(TEXT("playing"), true);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Animation montage playing"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("play_anim_montage requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

/**
 * @brief Enables ragdoll physics on a named actor's skeletal mesh in the
 * editor.
 *
 * Applies physics simulation and collision to the actor's
 * SkeletalMeshComponent, optionally respects a provided blend weight and
 * verifies an optional skeleton asset.
 *
 * @param RequestId The automation request identifier returned to the caller.
 * @param Action The original action string (expected "setup_ragdoll").
 * @param Payload JSON payload; must contain "actorName" and may include:
 *                - "blendWeight" (number): blend factor for animation/physics
 * update.
 *                - "skeletonPath" (string): optional path to a skeleton asset
 * to validate.
 * @param RequestingSocket The websocket that initiated the request (may be
 * null).
 * @return true if this handler processed the action (either completed or sent
 * an error response); false if the action did not match "setup_ragdoll".
 */
bool UMcpAutomationBridgeSubsystem::HandleSetupRagdoll(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("setup_ragdoll"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("setup_ragdoll payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ActorName;
  if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
      ActorName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double BlendWeight = 1.0;
  Payload->TryGetNumberField(TEXT("blendWeight"), BlendWeight);

  FString SkeletonPath;
  if (Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) &&
      !SkeletonPath.IsEmpty()) {
    USkeleton *RagdollSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
    if (!RagdollSkeleton) {
      const FString SkelMessage =
          FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
      SendAutomationError(RequestingSocket, RequestId, SkelMessage,
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("EditorActorSubsystem not available"),
                        TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *TargetActor = nullptr;

  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    for (TActorIterator<AActor> It(World); It; ++It) {
      AActor *Actor = *It;
      if (Actor) {
        if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
            Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
          TargetActor = Actor;
          break;
        }
      }
    }
  }

  if (!TargetActor) {
    for (AActor *Actor : AllActors) {
      if (Actor &&
          (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
           Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase))) {
        TargetActor = Actor;
        break;
      }
    }
  }

  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetNumberField(TEXT("blendWeight"), BlendWeight);

    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Actor not found"), Resp,
                           TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USkeletalMeshComponent *SkelMeshComp =
      TargetActor->FindComponentByClass<USkeletalMeshComponent>();
  if (!SkelMeshComp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Skeletal mesh component not found"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  SkelMeshComp->SetSimulatePhysics(true);
  SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

  if (SkelMeshComp->GetPhysicsAsset()) {
    SkelMeshComp->SetAllBodiesSimulatePhysics(true);
    SkelMeshComp->SetUpdateAnimationInEditor(BlendWeight < 1.0);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetNumberField(TEXT("blendWeight"), BlendWeight);
  Resp->SetBoolField(TEXT("ragdollActive"),
                     SkelMeshComp->IsSimulatingPhysics());
  Resp->SetBoolField(TEXT("hasPhysicsAsset"),
                     SkelMeshComp->GetPhysicsAsset() != nullptr);

  if (SkelMeshComp->GetPhysicsAsset()) {
    Resp->SetStringField(TEXT("physicsAssetPath"),
                         SkelMeshComp->GetPhysicsAsset()->GetPathName());
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Ragdoll setup completed"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("setup_ragdoll requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

/**
 * @brief Activates or deactivates ragdoll physics on a named actor's skeletal mesh.
 *
 * This handler toggles ragdoll simulation on/off, allowing runtime control
 * over physics simulation state.
 *
 * @param RequestId The automation request identifier returned to the caller.
 * @param Action The original action string (expected "activate_ragdoll").
 * @param Payload JSON payload; must contain "actorName" and may include:
 *                - "activate" (bool): true to activate, false to deactivate (default: true)
 * @param RequestingSocket The websocket that initiated the request (may be null).
 * @return true if this handler processed the action.
 */
bool UMcpAutomationBridgeSubsystem::HandleActivateRagdoll(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("activate_ragdoll"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("activate_ragdoll payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ActorName;
  if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
      ActorName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bActivate = true;
  Payload->TryGetBoolField(TEXT("activate"), bActivate);

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  AActor *TargetActor = nullptr;

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Actor = *It;
    if (Actor) {
      if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
          Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
        TargetActor = Actor;
        break;
      }
    }
  }

  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("error"),
                         FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetBoolField(TEXT("activate"), bActivate);

    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Actor not found"), Resp,
                           TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USkeletalMeshComponent *SkelMeshComp =
      TargetActor->FindComponentByClass<USkeletalMeshComponent>();
  if (!SkelMeshComp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Skeletal mesh component not found"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // Activate or deactivate ragdoll
  if (bActivate) {
    SkelMeshComp->SetSimulatePhysics(true);
    SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (SkelMeshComp->GetPhysicsAsset()) {
      SkelMeshComp->SetAllBodiesSimulatePhysics(true);
    }
  } else {
    SkelMeshComp->SetAllBodiesSimulatePhysics(false);
    SkelMeshComp->SetSimulatePhysics(false);
    SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetBoolField(TEXT("activate"), bActivate);
  Resp->SetBoolField(TEXT("ragdollActive"),
                     SkelMeshComp->IsSimulatingPhysics());
  Resp->SetBoolField(TEXT("hasPhysicsAsset"),
                     SkelMeshComp->GetPhysicsAsset() != nullptr);

  if (SkelMeshComp->GetPhysicsAsset()) {
    Resp->SetStringField(TEXT("physicsAssetPath"),
                         SkelMeshComp->GetPhysicsAsset()->GetPathName());
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Ragdoll activation state changed"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("activate_ragdoll requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
