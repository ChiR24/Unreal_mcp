// =============================================================================
// McpAutomationBridge_GameplayTagsHandlers.cpp
// =============================================================================
// manage_gameplay_tags tool handlers (Ch4).
//
// Dispatcher: HandleManageGameplayTagsAction reads payload `subAction` and
// routes to per-action helpers defined in this file.
//
// Per-action helpers are added one-per-commit (Ch4 Tasks 2-5). Unknown
// sub-actions return NOT_IMPLEMENTED so callers see a stable error shape.
//
// APIs used (verified against UE 5.7 headers):
//   - IGameplayTagsEditorModule::AddNewGameplayTagToINI (public header,
//     GameplayTagsEditor/Public/GameplayTagsEditorModule.h)
//   - IGameplayTagsEditorModule::DeleteTagFromINI
//   - IGameplayTagsEditorModule::AddNewGameplayTagSource
//   - UGameplayTagsManager::RequestAllGameplayTags
//   - UGameplayTagsManager::EditorRefreshGameplayTagTree
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "GameplayTagsManager.h"
#include "GameplayTagsModule.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#endif

namespace McpGameplayTagsHandlers
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

	// Avoid unused warning during skeleton compile — real uses appear in Task 2-5.
	static void SilenceUnusedHelpers()
	{
		(void)&SendSuccess;
		(void)&SendError;
	}

#endif // WITH_EDITOR

} // namespace McpGameplayTagsHandlers

// ---------------------------------------------------------------------------
// Dispatcher
// ---------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleManageGameplayTagsAction(
	const FString& RequestId, const FString& Action,
	const TSharedPtr<FJsonObject>& Payload,
	TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
	if (Action != TEXT("manage_gameplay_tags"))
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
		TEXT("manage_gameplay_tags requires an editor build"),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#else
	// Ensure static helpers are referenced so unused-function warnings stay silent
	// in the skeleton commit; real dispatches appear in Tasks 2-5.
	McpGameplayTagsHandlers::SilenceUnusedHelpers();

	SendAutomationError(RequestingSocket, RequestId,
		FString::Printf(TEXT("Sub-action not implemented: %s"), *SubAction),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}
