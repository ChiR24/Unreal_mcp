#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FFrame;
struct FBlueprintExceptionInfo;

// Why an Accessed None fired, read from the variable the engine names and the Blueprint that declares it, at most
// three steps: the variable, the Set node that writes it, the node that fed that Set. The engine's text only says
// "Accessed None trying to read property X", so an agent guessed which of the usual mistakes it was.
namespace McpScriptErrorCause
{
// kind: destroyed (X still points at an actor that is gone), instance_editable_unset (X is set per placed actor and
// this one left it None), never_set (nothing assigns X), cast_failed or function_returned_none (X was assigned from a
// cast that failed or a call that returned None), set_but_none (assigned, but None when read), or other (a local
// variable, a parameter, another object's property). With property, detail, nodeId of the Set or source node, and
// nextCall where one call fixes it. Null for any other error, or when the engine named no variable.
TSharedPtr<FJsonObject> Diagnose(const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info);
} // namespace McpScriptErrorCause
