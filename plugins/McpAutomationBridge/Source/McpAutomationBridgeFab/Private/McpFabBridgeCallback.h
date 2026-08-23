// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "McpFabBridgeCallback.generated.h"

/**
 * The object Fab's page calls back into.
 *
 * SWebBrowser::BindUObject publishes a UObject's UFUNCTIONs to the page under
 * window.ue.<name>, lowercased. Binding one into the page Fab already
 * authenticated is what lets the privileged work happen on Fab's side of the
 * boundary: the page holds the session, runs the call, and returns only the
 * result. No token, cookie or signed URL is read out of the browser into C++,
 * so none of it can reach a log, a receipt, or JSON-RPC.
 *
 * The surface is deliberately narrow. Page script is untrusted input -- it is
 * Fab's code, not ours, and a compromised or simply changed page reaches these
 * functions directly -- so every entry point is one-shot, correlated by request
 * id, and size-capped before anything is stored or logged.
 */
UCLASS()
class UMcpFabBridgeCallback : public UObject
{
	GENERATED_BODY()

public:
	/** Rejects anything larger; a Fab reply is kilobytes, not megabytes. */
	static constexpr int32 MaxPayloadChars = 256 * 1024;

	/** Result of the operation identified by RequestId. */
	UFUNCTION()
	void OnResult(const FString& RequestId, const FString& Payload);

	/** Failure of the operation identified by RequestId. */
	UFUNCTION()
	void OnError(const FString& RequestId, const FString& Message);

	/** Arms a request id; a reply for any other id is discarded. */
	void Expect(const FString& RequestId, TFunction<void(bool, const FString&)> InCompletion);

	/** True once the armed request has been answered. */
	bool IsSettled() const { return PendingId.IsEmpty(); }

	/**
	 * Fails the armed request so a reply that never came cannot hold the slot.
	 * The waiting caller is told, rather than left hanging on a page that has
	 * stopped answering.
	 */
	void Abandon(const FString& Reason);

	/** True when the armed request is this one; a timer must not settle a later call. */
	bool IsAwaiting(const FString& RequestId) const { return !PendingId.IsEmpty() && PendingId == RequestId; }

private:
	void Settle(const FString& RequestId, bool bSuccess, const FString& Payload);

	FString PendingId;
	TFunction<void(bool, const FString&)> Completion;
};
