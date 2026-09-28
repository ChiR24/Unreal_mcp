# BLUEPRINT BEHAVIOUR — gameplay logic written into Blueprint assets

`McpBlueprintBehaviour::Author` writes a behaviour INTO a Blueprint from a recipe: variables, event
dispatchers, custom events, functions with their bodies, logic hooked onto events other code also
uses, an Enhanced Input binding, and event-graph nodes. Every edit is an ordinary
`blueprint.build_graph` step run in process, so the result is plain engine nodes that run in a
packaged game with no plugin code. All or nothing, and re-runnable.

Every restored action whose promise is gameplay behaviour (sprint, slide, reload, ammo, inventory
and equipment functions, trigger events, hit pause, interaction events...) is built on it: the
domain handler validates its own parameters, fills a recipe file's placeholders and calls `Author`.
Never hand-roll graph nodes in a domain handler, never answer "configured" for logic that was not
authored, and never add a second mechanism for something this folder does (hooks, input, rollback).

Read this whole file before writing a recipe. Source of truth when in doubt: the code in this folder.

## FILES (12 of 25; each <= 250 pure lines)

| File | Holds |
|------|-------|
| `McpAutomationBridge_BlueprintBehaviour.h` | Public API (below) and the `Detail` internals shared by this folder |
| `...Behaviour.cpp` | `Author`: guards, plan, one batch, compile, commit or restore, save, reply text |
| `...Recipe.cpp` | `ParseRecipe`, `LoadRecipe`, top-level fields, `BuildPlan` |
| `...Members.cpp` | variables, dispatchers, custom events, functions: checks, flattening, variable flags and re-defaults |
| `...HookEvents.cpp` | hook targets (overridable, component and custom events), id uniqueness, event creation with its parent call |
| `...Hooks.cpp` | hook checks, the shared Sequence hub, `"$id"` rewriting |
| `...Input.cpp` | the `input` section: host class, assets, key, node, registration, key mapping |
| `...InputAssets.cpp` | Enhanced Input module and default-class checks, the context resolver, asset creation, `EnsureInputAssets` |
| `...Snapshot.cpp` | `TakeSnapshot`, `Restore` |
| `...Ownership.cpp` | owner tags, `FindOwned`, `RemoveOwned`, `CheckRemovable`, `TagNew` |
| `...Nodes.h/.cpp` | create_node kinds CallDelegate, Message, AsyncTask (also reached by direct create_node and build_graph) |

Recipes: `plugins/McpAutomationBridge/Resources/Recipes/<Domain>/<Name>.json`.
Recipe checks: `tests/unit/plugin/behaviour_recipes.test.ts`.
The member step `add_event_dispatcher` lives in `Domains/Blueprint/Functions/...AddEventDispatcher.cpp`.

## API (namespace `McpBlueprintBehaviour`, include `Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h`)

```cpp
// Resources/Recipes/<Domain>/<Name>.json through ParseRecipe. Null with OutError when the file is
// missing or a placeholder has no value. Domain and Name: letters, digits, underscores.
// The file text is cached for the editor session: an edited recipe needs an editor restart.
TSharedPtr<FJsonObject> LoadRecipe(const FString& Domain, const FString& Name,
                                   const TMap<FString, TSharedPtr<FJsonValue>>& Values, FString& OutError);

// Recipe JSON text -> object. Every string VALUE that is exactly "{{Name}}" becomes Values[Name]
// with its own JSON type. Null with OutError on bad JSON, a placeholder with no value, or a string
// holding "{{" or "}}" that is not exactly one placeholder.
TSharedPtr<FJsonObject> ParseRecipe(const FString& RecipeJson,
                                    const TMap<FString, TSharedPtr<FJsonValue>>& Values, FString& OutError);

// Builds Recipe into the Blueprint, all or nothing. Game thread. Refused during PIE.
// Before: your own TakeSnapshot, taken BEFORE your own edits of the same Blueprint (see ROLLBACK).
FAuthorResult Author(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                     const TSharedPtr<FMcpBridgeWebSocket>& Socket, const FString& BlueprintPath,
                     const TSharedPtr<FJsonObject>& Recipe, const FSnapshot* Before = nullptr);

struct FAuthorResult
{
    bool bApplied;                   // in the Blueprint, and the Blueprint was saved
    FString Message;                 // one sentence; on failure names the recipe step that failed
    FString ErrorCode;               // set when !bApplied (ERROR CODES below, or the failing step's own)
    TSharedPtr<FJsonObject> Report;  // always set; send it as the reply's result (REPORT below)
};

FSnapshot TakeSnapshot(UBlueprint* Blueprint);          // compiles first when Dirty or Unknown
TArray<UEdGraphNode*> FindOwned(UBlueprint*, const FString& Tag);  // Num() > 0: installed
FString OwnerTag(const FString& Tag, const UEdGraphNode& Node);    // "MCP behaviour: <Tag> #<guid>"

// The one input-asset resolver (see INPUT). Creates what is missing and saves it; never rolled back.
bool EnsureInputAssets(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, UBlueprint* Blueprint,
                       const FString& Tag, FString& InOutActionPath, FString& InOutContextPath,
                       TArray<FString>& OutCreated, FString& OutError, FString& OutCode);

constexpr int32 MaxAuthorSteps = 1000;                            // public build_graph keeps 200
constexpr const TCHAR* HubTag = TEXT("MCP event hub");            // shared nodes, never removed
constexpr const TCHAR* InputContextTag = TEXT("MCP input context");
```

`Detail::` is internal to this folder. Domain handlers use only the names above.

## THE HANDLER PATTERN

```cpp
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

// 1. Validate YOUR parameters first and refuse with a specific code; Author checks the recipe,
//    not the meaning of your parameters (a Character-only action checks the parent class itself).
// 2. Values for the recipe's placeholders, typed: a number stays a number.
TMap<FString, TSharedPtr<FJsonValue>> Values;
Values.Add(TEXT("SprintSpeed"), MakeShared<FJsonValueNumber>(SprintSpeed));
Values.Add(TEXT("Key"), MakeShared<FJsonValueString>(Key));
FString Error;
TSharedPtr<FJsonObject> Recipe = McpBlueprintBehaviour::LoadRecipe(TEXT("Character"), TEXT("Sprint"), Values, Error);
if (!Recipe)
{
    SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_RECIPE")); // a recipe or handler bug
    return true;
}
// 3. Optional parts: remove what the caller did not ask for (never pass a half-filled section).
if (Key.IsEmpty() && ActionPath.IsEmpty())
{
    Recipe->RemoveField(TEXT("input"));
    Recipe->RemoveField(TEXT("eventGraph"));
}
// 4. Author, and send its reply unchanged (add your own fields to R.Report first if you have any).
const McpBlueprintBehaviour::FAuthorResult R = McpBlueprintBehaviour::Author(*this, RequestId, Socket, BlueprintPath, Recipe);
SendAutomationResponse(Socket, RequestId, R.bApplied, R.Message, R.Report, R.ErrorCode);
return true;
```

The capability record declares the handler's own params (and `save` only if it reads one; Author
always saves). Declare the report fields callers should see as outputProps; undeclared ones still
arrive, folded into `details`. Useful set: `compiled`, `saved`, `rolledBack`, `previousRemoved`,
`variables`, `functions`, `dispatchers`, `customEvents`, `hooks`, `input`, `diagnostics`.

## RECIPE FILES

- One behaviour per file: `Resources/Recipes/<Domain>/<Name>.json`, both names letters, digits and
  underscores, loaded with `LoadRecipe(TEXT("<Domain>"), TEXT("<Name>"), ...)`. UAT BuildPlugin
  packages `Resources/`, so recipes ship with the plugin (the unit test keeps FilterPlugin.ini from
  excluding it). No recipe text in C++ literals: a literal that size breaks MSVC (C2026).
- Required in a file: `description` (what it authors) and `tag`. List every placeholder in
  `placeholders` with a one-line description; the unit test fails on a placeholder used but not
  listed (no handler would fill it) and on one listed but never used.
- `tests/unit/plugin/behaviour_recipes.test.ts` checks every file in CI: JSON, top-level and section
  fields (the same lists the C++ accepts), placeholder syntax, and CallFunction steps without
  `memberClass`. Such a step must call a recipe function, a recipe custom event, or a name in
  `DEFAULT_CLASS_FUNCTIONS` (a stock-library function that resolves on every Blueprint; add yours
  there only if it lives on KismetSystemLibrary, GameplayStatics, KismetMathLibrary,
  KismetStringLibrary or KismetTextLibrary). Anything else carries `memberClass`: a parent-class
  function (`LaunchCharacter` needs `"memberClass": "Character"`), KismetArrayLibrary,
  BlueprintMapLibrary, CharacterMovementComponent, a component class, NiagaraFunctionLibrary...
- Test a new recipe in the unit test (run `npx vitest run tests/unit/plugin/behaviour_recipes.test.ts`)
  and live through its action's integration case (`compiled == true`, see TESTS).

## RECIPE SCHEMA

Top level (anything else is `INVALID_RECIPE`):

| Field | Type | Default | Meaning |
|-------|------|---------|---------|
| `description` | string | | Documentation; required in files |
| `tag` | string | required | Owner name. Every node the run makes is tagged with it (OWNERSHIP) |
| `replace` | bool | true | First remove what an earlier run of this tag made (OWNERSHIP) |
| `placeholders` | object | | `{Name: "description"}`: documentation, checked by the unit test |
| `variables` | array | | Member variables (below) |
| `dispatchers` | array | | Event dispatchers (below) |
| `customEvents` | array | | Custom events (below) |
| `functions` | array | | Functions with bodies (below) |
| `hooks` | array | | Logic on shared events (HOOKS) |
| `input` | object | | One Enhanced Input binding (INPUT) |
| `eventGraph` | array | | build_graph steps on the event graph (STEPS) |

`variables[]`: `{variableName, variableType, defaultValue, category, isReplicated, instanceEditable, exposeOnSpawn, keepExisting}`
- `variableType`: the add_variable grammar (`McpBlueprintUtils::ResolvePinType`): Bool, Byte, Int, Int64,
  Float, Double, String, Name, Text, Vector, Vector2D, Rotator, Transform, Color, LinearColor,
  `Struct:<path>`, `Enum:<path>`, `Object:<class>`, `Class:<class>`, `SoftObject:<class>`,
  `SoftClass:<class>`, `Array:<T>`, `Set:<T>`, `Map:<K>,<V>`. A type that does not resolve is
  `TYPE_RESOLUTION_FAILED` before anything changes.
- A new name must be free on the Blueprint and its parents (`NAME_CONFLICT`). An existing variable
  must have the same type (`VARIABLE_TYPE_CONFLICT`; Float and double count as the same) and is
  kept; its `defaultValue` is re-applied unless `keepExisting` is true (report status `added`,
  `redefaulted` or `kept`).
- `defaultValue`: a JSON scalar is the value's export text (`30`, `true`, `"Idle"`); a JSON array
  sets an array. A map or set default is its export text as a string, the form get_property
  returns, e.g. `"((Potion, 0.5),(Sword, 3.0))"` for `Map:Name,Float`. A default the property
  cannot import, on a new variable (checked by add_variable itself, which then adds nothing) or an
  existing one, is `DEFAULT_NOT_APPLIED` and rolls back. The compiler alone would only warn and
  keep the zero value.
- `instanceEditable` (Instance Editable), `exposeOnSpawn` (Expose on Spawn; turns Instance Editable
  on, so SpawnActor nodes grow the pin), `category`, `isReplicated` (false also clears RepNotify).
  These apply to an existing variable too, whenever the recipe gives them.

`dispatchers[]`: `{name, parameters: [{name, type}]}`. What the editor's "+ Event Dispatcher" makes
(member variable plus signature graph, `FBlueprintEditor::OnAddNewDelegate`). An existing dispatcher
of that name is reused as it is (its parameters are not changed). Call it with a CallDelegate node;
other Blueprints bind to it.

`customEvents[]`: `{id, eventName, shared}`. Made before the batch, with no parameters (pass data
through variables, or use a function or a dispatcher). `"$id"` is the event node: wire its logic from
`"$id.then"`, call it with a CallFunction step of `memberName` = eventName. `shared: true` means other
behaviours may hook it: the node carries HubTag, is never removed, and even this recipe adds logic to
it only through a hook. An existing custom event that is neither this behaviour's nor shared is
`FUNCTION_EXISTS` (users often have their own `OnDeath`).

`functions[]`: `{id, functionName, inputs, outputs, pure, isPublic, operations}`
- `inputs` and `outputs`: `[{name, type}]`, types as above. `pure`: call sites have no exec pins.
  `isPublic` false: private.
- `operations`: build_graph steps run with graphName = the function. `"$id"` is the entry node
  (`then` plus one output pin per input); `"$id_return"` is the return node (`execute` plus one input
  pin per output), which exists when outputs are declared, and the entry is already wired to it:
  connecting `"$id.then"` elsewhere replaces that link. A second return node: a `FunctionResult`
  step (NODE KINDS).
- An existing function of that name is `FUNCTION_EXISTS` unless this behaviour made it and `replace`
  is on, in which case it is rebuilt.

## STEPS (build_graph vocabulary, used by `operations` and `eventGraph`)

Each step is `{edit, id?, ...that edit's own params}`, the exact public `blueprint.build_graph`
grammar, so any recipe can be replayed by hand.

| edit | Params |
|------|--------|
| `create_node` | `nodeType`, `memberName`, `memberClass` (or `targetClass`), `eventName`, `inputActionPath`, `posX`, `posY`, `pinDefaults` |
| `connect_pins` | `from` and `to` as `"$id.Pin"`, or `fromNodeId`, `fromPinName`, `toNodeId`, `toPinName` |
| `set_pin_default_value` | `nodeId`, `pinName`, `propertyValue` |
| `set_node_property` | `nodeId`, `propertyName`, `propertyValue` |
| `create_reroute_node` | position |
| `add_variable`, `add_function`, `add_event`, `add_event_dispatcher` | member steps; in a recipe use the sections above instead, which add the checks and the rollback bookkeeping |

References:
- `"$id"` names the node a step (or a section entry) with that `id` made; `"$id.Pin"` a pin of it.
  Ids are unique across the whole recipe, hooks and custom events included (`INVALID_RECIPE`).
- `"$fn"` and `"$fn_return"`: a function's entry and return nodes. `"$entry"`: the entry node of the
  batch's own graph.
- Hooks: `"$hook.then"` is the hook's hub pin (where its logic starts); `"$hook_event.<Pin>"` reads the
  event's data pins (`DeltaSeconds`, `OtherActor`, `NewController`...).
- Input: `"$input.Started"` and the other trigger pins (INPUT).
- `pinDefaults` (create_node only): `{Pin: value}` applied right after the node is made; an object pin
  takes an asset path (`"MappingContext": "/Game/Input/IMC_MCP.IMC_MCP"`); a read-only pin gets a
  MakeLiteral node wired into it. Declared on a member step it is refused.
- Nodes without posX and posY are auto-placed on a grid right of the event graph's nodes (function
  graphs too; they compile the same, they just sit to the right).

Engine pin spellings recipes use: `execute`, `then`, `self` (a call's Target), `ReturnValue`;
Branch `Condition`, `then`, `else`; Sequence `then_0`, `then_1`...; Cast `Object`, `then`,
`CastFailed`, `As<Class Display Name>` (`"AsPlayer Controller"`); VariableSet output `Output_Get`;
a VariableGet's output is the variable's name.

## NODE KINDS (create_node `nodeType`)

| nodeType | Fields | Notes |
|----------|--------|-------|
| `CallFunction` | `memberName`, `memberClass` | Without memberClass: the Blueprint's own class and parents, the recipe's functions and custom events, and KismetSystemLibrary, GameplayStatics, KismetMathLibrary, KismetStringLibrary, KismetTextLibrary. Array functions become array nodes, pure functions get no exec pins |
| `VariableGet`, `VariableSet` | `memberName`, `memberClass` | Another class's property with memberClass (`MaxWalkSpeed` on `CharacterMovementComponent`, fed through `self`) |
| `Event` | `eventName` | An existing event is reused. For shared events use HOOKS |
| `Branch`, `Sequence`, `Select`, `DoOnce`, `FlipFlop`, `Gate`, `Self`, `MakeArray`, `Literal`... | | Aliases of K2Node classes; any `K2Node_<Name>` class name works |
| `ForEachLoop`, `ForLoop`, `WhileLoop`... | | StandardMacros instances |
| `Cast` | `targetClass` | `pure: true` for a pure cast |
| `K2Node_GetSubsystemFromPC` | `targetClass` | Typed subsystem getter, pin `PlayerController` |
| `SpawnActorFromClass` | `targetClass` | Exposed-on-spawn variables become pins |
| `K2Node_EnhancedInputAction` | `inputActionPath` | Normally made by the `input` section |
| `CallDelegate` | `memberName`, `memberClass` | Calls (broadcasts) a dispatcher of this Blueprint, or of memberClass (then feed `self`). Pins: exec, `self`, one per parameter. The build_graph pre-check refuses an undeclared one (`DISPATCHER_NOT_FOUND`) |
| `Message` | `memberClass`, `memberName` | Interface message: memberClass an interface (Blueprint Interface asset path or native), memberName its function. Target any object; no-op when it does not implement it |
| `AsyncTask` (also `AsyncAction`, `K2Node_AsyncAction`, `K2Node_LatentAbilityCall`, `K2Node_LatentGameplayTaskCall`) | `memberClass`, `memberName` | The editor's own latent node for a static factory (`AsyncActionHandleSaveGame.AsyncLoadGameFromSlot`, `AbilityTask_WaitDelay.WaitDelay`): `then` fires at once, one exec pin per delegate (`Completed`, `OnFinish`) with its data pins. Event graphs only (`LATENT_NODE_IN_FUNCTION`); `ASYNC_NODE_NOT_AVAILABLE` when the module providing the node class is not loaded |
| `FunctionResult` | graphName = a function | Another return node; its pins mirror the first one's and take `pinDefaults` (a "fail" return) |

Latent calls (Delay, AsyncTask) belong in the event graph; in a function they fail to compile and
the run rolls back with the compiler's message.

## HOOKS

`hooks: [{id, event} | {id, customEvent} | {id, component, event}]`, exactly one form per hook,
`id` required.
- `event`: an overridable event without return value of the parent class chain. AActor's events take
  the editor's names (`BeginPlay`, `Tick`, `AnyDamage`, `ActorBeginOverlap`, `Hit`, `Destroyed` resolve
  to `Receive<Name>`); others go by their function name: Pawn `ReceiveControllerChanged`,
  `ReceivePossessed`; Character `OnLanded`, `OnJumped`, `K2_OnMovementModeChanged`, `K2_OnStartCrouch`.
- `component` + `event`: a component variable of the Blueprint (its own or inherited, e.g. a
  Character's `CapsuleComponent`) and a Blueprint-assignable delegate of it (`OnComponentBeginOverlap`,
  `OnComponentHit`). The component must exist before Author runs (see ROLLBACK for adding one).
- `customEvent`: an existing custom event no behaviour owns (a user's own, or one declared `shared`),
  or one this recipe declares with `shared: true`. Hooking one another behaviour owns is
  `HOOK_EVENT_OWNED`; hooking this recipe's own non-shared one is refused (wire `"$id.then"`).

What a hook does:
- The event gets ONE shared Sequence node, the hub (comment `MCP event hub`). The chain the event
  already ran moves to `then_0` and still runs first; each hook takes a free `then_N` (made when none
  is free) and `"$hook.then"` is that pin. No hook ever steals the event's link from the user or from
  another behaviour, and a replace frees exactly the pins the old nodes used.
- An event the Blueprint does not implement yet is created. When something above the Blueprint
  implements it (a parent Blueprint's graph, a BlueprintNativeEvent's C++), a `Parent:` call is made
  and runs first, as the editor's default events do (report `hooks[].parentCallAdded`).
- Created events, hubs and parent calls are shared: never removed by a replace.
- Wiring from `"$hook_event.then"` (the event's own exec pin) is refused, as `from` or as
  `fromNodeId` `"$hook_event"` with `fromPinName` `"then"`; wire from `"$hook.then"`.
- All hooked events of one recipe must be on one event graph page (`HOOK_EVENTS_ON_DIFFERENT_PAGES`);
  that page is where eventGraph steps run. Without hooks: the first event graph page (`NO_EVENT_GRAPH`
  when the Blueprint has none and the recipe needs one).
- Report `hooks[]`: `{id, event, eventNodeGuid, eventCreated, parentCallAdded, hubNodeGuid, pin}`.

## INPUT

`input: {id, inputActionPath, mappingContextPath, key, registerContext}`; `id` defaults to `input`,
`registerContext` to true. It authors, in order: the input node, the context registration when the
Blueprint lacks it, and (after the Blueprint compiles) the key mapping.

Checks, all before anything changes:
- Enhanced Input loaded, else `ENHANCEDINPUT_PLUGIN_NOT_ENABLED`.
- The project's DefaultPlayerInputClass and DefaultInputComponentClass are Enhanced Input's (UE 5.0
  projects and projects upgraded from UE4 often are not): else `ENHANCED_INPUT_NOT_DEFAULT` with
  `report.nextCall`, the system_control set_project_setting call that fixes the first wrong one (the
  node would compile and never fire).
- An Actor-based Blueprint; `registerContext` true needs a Pawn or PlayerController
  (`INPUT_NOT_SUPPORTED`). An Actor takes `registerContext: false`: only the node is authored, and
  `report.input.note` says it fires after EnableInput or with Auto Receive Input, while the possessed
  Pawn or PlayerController registers the context.
- `key` a valid key name (`LeftShift`, `SpaceBar`, `Gamepad_FaceButton_Bottom`): `INVALID_ARGUMENT`.
- Named assets exist (`ASSET_NOT_FOUND`); the K2Node_EnhancedInputAction class is loaded.

Resolver (also public as `EnsureInputAssets`):
- Action: the given path; else, with a key, `<BlueprintFolder>/Input/IA_<Tag>` (Digital), created
  or reused. No action and no key: no input node (the section then only registers the context;
  with `registerContext` false as well it is `INVALID_RECIPE`).
- Context: the given path; else the context this Blueprint already registers (an AddMappingContext
  node's `MappingContext` default, or the variable wired into it); else `/Game/Input/IMC_MCP`, created
  or reused. Created assets are saved at once, listed in `input.createdAssets` and NOT rolled back.

The node: `create_node K2Node_EnhancedInputAction` with the section's id. Exec pins `Started`,
`Triggered`, `Ongoing`, `Canceled`, `Completed`; data pins `ActionValue`, `ElapsedSeconds`,
`TriggeredSeconds`. Wire `"$input.Started"` (a toggle) or `"$input.Triggered"` (held).

Registration (no key fires without it, and nothing warns): unless the Blueprint already has an
AddMappingContext for this context, the recipe file `Behaviour/InputRegistrationPawn`
(On Controller Changed -> Cast to PlayerController -> Enhanced Input Local Player Subsystem ->
Branch on IsValid -> AddMappingContext(context, 0)) or `Behaviour/InputRegistrationController`
(BeginPlay -> Self -> ...) is added through a hook. The IsValid branch skips a PlayerController with
no LocalPlayer (the server's copy of a remote player's), which has no subsystem. Its nodes carry `MCP input context`, are shared and never removed.

Key mapping, last: a pair already in the context is left alone (`input.keyAlreadyMapped`; add_mapping
would reset its triggers and modifiers); else it is added through the Input domain's add_mapping
(`input.keyMapped`). A failure is `KEY_MAPPING_FAILED` and rolls the Blueprint back.

Remove the whole section when the caller asked for no binding.

## PLACEHOLDERS

- A string value that is exactly `"{{Name}}"` becomes `Values[Name]` with its JSON type:
  `MakeShared<FJsonValueNumber>`, `FJsonValueString`, `FJsonValueBoolean`, `FJsonValueArray`,
  `FJsonValueObject`. Nothing is spliced into text: `"IA_{{Name}}"` is an error, build the whole
  string in the handler.
- Every placeholder needs a value (missing: LoadRecipe fails, a handler bug). For an optional part,
  pass something and remove the field or section before Author.
- Keys are never substituted, only values.

## OWNERSHIP AND REPLACE

- Every node a run makes gets NodeComment `MCP behaviour: <Tag> #<NodeGuid>` (`OwnerTag`). A pasted
  copy gets a new guid, so it no longer matches and is never removed. A function is owned when its
  entry node is. `FindOwned(BP, Tag).Num() > 0` tells whether a behaviour is installed.
- `replace` (default): before rebuilding, the tag's nodes and function graphs are removed. Variables
  stay (re-defaulted unless `keepExisting`), dispatchers stay as they are, shared nodes stay (hubs,
  events a hook made and their parent calls, shared custom events, the input registration).
- `BEHAVIOUR_IN_USE`: the replace would remove a function or custom event the recipe no longer
  declares while a node this behaviour does not own still calls it.
- There is no Remove action. Author with a recipe holding only its `tag` removes the behaviour's
  nodes and functions; variables and dispatchers stay.

## ROLLBACK

The Blueprint is snapshotted before any change: the node set, each node's comment, enabled state and
comment bubble, every pin's links and defaults, member graphs by object, NewVariables (types, flags,
defaults), the SCS node set, whether it compiled, whether its package was dirty.

Any failure after planning (a step, a variable default, a Blueprint that compiled before and does not
now, the key mapping) restores it: new graphs, nodes, hub pins, variables and SCS nodes are removed,
links, defaults, node states and variables are put back, the Blueprint is recompiled
(`report.afterRollback`, `rolledBack: true`) and saved when its package was clean before, so the disk
matches the pre-call state. Rollback does not use editor undo (compiles can clear the undo buffer).
Exception: after a replace removed the previous version (`previousRemoved` > 0) nothing is saved,
because the disk copy is then the only one that still has that version.

Your own edits (add an SCS component, then hook its overlap): check PIE yourself
(`GEditor->IsPlaySessionInProgress()`), `FSnapshot Before = McpBlueprintBehaviour::TakeSnapshot(BP);`
BEFORE your edits, then `Author(..., Recipe, &Before)`. A failure anywhere, planning included, then
undoes your edits too. Author compiles a dirty Blueprint first so its checks see your edits. It
cannot undo them when it cannot load the Blueprint (PIE started, or the load failed); the message
then says your edits were NOT undone and `rolledBack` is false.

Not covered:
- A replace that fails after removing the previous version cannot bring it back in memory: the
  Blueprint is left without either, unsaved, and the message says so (`previousRemoved` > 0).
  Revert the asset (reload it without saving) to get the disk copy back, or re-run to rebuild.
- Input assets the resolver created stay (listed in `input.createdAssets`).
- Property changes on SCS components that existed before (template properties) are not snapshotted.
- A Blueprint that did not compile before may keep not compiling (`preExistingErrors: true`).

## ORDER OF WORK INSIDE AUTHOR

1. PIE refused; the Blueprint loads; snapshot (compiling a dirty Blueprint to learn its status).
2. Plan, changing nothing: fields, members (names, types, conflicts), hooks, input checks and
   resolver, event-graph steps, id uniqueness, hook targets and page, BEHAVIOUR_IN_USE; last, input
   assets are created (the only write of planning).
3. Replace: remove the tag's previous nodes and functions.
4. Custom events, hook events, parent calls and hubs are made; `"$id"` references to them rewritten.
5. ONE build_graph batch under a save deferral, in order: variables, dispatchers, each function
   followed by its body, the input node, the registration, the eventGraph steps. The batch pre-checks
   every function, variable, dispatcher and async factory a step names before running any step.
6. Variable flags and re-defaults; compile; a Blueprint that compiled before and does not now fails
   with `BEHAVIOUR_BREAKS_COMPILE` and the compiler's first error.
7. New nodes tagged (owner or shared); key mapping.
8. One save. On any failure from step 3 on: Restore, then save the restored state (not after a
   replace removed the previous version; see Not covered).

Each member step compiles once (variables, dispatchers, functions), plus the final compile.

## REPORT (`R.Report`)

| Field | When | Meaning |
|-------|------|---------|
| `blueprintPath`, `tag` | always | |
| `previousRemoved` | applied | Nodes and function graphs a replace removed first |
| `hooks` | applied | See HOOKS |
| `stepCount` | applied | build_graph steps run |
| `results`, `nodeIds`, `succeeded` | batch ran | The batch's per-step results; `nodeIds` maps recipe ids to node guids (`<id>_return` too) |
| `failedIndex`, `failedStep` | a step failed | Batch index, and where it came from: `functions[0].operations[3]`, `eventGraph[2]`, `variables[1] Speed`, `input`, `input registration` |
| `variables` | applied | `[{name, status: added, redefaulted or kept}]` |
| `compiled`, `compilerStatus`, `errorCount`, `warningCount`, `diagnostics` | compiled | The final compile |
| `preExistingErrors` | compiled | The Blueprint did not compile before either |
| `input` | input section | `defaultPlayerInputClass`, `defaultInputComponentClass`, `inputActionPath`, `mappingContextPath`, `key`, `contextRegistered`, `registrationAdded`, `nodeId`, `createdAssets`, `keyMapped`, `keyAlreadyMapped`, `note` |
| `nextCall` | ENHANCED_INPUT_NOT_DEFAULT | The call that fixes the project setting |
| `functions`, `dispatchers`, `customEvents` | applied | Names the recipe declared |
| `rolledBack`, `afterRollback` | always, failure | Whether Restore ran, and the compile after it |
| `saved` | applied, or rolled back | The one save |

## ERROR CODES (`R.ErrorCode`)

| Code | Cause |
|------|-------|
| `PIE_ACTIVE` | Play In Editor is running |
| `BLUEPRINT_NOT_FOUND` | BlueprintPath does not load |
| `INVALID_RECIPE` | No recipe; unknown field; malformed section; no tag; duplicate id; too many steps; a hook without id or with a bad form; wiring from a shared event's exec pin; hooking the recipe's own custom event; an input section that authors nothing |
| `TYPE_RESOLUTION_FAILED` | A variable, parameter, input or output type does not resolve |
| `NAME_CONFLICT` | A new member name is taken |
| `VARIABLE_TYPE_CONFLICT` | An existing variable has another type |
| `FUNCTION_EXISTS` | A function or custom event of that name is not this behaviour's |
| `HOOK_EVENT_NOT_FOUND`, `HOOK_EVENT_OWNED`, `HOOK_EVENTS_ON_DIFFERENT_PAGES`, `NO_EVENT_GRAPH` | HOOKS |
| `BEHAVIOUR_IN_USE` | OWNERSHIP AND REPLACE |
| `ENHANCEDINPUT_PLUGIN_NOT_ENABLED`, `ENHANCED_INPUT_NOT_DEFAULT`, `INPUT_NOT_SUPPORTED`, `INVALID_ARGUMENT`, `ASSET_NOT_FOUND`, `INPUT_ASSET_FAILED` | INPUT |
| the failing step's code (`FUNCTION_NOT_FOUND`, `VARIABLE_NOT_FOUND`, `DISPATCHER_NOT_FOUND`, `PIN_DEFAULT_FAILED`, `LATENT_NODE_IN_FUNCTION`, ...), else `STEP_FAILED` | A build_graph step failed; `failedStep` names it |
| `DEFAULT_NOT_APPLIED` | A variable's default (new or existing) does not import |
| `BEHAVIOUR_BREAKS_COMPILE` | The Blueprint compiled before and would not with the behaviour |
| `KEY_MAPPING_FAILED` | The key mapping could not be added |

## LIMITS

- At most 1000 steps per Author (MaxAuthorSteps); the public build_graph keeps 200.
- PIE is refused. One event graph page per recipe. Custom events take no parameters.
- Not authored here: timelines, macro definitions, interface implementations, function overrides
  with a return value (hooks cover events), Get Class Defaults nodes, AnimGraph logic, widget
  bindings (their own handlers do those).
- Recipe files are cached for the editor session: restart the editor after editing one.
- Engine pin spellings can drift across 5.0 to 5.8: every action's integration case must run it and
  assert `compiled == true`.

## TESTS FOR AN ACTION BUILT ON THIS

- Integration case (tests/mcp-tools/...): a success case that asserts `structuredContent.result.compiled`
  true and the fields that prove the effect (`variables`, `functions`, `hooks`, `input.keyMapped`...),
  a refusal case per action (bad input -> the specific error code), and every optional param of the
  record in some case (test:params).
- Live check (the main session, in PIE): play, drive the input or call the function, read the
  property that changed.

## WORKED EXAMPLE: configure_sprint (Character)

`Resources/Recipes/Character/Sprint.json`:

```json
{
  "description": "Sprint toggle on a Character: ToggleSprint flips bIsSprinting and sets CharacterMovement.MaxWalkSpeed to SprintSpeed or WalkSpeed; an Enhanced Input action calls it.",
  "tag": "Sprint",
  "placeholders": {
    "WalkSpeed": "Walk speed in cm/s: the caller's, else the class default CharacterMovement MaxWalkSpeed.",
    "SprintSpeed": "Sprint speed in cm/s.",
    "InputAction": "Input Action object path, or empty for <BlueprintFolder>/Input/IA_Sprint.",
    "MappingContext": "Input Mapping Context object path, or empty for the resolver's choice.",
    "Key": "Key name, e.g. LeftShift."
  },
  "variables": [
    { "variableName": "bIsSprinting", "variableType": "Bool", "defaultValue": false, "category": "Sprint" },
    { "variableName": "WalkSpeed", "variableType": "Float", "defaultValue": "{{WalkSpeed}}", "category": "Sprint", "instanceEditable": true },
    { "variableName": "SprintSpeed", "variableType": "Float", "defaultValue": "{{SprintSpeed}}", "category": "Sprint", "instanceEditable": true }
  ],
  "functions": [
    { "id": "toggle", "functionName": "ToggleSprint", "operations": [
      { "edit": "create_node", "id": "isSprint", "nodeType": "VariableGet", "memberName": "bIsSprinting" },
      { "edit": "create_node", "id": "flip", "nodeType": "CallFunction", "memberName": "Not_PreBool" },
      { "edit": "create_node", "id": "setFlag", "nodeType": "VariableSet", "memberName": "bIsSprinting" },
      { "edit": "create_node", "id": "sprint", "nodeType": "VariableGet", "memberName": "SprintSpeed" },
      { "edit": "create_node", "id": "walk", "nodeType": "VariableGet", "memberName": "WalkSpeed" },
      { "edit": "create_node", "id": "pick", "nodeType": "CallFunction", "memberName": "SelectFloat" },
      { "edit": "create_node", "id": "move", "nodeType": "VariableGet", "memberName": "CharacterMovement" },
      { "edit": "create_node", "id": "setSpeed", "nodeType": "VariableSet", "memberName": "MaxWalkSpeed", "memberClass": "CharacterMovementComponent" },
      { "edit": "connect_pins", "from": "$toggle.then", "to": "$setFlag.execute" },
      { "edit": "connect_pins", "from": "$isSprint.bIsSprinting", "to": "$flip.A" },
      { "edit": "connect_pins", "from": "$flip.ReturnValue", "to": "$setFlag.bIsSprinting" },
      { "edit": "connect_pins", "from": "$sprint.SprintSpeed", "to": "$pick.A" },
      { "edit": "connect_pins", "from": "$walk.WalkSpeed", "to": "$pick.B" },
      { "edit": "connect_pins", "from": "$setFlag.Output_Get", "to": "$pick.bPickA" },
      { "edit": "connect_pins", "from": "$move.CharacterMovement", "to": "$setSpeed.self" },
      { "edit": "connect_pins", "from": "$pick.ReturnValue", "to": "$setSpeed.MaxWalkSpeed" },
      { "edit": "connect_pins", "from": "$setFlag.then", "to": "$setSpeed.execute" }
    ] }
  ],
  "input": { "inputActionPath": "{{InputAction}}", "mappingContextPath": "{{MappingContext}}", "key": "{{Key}}" },
  "eventGraph": [
    { "edit": "create_node", "id": "call", "nodeType": "CallFunction", "memberName": "ToggleSprint" },
    { "edit": "connect_pins", "from": "$input.Started", "to": "$call.execute" }
  ]
}
```

Handler (Domains/Character): refuse a Blueprint that is not a Character (`LoadBlueprintAsset`, then
`ParentClass->IsChildOf(ACharacter::StaticClass())`) and a speed <= 0 before anything else; fill
WalkSpeed with the caller's value, else the class default object's CharacterMovement MaxWalkSpeed (so
the toggle returns to the real walk speed); pass `""` for InputAction and MappingContext the caller
left out (the resolver picks them); remove `input` and `eventGraph` when neither key nor action was
given (ToggleSprint stays callable); then Author and reply as in THE HANDLER PATTERN.

What one call does: 3 variables (added, or re-defaulted on a re-run), the function ToggleSprint
(rebuilt on a re-run), one input node, the registration when the Character has none, the key mapping:
24 build_graph steps, 32 with the registration, one compile check, one save. The report shows `compiled: true`,
`variables`, `functions: ["ToggleSprint"]`, `input.keyMapped`, `input.registrationAdded`. In PIE,
pressing the key flips `bIsSprinting` and the possessed pawn's `CharacterMovement.MaxWalkSpeed`
between the two speeds.
