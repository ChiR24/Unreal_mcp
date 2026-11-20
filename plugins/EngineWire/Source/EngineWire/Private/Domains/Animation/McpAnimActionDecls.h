#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class USkeletalMeshComponent;

namespace McpAnimationHandlers {
struct FActionContext;
// The named actor's skeletal mesh component, from the play world while Play In Editor runs (bOutPlaying) and from the
// editor world otherwise. Null once a refusal went out; AddNotFoundFields adds to the ACTOR_NOT_FOUND reply.
USkeletalMeshComponent *FindSkeletalMeshOrReply(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString &ActorName,
    bool &bOutPlaying, TFunctionRef<void(FJsonObject &)> AddNotFoundFields = [](FJsonObject &) {});
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
bool HandleAnimationCreatePhysicalMaterialAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationCreateAnimationAssetAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationCreatePoseLibraryAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleAnimationSetupRetargetingAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
bool HandleGetPoseSearchDatabase(FActionContext &Context, const TSharedPtr<FJsonObject> &Payload); // Assets/McpAnimPoseSearchRead.cpp
bool HandleGetChooserTable(FActionContext &Context, const TSharedPtr<FJsonObject> &Payload);       // Assets/McpAnimPoseSearchRead.cpp
bool HandleAnimationSkinMeshToSkeletonAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload);
} // namespace McpAnimationHandlers
