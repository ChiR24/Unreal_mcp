// McpReadinessState.h
// Task 47: the readiness facts a SOCKET-THREAD reader is allowed to consult.
//
// `ue://health` is answered on the transport thread, so deciding "is the editor
// up" must never reach into editor state from there. Instead the layer that
// OWNS each fact publishes a positive observation here and clears it on
// teardown; the health reader only reads these flags.
//
// Fail-closed by construction: every flag starts FALSE. A surface that has not
// yet published reads as NOT ready rather than ready-by-default, mirroring the
// TypeScript editor probe (`src/services/readiness.ts`) that refuses to infer
// health from "the socket is open". Nothing branches on these flags to admit,
// refuse, route or retry a request - they are reported, never enforced.
//
// Foundation-only: this header depends on nothing from MCP/, Domains/ or
// Transport/, so both transports can publish into it without a layering cycle.
#pragma once

#include "CoreMinimal.h"
#include <atomic>

class FMcpReadinessState
{
public:
	/** Process-wide state shared by the WebSocket bridge and native /mcp. */
	static FMcpReadinessState& Get()
	{
		static FMcpReadinessState State;
		return State;
	}

	/** Published by the native transport lifecycle: bound and accepting. */
	void SetTransportReady(bool bInReady) { bTransportReady.store(bInReady, std::memory_order_relaxed); }

	/** Published by the bridge subsystem lifecycle: editor subsystem is live. */
	void SetEditorReady(bool bInReady) { bEditorReady.store(bInReady, std::memory_order_relaxed); }

	bool IsTransportReady() const { return bTransportReady.load(std::memory_order_relaxed); }
	bool IsEditorReady() const { return bEditorReady.load(std::memory_order_relaxed); }

	/** Test seam: return to the fail-closed initial state. */
	void Reset()
	{
		bTransportReady.store(false, std::memory_order_relaxed);
		bEditorReady.store(false, std::memory_order_relaxed);
	}

private:
	std::atomic<bool> bTransportReady{ false };
	std::atomic<bool> bEditorReady{ false };
};
