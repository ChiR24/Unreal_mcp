// McpResourceHealthContent.h
// Task 47: the body of the EXISTING `ue://health` resource on the native
// transport. No new uri is introduced - this is what `McpResourceRead` serves
// when a client reads the health resource the native catalog already lists.
//
// Before this, the native counters accumulated in production and rendered in
// the shared exposition format, but nothing served that text: RenderPrometheus
// was reachable only from the test suite, so `ue://health` answered
// RESOURCE_UNAVAILABLE and a native client could not scrape anything.
//
// Everything here is socket-thread safe: it reads the immutable capability
// store, the mutex-guarded telemetry registry, and atomically published
// readiness flags. It never touches editor world state.
#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace McpResourceHealth
{
	/**
	 * Readiness + anonymous aggregate diagnostics + the rendered Prometheus
	 * exposition, mirroring what the TypeScript surface puts on `ue://health`.
	 * Bounded and redacted by construction: every value is a number, a bool, or
	 * a member of a closed schema set.
	 */
	TSharedRef<FJsonObject> BuildHealthData();
}
