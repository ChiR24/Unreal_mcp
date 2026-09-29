#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"

#include <initializer_list>

class FMcpBridgeWebSocket;
class FMulticastDelegateProperty;
class FObjectProperty;
class UEdGraph;
class UFunction;
class UMcpAutomationBridgeSubsystem;
class USCS_Node;

// Writes gameplay logic INTO a Blueprint asset from a recipe: variables, event
// dispatchers, custom events, functions, hooks on shared events, an Enhanced
// Input binding and event-graph steps. Every edit is an ordinary
// blueprint.build_graph step run in process, so the result is plain engine
// nodes that run in a packaged game without this plugin. All or nothing: a
// failed step, a Blueprint that stops compiling or a failed key mapping puts
// the Blueprint back as it was. The domain-facing guide is AGENTS.md in this
// folder; read it before writing a recipe.
namespace McpBlueprintBehaviour
{
/** Steps one Author call may run. The public build_graph keeps its own 200. */
constexpr int32 MaxAuthorSteps = 1000;
/** Comment on shared nodes no behaviour owns: event hubs, events a hook created,
 *  shared custom events. A replace never removes them. */
constexpr const TCHAR* HubTag = TEXT("MCP event hub");
/** Comment on the AddMappingContext registration nodes; never removed either. */
constexpr const TCHAR* InputContextTag = TEXT("MCP input context");

struct FPinState
{
    FGuid NodeGuid;
    FGuid PinId;
    TArray<TPair<FGuid, FGuid>> Links; // (node guid, pin id) of every LinkedTo entry
    FString DefaultValue;
    TWeakObjectPtr<UObject> DefaultObject;
    FText DefaultTextValue;
};

struct FNodeState
{
    FGuid NodeGuid;
    FString Comment;
    ENodeEnabledState EnabledState = ENodeEnabledState::Enabled;
    bool bUserSetEnabledState = false;
    bool bCommentBubbleVisible = false;
};

/** What Restore puts back. Pointers are only compared, never kept past the request. */
struct FSnapshot
{
    TSet<FGuid> NodeGuids;
    TArray<FNodeState> Nodes;
    TArray<FPinState> Pins;
    TSet<UEdGraph*> Graphs;                  // function, dispatcher and event graphs
    TArray<FBPVariableDescription> Variables; // NewVariables, defaults and flags included
    TSet<USCS_Node*> ScsNodes;
    bool bCompiled = false;                   // compile status before the call
    bool bPackageDirty = false;
};

/** Records the Blueprint for Restore. Compiles it first when its status is Dirty or
 *  Unknown, to learn whether it compiled before the call. A domain that adds SCS
 *  components takes this BEFORE its edits and passes it to Author, so one rollback
 *  also removes those components. */
FSnapshot TakeSnapshot(UBlueprint* Blueprint);

struct FAuthorResult
{
    /** The behaviour is in the Blueprint and the Blueprint was saved. */
    bool bApplied = false;
    /** One sentence for the reply; on failure it names the recipe step that failed. */
    FString Message;
    /** Set when !bApplied: the failing step's own code or one of the Author codes (AGENTS.md). */
    FString ErrorCode;
    /** Always set; send it as the reply's result. */
    TSharedPtr<FJsonObject> Report;
};

/** Parses recipe JSON text. Every string VALUE that is exactly "{{Name}}" becomes
 *  Values[Name] with its own JSON type (a number stays a number); nothing is spliced
 *  into text. Null with OutError on bad JSON, a placeholder with no value, or a
 *  string that holds "{{" without being exactly one placeholder. */
TSharedPtr<FJsonObject> ParseRecipe(const FString& RecipeJson, const TMap<FString, TSharedPtr<FJsonValue>>& Values,
                                    FString& OutError);

/** ParseRecipe on this plugin's Resources/Recipes/<Domain>/<Name>.json (file text cached
 *  for the editor session). Domain and Name are letters, digits and underscores. */
TSharedPtr<FJsonObject> LoadRecipe(const FString& Domain, const FString& Name,
                                   const TMap<FString, TSharedPtr<FJsonValue>>& Values, FString& OutError);

/** Builds Recipe into the Blueprint at BlueprintPath, all or nothing. Refused during PIE.
 *  Game thread only; the caller sends the reply:
 *  SendAutomationResponse(Socket, RequestId, R.bApplied, R.Message, R.Report, R.ErrorCode).
 *  Before: a TakeSnapshot the caller made before its own edits; any failure undoes them too. */
FAuthorResult Author(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                     const TSharedPtr<FMcpBridgeWebSocket>& Socket, const FString& BlueprintPath,
                     const TSharedPtr<FJsonObject>& Recipe, const FSnapshot* Before = nullptr);

/** "MCP behaviour: <Tag> #<NodeGuid>": the NodeComment that marks Node as owned by Tag.
 *  A pasted copy gets a new guid, so it no longer matches and is never removed. */
FString OwnerTag(const FString& Tag, const UEdGraphNode& Node);

/** Every node the behaviour Tag owns; FindOwned(BP, Tag).Num() > 0 means it is installed. */
TArray<UEdGraphNode*> FindOwned(UBlueprint* Blueprint, const FString& Tag);

/** The one input-asset resolver every caller shares; creates what is missing.
 *  InOutActionPath empty: <BlueprintFolder>/Input/IA_<Tag> (Digital), created or reused.
 *  InOutContextPath empty: the context this Blueprint already registers, else
 *  /Game/Input/IMC_MCP, created or reused. Created assets are saved at once, listed in
 *  OutCreated and NOT rolled back. False with OutError and OutCode on failure. */
bool EnsureInputAssets(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, UBlueprint* Blueprint,
                       const FString& Tag, FString& InOutActionPath, FString& InOutContextPath,
                       TArray<FString>& OutCreated, FString& OutError, FString& OutCode);

// ---- Internal: shared by the .cpp files of this folder, not for domain handlers. ----
namespace Detail
{
/** A recipe checked against the Blueprint and flattened to build_graph steps. Nothing
 *  in the Blueprint changes while a plan is built (input assets are created last). */
struct FPlan
{
    FString Tag;
    bool bReplace = true;
    UEdGraph* Page = nullptr;                      // event graph page of hooks and event-graph steps
    TArray<TSharedPtr<FJsonValue>> Ops;            // the build_graph steps, in order
    TArray<FString> Labels;                        // where each step came from in the recipe
    TArray<TSharedPtr<FJsonObject>> Hooks;         // recipe hooks plus the input registration hook
    TArray<TSharedPtr<FJsonObject>> CustomEvents;
    TArray<TSharedPtr<FJsonObject>> Variables;
    TSet<FName> Rebuilt;                           // functions and custom events the recipe declares
    TMap<FString, FString> SharedStepTags;         // step id -> tag of a node no behaviour owns
    TSharedPtr<FJsonObject> Input;                 // the input report; null without an input section
    FString InputId, ActionPath, ContextPath, Key; // resolved input assets (object paths) and key
    bool bCreateAction = false;
    bool bCreateContext = false;
};

/** Nodes made before the batch and what their "$id" references mean. */
struct FWiring
{
    TMap<FString, FString> NodeRefs;               // id -> node guid
    TMap<FString, FString> ThenPins;               // id -> the pin "$id.then" names
    TMap<FGuid, FString> SharedTags;               // node guid -> tag, for nodes no behaviour owns
    TArray<TSharedPtr<FJsonValue>> Report;         // one entry per hook
};

/** What a hook names, resolved; Existing is null when the event must be created. */
struct FHookTarget
{
    FString Label;
    UEdGraphNode* Existing = nullptr;
    UFunction* Function = nullptr;                 // an overridable event
    UFunction* ParentCall = nullptr;               // what a new override of it must call first
    FObjectProperty* Component = nullptr;          // or a component-bound event
    FMulticastDelegateProperty* Delegate = nullptr;
};

// Recipe.cpp
bool CheckKeys(const TSharedPtr<FJsonObject>& Object, std::initializer_list<const TCHAR*> Allowed, const FString& Where,
               FString& OutError);
bool ObjectsIn(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<TSharedPtr<FJsonObject>>& Out,
               FString& OutError);
TSharedPtr<FJsonObject> CopyObject(const TSharedPtr<FJsonObject>& Source);
void AddOp(FPlan& Plan, const TSharedPtr<FJsonObject>& Step, const FString& Label);
bool BuildPlan(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, UBlueprint* Blueprint,
               const TSharedPtr<FJsonObject>& Recipe, FPlan& Plan, const TSharedPtr<FJsonObject>& Report,
               FString& OutError, FString& OutCode);
// Members.cpp: variables, dispatchers, custom events and functions (checked, then flattened).
// Empty when the Blueprint's existing dispatcher Name has the recipe's parameters, else what differs.
FString DispatcherSignatureMismatch(UBlueprint* Blueprint, const FString& Name, const TSharedPtr<FJsonObject>& Declared);
bool PlanMembers(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Recipe, FPlan& Plan, FString& OutError,
                 FString& OutCode);
bool CommitVariables(UBlueprint* Blueprint, const FPlan& Plan, const FSnapshot& Before,
                     const TSharedPtr<FJsonObject>& Report, FString& OutError);
// HookEvents.cpp
bool CheckRecipeIds(const FPlan& Plan, FString& OutError);
bool ResolveHookTarget(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Hook, FHookTarget& Out, FString& OutError,
                       FString& OutCode);
void PlaceBelow(const UEdGraph& Page, UEdGraphNode& Node);
/** The event (and the call to its parent implementation), tagged shared in SharedTags. */
UEdGraphNode* CreateHookEvent(UEdGraph& Page, const FHookTarget& Target, TMap<FGuid, FString>& SharedTags);
// Hooks.cpp
bool CheckHooks(UBlueprint* Blueprint, FPlan& Plan, FString& OutError, FString& OutCode);
bool WireHooks(UBlueprint* Blueprint, const FPlan& Plan, FWiring& Out, FString& OutError, FString& OutCode);
void RewriteRefs(const FPlan& Plan, const FWiring& Wiring);
// Input.cpp and InputAssets.cpp
/** The one project-wide mapping context used when neither the caller nor the Blueprint names one. */
constexpr const TCHAR* DefaultInputContext = TEXT("/Game/Input/IMC_MCP");
bool PlanInput(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Recipe, FPlan& Plan,
               const TSharedPtr<FJsonObject>& Report, FString& OutError, FString& OutCode);
bool CreateInputAssets(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FPlan& Plan,
                       FString& OutError, FString& OutCode);
bool EnsureKeyMapping(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FPlan& Plan,
                      FString& OutError);
bool EnhancedInputLoaded();
bool CheckDefaultInputClasses(const TSharedPtr<FJsonObject>& Input, const TSharedPtr<FJsonObject>& Report,
                              FString& OutError, FString& OutCode);
FString DefaultActionPath(UBlueprint* Blueprint, const FString& Tag);
/** The first mapping context an AddMappingContext node of the Blueprint registers; bOutWanted when Wanted is one. */
UObject* RegisteredContext(UBlueprint* Blueprint, const UObject* Wanted, bool& bOutWanted);
bool AppendRegistration(FPlan& Plan, bool bPawn, FString& OutError);
bool CreateInputAsset(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, bool bContext, const FString& Path,
                      TArray<FString>& OutCreated, FString& OutError, FString& OutCode);
// Snapshot.cpp and Ownership.cpp
bool IsOwnedBy(const UEdGraphNode& Node, const FString& Tag);
/** True for a node any behaviour owns (whatever its tag). */
bool IsBehaviourOwned(const UEdGraphNode& Node);
bool IsFunctionOwnedBy(const UEdGraph* Graph, const FString& Tag);
FString CheckRemovable(UBlueprint* Blueprint, const FString& Tag, const TSet<FName>& Rebuilt);
int32 RemoveOwned(UBlueprint* Blueprint, const FString& Tag);
void Restore(UBlueprint* Blueprint, const FSnapshot& Before, const TSharedPtr<FJsonObject>& Report);
void TagNew(UBlueprint* Blueprint, const FSnapshot& Before, const FString& Tag, const TMap<FGuid, FString>& SharedTags);
} // namespace Detail
} // namespace McpBlueprintBehaviour
