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
	// create_data_table
	// -----------------------------------------------------------------------
	static bool HandleCreateDataTable(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr, NameStr, RowStructPath;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr) ||
			!Payload->TryGetStringField(TEXT("name"), NameStr) ||
			!Payload->TryGetStringField(TEXT("rowStructPath"), RowStructPath))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field(s): path, name, rowStructPath"));
			return true;
		}

		UScriptStruct* RowStruct = LoadRowStruct(RowStructPath);
		if (!RowStruct)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("Row struct not found: %s"), *RowStructPath));
			return true;
		}

		const FString FullPath = PathStr / NameStr;
		if (UEditorAssetLibrary::DoesAssetExist(FullPath))
		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("assetPath"), FullPath);
			Data->SetBoolField(TEXT("alreadyExists"), true);
			SendSuccess(Self, Socket, RequestId,
				FString::Printf(TEXT("DataTable already exists at '%s'"), *FullPath), Data);
			return true;
		}

		FString OutError;
		bool bSaved = false;
		UObject* NewTable = McpGenericAssetFactory::CreateAssetOfClass(
			UDataTable::StaticClass(), PathStr, NameStr,
			[RowStruct](UObject* Asset)
			{
				if (UDataTable* DT = Cast<UDataTable>(Asset))
				{
					DT->RowStruct = RowStruct;
				}
			},
			OutError, bSaved);

		if (!NewTable)
		{
			SendError(Self, Socket, RequestId, TEXT("ENGINE_API_ERROR"),
				OutError.IsEmpty() ? TEXT("CreateAsset returned nullptr") : OutError);
			return true;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), NewTable->GetPathName());
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved && !OutError.IsEmpty())
		{
			Data->SetStringField(TEXT("saveWarning"), OutError);
		}
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Created DataTable at '%s'"), *NewTable->GetPathName()),
			Data);
		return true;
	}

	// -----------------------------------------------------------------------
	// Row buffer RAII helper: allocates + InitializeStruct, frees on scope exit.
	// -----------------------------------------------------------------------
	struct FScopedRowBuffer
	{
		const UScriptStruct* Struct = nullptr;
		uint8* Data = nullptr;

		explicit FScopedRowBuffer(const UScriptStruct* InStruct)
			: Struct(InStruct)
		{
			if (Struct)
			{
				Data = static_cast<uint8*>(FMemory::Malloc(Struct->GetStructureSize()));
				Struct->InitializeStruct(Data);
			}
		}
		FScopedRowBuffer(const FScopedRowBuffer&) = delete;
		FScopedRowBuffer& operator=(const FScopedRowBuffer&) = delete;
		~FScopedRowBuffer()
		{
			if (Struct && Data)
			{
				Struct->DestroyStruct(Data);
				FMemory::Free(Data);
			}
		}
	};

	// Load a UDataTable at PackagePath. Sends an error + returns nullptr on miss.
	static UDataTable* LoadDataTableOrError(UMcpAutomationBridgeSubsystem* Self,
		TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
		const FString& PathStr)
	{
		UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
		if (!DT)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("DataTable not found: %s"), *PathStr));
			return nullptr;
		}
		if (!DT->RowStruct)
		{
			SendError(Self, Socket, RequestId, TEXT("CONFLICT_STATE"),
				FString::Printf(TEXT("DataTable '%s' has no RowStruct"), *PathStr));
			return nullptr;
		}
		return DT;
	}

	// -----------------------------------------------------------------------
	// add_data_table_row
	// -----------------------------------------------------------------------
	static bool HandleAddDataTableRow(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr, RowNameStr;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr) ||
			!Payload->TryGetStringField(TEXT("rowName"), RowNameStr))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field(s): path, rowName"));
			return true;
		}

		UDataTable* DT = LoadDataTableOrError(Self, Socket, RequestId, PathStr);
		if (!DT) { return true; }

		const FName RowName(*RowNameStr);
		if (DT->GetRowMap().Contains(RowName))
		{
			SendError(Self, Socket, RequestId, TEXT("CONFLICT_STATE"),
				FString::Printf(TEXT("Row already exists: %s"), *RowNameStr));
			return true;
		}

		FScopedRowBuffer Buf(DT->RowStruct);
		if (!Buf.Data)
		{
			SendError(Self, Socket, RequestId, TEXT("ENGINE_API_ERROR"),
				TEXT("Failed to allocate row buffer"));
			return true;
		}

		const TSharedPtr<FJsonObject>* FieldsObj = nullptr;
		if (Payload->TryGetObjectField(TEXT("fields"), FieldsObj) &&
			FieldsObj && (*FieldsObj).IsValid() && (*FieldsObj)->Values.Num() > 0)
		{
			FString StructError;
			if (!McpStructReflection::SetStructFieldsFromJsonObject(
				DT->RowStruct, Buf.Data, *FieldsObj, StructError))
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), StructError);
				return true;
			}
		}

		// Use the (FName, const uint8*, const UScriptStruct*) overload to avoid
		// unsafe reinterpret_cast to FTableRowBase* (UDS rows aren't FTableRowBase).
		DT->AddRow(RowName, Buf.Data, DT->RowStruct);

		DT->MarkPackageDirty();
		McpSafeAssetSave(DT);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("rowName"), RowNameStr);
		Data->SetStringField(TEXT("assetPath"), DT->GetPathName());
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Added row '%s' to DataTable '%s'"), *RowNameStr, *DT->GetName()),
			Data);
		return true;
	}

	// -----------------------------------------------------------------------
	// set_data_table_row — full overwrite (missing fields → struct defaults).
	// -----------------------------------------------------------------------
	static bool HandleSetDataTableRow(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr, RowNameStr;
		const TSharedPtr<FJsonObject>* FieldsObj = nullptr;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr) ||
			!Payload->TryGetStringField(TEXT("rowName"), RowNameStr) ||
			!Payload->TryGetObjectField(TEXT("fields"), FieldsObj) ||
			!FieldsObj || !(*FieldsObj).IsValid())
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field(s): path, rowName, fields"));
			return true;
		}

		UDataTable* DT = LoadDataTableOrError(Self, Socket, RequestId, PathStr);
		if (!DT) { return true; }

		const FName RowName(*RowNameStr);
		uint8* ExistingRow = nullptr;
		if (uint8* const* Found = DT->GetRowMap().Find(RowName))
		{
			ExistingRow = *Found;
		}
		if (!ExistingRow)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("Row not found: %s"), *RowNameStr));
			return true;
		}

		// Reset row to struct defaults then apply fields.
		DT->RowStruct->DestroyStruct(ExistingRow);
		DT->RowStruct->InitializeStruct(ExistingRow);

		FString StructError;
		if (!McpStructReflection::SetStructFieldsFromJsonObject(
			DT->RowStruct, ExistingRow, *FieldsObj, StructError))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), StructError);
			return true;
		}

		DT->HandleDataTableChanged(RowName);
		DT->MarkPackageDirty();
		McpSafeAssetSave(DT);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("rowName"), RowNameStr);
		Data->SetStringField(TEXT("assetPath"), DT->GetPathName());
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Overwrote row '%s' in DataTable '%s'"), *RowNameStr, *DT->GetName()),
			Data);
		return true;
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
	if (SubAction == TEXT("create_data_table"))
	{
		return McpDataHandlers::HandleCreateDataTable(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("add_data_table_row"))
	{
		return McpDataHandlers::HandleAddDataTableRow(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("set_data_table_row"))
	{
		return McpDataHandlers::HandleSetDataTableRow(this, RequestId, Payload, RequestingSocket);
	}

	SendAutomationError(RequestingSocket, RequestId,
		FString::Printf(TEXT("manage_data sub-action not yet implemented: %s"), *SubAction),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}
