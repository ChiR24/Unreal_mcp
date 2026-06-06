#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationPlayMontageAliasAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
    return Context.InvokePrivateAnimationHandler(TEXT("play_anim_montage"),
                                                 Payload);
  return false;
}
#endif
} // namespace McpAnimationHandlers
