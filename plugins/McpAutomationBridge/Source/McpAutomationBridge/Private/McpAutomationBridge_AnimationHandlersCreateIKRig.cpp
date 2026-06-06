#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpVersionCompatibility.h"

#include "Animation/Skeleton.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateIKRigAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


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
        UBlueprint *IKRigBP = Context.Bridge.CreateControlRigBlueprint(RigName, SavePath, TargetSkeleton, FactoryError);
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
    return false;
}
#endif
} // namespace McpAnimationHandlers
