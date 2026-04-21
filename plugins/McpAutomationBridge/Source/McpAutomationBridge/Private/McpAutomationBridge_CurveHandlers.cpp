// =============================================================================
// McpAutomationBridge_CurveHandlers.cpp
// =============================================================================
// manage_curve tool handlers (Ch5 UCurveFloat authoring).
//
// Dispatcher: HandleManageCurveAction reads payload `subAction` and routes to
// per-action helpers declared as static functions below. This mirrors the
// HandleManageDataAction pattern used in the Ch2/Ch3 DataTable/DataAsset bridge.
//
// Per-action helpers added one-per-commit (Ch5 Tasks 2-5).
// Unknown sub-actions return NOT_IMPLEMENTED so callers get a stable error
// shape even before every action is wired up.
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "MCP/Helpers/McpGenericAssetFactory.h"

#if WITH_EDITOR
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
#include "EditorAssetLibrary.h"
#include "UObject/Package.h"
#endif

// ---------------------------------------------------------------------------
// Per-action handler helpers (definitions added in follow-on tasks)
// ---------------------------------------------------------------------------
namespace McpCurveHandlers
{

#if WITH_EDITOR

	static void SendSuccess(UMcpAutomationBridgeSubsystem* Self,
		TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
		const FString& Message, const TSharedPtr<FJsonObject>& Data)
	{
		Self->SendAutomationResponse(Socket, RequestId, true, Message, Data);
	}

	static void SendError(UMcpAutomationBridgeSubsystem* Self,
		TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
		const FString& Category, const FString& Message)
	{
		Self->SendAutomationError(Socket, RequestId, Message, Category);
	}

#endif // WITH_EDITOR

} // namespace McpCurveHandlers

// ---------------------------------------------------------------------------
// Dispatcher
// ---------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleManageCurveAction(
	const FString& RequestId, const FString& Action,
	const TSharedPtr<FJsonObject>& Payload,
	TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
	if (Action != TEXT("manage_curve"))
	{
		return false;
	}

	FString SubAction;
	if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("subAction"), SubAction))
	{
		SendAutomationError(RequestingSocket, RequestId,
			TEXT("Missing required parameter: subAction"),
			TEXT("INVALID_PARAMS"));
		return true;
	}

#if !WITH_EDITOR
	SendAutomationError(RequestingSocket, RequestId,
		TEXT("manage_curve requires an editor build"),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#else
	// Per-action dispatch added in Tasks 2-5.
	SendAutomationError(RequestingSocket, RequestId,
		FString::Printf(TEXT("manage_curve sub-action not yet implemented: %s"), *SubAction),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}
