#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UBlueprintNodeSpawner;
class UEdGraph;
class UEdGraphNode;
class UK2Node;

namespace McpBlueprintGraphHandlers
{
struct FActionContext;
}

// manage_blueprint read_graph_as_code: a Blueprint's event and function graphs as the S-expression text of Epic's 5.8
// Blueprint DSL (read_graph_dsl / write_graph_dsl), read here in C++ on every 5.x. Nodes are named by their action-menu
// id (Category|Title, whitespace removed); every statement ends in a comment naming its node (n and the first hex digits
// of its GUID, the same on every read), and the reply lists each node's GUID and every link, so nothing in the graph goes
// unsaid. Where Epic's reader drops a path that runs into a node already printed, this one writes (goto nX). The read
// changes nothing; write_graph_from_code (McpGraphCodeWrite.h) turns the same text back into nodes.
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
    // Category|Title as Epic's reader writes it; a call's id is made the one its menu shows by ExpandTypeIds.
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

/** Gives each call node the id its graph's menu shows: its own when the menu has it, else the menu id ending in its
 *  title, as Epic's reader fills '|Name' ids (Reads[i] is Graphs[i]). */
void ExpandTypeIds(UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, TArray<FGraph>& Reads);

/** The graph as text; OutPlaced gets the index of every node the text names, OutWarnings what could not be said. */
FString Decompile(const FGraph& Graph, TSet<int32>& OutPlaced, TArray<FString>& OutWarnings);

/** Epic's node type id: menu category and list title, whitespace removed (a custom event: AddEvent|Custom|Name). */
FString TypeIdOf(const UK2Node* Node);

/** A node the flow starts from: an exec output and no exec input (an event, a function entry). */
bool IsEntry(const FNode& Node);

/** The name an (event ...) or (fn ...) head gives an entry of this type id: an event's id past AddEvent|, else its title. */
FString EntryName(const FString& TypeId);

// ---- The action menu (McpGraphCodeCatalog.cpp) ----
// The Blueprint action menu as Epic's list_available_nodes and create_node_from_name see it: each entry a spawner with
// the id its menu shows, Category|Title with spaces removed. Walking the action database can crash the editor: a
// spawner's UI spec is asked in a graph's context only after the filter passed it (a node that fits no graph of the
// Blueprint asserts while making its template), and never with an empty context (a variable's spawner reads the
// context's first Blueprint); a title without a graph comes from PrimeDefaultUiSpec, as the database primes.
struct FMenuEntry
{
    UObject* Owner = nullptr;
    const UBlueprintNodeSpawner* Spawner = nullptr;
    FString Id;
};

/** Spawners whose default title (spaces removed, any case) is one of Titles (lower case), or that show none without a graph. */
TArray<FMenuEntry> MenuCandidates(const TSet<FString>& Titles);

/** Of Candidates, those this graph's menu offers with a title in Titles, each with the id the menu shows there. */
TArray<FMenuEntry> MenuEntries(UBlueprint* Blueprint, UEdGraph* Graph, const TArray<FMenuEntry>& Candidates, const TSet<FString>& Titles);

/** The title part of a menu id (after its last |), lower case, as Titles hold it. */
FString MenuTitleKey(const FString& MenuId);

/** The node MenuId names in this graph's menu, made at At by its spawner (McpGraphCodeCatalog.cpp); null with OutError naming
 *  the ids the menu offers for that title. */
UEdGraphNode* CreateByMenuId(UBlueprint* Blueprint, UEdGraph* Graph, const FString& MenuId, const FVector2D& At, FString& OutError);

/** False for any other sub-action. */
bool HandleReadGraphAsCode(const McpBlueprintGraphHandlers::FActionContext& Context);

/** manage_blueprint write_graph_from_code (McpGraphCodeWriteHandler.cpp); false for any other sub-action. */
bool HandleWriteGraphFromCode(const McpBlueprintGraphHandlers::FActionContext& Context);

/** create_node with menuId: the node by its action-menu id (McpGraphCodeCatalog.cpp); false when the payload has none. */
bool TryCreateMenuNode(McpBlueprintGraphHandlers::FActionContext& Context, float X, float Y);
} // namespace McpGraphCode
