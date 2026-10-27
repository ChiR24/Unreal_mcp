// McpNativeReceiptOutcome.h — reusable outcome metadata mined from a handler result
//
// Mirrors src/tools/catalog/capabilities/semantic/receipt-outcome.ts so both
// transports surface the same handles / changed entities / task state for the
// same handler result. Nothing is fabricated: a derived handle is emitted only
// when its identifier passes the same allowed-root / shape gate the TypeScript
// schemas apply, so a malformed identifier is dropped rather than coerced.

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
