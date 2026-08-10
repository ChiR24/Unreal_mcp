#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Task 43 compensating-cleanup receipt.
//
// Save-all, build and render work is NOT atomic: each package, each build step
// and each render frame lands independently, and the ones that landed stay
// landed when a later one fails. No editor transaction can revert a completed
// save, so a receipt for this class of work must never suggest one could.
//
// This type therefore has no "rollback succeeded" shape to fill in. It states
// three things and nothing else: what completed (and is now durable), what did
// not, and what the CALLER must do to reach a clean state. `rollback` is always
// the literal string "unavailable" and `atomic` is always false, on every path,
// including the all-succeeded path -- because "everything worked" still does not
// mean "this could be undone".
//
// Compensation is not undo. A compensating capability is a separate, explicit
// call the caller can make to reverse a durable effect (delete what was created,
// stop what was started). Naming one is honest; implying the original call was
// rolled back is not.

/** One unit of a non-atomic multi-step operation. */
struct FMcpCompensationStep
{
	/** Machine-readable step id, e.g. "save:/Game/Maps/Main". */
	FString Step;
	/** Human detail: what landed, or why it did not. */
	FString Detail;
};

class FMcpCompensationReceipt
{
public:
	explicit FMcpCompensationReceipt(const FString& InOperation);

	/** Records a step that completed and whose effect is now durable. */
	void NoteCompleted(const FString& Step, const FString& Detail);

	/** Records a step that did not complete, with the reason it did not. */
	void NoteNotCompleted(const FString& Step, const FString& Reason);

	/** Records a step deliberately not attempted; neither done nor failed. */
	void NoteSkipped(const FString& Step, const FString& Reason);

	/** A capability the caller may call to reverse a durable effect. */
	void AddCompensatingCapability(const FString& CapabilityId);

	/** Exact instruction for reaching a clean state. Required when partial. */
	void SetCallerAction(const FString& Guidance);

	/** True when something landed AND something did not. */
	bool IsPartial() const;

	/** "completed" | "partial" | "failed" | "noop". */
	FString GetState() const;

	int32 GetCompletedCount() const;
	int32 GetNotCompletedCount() const;

	/**
	 * Writes the `compensation` block. Emits `atomic:false` and
	 * `rollback:"unavailable"` unconditionally; there is no code path that can
	 * emit a rollback promise.
	 */
	void DescribeInto(const TSharedPtr<FJsonObject>& Result) const;

private:
	FString Operation;
	FString CallerAction;
	TArray<FMcpCompensationStep> Completed;
	TArray<FMcpCompensationStep> NotCompleted;
	TArray<FMcpCompensationStep> Skipped;
	TArray<FString> CompensatingCapabilities;
};
