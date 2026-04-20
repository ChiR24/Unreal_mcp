// McpAutomationBridge_BlueprintTypeHandlers.cpp

#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BlueprintTypeHandlers.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "MCP/McpPinTypeParser.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Factories/EnumFactory.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UObject/Package.h"
#endif

namespace McpBlueprintTypeHandlers
{
namespace
{
	void SendError(UMcpAutomationBridgeSubsystem* Self,
	               TSharedPtr<FMcpBridgeWebSocket> Socket,
	               const FString& RequestId,
	               const FString& Code,
	               const FString& Message)
	{
		Self->SendAutomationError(Socket, RequestId, Message, Code);
	}

	void SendSuccess(UMcpAutomationBridgeSubsystem* Self,
	                 TSharedPtr<FMcpBridgeWebSocket> Socket,
	                 const FString& RequestId,
	                 const FString& Message,
	                 const TSharedPtr<FJsonObject>& Data)
	{
		Self->SendAutomationResponse(Socket, RequestId, true, Message, Data);
	}

	bool HandleCreateEnum(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("create_enum stub"));
		return true;
	}
	bool HandleCreateStruct(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("create_struct stub"));
		return true;
	}
	bool HandleModifyEnum(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("modify_enum stub"));
		return true;
	}
	bool HandleModifyStruct(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("modify_struct stub"));
		return true;
	}
	bool HandleInspectEnum(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("inspect_enum stub"));
		return true;
	}
	bool HandleInspectStruct(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("inspect_struct stub"));
		return true;
	}
}

bool HandleAction(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                  const FString& Action, const TSharedPtr<FJsonObject>& Payload,
                  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
	const FString Lower = Action.ToLower();
	if (Lower == TEXT("create_enum"))    return HandleCreateEnum   (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("create_struct"))  return HandleCreateStruct (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("modify_enum"))    return HandleModifyEnum   (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("modify_struct"))  return HandleModifyStruct (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("inspect_enum"))   return HandleInspectEnum  (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("inspect_struct")) return HandleInspectStruct(Self, RequestId, Payload, RequestingSocket);
	return false;
#else
	return false;
#endif
}

} // namespace McpBlueprintTypeHandlers
