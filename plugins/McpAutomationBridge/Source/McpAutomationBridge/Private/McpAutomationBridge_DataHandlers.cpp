// =============================================================================
// McpAutomationBridge_DataHandlers.cpp
// =============================================================================
// manage_data tool handlers (Ch2 DataTable + Ch3 DataAsset CRUD).
//
// Dispatcher: HandleManageDataAction reads payload `subAction` and routes to
// per-action helpers declared as static functions below. This mirrors the
// HandleManageInventoryAction pattern used elsewhere in the bridge.
//
// Per-action helpers are added one-per-commit (Ch2 Tasks 2-9, Ch3 Tasks).
// Unknown sub-actions return NOT_IMPLEMENTED so callers get a stable error
// shape even before every action is wired up.
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "MCP/Helpers/McpStructReflection.h"
#include "MCP/Helpers/McpGenericAssetFactory.h"
#include "MCP/Helpers/McpPropertyPath.h"

#if WITH_EDITOR
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "StructUtils/UserDefinedStruct.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "UObject/Package.h"
#endif

// ---------------------------------------------------------------------------
// Per-action handler forward declarations (definitions added in follow-on tasks)
// ---------------------------------------------------------------------------
namespace McpDataHandlers
{

#if WITH_EDITOR

	// Split "/Game/Folder/Name" → ("/Game/Folder", "Name").
	static bool SplitPackagePath(const FString& InPath, FString& OutDir, FString& OutName)
	{
		int32 LastSlash = INDEX_NONE;
		if (!InPath.FindLastChar('/', LastSlash) || LastSlash <= 0)
		{
			return false;
		}
		OutDir = InPath.Left(LastSlash);
		OutName = InPath.Mid(LastSlash + 1);
		return !OutName.IsEmpty();
	}

	// Try to load a UScriptStruct by package path, falling back to UUserDefinedStruct.
	static UScriptStruct* LoadRowStruct(const FString& Path)
	{
		UScriptStruct* Native = LoadObject<UScriptStruct>(nullptr, *Path);
		if (Native) { return Native; }
		UObject* Obj = LoadObject<UObject>(nullptr, *Path);
		return Cast<UScriptStruct>(Obj);
	}

	static TSharedPtr<FJsonObject> MakeErrorJson(const FString& Message, const FString& Category)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetBoolField(TEXT("success"), false);
		Obj->SetStringField(TEXT("error"), Message);
		if (!Category.IsEmpty())
		{
			Obj->SetStringField(TEXT("errorCategory"), Category);
		}
		return Obj;
	}

#endif // WITH_EDITOR

} // namespace McpDataHandlers

// ---------------------------------------------------------------------------
// Dispatcher
// ---------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleManageDataAction(
	const FString& RequestId, const FString& Action,
	const TSharedPtr<FJsonObject>& Payload,
	TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
	if (Action != TEXT("manage_data"))
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
		TEXT("manage_data requires an editor build"),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#else
	// Per-action handlers are wired in follow-on Ch2/Ch3 tasks. For now every
	// sub-action resolves to NOT_IMPLEMENTED with a stable error shape.
	SendAutomationError(RequestingSocket, RequestId,
		FString::Printf(TEXT("manage_data sub-action not yet implemented: %s"), *SubAction),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}
