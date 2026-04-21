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

	// -----------------------------------------------------------------------
	// add_gameplay_tag
	// -----------------------------------------------------------------------
	static bool HandleAddGameplayTag(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString TagStr;
		if (!Payload->TryGetStringField(TEXT("tag"), TagStr) || TagStr.IsEmpty())
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field: tag"));
			return true;
		}

		FString Comment;
		Payload->TryGetStringField(TEXT("comment"), Comment);

		FString SourceIni;
		Payload->TryGetStringField(TEXT("sourceIni"), SourceIni);
		// When caller passes empty sourceIni, interpret as NAME_None which writes to
		// the project's default tag source (DefaultGameplayTags.ini).
		const FName TagSourceName = SourceIni.IsEmpty() ? FName(NAME_None) : FName(*SourceIni);

		if (!IGameplayTagsEditorModule::IsAvailable())
		{
			SendError(Self, Socket, RequestId, TEXT("ENGINE_API_ERROR"),
				TEXT("GameplayTagsEditor module is not available"));
			return true;
		}

		IGameplayTagsEditorModule& EditorModule = IGameplayTagsEditorModule::Get();
		const bool bAdded = EditorModule.AddNewGameplayTagToINI(
			TagStr, Comment, TagSourceName, /*bIsRestrictedTag=*/false,
			/*bAllowNonRestrictedChildren=*/true);

		if (!bAdded)
		{
			SendError(Self, Socket, RequestId, TEXT("ENGINE_API_ERROR"),
				FString::Printf(TEXT("AddNewGameplayTagToINI failed for tag '%s' (source='%s'). "
					"Tag may already exist, be malformed, or the source ini is invalid."),
					*TagStr, *SourceIni));
			return true;
		}

		// Refresh editor tag tree so UI / dropdowns update immediately.
		UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("tag"), TagStr);
		Data->SetStringField(TEXT("sourceIni"),
			SourceIni.IsEmpty() ? TEXT("DefaultGameplayTags.ini") : SourceIni);
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Added gameplay tag '%s'"), *TagStr), Data);
		return true;
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
	if (SubAction == TEXT("add_gameplay_tag"))
	{
		return McpGameplayTagsHandlers::HandleAddGameplayTag(this, RequestId, Payload, RequestingSocket);
	}

	SendAutomationError(RequestingSocket, RequestId,
		FString::Printf(TEXT("Sub-action not implemented: %s"), *SubAction),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}
