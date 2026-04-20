// McpAutomationBridge_BlueprintTypeHandlers.h
// Handlers for manage_blueprint's enum/struct authoring actions.

#pragma once

#include "CoreMinimal.h"

class UMcpAutomationBridgeSubsystem;
class FMcpBridgeWebSocket;
class FJsonObject;

namespace McpBlueprintTypeHandlers
{
	/**
	 * Dispatch entry. Returns true iff Action is one of:
	 * create_enum, create_struct, modify_enum, modify_struct,
	 * inspect_enum, inspect_struct.
	 * Called from HandleBlueprintAction (early dispatch).
	 */
	bool HandleAction(UMcpAutomationBridgeSubsystem* Self,
	                  const FString& RequestId,
	                  const FString& Action,
	                  const TSharedPtr<FJsonObject>& Payload,
	                  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
