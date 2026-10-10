#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/BlueprintCreation/McpBPCreateHandlers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintCreateExists(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  // blueprint_create handler: parse payload and prepare coalesced creation
  // Support both explicit blueprint_create and the nested 'create' action from
  // manage_blueprint
  if (ActionMatchesPattern(TEXT("create_blueprint")) ||
      ActionMatchesPattern(TEXT("create"))) {
    return FBlueprintCreationHandlers::HandleBlueprintCreate(
        &Bridge, RequestId, LocalPayload, RequestingSocket);
  }

  return false;
}
} // namespace McpBlueprintHandlers
