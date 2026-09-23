// McpResponseCaptureRegistry.h — park a handler's reply instead of sending it
//
// A batch action (blueprint.build_graph) runs other handlers in-process under a
// synthetic request id and needs their answers as data. Every handler ends in
// SendAutomationResponse, so that one funnel checks here first: a registered id
// is stored and never delivered to a transport. Handlers need no change.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FMcpCapturedResponse
{
	bool bCaptured = false;
	bool bSuccess = false;
	FString Message;
	FString ErrorCode;
	TSharedPtr<FJsonObject> Result;
};

class FMcpResponseCaptureRegistry
{
public:
	static FMcpResponseCaptureRegistry& Get();

	/** Start holding whatever response is sent for `RequestId`. */
	void Begin(const FString& RequestId);

	/** True when `RequestId` is being captured; the reply is stored and must not be delivered. */
	bool TryCapture(const FString& RequestId, bool bSuccess, const FString& Message,
	                const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode);

	/** Stop capturing and hand back what arrived (bCaptured false when nothing did). */
	FMcpCapturedResponse End(const FString& RequestId);

private:
	FCriticalSection Mutex;
	TMap<FString, FMcpCapturedResponse> Pending;
};
