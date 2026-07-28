#include "Foundation/McpCompensationReceipt.h"
#include "Foundation/McpScopedEditorTransaction.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Misc/AutomationTest.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
const FName ProbeTag(TEXT("McpTask43UndoProbe"));

/** A fresh transactional object with no outer asset, so nothing on disk moves. */
USceneComponent* MakeTransactionalProbe()
{
	return NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transactional);
}

/** Same probe, minus RF_Transactional: Modify() cannot reach the undo buffer. */
USceneComponent* MakeNonTransactionalProbe()
{
	return NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_NoFlags);
}

void ClearUndoBuffer()
{
	if (GEditor && GEditor->Trans)
	{
		GEditor->Trans->Reset(FText::FromString(TEXT("Mcp Task 43 test isolation")));
	}
}

FString ToJson(const TSharedPtr<FJsonObject>& Object)
{
	// Condensed on purpose: the substring checks below must see `"rollback":true`
	// with no whitespace to hide behind.
	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	return Out;
}

const TSharedPtr<FJsonObject>* Block(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Key)
{
	const TSharedPtr<FJsonObject>* Found = nullptr;
	return Payload->TryGetObjectField(Key, Found) ? Found : nullptr;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpScopedTransactionUndoTest,
	"McpAutomationBridge.Foundation.ScopedTransaction.UndoRevertsEditorStateOnlyMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpScopedTransactionUndoTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GEditor || !GEditor->Trans)
	{
		AddError(TEXT("No editor transaction buffer; undo cannot be proven here."));
		return false;
	}

	ClearUndoBuffer();
	USceneComponent* Probe = MakeTransactionalProbe();
	TestEqual(TEXT("the probe starts clean"), Probe->ComponentTags.Num(), 0);

	{
		FMcpScopedEditorTransaction Transaction(
			FText::FromString(TEXT("Mcp Probe Tag")),
			EMcpMutationDurability::EditorStateOnly,
			TArray<UObject*>{Probe});

		TestTrue(TEXT("an editor-state-only mutation records undo"), Transaction.IsRecordingUndo());
		TestEqual(TEXT("no refusal while recording"), Transaction.GetRefusalCode(), FString());

		const TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Transaction.DescribeInto(Payload);
		const TSharedPtr<FJsonObject>* Undo = Block(Payload, TEXT("undo"));
		if (Undo)
		{
			TestTrue(TEXT("the payload claims undo only when recording"),
				(*Undo)->GetBoolField(TEXT("undoable")));
			TestEqual(TEXT("the payload names the real transaction scope"),
				(*Undo)->GetStringField(TEXT("transactionScope")), FString(TEXT("Mcp Probe Tag")));
		}
		else
		{
			AddError(TEXT("DescribeInto wrote no undo block."));
		}

		Probe->ComponentTags.Add(ProbeTag);
	}

	TestEqual(TEXT("the mutation applied inside the scope"), Probe->ComponentTags.Num(), 1);

	// The load-bearing assertion: a real editor undo, not a simulated one.
	GEditor->UndoTransaction();
	TestEqual(TEXT("editor undo genuinely reverted the wrapped mutation"),
		Probe->ComponentTags.Num(), 0);

	ClearUndoBuffer();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpScopedTransactionRefusalTest,
	"McpAutomationBridge.Foundation.ScopedTransaction.RefusesNonUndoableWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpScopedTransactionRefusalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Each case is work no transaction can revert. The gate must open nothing,
	// so the mutation must survive an undo attempt afterwards.
	struct FCase
	{
		EMcpMutationDurability Durability;
		const TCHAR* Code;
		const TCHAR* Label;
	};
	const FCase Cases[] = {
		{EMcpMutationDurability::DurablePackageWrite, TEXT("UNDO_UNAVAILABLE_DURABLE_WRITE"), TEXT("save")},
		{EMcpMutationDurability::ExternalProcess, TEXT("UNDO_UNAVAILABLE_EXTERNAL_PROCESS"), TEXT("build")},
		{EMcpMutationDurability::AsyncPipeline, TEXT("UNDO_UNAVAILABLE_ASYNC_PIPELINE"), TEXT("render")}
	};

	for (const FCase& Case : Cases)
	{
		ClearUndoBuffer();
		USceneComponent* Probe = MakeTransactionalProbe();
		const TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();

		{
			FMcpScopedEditorTransaction Transaction(
				FText::FromString(FString::Printf(TEXT("Mcp %s"), Case.Label)),
				Case.Durability,
				TArray<UObject*>{Probe});

			TestFalse(FString::Printf(TEXT("%s is not wrapped"), Case.Label),
				Transaction.IsRecordingUndo());
			TestEqual(FString::Printf(TEXT("%s reports its true semantics"), Case.Label),
				Transaction.GetRefusalCode(), FString(Case.Code));
			TestTrue(FString::Printf(TEXT("%s explains why"), Case.Label),
				Transaction.GetRefusalDetail().Len() > 0);
			Transaction.DescribeInto(Payload);
			Probe->ComponentTags.Add(ProbeTag);
		}

		const TSharedPtr<FJsonObject>* Undo = Block(Payload, TEXT("undo"));
		if (Undo)
		{
			TestFalse(FString::Printf(TEXT("%s payload claims no undo"), Case.Label),
				(*Undo)->GetBoolField(TEXT("undoable")));
			FString Scope;
			TestFalse(FString::Printf(TEXT("%s payload names no transaction"), Case.Label),
				(*Undo)->TryGetStringField(TEXT("transactionScope"), Scope));
		}
		else
		{
			AddError(TEXT("DescribeInto wrote no undo block on a refusal."));
		}

		GEditor->UndoTransaction();
		TestEqual(FString::Printf(TEXT("%s left no false undo entry behind"), Case.Label),
			Probe->ComponentTags.Num(), 1);
	}

	// An object that cannot reach the undo buffer must not be wrapped either; an
	// empty transaction is the same lie in a different shape.
	ClearUndoBuffer();
	USceneComponent* Untracked = MakeNonTransactionalProbe();
	FMcpScopedEditorTransaction Transaction(
		FText::FromString(TEXT("Mcp Untracked")),
		EMcpMutationDurability::EditorStateOnly,
		TArray<UObject*>{Untracked});
	TestFalse(TEXT("a non-transactional object is not wrapped"), Transaction.IsRecordingUndo());
	TestEqual(TEXT("the refusal names the cause"), Transaction.GetRefusalCode(),
		FString(TEXT("UNDO_UNAVAILABLE_NON_TRANSACTIONAL_OBJECT")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpScopedTransactionDurableEscapeTest,
	"McpAutomationBridge.Foundation.ScopedTransaction.DurableWriteEscapeIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpScopedTransactionDurableEscapeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	ClearUndoBuffer();
	USceneComponent* Probe = MakeTransactionalProbe();

	FMcpScopedEditorTransaction Transaction(
		FText::FromString(TEXT("Mcp Escape Probe")),
		EMcpMutationDurability::EditorStateOnly,
		TArray<UObject*>{Probe});
	TestTrue(TEXT("recording before any durable write"), Transaction.IsRecordingUndo());

	// A save inside a scope declared in-memory-only is the exact defect this
	// witness exists to surface: undo restores the object, not the bytes.
	McpDurableWrites::RecordDurableWrite(TEXT("/Game/Task43/EscapeProbe"));

	TestFalse(TEXT("a durable write withdraws the undo claim"), Transaction.IsRecordingUndo());
	TestTrue(TEXT("the state names the escape"),
		Transaction.GetState() == EMcpTransactionState::BrokenByDurableWrite);
	TestEqual(TEXT("the refusal code is stable"), Transaction.GetRefusalCode(),
		FString(TEXT("UNDO_BROKEN_BY_DURABLE_WRITE")));
	TestTrue(TEXT("the detail names the package that escaped"),
		Transaction.GetRefusalDetail().Contains(TEXT("/Game/Task43/EscapeProbe")));

	const TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Transaction.DescribeInto(Payload);
	const TSharedPtr<FJsonObject>* Undo = Block(Payload, TEXT("undo"));
	if (Undo)
	{
		TestFalse(TEXT("the payload withdraws the undo claim too"),
			(*Undo)->GetBoolField(TEXT("undoable")));
	}
	else
	{
		AddError(TEXT("DescribeInto wrote no undo block after an escape."));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpCompensationReceiptPartialTest,
	"McpAutomationBridge.Foundation.CompensationReceipt.PartialCompletionIsHonest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpCompensationReceiptPartialTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpCompensationReceipt Receipt(TEXT("control_editor.save_all"));
	Receipt.NoteCompleted(TEXT("save:/Game/A"), TEXT("package written to disk"));
	Receipt.NoteCompleted(TEXT("save:/Game/B"), TEXT("package written to disk"));
	Receipt.NoteNotCompleted(TEXT("save:/Game/C"), TEXT("file is read-only"));
	Receipt.NoteSkipped(TEXT("save:/Temp/D"), TEXT("transient package"));
	Receipt.AddCompensatingCapability(TEXT("control_editor.save_all"));
	Receipt.SetCallerAction(
		TEXT("Clear the read-only flag on /Game/C and call control_editor.save_all again; "
			 "/Game/A and /Game/B are already on disk and stay there."));

	TestTrue(TEXT("two landed and one did not, so this is partial"), Receipt.IsPartial());
	TestEqual(TEXT("the state says partial, not failed"), Receipt.GetState(), FString(TEXT("partial")));
	TestEqual(TEXT("completed count is exact"), Receipt.GetCompletedCount(), 2);
	TestEqual(TEXT("not-completed count is exact"), Receipt.GetNotCompletedCount(), 1);

	const TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Receipt.DescribeInto(Payload);
	const TSharedPtr<FJsonObject>* Compensation = Block(Payload, TEXT("compensation"));
	if (!Compensation)
	{
		AddError(TEXT("DescribeInto wrote no compensation block."));
		return false;
	}

	const TSharedPtr<FJsonObject>& Body = *Compensation;
	TestFalse(TEXT("the receipt never claims atomicity"), Body->GetBoolField(TEXT("atomic")));
	TestEqual(TEXT("rollback is reported unavailable, not attempted"),
		Body->GetStringField(TEXT("rollback")), FString(TEXT("unavailable")));
	TestTrue(TEXT("the reason rollback is unavailable is stated"),
		Body->GetStringField(TEXT("rollbackReason")).Len() > 0);
	TestEqual(TEXT("the block repeats the honest state"),
		Body->GetStringField(TEXT("state")), FString(TEXT("partial")));

	const TArray<TSharedPtr<FJsonValue>>* Done = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Missing = nullptr;
	if (Body->TryGetArrayField(TEXT("completed"), Done) &&
		Body->TryGetArrayField(TEXT("notCompleted"), Missing))
	{
		TestEqual(TEXT("every completed step is listed"), Done->Num(), 2);
		TestEqual(TEXT("every incomplete step is listed"), Missing->Num(), 1);
		TestEqual(TEXT("the first completed step is named"),
			(*Done)[0]->AsObject()->GetStringField(TEXT("step")), FString(TEXT("save:/Game/A")));
		TestEqual(TEXT("the incomplete step carries its reason"),
			(*Missing)[0]->AsObject()->GetStringField(TEXT("detail")), FString(TEXT("file is read-only")));
	}
	else
	{
		AddError(TEXT("The receipt did not list completed and not-completed steps."));
	}

	TestTrue(TEXT("the caller is told exactly what to do"),
		Body->GetStringField(TEXT("callerAction")).Contains(TEXT("/Game/C")));

	// No shape of this receipt may read as a rollback promise.
	const FString Serialized = ToJson(Payload);
	TestFalse(TEXT("no rollback promise survives serialization"),
		Serialized.Contains(TEXT("rolled back")) ||
		Serialized.Contains(TEXT("\"rollback\":true")) ||
		Serialized.Contains(TEXT("\"atomic\":true")) ||
		Serialized.Contains(TEXT("\"undoable\":true")));
	return true;
}

#endif
