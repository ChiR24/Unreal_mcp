#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;

namespace McpBlueprintGraphHandlers
{
struct FActionContext;
}

// manage_blueprint read_graph_as_code: a Blueprint's event and function graphs as the S-expression text of Epic's 5.8
// Blueprint DSL (read_graph_dsl / write_graph_dsl), read here in C++ on every 5.x. Nodes are named by their action-menu
// id (Category|Title, whitespace removed); every statement ends in a comment naming its node (n and the first hex digits
// of its GUID, the same on every read), and the reply lists each node's GUID and every link, so nothing in the graph goes
// unsaid. Where Epic's reader drops a path that runs
// into a node already printed, this one writes (goto nX). Reads only: nothing is created, compiled or changed.
namespace McpGraphCode
{
// What a node does to the flow, from its class (titles and categories are localised text and change between versions).
enum class EKind : uint8
{
    Call,
    Return,
    Knot,
    Self,
    Branch,
    Sequence,
    Switch,
    Select,
    ForLoop,
    ForLoopBreak,
    ForEach,
    ForEachBreak,
    While
};

// A pin on another node: its index among that node's visible pins of its direction.
struct FPinRef
{
    int32 Node = INDEX_NONE;
    int32 Pin = INDEX_NONE;
};

struct FPin
{
    FString Name;
    bool bExec = false;
    bool bBool = false;
    // A string, name or text: printed quoted.
    bool bText = false;
    // Still the value the node was made with, so a trailing argument can be left out.
    bool bAtDefault = false;
    FString Value;
    TArray<FPinRef> Links;
};

struct FNode
{
    // Category|Title as Epic's reader writes it; a '|Name' id is filled from the action menu by ExpandTypeIds.
    FString TypeId;
    EKind Kind = EKind::Call;
    // n and the first hex digits of Guid (ShortenIds).
    FString Id;
    FString Guid;
    int32 Y = 0;
    TArray<FPin> Ins;
    TArray<FPin> Outs;
};

struct FGraph
{
    FString Name;
    TArray<FNode> Nodes;
};

/** The graph's K2 nodes with their visible pins and links, read without changing anything. */
FGraph Snapshot(UEdGraph* Graph);

/** Each node's id: n and the fewest leading hex digits of its GUID (6 at least) that no other node read shares. */
void ShortenIds(TArray<FGraph>& Graphs);

/** Fills each '|Name' type id with the action-menu id it matches in its graph, as Epic's reader does (Reads[i] is Graphs[i]). */
void ExpandTypeIds(UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, TArray<FGraph>& Reads);

/** The graph as text; OutPlaced gets the index of every node the text names, OutWarnings what could not be said. */
FString Decompile(const FGraph& Graph, TSet<int32>& OutPlaced, TArray<FString>& OutWarnings);

/** False for any other sub-action. */
bool HandleReadGraphAsCode(const McpBlueprintGraphHandlers::FActionContext& Context);
} // namespace McpGraphCode
