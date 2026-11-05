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
	bool bBatchStep = true;
	/** Sent inside a request's brackets (a dispatch, or RunLatentReply): what led to it was this server's, as attributed. */
	bool bInRequest = false;
	/** The errors the engine logged in those brackets before it was sent (a step that answers later is judged by them). */
	TArray<FString> LoggedErrors;
};

class FMcpResponseCaptureRegistry
{
public:
	static FMcpResponseCaptureRegistry& Get();

	/** Start holding whatever response is sent for `RequestId`. bBatchStep: the caller finishes the step's work itself
	 *  (one transaction, compile or placement check for every step); false for a step that finishes its own, as a
	 *  direct call would (system_control run_script). */
	void Begin(const FString& RequestId, bool bBatchStep = true);

	/** True when `RequestId` is being captured; the reply is stored and must not be delivered. */
	bool TryCapture(const FString& RequestId, bool bSuccess, const FString& Message,
	                const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode, bool bInRequest = false,
	                const TArray<FString>& LoggedErrors = TArray<FString>());

	/** Stop capturing and hand back what arrived (bCaptured false when nothing did). */
	FMcpCapturedResponse End(const FString& RequestId);

	/** True once a reply for `RequestId` arrived; it is held until End. A step that answers later is awaited by this. */
	bool HasReply(const FString& RequestId) const;

	/** True between Begin and End of `RequestId`: the handler runs as a step of a bigger call. */
	bool IsCapturing(const FString& RequestId) const;

	/** Captured for a batch that finishes the step's work: a handler leaves its transaction, recompile or placement
	 *  check to it. A captured step that is no batch step does all of that itself. */
	bool IsBatchStep(const FString& RequestId) const;

private:
	mutable FCriticalSection Mutex;
	TMap<FString, FMcpCapturedResponse> Pending;
};
