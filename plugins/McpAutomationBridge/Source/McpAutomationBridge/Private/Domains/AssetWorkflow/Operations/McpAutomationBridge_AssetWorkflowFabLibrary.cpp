// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#if MCP_HAS_TEDS
#include "DataStorage/Features.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "UObject/UnrealType.h"

namespace
{
/**
 * Default columns the Fab plugin writes for each My Library entry.
 *
 * Resolved by path string rather than by type, so this never links the Fab
 * module: the columns are reflected USTRUCTs, and a caller can override the
 * list when Fab changes its schema without this needing a rebuild.
 */
const TCHAR* DefaultFabColumns[] = {
	TEXT("/Script/Fab.FabObjectNameColumn"),
	// A name on its own is a dead end: nothing in it can be handed to
	// add_fab_asset_to_project, so the library read like an inventory but could
	// not be acted on. FabObjectColumn carries the AssetId that capability needs,
	// plus ListingType, Seller and Source. Selecting a column is also the row
	// filter, so this is only safe because Fab writes both for every synced row.
	TEXT("/Script/Fab.FabObjectColumn"),
};

/** Reads one struct instance into JSON via reflection, property by property. */
TSharedPtr<FJsonObject> ReadStructAsJson(const UScriptStruct* Type, const void* Element)
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(Type); It; ++It)
	{
		FString Text;
		MCP_PROPERTY_EXPORT_TEXT(It, Text, It->ContainerPtrToValuePtr<void>(Element), nullptr, nullptr, PPF_None);
		Out->SetStringField(It->GetName(), Text);
	}
	return Out;
}
} // namespace
#endif

/**
 * Lists the Fab "My Library" rows the plugin synced into editor data storage.
 *
 * Fab.TEDS.MyFolderIntegration fetches the account's library over the plugin's
 * own authenticated session and writes it into TEDS; this reads it back. The
 * data storage is reached through the modular-features registry, and columns
 * are resolved by path, so nothing here depends on the Fab module — the same
 * reflection approach describe_reflected_api uses.
 *
 * Run `Fab.Login` then `Fab.TEDS.MyFolderIntegration <batchSize>` first; the
 * sync is paginated, so a large library needs several passes.
 */
bool UMcpAutomationBridgeSubsystem::HandleListFabLibrary(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if MCP_HAS_TEDS
  using namespace UE::Editor::DataStorage;
  using namespace UE::Editor::DataStorage::Queries;

  ICoreProvider *Storage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
  if (Storage == nullptr) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor data storage (TEDS) is unavailable in this build."),
                           nullptr, TEXT("NOT_SUPPORTED"));
    return true;
  }

  TArray<FString> ColumnPaths;
  const TArray<TSharedPtr<FJsonValue>> *Requested = nullptr;
  if (Payload->TryGetArrayField(TEXT("columnTypes"), Requested) && Requested != nullptr &&
      Requested->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Value : *Requested) {
      ColumnPaths.Add(Value->AsString());
    }
  } else {
    for (const TCHAR *Path : DefaultFabColumns) {
      ColumnPaths.Add(Path);
    }
  }

  TArray<const UScriptStruct *> Columns;
  TArray<TSharedPtr<FJsonValue>> Unresolved;
  for (const FString &Path : ColumnPaths) {
    const UScriptStruct *Resolved = Type(FTopLevelAssetPath(Path));
    if (Resolved != nullptr) {
      Columns.Add(Resolved);
    } else {
      Unresolved.Add(MakeShared<FJsonValueString>(Path));
    }
  }
  if (Columns.Num() == 0) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("No requested column type resolved. Fab writes its columns only after a successful Fab.TEDS.MyFolderIntegration sync."),
        nullptr, TEXT("NOT_FOUND"));
    return true;
  }

  // ReadOnly already declares the column as a required fragment, so adding the
  // same type again through Where().All() registers a duplicate requirement and
  // TEDS asserts ("Duplicated requirements are not supported"). Selecting the
  // columns IS the filter: only rows carrying all of them match.
  Select Builder;
  for (const UScriptStruct *Column : Columns) {
    Builder.ReadOnly(Column);
  }
  FQueryDescription Description = Builder.Compile();
  const QueryHandle Handle = Storage->RegisterQuery(MoveTemp(Description));

  // A real library is mostly engine versions and plugins (Source "uem"), which
  // crowd the genuine content out of any row limit. `filter` is already in this
  // capability's contract with exactly this meaning for the Megascans lookup;
  // honouring it here lets a caller ask for "fab" and get an inventory worth
  // reading, instead of paging past eleven rows called "Unreal Engine".
  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);

  double Limit = 200;
  Payload->TryGetNumberField(TEXT("limit"), Limit);
  const int32 MaxRows = FMath::Clamp(static_cast<int32>(Limit), 1, 1000);

  TArray<TSharedPtr<FJsonValue>> Entries;
  Storage->RunQuery(Handle, DirectQueryCallbackRef(
      [&Entries, &Columns, MaxRows, &Filter](const FQueryDescription &, IDirectQueryContext &Context) {
        const uint32 Count = Context.GetRowCount();
        for (uint32 Index = 0; Index < Count && Entries.Num() < MaxRows; ++Index) {
          TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
          for (const UScriptStruct *Column : Columns) {
            const void *Base = Context.GetColumn(Column);
            if (Base == nullptr) {
              continue;
            }
            // GetColumn returns the batch array for this column, one packed
            // element per row, so the row index strides by the struct size.
            const void *Element =
                static_cast<const uint8 *>(Base) + (Index * Column->GetStructureSize());
            Entry->SetObjectField(Column->GetName(), ReadStructAsJson(Column, Element));
          }
          if (!Filter.IsEmpty()) {
            FString Flat;
            const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Flat);
            FJsonSerializer::Serialize(Entry.ToSharedRef(), Writer);
            if (!Flat.Contains(Filter, ESearchCase::CaseSensitive)) {
              continue;
            }
          }
          Entries.Add(MakeShared<FJsonValueObject>(Entry));
        }
      }));

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("entries"), Entries);
  Result->SetNumberField(TEXT("entryCount"), Entries.Num());
  Result->SetArrayField(TEXT("unresolvedColumnTypes"), Unresolved);
  Result->SetStringField(
      TEXT("note"),
      Entries.Num() > 0
          ? TEXT("Rows come from the last Fab.TEDS.MyFolderIntegration sync; re-run it to refresh or page further.")
          : TEXT("No rows. Being signed in is not enough: the library is readable "
                 "only after a sync, so run Fab.TEDS.MyFolderIntegration <batchSize> "
                 "through control_editor.console_command and retry. Fab.Login is "
                 "needed only when the editor's Fab tab shows you signed out."));
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Fab library holds %d synced row(s)."), Entries.Num()), Result);
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("This build has no TypedElementFramework module, so the Fab library cannot be read."),
      nullptr, TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
#else
bool UMcpAutomationBridgeSubsystem::HandleListFabLibrary(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor required."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif
