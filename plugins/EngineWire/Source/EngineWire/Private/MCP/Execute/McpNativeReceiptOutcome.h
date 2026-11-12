// McpNativeReceiptOutcome.h — reusable outcome metadata mined from a handler result
//
// Handles, changed entities and task state for a handler result, the same on
// both transports. Nothing is fabricated: a derived handle is emitted only
// when its identifier passes the allowed-root / shape gate, so a malformed
// identifier is dropped rather than coerced.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/** Changed-entity paths the handler reported (deduped, unbounded here; the
 *  receipt builder bounds + redacts them). */
TArray<FString> McpExtractReceiptChanges(const TSharedPtr<FJsonObject>& RawResult);

/** Typed handles (each a {kind, ref|path} object) reusable as inputs. */
TArray<TSharedPtr<FJsonValue>> McpExtractReceiptHandles(const TSharedPtr<FJsonObject>& RawResult);

/** A schema-valid task state ({taskId, state, progress?}) or null. */
TSharedPtr<FJsonObject> McpExtractReceiptTask(const TSharedPtr<FJsonObject>& RawResult);
