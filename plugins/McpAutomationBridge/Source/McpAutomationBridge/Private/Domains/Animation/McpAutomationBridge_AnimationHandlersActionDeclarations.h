#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpAnimationHandlers {
struct FActionContext;
bool HandleAnimationCleanupAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationCreateBlendTreeAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationCreateProceduralAnimAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationCreateStateMachineAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationSetupIKAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationConfigureVehicleAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationSetupPhysicsSimulationAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationCreateAnimationAssetAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationSetupRetargetingAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationSkinMeshToSkeletonAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
} // namespace McpAnimationHandlers
