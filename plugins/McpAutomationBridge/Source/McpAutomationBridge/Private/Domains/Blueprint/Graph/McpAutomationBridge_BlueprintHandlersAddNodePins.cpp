#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2

// A newly created node is left exactly as the caller asked for: unwired.
//
// Two auto-wiring conveniences used to live here and both produced graphs that
// compiled clean and did the wrong thing.
//
// The VALUE pin of a VariableSet was wired from a Get of the SAME variable,
// spawning one when the graph had none, so every Set came out as
// `Set X = Get X` -- a self-assignment that looks plausible in a node dump and
// silently never changes the variable. Four state flags in one project were
// authored that way before the symptoms were traced back here.
//
// The EXEC pin was wired to the graph's "preferred event" -- whatever event
// happened to be found first: BeginPlay, Tick, PreConstruct. Adding a Set to a
// cast chain therefore ALSO hung it off Event Tick, where it ran every frame
// against a target the cast had not produced yet and logged a Blueprint
// runtime error every frame. Worse, an exec output takes one link, so wiring
// to an event that already had a chain would have severed it.
//
// What the editor does when you drag a variable in is create the node and stop.
// The caller's set_pin_default_value and connect_pins decide the rest.
void LinkBlueprintGraphNodePins(UEdGraph *TargetGraph, UEdGraphNode *NewNode,
                                bool &bExecLinked, bool &bValueLinked) {
  (void)TargetGraph;
  (void)NewNode;
  bExecLinked = false;
  bValueLinked = false;
}
#endif
}
