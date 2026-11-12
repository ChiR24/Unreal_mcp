#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/BlueprintCreation/McpBPCreateHandlers.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintCreateExists(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  // A Blueprint is made by the BlueprintCreation domain.
  if (ActionMatchesPattern(TEXT("create_blueprint")) ||
      ActionMatchesPattern(TEXT("create"))) {
    return FBlueprintCreationHandlers::HandleBlueprintCreate(
        &Bridge, RequestId, LocalPayload, RequestingSocket);
  }

  return false;
}
} // namespace McpBlueprintHandlers
