// Shared UFunction invocation marshalling.
//
// Two capabilities invoke a reflected UFunction from a JSON payload —
// control_editor.invoke_reflected_function and control_actor.call_function.
// They had independent parameter handling and only one of them was correct:
// call_function passed a zeroed buffer and silently dropped every argument the
// caller sent, so a bool argument always arrived false and the call still
// reported success. Keeping the marshalling here means both surfaces bind
// arguments, run property constructors/destructors, and read back out and
// return parameters the same way.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR

/**
 * Owns the parameter block for one reflected call.
 *
 * UFunction parameters are a packed struct whose properties must each be
 * constructed before use and destroyed afterwards; skipping either leaks or
 * corrupts for any non-POD parameter (FString, TArray, a USTRUCT). RAII keeps
 * that correct across the early returns marshalling needs.
 */
class FMcpScopedParamBlock {
public:
  explicit FMcpScopedParamBlock(UFunction *InFunction);
  ~FMcpScopedParamBlock();

  FMcpScopedParamBlock(const FMcpScopedParamBlock &) = delete;
  FMcpScopedParamBlock &operator=(const FMcpScopedParamBlock &) = delete;

  uint8 *Data() const { return Memory; }

private:
  UFunction *Function;
  uint8 *Memory;
};

/**
 * Bind Args onto the function's own parameter chain.
 *
 * Every input parameter is looked up by name; ones the caller omitted are
 * reported through OutUnset so a caller can tell "defaulted to zero" apart from
 * "I passed it". Returns false with OutError set when a supplied argument does
 * not convert to its declared property type — a wrong type is refused rather
 * than quietly becoming the zero value.
 */
bool McpBindJsonArgsToParams(UFunction *Function,
                             const TSharedPtr<FJsonObject> &Args,
                             uint8 *ParamBlock,
                             TArray<TSharedPtr<FJsonValue>> &OutUnset,
                             FString &OutError);

/** Read the function's out and return parameters back into a JSON object. */
TSharedPtr<FJsonObject> McpReadParamOutputs(UFunction *Function,
                                            const uint8 *ParamBlock);

#endif
