#include "Foundation/McpScopedEditorTransaction.h"

#include "HAL/CriticalSection.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/Transactor.h"
#include "ScopedTransaction.h"
#include "UObject/ObjectSaveContext.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpScopedTransaction, Log, All);

namespace McpDurableWrites
{
namespace
{
FCriticalSection GWitnessMutex;
uint64 GWriteCount = 0;
FString GLastPackageName;
bool GBound = false;

#if WITH_EDITOR
void OnPackageSaved(const FString& PackageFilename, UPackage* Package, FObjectPostSaveContext Context)
{
	(void)Context;
	RecordDurableWrite(Package ? Package->GetName() : PackageFilename);
}
#endif
} // namespace

void EnsureWitnessBound()
{
#if WITH_EDITOR
	FScopeLock Lock(&GWitnessMutex);
	if (GBound)
	{
		return;
	}
	GBound = true;
	// Static handler on purpose: a raw `this` would dangle if editor shutdown
	// destroyed the witness before the delegate was broadcast.
	UPackage::PackageSavedWithContextEvent.AddStatic(&OnPackageSaved);
#endif
}

void RecordDurableWrite(const FString& PackageName)
{
	FScopeLock Lock(&GWitnessMutex);
	++GWriteCount;
	GLastPackageName = PackageName;
}

uint64 GetWriteCount()
{
	FScopeLock Lock(&GWitnessMutex);
	return GWriteCount;
}

FString GetLastPackageName()
{
	FScopeLock Lock(&GWitnessMutex);
	return GLastPackageName;
}
} // namespace McpDurableWrites

namespace
{
const TCHAR* const BrokenByWriteCode = TEXT("UNDO_BROKEN_BY_DURABLE_WRITE");

bool ClassifyDurability(EMcpMutationDurability Durability, FString& OutCode, FString& OutDetail)
{
	switch (Durability)
	{
	case EMcpMutationDurability::EditorStateOnly:
		return true;
	case EMcpMutationDurability::DurablePackageWrite:
		OutCode = TEXT("UNDO_UNAVAILABLE_DURABLE_WRITE");
		OutDetail = TEXT("This operation writes a package to disk. Editor undo restores the "
						 "in-memory object, never the bytes already written, so no transaction is "
						 "opened. Reverse it with an explicit compensating call.");
		return false;
	case EMcpMutationDurability::ExternalProcess:
		OutCode = TEXT("UNDO_UNAVAILABLE_EXTERNAL_PROCESS");
		OutDetail = TEXT("This operation hands work to an out-of-process tool (build, cook or "
						 "compile). Its effects live outside the editor transaction buffer, so no "
						 "transaction is opened.");
		return false;
	case EMcpMutationDurability::AsyncPipeline:
		OutCode = TEXT("UNDO_UNAVAILABLE_ASYNC_PIPELINE");
		OutDetail = TEXT("This operation queues work that outlives the request (render or async "
						 "job). It is still running when the scope closes, so no transaction is "
						 "opened.");
		return false;
	}
	OutCode = TEXT("UNDO_UNAVAILABLE_DURABLE_WRITE");
	OutDetail = TEXT("Unclassified durability; refusing to claim undo.");
	return false;
}
} // namespace

FMcpScopedEditorTransaction::FMcpScopedEditorTransaction(
	const FText& InTitle,
	EMcpMutationDurability InDurability,
	const TArray<UObject*>& InObjects)
	: Title(InTitle)
	, Durability(InDurability)
	, OpenState(EMcpTransactionState::RefusedNotUndoable)
	, DurableWritesAtOpen(0)
{
	McpDurableWrites::EnsureWitnessBound();
	DurableWritesAtOpen = McpDurableWrites::GetWriteCount();

	if (!ClassifyDurability(Durability, OpenCode, OpenDetail))
	{
		return;
	}

#if WITH_EDITOR
	if (!GEditor || !GEditor->Trans)
	{
		OpenState = EMcpTransactionState::RefusedNoTransactionBuffer;
		OpenCode = TEXT("UNDO_UNAVAILABLE_NO_TRANSACTION_BUFFER");
		OpenDetail = TEXT("No editor transaction buffer exists, so a transaction would record "
						  "nothing and undo could not restore this mutation.");
		return;
	}

	for (const UObject* Object : InObjects)
	{
		if (!Object || !Object->HasAnyFlags(RF_Transactional))
		{
			OpenState = EMcpTransactionState::RefusedNonTransactionalObject;
			OpenCode = TEXT("UNDO_UNAVAILABLE_NON_TRANSACTIONAL_OBJECT");
			OpenDetail = FString::Printf(
				TEXT("'%s' does not carry RF_Transactional, so Modify() cannot reach the undo "
					 "buffer and a transaction here would be empty."),
				Object ? *Object->GetName() : TEXT("<null>"));
			return;
		}
	}

	Transaction = MakeUnique<FScopedTransaction>(Title);
	for (UObject* Object : InObjects)
	{
		// Modify() returns whether the object actually reached the transaction
		// buffer, so a false here means undo could not restore it. Cancel rather
		// than leave a transaction open that records only part of the change --
		// safe precisely because this runs before the caller mutates anything.
		if (!Object->Modify())
		{
			Transaction->Cancel();
			Transaction.Reset();
			OpenState = EMcpTransactionState::RefusedNonTransactionalObject;
			OpenCode = TEXT("UNDO_UNAVAILABLE_NON_TRANSACTIONAL_OBJECT");
			OpenDetail = FString::Printf(
				TEXT("'%s' did not reach the undo buffer, so undo could not restore it."),
				*Object->GetName());
			return;
		}
	}

	OpenState = EMcpTransactionState::Recording;
	OpenCode.Reset();
	OpenDetail.Reset();
#else
	OpenState = EMcpTransactionState::RefusedNoTransactionBuffer;
	OpenCode = TEXT("UNDO_UNAVAILABLE_NO_TRANSACTION_BUFFER");
	OpenDetail = TEXT("Undo requires the editor.");
#endif
}

FMcpScopedEditorTransaction::~FMcpScopedEditorTransaction()
{
	if (GetState() == EMcpTransactionState::BrokenByDurableWrite)
	{
		UE_LOG(LogMcpScopedTransaction, Warning,
			TEXT("Transaction '%s' was opened as undoable but '%s' was written to disk inside its "
				 "scope; undo restores the object and not the file."),
			*Title.ToString(), *McpDurableWrites::GetLastPackageName());
	}
}

bool FMcpScopedEditorTransaction::IsRecordingUndo() const
{
	return GetState() == EMcpTransactionState::Recording;
}

EMcpTransactionState FMcpScopedEditorTransaction::GetState() const
{
	if (OpenState == EMcpTransactionState::Recording &&
		McpDurableWrites::GetWriteCount() != DurableWritesAtOpen)
	{
		return EMcpTransactionState::BrokenByDurableWrite;
	}
	return OpenState;
}

FString FMcpScopedEditorTransaction::GetRefusalCode() const
{
	const EMcpTransactionState State = GetState();
	if (State == EMcpTransactionState::Recording)
	{
		return FString();
	}
	return State == EMcpTransactionState::BrokenByDurableWrite
		? FString(BrokenByWriteCode)
		: OpenCode;
}

FString FMcpScopedEditorTransaction::GetRefusalDetail() const
{
	const EMcpTransactionState State = GetState();
	if (State == EMcpTransactionState::Recording)
	{
		return FString();
	}
	if (State == EMcpTransactionState::BrokenByDurableWrite)
	{
		return FString::Printf(
			TEXT("'%s' was written to disk inside this scope. Editor undo restores the in-memory "
				 "object only; the file keeps the change."),
			*McpDurableWrites::GetLastPackageName());
	}
	return OpenDetail;
}

void FMcpScopedEditorTransaction::DescribeInto(const TSharedPtr<FJsonObject>& Result) const
{
	if (!Result.IsValid())
	{
		return;
	}

	const bool bUndoable = IsRecordingUndo();
	const TSharedPtr<FJsonObject> Undo = MakeShared<FJsonObject>();
	Undo->SetBoolField(TEXT("undoable"), bUndoable);
	if (bUndoable)
	{
		Undo->SetStringField(TEXT("transactionScope"), Title.ToString());
	}
	else
	{
		Undo->SetStringField(TEXT("reasonCode"), GetRefusalCode());
		Undo->SetStringField(TEXT("reason"), GetRefusalDetail());
	}
	Result->SetObjectField(TEXT("undo"), Undo);
}
