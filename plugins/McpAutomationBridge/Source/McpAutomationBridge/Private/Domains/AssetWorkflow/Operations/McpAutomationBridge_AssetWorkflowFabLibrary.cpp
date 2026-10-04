// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// The ICoreProvider / DataStorage::Features API used below first ships in UE 5.7; the
// TypedElementFramework module exists from 5.0, so MCP_HAS_TEDS alone is not enough.
#if MCP_HAS_TEDS && __has_include("DataStorage/Features.h")
#define MCP_FAB_LIBRARY_HAS_TEDS_API 1
#else
#define MCP_FAB_LIBRARY_HAS_TEDS_API 0
#endif

#if MCP_FAB_LIBRARY_HAS_TEDS_API
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

// Fab's own command that loads My Library into the table. It takes a page size and nothing else, and the
// text below is fixed: no caller text ever reaches a console, so there is nothing for the command
// validator to judge. The sync it starts is HTTP, a page at a time, answered over the following seconds.
const TCHAR* const FabSyncCommand = TEXT("Fab.TEDS.MyFolderIntegration");
constexpr int32 FabSyncPageSize = 1000;
// How long an empty table is given to fill, and how long its row count must hold still to call it done.
constexpr double FabSyncBudgetSeconds = 12.0;
constexpr double FabSyncQuietSeconds = 1.5;
constexpr float FabSyncPollSeconds = 0.5f;

/** What one library read asks for. Shared by value with the ticker that finishes a read after a sync. */
struct FFabLibraryRead
{
	TArray<const UScriptStruct *> Columns;
	TArray<TSharedPtr<FJsonValue>> Unresolved;
	FString Filter;
	int32 MaxRows = 200;
	uint64 Handle = 0;
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

/** Runs the query over the table once and returns how many rows it holds, before the filter and limit. */
int32 ReadFabLibraryRows(UE::Editor::DataStorage::ICoreProvider& Storage, const FFabLibraryRead& Read,
               TArray<TSharedPtr<FJsonValue>>& Entries)
{
	using namespace UE::Editor::DataStorage;
	Entries.Reset();
	int32 Total = 0;
	Storage.RunQuery(Read.Handle, DirectQueryCallbackRef(
		[&](const FQueryDescription &, IDirectQueryContext &Context) {
			const uint32 Count = Context.GetRowCount();
			Total += static_cast<int32>(Count);
			for (uint32 Index = 0; Index < Count && Entries.Num() < Read.MaxRows; ++Index) {
				TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
				for (const UScriptStruct *Column : Read.Columns) {
					const void *Base = Context.GetColumn(Column);
					if (Base == nullptr) {
						continue;
					}
					// GetColumn returns the batch array for this column, one packed
					// element per row, so the row index strides by the struct size.
					const void *Element = static_cast<const uint8 *>(Base) + (Index * Column->GetStructureSize());
					Entry->SetObjectField(Column->GetName(), ReadStructAsJson(Column, Element));
				}
				if (!Read.Filter.IsEmpty()) {
					FString Flat;
					const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Flat);
					FJsonSerializer::Serialize(Entry.ToSharedRef(), Writer);
					if (!Flat.Contains(Read.Filter, ESearchCase::CaseSensitive)) {
						continue;
					}
				}
				Entries.Add(MakeShared<FJsonValueObject>(Entry));
			}
		}));
	return Total;
}

/** Runs Fab's sync command. False, with the reason, when this editor does not register it. */
bool StartFabSync(FString& OutWhy)
{
	IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(FabSyncCommand);
	IConsoleCommand* Command = Object != nullptr ? Object->AsCommand() : nullptr;
	if (Command == nullptr)
	{
		OutWhy = TEXT("This editor registers no Fab.TEDS.MyFolderIntegration command, so the library cannot be synced.");
		return false;
	}
	TArray<FString> Args;
	Args.Add(FString::FromInt(FabSyncPageSize));
	if (!Command->Execute(Args, nullptr, *GLog))
	{
		OutWhy = TEXT("Fab's library sync command declined to run.");
		return false;
	}
	return true;
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
 * A table with no rows has simply never been synced, so this runs the sync itself
 * and answers once the rows have stopped arriving, or when the wait is spent. The
 * sync removes every row before it starts, which is why it is never run over a table
 * that already holds some.
 */
bool UMcpAutomationBridgeSubsystem::HandleListFabLibrary(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if MCP_FAB_LIBRARY_HAS_TEDS_API
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

  TSharedRef<FFabLibraryRead> Read = MakeShared<FFabLibraryRead>();
  for (const FString &Path : ColumnPaths) {
    const UScriptStruct *Resolved = Type(FTopLevelAssetPath(Path));
    if (Resolved != nullptr) {
      Read->Columns.Add(Resolved);
    } else {
      Read->Unresolved.Add(MakeShared<FJsonValueString>(Path));
    }
  }
  if (Read->Columns.Num() == 0) {
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
  for (const UScriptStruct *Column : Read->Columns) {
    Builder.ReadOnly(Column);
  }
  FQueryDescription Description = Builder.Compile();
  Read->Handle = Storage->RegisterQuery(MoveTemp(Description));

  // A real library is mostly engine versions and plugins (Source "uem"), which
  // crowd the genuine content out of any row limit. `filter` is already in this
  // capability's contract with exactly this meaning for the Megascans lookup;
  // honouring it here lets a caller ask for "fab" and get an inventory worth
  // reading, instead of paging past eleven rows called "Unreal Engine".
  Payload->TryGetStringField(TEXT("filter"), Read->Filter);
  double Limit = 200;
  Payload->TryGetNumberField(TEXT("limit"), Limit);
  Read->MaxRows = FMath::Clamp(static_cast<int32>(Limit), 1, 1000);

  // The answer, once the rows to report are in hand. WaitedSeconds is negative when no sync ran.
  const auto Reply = [](UMcpAutomationBridgeSubsystem *Self, const TSharedPtr<FMcpBridgeWebSocket> &Sock,
                        const FString &Id, const FFabLibraryRead &Done, const TArray<TSharedPtr<FJsonValue>> &Entries,
                        double WaitedSeconds, const FString &SkippedWhy) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("entries"), Entries);
    Result->SetNumberField(TEXT("entryCount"), Entries.Num());
    Result->SetArrayField(TEXT("unresolvedColumnTypes"), Done.Unresolved);
    Result->SetBoolField(TEXT("syncTriggered"), WaitedSeconds >= 0.0);
    if (WaitedSeconds >= 0.0) {
      Result->SetNumberField(TEXT("syncWaitedSeconds"), FMath::RoundToInt(WaitedSeconds * 10.0) / 10.0);
    }
    if (!SkippedWhy.IsEmpty()) {
      Result->SetStringField(TEXT("syncSkipped"), SkippedWhy);
    }
    FString Note = TEXT("Rows come from the last Fab.TEDS.MyFolderIntegration sync; re-run it to refresh the table.");
    if (Entries.Num() == 0 && WaitedSeconds >= 0.0) {
      Note = TEXT("The library sync ran and no rows arrived. Either the account's library is empty or the editor's Fab tab is signed out: Fab.Login (control_editor console_command) opens Epic's sign-in, and a repeat call syncs again.");
    } else if (Entries.Num() == 0 && !SkippedWhy.IsEmpty()) {
      Note = TEXT("The table is empty and Fab's library sync could not be run: syncSkipped says why.");
    } else if (Entries.Num() == 0) {
      Note = TEXT("No rows matched: the table holds rows that the filter, or the columns asked for, left out.");
    } else if (WaitedSeconds >= 0.0) {
      Note = TEXT("The table was empty, so Fab's library sync was run and waited for. It keeps loading pages in the background: call again for rows that arrive later.");
    }
    Result->SetStringField(TEXT("note"), Note);
    Self->SendAutomationResponse(Sock, Id, true,
        FString::Printf(TEXT("Fab library holds %d synced row(s)."), Entries.Num()), Result);
  };

  TArray<TSharedPtr<FJsonValue>> Entries;
  const int32 Total = ReadFabLibraryRows(*Storage, *Read, Entries);
  FString SkippedWhy;
  if (Total > 0 || !StartFabSync(SkippedWhy)) {
    Storage->UnregisterQuery(Read->Handle);
    Reply(this, Socket, RequestId, *Read, Entries, -1.0, Total > 0 ? FString() : SkippedWhy);
    return true;
  }

  // The sync answers over the next seconds, so the reply waits on the core ticker, never on the game thread.
  // Timed by the wall clock: a delayed ticker is handed the frame's delta, not the time since it last fired,
  // so summing it turned the 12 s budget into minutes and every signed-out read outlasted the client.
  struct FWait { double Start = FPlatformTime::Seconds(); double QuietSince = Start; int32 LastTotal = 0; };
  const TSharedRef<FWait> Wait = MakeShared<FWait>();
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
      [WeakThis, Wait, Read, Socket, RequestId, Reply](float) {
        ICoreProvider *Live = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
        UMcpAutomationBridgeSubsystem *Self = WeakThis.Get();
        if (Self == nullptr) {
          return false;
        }
        if (Live == nullptr) {
          Self->SendAutomationResponse(Socket, RequestId, false,
                                       TEXT("Editor data storage (TEDS) went away while the library synced."),
                                       nullptr, TEXT("NOT_SUPPORTED"));
          return false;
        }
        const double Now = FPlatformTime::Seconds();
        TArray<TSharedPtr<FJsonValue>> Rows;
        const int32 RowsNow = ReadFabLibraryRows(*Live, *Read, Rows);
        if (RowsNow != Wait->LastTotal) {
          Wait->LastTotal = RowsNow;
          Wait->QuietSince = Now;
        }
        if ((RowsNow == 0 || Now - Wait->QuietSince < FabSyncQuietSeconds) &&
            Now - Wait->Start < FabSyncBudgetSeconds) {
          return true;
        }
        Live->UnregisterQuery(Read->Handle);
        Reply(Self, Socket, RequestId, *Read, Rows, Now - Wait->Start, FString());
        return false;
      }), FabSyncPollSeconds);
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("This engine version has no editor data storage (TEDS) query API, so the Fab library cannot be read."),
      nullptr, TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
