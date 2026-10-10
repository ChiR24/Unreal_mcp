#include "Foundation/McpCompensationReceipt.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> StepsToJson(const TArray<FMcpCompensationStep>& Steps)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FMcpCompensationStep& Step : Steps)
	{
		const TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("step"), Step.Step);
		Entry->SetStringField(TEXT("detail"), Step.Detail);
		Values.Add(MakeShared<FJsonValueObject>(Entry));
	}
	return Values;
}
} // namespace

FMcpCompensationReceipt::FMcpCompensationReceipt(const FString& InOperation)
	: Operation(InOperation)
{
}

void FMcpCompensationReceipt::NoteCompleted(const FString& Step, const FString& Detail)
{
	Completed.Add(FMcpCompensationStep{Step, Detail});
}

void FMcpCompensationReceipt::NoteNotCompleted(const FString& Step, const FString& Reason)
{
	NotCompleted.Add(FMcpCompensationStep{Step, Reason});
}

void FMcpCompensationReceipt::NoteSkipped(const FString& Step, const FString& Reason)
{
	Skipped.Add(FMcpCompensationStep{Step, Reason});
}

void FMcpCompensationReceipt::AddCompensatingCapability(const FString& CapabilityId)
{
	CompensatingCapabilities.AddUnique(CapabilityId);
}

void FMcpCompensationReceipt::SetCallerAction(const FString& Guidance)
{
	CallerAction = Guidance;
}

bool FMcpCompensationReceipt::IsPartial() const
{
	return Completed.Num() > 0 && NotCompleted.Num() > 0;
}

FString FMcpCompensationReceipt::GetState() const
{
	if (Completed.Num() == 0 && NotCompleted.Num() == 0)
	{
		return TEXT("noop");
	}
	if (NotCompleted.Num() == 0)
	{
		return TEXT("completed");
	}
	return Completed.Num() == 0 ? TEXT("failed") : TEXT("partial");
}

int32 FMcpCompensationReceipt::GetCompletedCount() const
{
	return Completed.Num();
}

int32 FMcpCompensationReceipt::GetNotCompletedCount() const
{
	return NotCompleted.Num();
}

void FMcpCompensationReceipt::DescribeInto(const TSharedPtr<FJsonObject>& Result) const
{
	if (!Result.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject> Block = MakeShared<FJsonObject>();
	Block->SetStringField(TEXT("operation"), Operation);

	// Unconditional on every path, including the all-succeeded one: "everything
	// worked" still does not mean "this could be undone".
	Block->SetBoolField(TEXT("atomic"), false);
	Block->SetStringField(TEXT("rollback"), TEXT("unavailable"));
	Block->SetStringField(TEXT("rollbackReason"),
		TEXT("Completed steps are already durable on disk. No editor transaction can reach a "
			 "finished save, build or render, so nothing here was or can be undone."));
	Block->SetStringField(TEXT("state"), GetState());

	Block->SetArrayField(TEXT("completed"), StepsToJson(Completed));
	Block->SetArrayField(TEXT("notCompleted"), StepsToJson(NotCompleted));
	Block->SetArrayField(TEXT("skipped"), StepsToJson(Skipped));

	TArray<TSharedPtr<FJsonValue>> Capabilities;
	for (const FString& CapabilityId : CompensatingCapabilities)
	{
		Capabilities.Add(MakeShared<FJsonValueString>(CapabilityId));
	}
	Block->SetArrayField(TEXT("compensatingCapabilities"), Capabilities);
	Block->SetStringField(TEXT("callerAction"), CallerAction);
	Result->SetObjectField(TEXT("compensation"), Block);
}
