// McpResponseCaptureRegistry.cpp — see header.

#include "Core/Requests/McpResponseCaptureRegistry.h"

FMcpResponseCaptureRegistry& FMcpResponseCaptureRegistry::Get()
{
	static FMcpResponseCaptureRegistry Instance;
	return Instance;
}

void FMcpResponseCaptureRegistry::Begin(const FString& RequestId, bool bBatchStep)
{
	FScopeLock Lock(&Mutex);
	FMcpCapturedResponse Slot;
	Slot.bBatchStep = bBatchStep;
	Pending.Add(RequestId, Slot);
}

bool FMcpResponseCaptureRegistry::TryCapture(const FString& RequestId, bool bSuccess,
                                             const FString& Message,
                                             const TSharedPtr<FJsonObject>& Result,
                                             const FString& ErrorCode)
{
	FScopeLock Lock(&Mutex);
	FMcpCapturedResponse* Slot = Pending.Find(RequestId);
	if (Slot == nullptr)
	{
		return false;
	}
	Slot->bCaptured = true;
	Slot->bSuccess = bSuccess;
	Slot->Message = Message;
	Slot->ErrorCode = ErrorCode;
	Slot->Result = Result;
	return true;
}

FMcpCapturedResponse FMcpResponseCaptureRegistry::End(const FString& RequestId)
{
	FScopeLock Lock(&Mutex);
	FMcpCapturedResponse Captured;
	Pending.RemoveAndCopyValue(RequestId, Captured);
	return Captured;
}

bool FMcpResponseCaptureRegistry::IsCapturing(const FString& RequestId) const
{
	FScopeLock Lock(&Mutex);
	return Pending.Contains(RequestId);
}

bool FMcpResponseCaptureRegistry::IsBatchStep(const FString& RequestId) const
{
	FScopeLock Lock(&Mutex);
	const FMcpCapturedResponse* Slot = Pending.Find(RequestId);
	return Slot && Slot->bBatchStep;
}
