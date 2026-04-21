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
		const bool bSaved = McpSafeAssetSave(DT);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("rowName"), RowNameStr);
		Data->SetStringField(TEXT("assetPath"), DT->GetPathName());
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved)
		{
			Data->SetStringField(TEXT("saveWarning"),
				TEXT("Asset changes in memory but save failed"));
		}
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

		// Stage fields into a temp buffer; commit into ExistingRow only on success
		// so a failed SetStructFieldsFromJsonObject leaves the existing row intact.
		FScopedRowBuffer Temp(DT->RowStruct);
		if (!Temp.Data)
		{
			SendError(Self, Socket, RequestId, TEXT("ENGINE_API_ERROR"),
				TEXT("Failed to allocate row buffer"));
			return true;
		}

		FString StructError;
		if (!McpStructReflection::SetStructFieldsFromJsonObject(
			DT->RowStruct, Temp.Data, *FieldsObj, StructError))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), StructError);
			return true;
		}

		// Commit: overwrite ExistingRow with validated temp contents.
		DT->RowStruct->CopyScriptStruct(ExistingRow, Temp.Data);

		DT->HandleDataTableChanged(RowName);
		DT->MarkPackageDirty();
		const bool bSaved = McpSafeAssetSave(DT);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("rowName"), RowNameStr);
		Data->SetStringField(TEXT("assetPath"), DT->GetPathName());
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved)
		{
			Data->SetStringField(TEXT("saveWarning"),
				TEXT("Asset changes in memory but save failed"));
		}
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Overwrote row '%s' in DataTable '%s'"), *RowNameStr, *DT->GetName()),
			Data);
		return true;
	}

	// -----------------------------------------------------------------------
	// update_data_table_row — partial patch (preserves un-specified fields).
	// -----------------------------------------------------------------------
	static bool HandleUpdateDataTableRow(UMcpAutomationBridgeSubsystem* Self,
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

		// Snapshot existing row into a temp buffer, apply patches to the temp
		// only, and copy back on full success — a mid-iteration failure leaves
		// the existing row untouched.
		FScopedRowBuffer Temp(DT->RowStruct);
		if (!Temp.Data)
		{
			SendError(Self, Socket, RequestId, TEXT("ENGINE_API_ERROR"),
				TEXT("Failed to allocate row buffer"));
			return true;
		}
		DT->RowStruct->CopyScriptStruct(Temp.Data, ExistingRow);

		TArray<TSharedPtr<FJsonValue>> UpdatedFieldNames;
		for (const auto& Pair : (*FieldsObj)->Values)
		{
			const FName ResolvedName = McpStructReflection::ResolveFieldName(DT->RowStruct, Pair.Key);
			if (ResolvedName.IsNone())
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
					FString::Printf(TEXT("Unknown field: %s"), *Pair.Key));
				return true;
			}
			FString SetError;
			if (!McpStructReflection::SetStructFieldFromJson(
				DT->RowStruct, Temp.Data, ResolvedName, Pair.Value, SetError))
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), SetError);
				return true;
			}
			UpdatedFieldNames.Add(MakeShared<FJsonValueString>(Pair.Key));
		}

		// Commit: all patches applied cleanly — overwrite ExistingRow.
		DT->RowStruct->CopyScriptStruct(ExistingRow, Temp.Data);

		DT->HandleDataTableChanged(RowName);
		DT->MarkPackageDirty();
		const bool bSaved = McpSafeAssetSave(DT);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("rowName"), RowNameStr);
		Data->SetStringField(TEXT("assetPath"), DT->GetPathName());
		Data->SetArrayField(TEXT("updatedFields"), UpdatedFieldNames);
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved)
		{
			Data->SetStringField(TEXT("saveWarning"),
				TEXT("Asset changes in memory but save failed"));
		}
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Updated %d field(s) in row '%s'"),
				UpdatedFieldNames.Num(), *RowNameStr),
			Data);
		return true;
	}

	// -----------------------------------------------------------------------
	// remove_data_table_row
	// -----------------------------------------------------------------------
	static bool HandleRemoveDataTableRow(UMcpAutomationBridgeSubsystem* Self,
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

		UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
		if (!DT)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("DataTable not found: %s"), *PathStr));
			return true;
		}

		const FName RowName(*RowNameStr);
		if (!DT->GetRowMap().Contains(RowName))
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("Row not found: %s"), *RowNameStr));
			return true;
		}

		DT->RemoveRow(RowName);
		DT->MarkPackageDirty();
		const bool bSaved = McpSafeAssetSave(DT);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("rowName"), RowNameStr);
		Data->SetStringField(TEXT("assetPath"), DT->GetPathName());
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved)
		{
			Data->SetStringField(TEXT("saveWarning"),
				TEXT("Asset changes in memory but save failed"));
		}
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Removed row '%s' from DataTable '%s'"),
				*RowNameStr, *DT->GetName()),
			Data);
		return true;
	}

	// -----------------------------------------------------------------------
	// get_data_table_rows
	// -----------------------------------------------------------------------
	static bool HandleGetDataTableRows(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field: path"));
			return true;
		}

		UDataTable* DT = LoadDataTableOrError(Self, Socket, RequestId, PathStr);
		if (!DT) { return true; }

		TSet<FString> Filter;
		const TArray<TSharedPtr<FJsonValue>>* NamesArr = nullptr;
		if (Payload->TryGetArrayField(TEXT("rowNames"), NamesArr) && NamesArr)
		{
			for (const auto& V : *NamesArr)
			{
				if (V.IsValid() && V->Type == EJson::String)
				{
					Filter.Add(V->AsString());
				}
			}
		}

		TSharedPtr<FJsonObject> RowsObj = MakeShared<FJsonObject>();
		int32 RowCount = 0;
		for (const auto& Pair : DT->GetRowMap())
		{
			const FString RowName = Pair.Key.ToString();
			if (Filter.Num() > 0 && !Filter.Contains(RowName)) { continue; }
			TSharedPtr<FJsonObject> RowJson = McpStructReflection::StructInstanceToJson(
				DT->RowStruct, Pair.Value);
			RowsObj->SetObjectField(RowName, RowJson);
			++RowCount;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), DT->GetPathName());
		Data->SetObjectField(TEXT("rows"), RowsObj);
		Data->SetNumberField(TEXT("rowCount"), RowCount);
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Fetched %d row(s) from DataTable '%s'"),
				RowCount, *DT->GetName()),
			Data);
		return true;
	}

	// -----------------------------------------------------------------------
	// list_data_table_rows — row name array only (no field deserialization).
	// -----------------------------------------------------------------------
	static bool HandleListDataTableRows(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field: path"));
			return true;
		}

		UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
		if (!DT)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("DataTable not found: %s"), *PathStr));
			return true;
		}

		TArray<TSharedPtr<FJsonValue>> Names;
		for (const auto& Pair : DT->GetRowMap())
		{
			Names.Add(MakeShared<FJsonValueString>(Pair.Key.ToString()));
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), DT->GetPathName());
		Data->SetArrayField(TEXT("rowNames"), Names);
		Data->SetNumberField(TEXT("rowCount"), Names.Num());
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Listed %d row name(s) for DataTable '%s'"),
				Names.Num(), *DT->GetName()),
			Data);
		return true;
	}

	// -----------------------------------------------------------------------
	// set_data_table_row_struct — swap RowStruct (schema migration).
	//
	// WARNING: Field values in columns that do not exist on the new struct are
	// destroyed. Callers that need to preserve per-column data must first
	// snapshot via get_data_table_rows, then re-set via set_data_table_row.
	// -----------------------------------------------------------------------
	static bool HandleSetDataTableRowStruct(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr, NewStructPath;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr) ||
			!Payload->TryGetStringField(TEXT("newRowStructPath"), NewStructPath))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field(s): path, newRowStructPath"));
			return true;
		}

		UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
		if (!DT)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("DataTable not found: %s"), *PathStr));
			return true;
		}

		UScriptStruct* NewStruct = LoadRowStruct(NewStructPath);
		if (!NewStruct)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("New row struct not found: %s"), *NewStructPath));
			return true;
		}

		const int32 RowCount = DT->GetRowMap().Num();

		// Compute field-name deltas between the old and new row structs so the
		// response can tell callers exactly which columns survived, which were
		// dropped, and which are newly available. Names are raw FProperty names
		// (for UDS these include the internal GUID suffix — acceptable for
		// display; callers cross-reference with get_data_table_rows field keys).
		TSet<FString> OldFields;
		if (const UScriptStruct* OldRowStruct = DT->RowStruct)
		{
			for (TFieldIterator<FProperty> It(OldRowStruct); It; ++It)
			{
				OldFields.Add(It->GetName());
			}
		}
		TSet<FString> NewFields;
		for (TFieldIterator<FProperty> It(NewStruct); It; ++It)
		{
			NewFields.Add(It->GetName());
		}
		const TSet<FString> Preserved = OldFields.Intersect(NewFields);
		const TSet<FString> Dropped = OldFields.Difference(NewFields);
		const TSet<FString> Added = NewFields.Difference(OldFields);

		auto SetToJsonArray = [](const TSet<FString>& Src) -> TArray<TSharedPtr<FJsonValue>>
		{
			TArray<TSharedPtr<FJsonValue>> Out;
			Out.Reserve(Src.Num());
			for (const FString& Name : Src)
			{
				Out.Add(MakeShared<FJsonValueString>(Name));
			}
			return Out;
		};

		// UE 5.7 editor path: CleanBeforeStructChange() frees the existing row
		// allocations so we can assign a new RowStruct without corrupting the
		// map; RestoreAfterStructChange() re-allocates rows using the new
		// struct's size. HandleDataTableChanged broadcasts to open editors.
		DT->CleanBeforeStructChange();
		DT->RowStruct = NewStruct;
		DT->RestoreAfterStructChange();
		DT->HandleDataTableChanged();

		DT->MarkPackageDirty();
		const bool bSaved = McpSafeAssetSave(DT);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), DT->GetPathName());
		Data->SetStringField(TEXT("rowStructPath"), NewStruct->GetPathName());
		// Renamed from misleading "rowsMigrated": UE's RestoreAfterStructChange
		// reinitializes each row in the new struct's shape, preserving only
		// compatible same-named fields. It is not a lossless migration.
		Data->SetNumberField(TEXT("rowsReinitialized"), RowCount);
		Data->SetArrayField(TEXT("fieldsPreserved"), SetToJsonArray(Preserved));
		Data->SetArrayField(TEXT("fieldsDropped"), SetToJsonArray(Dropped));
		Data->SetArrayField(TEXT("fieldsAdded"), SetToJsonArray(Added));
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved)
		{
			Data->SetStringField(TEXT("saveWarning"),
				TEXT("Asset changes in memory but save failed"));
		}
		Data->SetStringField(TEXT("warning"),
			TEXT("Field values in removed columns were destroyed; callers "
				"must snapshot rows via get_data_table_rows before migration "
				"if preservation is required."));
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Reinitialized DataTable '%s' under RowStruct '%s' (%d row(s))"),
				*DT->GetName(), *NewStruct->GetName(), RowCount),
			Data);
		return true;
	}

	// -----------------------------------------------------------------------
	// create_data_asset — instantiate a UDataAsset subclass at path/name.
	//
	// dataAssetClassPath accepts either a fully qualified generated-class path
	// ("/Game/DataTest/BP_Ch3ItemData.BP_ItemData_C") or a shorthand package
	// path ("/Game/DataTest/BP_Ch3ItemData"), which is normalized to the
	// generated class form on lookup miss.
	// -----------------------------------------------------------------------
	static bool HandleCreateDataAsset(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr, NameStr, ClassPathStr;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr) ||
			!Payload->TryGetStringField(TEXT("name"), NameStr) ||
			!Payload->TryGetStringField(TEXT("dataAssetClassPath"), ClassPathStr))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field(s): path, name, dataAssetClassPath"));
			return true;
		}

		UClass* AssetClass = LoadObject<UClass>(nullptr, *ClassPathStr);
		if (!AssetClass)
		{
			// Normalize "/Game/Foo/Bar" → "/Game/Foo/Bar.Bar_C" for Blueprint
			// generated classes (common MCP caller shortcut).
			FString Normalized = ClassPathStr;
			if (!Normalized.Contains(TEXT(".")))
			{
				int32 SlashIdx = INDEX_NONE;
				if (Normalized.FindLastChar(TEXT('/'), SlashIdx))
				{
					Normalized = Normalized + TEXT(".") + Normalized.Mid(SlashIdx + 1) + TEXT("_C");
				}
			}
			AssetClass = LoadObject<UClass>(nullptr, *Normalized);
		}
		if (!AssetClass || !AssetClass->IsChildOf(UDataAsset::StaticClass()))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				FString::Printf(TEXT("Not a UDataAsset subclass: %s"), *ClassPathStr));
			return true;
		}

		const FString FullPath = PathStr / NameStr;
		if (UEditorAssetLibrary::DoesAssetExist(FullPath))
		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("assetPath"), FullPath);
			Data->SetBoolField(TEXT("alreadyExists"), true);
			SendSuccess(Self, Socket, RequestId,
				FString::Printf(TEXT("DataAsset already exists at '%s'"), *FullPath), Data);
			return true;
		}

		FString OutError;
		bool bSaved = false;
		UObject* NewAsset = McpGenericAssetFactory::CreateAssetOfClass(
			AssetClass, PathStr, NameStr, nullptr, OutError, bSaved);
		if (!NewAsset)
		{
			SendError(Self, Socket, RequestId, TEXT("ENGINE_API_ERROR"),
				OutError.IsEmpty() ? TEXT("CreateAsset returned nullptr") : OutError);
			return true;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved && !OutError.IsEmpty())
		{
			Data->SetStringField(TEXT("saveWarning"), OutError);
		}
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Created DataAsset at '%s'"), *NewAsset->GetPathName()),
			Data);
		return true;
	}

	// -----------------------------------------------------------------------
	// set_data_asset_property — write a JSON value at a dotted/indexed path.
	//
	// NOTE: McpPropertyPath::SetValueAtPath writes directly to the UObject via
	// FJsonObjectConverter::JsonValueToUProperty. For a single scalar write
	// this is atomic. For nested struct writes, partial failure can leave the
	// target mutated; callers that need transactional semantics should read
	// back via get_data_asset_property and compare.
	// -----------------------------------------------------------------------
	static bool HandleSetDataAssetProperty(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr, PropPath;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr) ||
			!Payload->TryGetStringField(TEXT("propertyPath"), PropPath))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field(s): path, propertyPath"));
			return true;
		}
		const TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("value"));
		if (!Value.IsValid())
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field: value"));
			return true;
		}

		UObject* Asset = LoadObject<UObject>(nullptr, *PathStr);
		if (!Asset)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("Asset not found: %s"), *PathStr));
			return true;
		}

		FString WalkError;
		if (!McpPropertyPath::SetValueAtPath(Asset, PropPath, Value, WalkError))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				WalkError.IsEmpty() ? TEXT("SetValueAtPath failed") : WalkError);
			return true;
		}

		Asset->MarkPackageDirty();
		const bool bSaved = McpSafeAssetSave(Asset);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), Asset->GetPathName());
		Data->SetStringField(TEXT("propertyPath"), PropPath);
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved)
		{
			Data->SetStringField(TEXT("saveWarning"),
				TEXT("Asset changes in memory but save failed"));
		}
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Set '%s' on asset '%s'"), *PropPath, *Asset->GetName()),
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
	if (SubAction == TEXT("update_data_table_row"))
	{
		return McpDataHandlers::HandleUpdateDataTableRow(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("remove_data_table_row"))
	{
		return McpDataHandlers::HandleRemoveDataTableRow(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("get_data_table_rows"))
	{
		return McpDataHandlers::HandleGetDataTableRows(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("list_data_table_rows"))
	{
		return McpDataHandlers::HandleListDataTableRows(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("set_data_table_row_struct"))
	{
		return McpDataHandlers::HandleSetDataTableRowStruct(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("create_data_asset"))
	{
		return McpDataHandlers::HandleCreateDataAsset(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("set_data_asset_property"))
	{
		return McpDataHandlers::HandleSetDataAssetProperty(this, RequestId, Payload, RequestingSocket);
	}

	SendAutomationError(RequestingSocket, RequestId,
		FString::Printf(TEXT("manage_data sub-action not yet implemented: %s"), *SubAction),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}
