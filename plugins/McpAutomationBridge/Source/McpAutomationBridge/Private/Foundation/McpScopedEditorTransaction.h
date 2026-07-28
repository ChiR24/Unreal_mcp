#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Task 43 scoped-transaction gate.
//
// An FScopedTransaction only buys rollback for mutations that stay in the editor
// object graph. Wrapping work that already wrote bytes to disk, launched a build
// or queued a render does NOT make it undoable -- it manufactures an undo entry
// that tells the user an irreversible action can be reverted. That is strictly
// worse than no transaction at all, so this gate exists to make the wrong wrap
// unrepresentable: the caller must classify the durability of its own mutation
// up front, and only EMcpMutationDurability::EditorStateOnly ever opens one.
//
// Three further checks run before a transaction is opened, because a transaction
// that records nothing is the same lie in a different shape:
//   * GEditor->Trans must exist (no buffer, no undo).
//   * Every captured object must carry RF_Transactional.
//   * Modify() must actually return true for every captured object; UObject::
//     Modify() reports whether the object reached the transaction buffer, and a
//     false there means undo cannot restore that object. The transaction is
//     cancelled in that case, which is safe precisely because the gate runs
//     before the caller has mutated anything.
//
// Nothing here is silent: every refusal carries a stable machine code and a
// human detail, and DescribeInto() writes the honest answer into the handler
// payload so the caller is told "not undoable" instead of being left to assume.
namespace McpDurableWrites
{
/**
 * Subscribes once to UPackage::PackageSavedWithContextEvent. Idempotent, and
 * bound to a static handler so editor shutdown order cannot leave a dangling
 * receiver behind.
 */
void EnsureWitnessBound();

/** Records one confirmed durable package write. The delegate calls this. */
void RecordDurableWrite(const FString& PackageName);

/** Monotonic count of confirmed durable package writes this process. */
uint64 GetWriteCount();

/** Package name of the most recent confirmed durable write, or empty. */
FString GetLastPackageName();
} // namespace McpDurableWrites

/** How durable the mutation about to run is. The CALLER declares this. */
enum class EMcpMutationDurability : uint8
{
	/** Editor object graph only; nothing leaves memory inside the scope. */
	EditorStateOnly,
	/** A package is written to disk in this scope (McpSafeAssetSave/LevelSave). */
	DurablePackageWrite,
	/** Work is handed to an out-of-process tool: build, cook, compile. */
	ExternalProcess,
	/** Work outlives the request on a queue: render, async build, import job. */
	AsyncPipeline
};

enum class EMcpTransactionState : uint8
{
	/** A real FScopedTransaction is open and every object reached the buffer. */
	Recording,
	/** The declared durability class forbids a transaction. */
	RefusedNotUndoable,
	/** No transaction buffer exists, so undo could not record anything. */
	RefusedNoTransactionBuffer,
	/** An object cannot record undo, so the transaction was never left open. */
	RefusedNonTransactionalObject,
	/** Opened as undoable, then a durable write was observed inside the scope. */
	BrokenByDurableWrite
};

/**
 * RAII gate in front of FScopedTransaction. Construct it on the stack with the
 * objects whose state is about to change; check IsRecordingUndo() if the caller
 * needs to branch; call DescribeInto() on the result payload either way.
 */
class FMcpScopedEditorTransaction
{
public:
	FMcpScopedEditorTransaction(
		const FText& InTitle,
		EMcpMutationDurability InDurability,
		const TArray<UObject*>& InObjects);
	~FMcpScopedEditorTransaction();

	FMcpScopedEditorTransaction(const FMcpScopedEditorTransaction&) = delete;
	FMcpScopedEditorTransaction& operator=(const FMcpScopedEditorTransaction&) = delete;

	/** True only while a real transaction is open AND no durable write escaped. */
	bool IsRecordingUndo() const;

	/** Recomputed on each call so a durable write mid-scope is never hidden. */
	EMcpTransactionState GetState() const;

	/** Stable machine code; empty only while genuinely recording. */
	FString GetRefusalCode() const;

	/** Human detail for the refusal; empty only while genuinely recording. */
	FString GetRefusalDetail() const;

	/**
	 * Writes an `undo` block stating plainly whether editor undo reverts this
	 * mutation. A transaction scope name is emitted ONLY when one is recording,
	 * so the payload can never name a transaction that does not exist.
	 */
	void DescribeInto(const TSharedPtr<FJsonObject>& Result) const;

private:
	FText Title;
	EMcpMutationDurability Durability;
	EMcpTransactionState OpenState;
	FString OpenCode;
	FString OpenDetail;
	uint64 DurableWritesAtOpen;

#if WITH_EDITOR
	TUniquePtr<class FScopedTransaction> Transaction;
#endif
};
