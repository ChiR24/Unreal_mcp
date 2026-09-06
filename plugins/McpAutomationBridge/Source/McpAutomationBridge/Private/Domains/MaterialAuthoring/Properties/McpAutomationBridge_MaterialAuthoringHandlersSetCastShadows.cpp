#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleSetCastShadows(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_cast_shadows")) {

    Bridge->SendAutomationError(Socket, RequestId,
                        TEXT("set_cast_shadows cannot be applied to a material asset. Configure shadow casting on a mesh/light component instead."),
                        TEXT("UNSUPPORTED_OPERATION"));
    return true;
  }

  return false;
}
}
#endif
