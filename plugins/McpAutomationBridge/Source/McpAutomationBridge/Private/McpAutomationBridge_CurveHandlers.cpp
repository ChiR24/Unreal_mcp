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

	// -----------------------------------------------------------------------
	// create_curve_float — instantiate a UCurveFloat at path/name.
	// -----------------------------------------------------------------------
	static bool HandleCreateCurveFloat(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr, NameStr;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr) ||
			!Payload->TryGetStringField(TEXT("name"), NameStr))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field(s): path, name"));
			return true;
		}

		const FString FullPath = PathStr / NameStr;
		if (UEditorAssetLibrary::DoesAssetExist(FullPath))
		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("assetPath"), FullPath);
			Data->SetBoolField(TEXT("alreadyExists"), true);
			SendSuccess(Self, Socket, RequestId,
				FString::Printf(TEXT("Curve already exists at '%s'"), *FullPath), Data);
			return true;
		}

		FString OutError;
		bool bSaved = false;
		UObject* NewCurve = McpGenericAssetFactory::CreateAssetOfClass(
			UCurveFloat::StaticClass(), PathStr, NameStr, nullptr, OutError, bSaved);
		if (!NewCurve)
		{
			SendError(Self, Socket, RequestId, TEXT("ENGINE_API_ERROR"),
				OutError.IsEmpty() ? TEXT("CreateAsset returned nullptr") : OutError);
			return true;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), NewCurve->GetPathName());
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved && !OutError.IsEmpty())
		{
			Data->SetStringField(TEXT("saveWarning"), OutError);
		}
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Created UCurveFloat at '%s'"), *NewCurve->GetPathName()),
			Data);
		return true;
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
	if (SubAction == TEXT("create_curve_float"))
	{
		return McpCurveHandlers::HandleCreateCurveFloat(this, RequestId, Payload, RequestingSocket);
	}

	SendAutomationError(RequestingSocket, RequestId,
		FString::Printf(TEXT("manage_curve sub-action not yet implemented: %s"), *SubAction),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}
