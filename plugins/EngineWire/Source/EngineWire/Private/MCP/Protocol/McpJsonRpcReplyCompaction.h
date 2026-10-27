#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * The gateway reply a client reads. Execute, describe and search replies carried fields only logs and
 * replay bookkeeping use (revision hashes, correlation ids, timings, live-state counters) and fields
 * that repeat another word for word: one "Actor not found" arrived six times, every success twice
 * over in `receipt`. Both are left out; what can inform a next call stays. Mirrors
 * src/utils/responses/gateway-reply-compaction.ts.
 */
namespace McpJsonRpcReply
{
/** A compacted copy of an execute, describe or search reply; any other object comes back as is. Reply is never changed. */
TSharedPtr<FJsonObject> MakeCompactReply(const TSharedPtr<FJsonObject>& Reply);
}
