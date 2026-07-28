// McpTaskMethods.cpp — Task 44: native tasks/* handlers + read-only checkpoint.

#include "MCP/Primitives/McpTaskMethods.h"

#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayDescribe.h"
#include "MCP/Protocol/McpJsonRpc.h"

const TCHAR* const McpTaskCheckpointRefusedCode = TEXT("TASK_CHECKPOINT_NOT_AVAILABLE");

const TArray<FString>& McpTaskCheckpointOperations()
{
	static const TArray<FString> Operations = { TEXT("search"), TEXT("describe") };
	return Operations;
}

bool McpTaskCheckpointOperationAllowed(const FString& Operation)
{
	return McpTaskCheckpointOperations().Contains(Operation);
}

namespace
{
FString McpTaskIsoStamp(int64 Milliseconds)
{
	return FDateTime(Milliseconds * ETimespan::TicksPerMillisecond).ToIso8601();
}

TSharedPtr<FJsonObject> McpTaskRefusalData(const FString& Operation)
{
	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("code"), McpTaskCheckpointRefusedCode);
	TArray<TSharedPtr<FJsonValue>> Allowed;
	for (const FString& Op : McpTaskCheckpointOperations())
	{
		Allowed.Add(MakeShared<FJsonValueString>(Op));
	}
	Data->SetArrayField(TEXT("taskableOperations"), Allowed);
	auto Args = MakeShared<FJsonObject>();
	Args->SetStringField(TEXT("operation"), Operation);
	auto Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("name"), TEXT("unreal"));
	Params->SetObjectField(TEXT("arguments"), Args);
	auto NextCall = MakeShared<FJsonObject>();
	NextCall->SetStringField(TEXT("method"), TEXT("tools/call"));
	NextCall->SetObjectField(TEXT("params"), Params);
	Data->SetObjectField(TEXT("nextCall"), NextCall);
	return Data;
}

// The discovery result, wrapped exactly the way the synchronous gateway wraps
// it, so a retained checkpoint and a plain tools/call cannot disagree.
TSharedPtr<FJsonObject> McpRunReadOnlyOperation(
	const FString& Operation, const TSharedPtr<FJsonObject>& Arguments,
	FMcpToolEnabledPredicate IsToolEnabled, bool& bOutSuccess)
{
	const FMcpCapabilityStore& CapabilityStore = FMcpCapabilityStore::Get();
	FMcpDiscoveryQuery Query;
	Arguments->TryGetStringField(TEXT("query"), Query.Query);
	int32 Value = 0;
	if (Operation == TEXT("search"))
	{
		Query.bHasDomain = Arguments->TryGetStringField(TEXT("domain"), Query.Domain);
		Query.bHasFamily = Arguments->TryGetStringField(TEXT("family"), Query.Family);
		Query.Limit = McpSearchDefaultLimit;
		if (Arguments->TryGetNumberField(TEXT("limit"), Value))
		{
			Query.Limit = FMath::Clamp(Value, 1, McpSearchMaxLimit);
		}
		if (Arguments->TryGetNumberField(TEXT("offset"), Value)) Query.Offset = FMath::Max(0, Value);
	}
	else
	{
		Arguments->TryGetStringField(TEXT("tool"), Query.Tool);
		Query.bHasAction = Arguments->TryGetStringField(TEXT("action"), Query.Action);
		Query.bHasParam = Arguments->TryGetStringField(TEXT("param"), Query.Param);
		Query.Limit = McpDescribeDefaultLimit;
		if (Arguments->TryGetNumberField(TEXT("limit"), Value))
		{
			Query.Limit = FMath::Clamp(Value, 1, McpDescribeMaxLimit);
		}
		if (Arguments->TryGetNumberField(TEXT("offset"), Value)) Query.Offset = FMath::Max(0, Value);
	}

	const TSharedPtr<FJsonObject> Result = Operation == TEXT("search")
		? McpGatewaySearchCapabilities(Query, CapabilityStore, IsToolEnabled)
		: McpGatewayDescribeCapability(Query, CapabilityStore, IsToolEnabled);

	bOutSuccess = false;
	if (Result.IsValid()) Result->TryGetBoolField(TEXT("success"), bOutSuccess);
	const FString Message = bOutSuccess ? TEXT("ok")
		: (Result.IsValid() ? Result->GetStringField(TEXT("message")) : TEXT("discovery failed"));
	const FString Code = bOutSuccess ? FString()
		: (Result.IsValid() ? Result->GetStringField(TEXT("errorCode")) : FString());
	return FMcpJsonRpc::BuildToolResult(bOutSuccess, Message, Result, Code);
}
}  // namespace

TSharedPtr<FJsonObject> McpTaskRecordToJson(const FMcpTaskRecord& Record)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("taskId"), Record.TaskId);
	Json->SetStringField(TEXT("status"), McpTaskStatusToString(Record.Status));
	Json->SetNumberField(TEXT("ttl"), static_cast<double>(Record.TtlMs));
	Json->SetStringField(TEXT("createdAt"), McpTaskIsoStamp(Record.CreatedAtMs));
	Json->SetStringField(TEXT("lastUpdatedAt"), McpTaskIsoStamp(Record.LastUpdatedAtMs));
	if (!Record.StatusMessage.IsEmpty())
	{
		Json->SetStringField(TEXT("statusMessage"), Record.StatusMessage);
	}
	return Json;
}

void FMcpTaskSurface::CloseSession(const FString& SessionId)
{
	Store.CloseSession(SessionId);
}

bool FMcpTaskSurface::HandleMethod(
	const FString& Method, const TSharedPtr<FJsonObject>& Params,
	const TSharedPtr<FJsonValue>& Id, const FString& SessionId, FString& OutBody)
{
	if (!Method.StartsWith(TEXT("tasks/"))) return false;

	if (Method == TEXT("tasks/list"))
	{
		TArray<FMcpTaskRecord> Records;
		Store.ListTasks(SessionId, Records);
		TArray<TSharedPtr<FJsonValue>> Tasks;
		for (const FMcpTaskRecord& Record : Records)
		{
			Tasks.Add(MakeShared<FJsonValueObject>(McpTaskRecordToJson(Record)));
		}
		auto Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("tasks"), Tasks);
		OutBody = FMcpJsonRpc::BuildResponse(Id, Result);
		return true;
	}

	FString TaskId;
	if (!Params.IsValid() || !Params->TryGetStringField(TEXT("taskId"), TaskId))
	{
		OutBody = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("taskId is required"));
		return true;
	}

	// Every lookup below is session-scoped, so another session's task is
	// reported exactly like one that never existed.
	if (Method == TEXT("tasks/get"))
	{
		FMcpTaskRecord Record;
		if (Store.GetTask(SessionId, TaskId, Record) != EMcpTaskStoreError::None)
		{
			OutBody = FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
				FString::Printf(TEXT("Task not found: %s"), *TaskId));
			return true;
		}
		OutBody = FMcpJsonRpc::BuildResponse(Id, McpTaskRecordToJson(Record));
		return true;
	}

	if (Method == TEXT("tasks/cancel"))
	{
		FMcpTaskRecord Record;
		if (Store.GetTask(SessionId, TaskId, Record) != EMcpTaskStoreError::None)
		{
			OutBody = FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
				FString::Printf(TEXT("Task not found: %s"), *TaskId));
			return true;
		}
		const EMcpTaskStoreError Cancelled = Store.UpdateStatus(
			SessionId, TaskId, EMcpTaskStatus::Cancelled,
			TEXT("Client cancelled task execution."));
		if (Cancelled == EMcpTaskStoreError::AlreadyTerminal)
		{
			OutBody = FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
				FString::Printf(TEXT("Cannot cancel task in terminal status: %s"),
					McpTaskStatusToString(Record.Status)));
			return true;
		}
		Store.GetTask(SessionId, TaskId, Record);
		OutBody = FMcpJsonRpc::BuildResponse(Id, McpTaskRecordToJson(Record));
		return true;
	}

	if (Method == TEXT("tasks/result"))
	{
		FMcpTaskRecord Record;
		if (Store.GetTask(SessionId, TaskId, Record) != EMcpTaskStoreError::None)
		{
			OutBody = FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
				FString::Printf(TEXT("Task not found: %s"), *TaskId));
			return true;
		}
		TSharedPtr<FJsonObject> Result;
		if (Store.GetResult(SessionId, TaskId, Result) != EMcpTaskStoreError::None)
		{
			OutBody = FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
				FString::Printf(TEXT("Task has no retained result: %s"),
					McpTaskStatusToString(Record.Status)));
			return true;
		}
		auto Related = MakeShared<FJsonObject>();
		Related->SetStringField(TEXT("taskId"), TaskId);
		auto Meta = MakeShared<FJsonObject>();
		Meta->SetObjectField(TEXT("io.modelcontextprotocol/related-task"), Related);
		Result->SetObjectField(TEXT("_meta"), Meta);
		OutBody = FMcpJsonRpc::BuildResponse(Id, Result);
		return true;
	}

	OutBody = FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorMethodNotFound,
		FString::Printf(TEXT("Unknown method: %s"), *Method));
	return true;
}

void FMcpTaskSurface::HandleToolCallCheckpoint(
	const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
	const TSharedPtr<FJsonObject>& TaskCreation, const TSharedPtr<FJsonValue>& Id,
	const FString& SessionId, FMcpToolEnabledPredicate IsToolEnabled, FString& OutBody)
{
	FString Operation;
	if (Arguments.IsValid()) Arguments->TryGetStringField(TEXT("operation"), Operation);

	if (ToolName != TEXT("unreal"))
	{
		OutBody = FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
			FString::Printf(
				TEXT("Task creation is not available for '%s'. The only public tool is the 'unreal' gateway."),
				*ToolName),
			McpTaskRefusalData(Operation));
		return;
	}
	// Refused BEFORE anything runs: a call that is about to be refused must not
	// have already mutated the editor.
	if (!McpTaskCheckpointOperationAllowed(Operation))
	{
		OutBody = FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
			FString::Printf(
				TEXT("MCP Tasks on this server are safe read-only checkpoints, so only search and describe ")
				TEXT("may be task-augmented; '%s' may not. Cancelling a task cannot interrupt work already ")
				TEXT("dispatched to the editor, so a mutating operation is never handed back as a cancellable ")
				TEXT("handle. Re-send this call without params.task to run it synchronously."),
				Operation.IsEmpty() ? TEXT("unknown") : *Operation),
			McpTaskRefusalData(Operation));
		return;
	}

	int64 RequestedTtl = 0;
	bool bHasTtl = false;
	if (TaskCreation.IsValid())
	{
		double Ttl = 0.0;
		bHasTtl = TaskCreation->TryGetNumberField(TEXT("ttl"), Ttl);
		RequestedTtl = static_cast<int64>(Ttl);
	}

	FMcpTaskRecord Record;
	if (Store.CreateTask(SessionId, bHasTtl, RequestedTtl, Record) != EMcpTaskStoreError::None)
	{
		OutBody = FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidRequest,
			TEXT("Task store is at its per-session capacity and every retained task is still running."));
		return;
	}

	bool bSuccess = false;
	const TSharedPtr<FJsonObject> ToolResult =
		McpRunReadOnlyOperation(Operation, Arguments, IsToolEnabled, bSuccess);
	Store.StoreResult(SessionId, Record.TaskId,
		bSuccess ? EMcpTaskStatus::Completed : EMcpTaskStatus::Failed, ToolResult);

	Store.GetTask(SessionId, Record.TaskId, Record);
	auto Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("task"), McpTaskRecordToJson(Record));
	OutBody = FMcpJsonRpc::BuildResponse(Id, Result);
}
