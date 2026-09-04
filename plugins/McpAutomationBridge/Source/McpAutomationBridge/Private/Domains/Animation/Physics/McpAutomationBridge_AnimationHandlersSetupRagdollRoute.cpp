// McpAutomationBridge_AnimationHandlersSetupRagdollRoute.cpp — routes animation_physics.setup_ragdoll.
//
// Dogfood #143: the ragdoll handler was only registered with the direct action registry, but the
// animation_physics parent dispatches through its own route table first and answered NOT_IMPLEMENTED
// before the direct registry was ever consulted. This adapter puts the handler on the route table.
#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
bool HandleAnimationSetupRagdollRouteAction(FActionContext &Context,
                                            const TSharedPtr<FJsonObject> &Payload) {
  // HandleSetupRagdoll sends its own response; returning true tells the dispatcher not to send another.
  return Context.InvokePrivateAnimationHandler(TEXT("setup_ragdoll"), Payload);
}
} // namespace McpAnimationHandlers
