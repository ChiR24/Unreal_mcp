# PCG authoring and execution

PCG actions use the existing `unreal` gateway and `manage_pcg` parent over the connected editor bridge or native HTTP/SSE transport. There is no separate PCG server. The Unreal project must enable the PCG plugin.

This guide describes the bounded editor-local CPU workflows exposed by the current implementation. It is not a claim that every PCG node, data type, engine, platform, or execution mode is supported. See the [PCG validation guide](pcg-validation.md) for runnable tests and evidence limits.

## Discover before editing

Use `list_pcg_node_types` to discover loaded concrete settings classes, then `get_pcg_settings_schema` to read class-level editable property types and defaults. This is a loaded-class inventory, not a scan that loads every Blueprint in the project.

`get_pcg_graph` returns node IDs, settings classes, positions, input/output pins, native connection restrictions, edges, and an authored graph `revision`. Use the actual pin labels returned by discovery; do not assume every node uses `In` and `Out`. `includeSettings: true` includes authored settings and owned subobjects. Node/schema pages default to 50 rows (maximum 100); edges have independent `edgeOffset`/`edgeLimit` pagination (default 500, maximum 1000).

```json
{
  "operation": "execute",
  "tool": "manage_pcg",
  "action": "get_pcg_graph",
  "params": {
    "graphPath": "/Game/PCG/Example",
    "includeSettings": true,
    "offset": 0,
    "limit": 50,
    "edgeOffset": 0,
    "edgeLimit": 500
  }
}
```

`get_pcg_editor_graph` inspects an existing editor representation; it does not open a window or create that representation. `validate_pcg_graph` checks native pin compatibility and reports editor consistency separately. Neither action proves generation will succeed or that packages are saved.

### Inspect an existing node and initialized selector

On UE 5.7+, `get_pcg_node_schema` (or `inspect_pcg` with `view: "node_schema"`) reads property metadata from an already-loaded graph and its exact backend node ID. It follows the selected instance scope, unlike class-default `get_pcg_settings_schema`. It does not load a graph or class, create a selector or packer, export authored values, open an editor, or enable inspection.

```json
{
  "operation": "execute",
  "tool": "manage_pcg",
  "action": "get_pcg_node_schema",
  "params": {
    "graphPath": "/Game/PCG/Example",
    "nodeId": "<backend nodeId from graph discovery>",
    "scopePath": "MeshSelectorParameters",
    "offset": 0,
    "limit": 16
  }
}
```

The reply identifies `settingsClass`, `scopeType`, `scopeKind`, canonical `scopePath`, and graph `revision`. `metadataOnly: true` and `valuesIncluded: false` are intentional: schema metadata is not a value snapshot or permission to mutate. Descriptors report read-only state, native enum options/counts, class constraints, struct paths, and shallow array/set/map types. Hidden enum options and terminal native `MAX` entries are omitted with separate counts; signed enum values outside JSON's safe-integer range use exact decimal strings.

An empty `scopePath` selects the existing settings object. A relative path may enter an existing owned hard-object subobject or by-value struct, such as `OffsetMin`. Paths are limited to eight property segments and 512 characters. Null/shared/CDO/class/soft references, scalar leaves, and array traversal/indices are refused. A read-only selector pointer does not prevent editing properties on its existing owned object through the established settings writer; it never permits pointer replacement or callback/atomic-edit support.

Property pages default to 16 rows (maximum 32), with sorted names and empty off-end pages. Limits include 1,024 visited / 256 eligible properties, 1,024 characters per metadata string, 64 emitted usable enum options, three container-descriptor levels, and 60,000 encoded schema characters. The complete handler result is limited to 80,000 characters with a cooperative 100 ms budget; this is not hard preemption. `PCG_SCHEMA_UNAVAILABLE` reports unavailable scopes or budget failures. Malformed input may also be rejected by the gateway before native execution.

## Author graphs safely

Use the revision returned by the latest graph read as an optimistic-concurrency token. Existing add/connect/settings actions retain their names, aliases, and save defaults; they accept optional `expectedGraphRevision` and refuse stale state before mutation. A revision covers authored graph content, including settings and parameters, and referenced subgraph content. It is process-local, not a durable ID or credential; never compare tokens across editor processes as proof of persistence.

Pin tooltip comparisons use culture-independent source text, not localization IDs assigned during saving or autosaving. String-table entry identity and invariant/localizable text kinds remain distinct. Real tooltip source, pin restriction and settings edits still change the revision. Graph inspection continues to return native property exports, which may change their text-history representation without an authored edit.

Settings are prevalidated before commit. Nested paths may traverse settings-owned objects, not shared assets or class defaults. Invalid mixed settings and invalid spawner resources must not leave a partially edited node. Incompatible pins, required conversions/filters, and connections that would silently replace links are refused. Disconnect explicitly through a destructive action when intended.

New single-graph actions include `move_pcg_node`, `disconnect_pcg_pins`, and `remove_pcg_node`. Input/output boundary nodes cannot be removed. `batch_edit_pcg_graph` accepts 1–100 operations and requires a current nonempty `expectedGraphRevision`; supported operations are `add_node`, `set_settings`, `move_node`, and `connect`. An add step may name `tempId`, which later steps may use where a node ID is expected; the response maps temporary IDs to real node names. Numeric node indices are not accepted in a batch. Positions use native signed 32-bit coordinates; accepted fractional coordinates are truncated toward zero consistently with standalone node metadata. Use integers when exact placement matters. `batch_edit_pcg_graph_destructive` additionally permits disconnect and remove operations and requires destructive authorization and consent.

Batches do not include generation, parameter-schema propagation, world settings, or multi-asset creation. Staging is not a sandbox for arbitrary custom Blueprint callbacks; unsupported isolation must remain refused.

Authored-state capture is bounded across referenced graphs: 65,536 reflected values, 8,192 objects, 4,096 ordinary nodes, 16,384 outgoing edges, and 1,048,576 string/text characters. Nesting and dependency depth are limited to 32 and dependencies to 256 graphs. A cooperative 100 ms capture deadline cannot interrupt one native serializer call. An incomplete or invalid/cyclic capture produces no partial revision. Read/observation receipts report `PCG_SNAPSHOT_UNAVAILABLE`; mutating actions require a complete capture even without an expected revision. Managed inventory applies separate traversal and response limits; page limits alone are not a constant-time guarantee. Native calls that have no public breakable overload remain cooperative, not hard-preemptible.

### Read the complete edit receipt

- `committed`: the in-memory edit was applied.
- `saved`: the selected package was saved; this does not mean every dirty package or dependent instance was saved.
- `rolledBack`: explicit failure recovery restored captured state.
- `residualChanges`: a failed operation left changes or could not establish restoration, including a created child asset. Inspect before continuing; failure does not guarantee that nothing changed.
- `revision` and `editorGraph`: the observed authored graph state and editor consistency.
- `undo`: editor transaction availability, distinct from explicit failure recovery and disk persistence.

A save failure can leave `committed: true, saved: false`; disk writes are not atomically rolled back. An editor transaction is not automatic recovery. Mutations refuse an already-active caller editor transaction with `EDITOR_BUSY`; read-only discovery remains available.

`create_pcg_subgraph` is a composite, non-atomic operation. For a new child it creates/saves the child before attaching it to the parent and optionally saving the parent. Read `subgraphSaved`, `parentCommitted`, `parentSaved`, `partialSave`, `rolledBack`, `residualChanges`, and `atomic: false`. An attachment failure does not automatically delete a created child or roll back saved bytes. Inspect both owners before retrying; the receipt is not an idempotency guarantee.

`sync_pcg_editor_graph` repairs an existing representation through public schema actions. Only `openEditor: true` permits it to open the graph when needed. It preserves existing model identities and comments/layout rather than replacing the graph.

The graph's native input/output boundary nodes may be addressed by IDs returned from graph reads or the `input`/`output` aliases. Their settings are graph-owned; writes are accepted only for the same graph's boundary settings. Configure point-only boundaries through `set_pcg_node_settings` with `settings.Pins`, then connect the actual returned labels through normal PCG edit actions. Preflight refuses pin changes that would silently remove existing edges.

```json
{
  "operation": "execute",
  "tool": "manage_pcg",
  "action": "set_pcg_node_settings",
  "params": {
    "graphPath": "/Game/PCG/PointTransform",
    "nodeId": "DefaultInputNode",
    "expectedGraphRevision": "<revision from get_pcg_graph>",
    "settings": {
      "Pins": [
        {
          "Label": "In",
          "AllowedTypes": {
            "Ids": [{ "Struct": "/Script/PCG.PCGDataTypeInfoPoint" }],
            "CustomSubtype": -1
          }
        }
      ]
    },
    "save": false
  }
}
```

Configure the output boundary similarly, using its real node ID and the intended label, before connecting edges that depend on it.

### Branch and Switch boundaries

The [Branch/interface integration suite](../tests/mcp-tools/world/manage-pcg-flow.test.mjs) is a bounded editor-local CPU recipe. It creates a uniquely owned standalone child graph, discovers its actual boundary IDs and pin labels, authors a two-pin interface and a native `PCGBranchSettings` node, refuses removal of a connected pin unchanged, then explicitly disconnects/edits/reconnects before generation. It verifies typed point output and cleans only its owned products. The recipe changes the standalone child's interface, not an already-attached parent's interface.

An exact-native `UPCGSwitchSettings` settings case on UE 5.7+ refreshes its cached pin labels through public `UpdateSettings` after a complete settings edit. This narrow behavior does not cover Blueprint/subclass settings, other Switch modes, Select option-cache behavior, loops/feedback, parent propagation, or all notification/edge guards. No connection is silently removed to make an edit succeed.

## Configure selectors and packers

On UE 5.7+, the existing `set_pcg_node_settings.settings` writer supports class initialization on exact native `PCGStaticMeshSpawnerSettings`:

- `MeshSelectorType`: `PCGMeshSelectorWeighted` or `PCGMeshSelectorByAttribute`.
- `InstanceDataPackerType`: `PCGInstanceDataPackerByAttribute`, or JSON `null` to disable packing. Empty string and `"None"` also clear the packer; the mesh selector cannot be cleared.

Only this fixed supported set is accepted, by short native class name or full `/Script/PCG` path; arbitrary class loading is not supported. Type initialization occurs before dependent property paths regardless of JSON key order. For example, after authored input points have a SoftObjectPath metadata attribute named `MCP_Mesh`:

```json
{
  "operation": "execute",
  "tool": "manage_pcg",
  "action": "set_pcg_node_settings",
  "params": {
    "graphPath": "/Game/PCG/Example",
    "nodeId": "<backend spawner nodeId>",
    "expectedGraphRevision": "<latest graph revision>",
    "save": false,
    "settings": {
      "MeshSelectorParameters.AttributeName": "MCP_Mesh",
      "InstanceDataPackerParameters.AttributeSelectors": [
        "PCGBegin($Density)PCGEnd"
      ],
      "InstanceDataPackerType": "/Script/PCG.PCGInstanceDataPackerByAttribute",
      "MeshSelectorType": "/Script/PCG.PCGMeshSelectorByAttribute"
    }
  }
}
```

`PCGBegin($Density)PCGEnd` is the native selector import text; bare `$Density` is not a valid array element. Re-read `get_pcg_node_schema` after initialization to discover the actual scope. The parameter pointers themselves remain read-only: assigning `MeshSelectorParameters` or `InstanceDataPackerParameters` directly is refused. Restating the same class preserves its owned object and values. Unsupported existing parameter classes, Blueprint-derived/custom settings, shared/CDO objects, duplicate type fields, and scheduled/executing PCG work are refused; staging is not a callback sandbox.

For weighted selection, read the initialized `MeshSelectorParameters` schema and configure its existing `MeshEntries` array through the normal writer. Each entry contains a `Descriptor` (including `StaticMesh` and `ComponentClass`) and a `Weight`. A deterministic test can use two entries with weights 0 and 1. The legacy `meshPath` convenience option still replaces the weighted list with one entry of weight 1 and switches back from By Attribute when needed. A conflicting explicit non-weighted type and `meshPath` in one edit are refused. Reacquire generated component paths after each generation; object reuse and merged instance order are not guaranteed.

The [selector/packer integration suite](../tests/mcp-tools/world/manage-pcg-spawner-types.test.mjs) covers the bounded native workflow. It does not certify every selector, packer, descriptor, callback, Undo combination, or cold-persistence case.

## Set up placed components

The [component-setup suite](../tests/mcp-tools/world/manage-pcg-component-setup.test.mjs) uses existing actor/component and PCG tools; it does not add a component factory or use `execute_pcg_graph` as a setup side channel.

1. Create a graph and a uniquely owned, bounds-valid actor. Use `control_actor.edit_component` with `edit: "add"` to add named native `PCGComponent` instances. Discover their exact `path` values through `inspect.get_components`, filtered by intended names and the native class.
2. On each fresh component, set `GenerationTrigger: "GenerateOnDemand"`, `bRegenerateInEditor: false`, `bActivated: true`, and the intended `Seed` with `edit_component` / `set_properties`.
3. Bind the intended graph through its native `SetGraph` function, not a raw `GraphInstance.Graph` property assignment. Generic `control_actor.call_actor_function` requires **Destructive/elevated** authorization:

   ```json
   {
     "operation": "execute",
     "tool": "control_actor",
     "action": "call_actor_function",
     "params": {
       "actorName": "ManualGenerator",
       "componentName": "PCG_A",
       "functionName": "SetGraph",
       "arguments": { "InGraph": "/Game/PCG/G_Manual.G_Manual" }
     },
     "consent": {
       "capability": "control_actor.call_actor_function",
       "acknowledge": "elevated"
     }
   }
   ```

4. Read back each exact binding, flags, and seed; prove the components are idle and empty before explicitly generating only the selected component with `generate_pcg_graph`. Preserve the authored graph revision across setup. Observe native completion and inspect actual root data; verify the other component remains empty.
5. Release data-view/root handles, clean generated resources, then delete only the actor and graph whose creation this run established.

This is a selected-owner, non-partitioned component workflow. It does not certify Blueprint templates, existing-parent reassignment, override migration, busy targets, scoped-profile policy, or arbitrary shared ownership.

### Save selected owners and verify after a real restart

The [component-save suite](../tests/mcp-tools/world/manage-pcg-component-save.test.mjs) and [component cold-load suite](../tests/mcp-tools/world/manage-pcg-component-cold-load.test.mjs) exercise a selected-owner persistence slice:

1. Use an explicitly selected isolated project. Create only a unique non-partitioned Map and native Graph; first prove the Map did not already exist. Do not save or discard unknown dirty packages.
2. Create two fresh native components, bind them with the elevated `SetGraph` call above, and read exact component paths, manual flags, seeds, and idle/empty results. Save only the intended Graph and Map. Saving the graph does not save its placed actor.
3. Retain a pure nine-field fixture manifest separately from any provenance record. The cold suite reads `UNREAL_MCP_PCG_COMPONENT_PERSISTENCE_MANIFEST` as a bounded JSON file under the selected project's `Saved/PCGValidation`; the manifest schema and a no-helper workflow are in the [validation guide](pcg-validation.md).
4. Normally exit and start a different editor process before running the cold suite. Reopening the map in the same process is not independent-process evidence. Independently verify source, loaded plugin, selected project, and saved-file identity if making a provenance claim; the cold suite itself does not compute or validate a cross-process hash index.
5. The cold suite reads persisted bindings/flags/seeds before generation, generates only the first component, verifies typed point transforms/quaternions/scales/density/seeds, and confirms the second remains empty. Release handles and clean only the owned native products.
6. Cleanup deletes only the actor and graph whose creation the run established. Save only the intended Map owner when required, and check dirty state before unloading. A tested UE 5.8 Map could become dirty after the public cold phase; the cause is not established. Do not use save-all or discard fallback. Review associated sidecars before deleting an unloaded run-created Map; unexpected files require ownership review.

This proves only the selected-owner placed-component slice. It does not establish graph-instance-asset persistence, Blueprint templates, arbitrary reassignment, concurrent ownership, or every persistence combination.

## Graph parameters and instance overrides

`get_pcg_parameters` returns native schema, stable parameter IDs, graph defaults, effective values, and `overridden`. Select exactly one target:

| `target` | Matching locator |
| --- | --- |
| `graph` (default) | `graphPath` |
| `graphInstance` | `graphInstancePath` |
| `component` | exact absolute `componentPath` |

Do not mix locators or substitute an actor name for a component object path. Schema add/rename/remove operations target the graph, not an instance.

Use `add_pcg_parameter`, `set_pcg_parameter`, `rename_pcg_parameter`, `reset_pcg_parameter_override`, and `remove_pcg_parameter`. Supported schema kinds are Bool, Byte, Int32, Int64, Float, Double, Name, String, Enum, Struct, Object, Class, SoftObject, and SoftClass, as scalars or a single array container. Struct support is Vector, Rotator, and Transform. Enum/struct/reference types use `typeObject`.

JSON integers are exact only through ±9,007,199,254,740,991. Supply larger-magnitude Int64 values as decimal strings; unsafe JSON numbers are refused before a parameter or override is changed, including array elements. Native Int64 defaults/effective values outside that safe range return as exact decimal strings. For example, `"9223372036854775807"` round-trips without double-precision rounding. Array writes are limited to 10,000 elements and validated element by element. Floating-point inputs must be finite and fit their native type. Bool accepts JSON booleans, numeric 0/1, or case-insensitive `true`/`false` strings; other values are rejected. Unsupported existing bag types are refused rather than coerced.

Setting an instance parameter establishes an override. Reset clears that override and resumes inheritance. Rename uses the native API and retains the parameter GUID. A `PCGUserParameterGetSettings` node can bind through `parameterName`, and graph reads expose its binding metadata.

Removal refuses references in loaded PCG nodes rather than cascading node deletion. `loadedReferencesOnly: true` does not prove no unloaded asset references the parameter. Native propagation can cross instances; its receipt does not promise single-graph atomic rollback. Save the owners required by the workflow; saving the graph does not save every instance. A structured graph-instance asset factory/rebind workflow is not provided by these common parameter actions.

## Run and observe generation

Scheduling and completion are different operations. The legacy `execute_pcg_graph` starts native work and returns a numeric `taskId`; its `saved` field describes scheduling-time save behavior, not persistence of future generated products. Supported components may also return an `operationId`, which has a separate identity and lifetime.

For an already assigned, unpartitioned editor component, use `generate_pcg_graph` with its exact `componentPath`. It awaits native execution and inspects captured node diagnostics. `force` defaults to true, `timeoutSeconds` to 60 (0.1–600), and `save` to false. It does not create a component or reassign its graph. A generated delegate alone is not success; unavailable diagnostics may yield `unknown`, and zero output may be valid.

`get_pcg_operation_status` requires both the `operationId` and original `componentPath`, for the authenticated principal. Inspect `state`, `succeeded`, `operationErrorCode`, `engineStillRunning`, and native delegate flags. `completed` means observation ended, not necessarily that generation succeeded. Terminal outcomes include `succeeded`, `failed`, `cancelled`, `timed_out`, `graph_changed`, `superseded`, `target_gone`, `target_moved`, `input_unavailable`, `unknown`, `save_failed`, and `observation_cancelled`.

A timeout or MCP request cancellation does not silently cancel native work. The native lease remains until that work is idle; another operation on the same component is refused as busy. `cancel_pcg_operation` explicitly requests native cancellation and is not rollback. `cleanup_pcg_component` cleans only the selected component's managed resources and waits for native cleanup; nonmanaged world objects are not deleted.

An operation record describes its original observation, not every later task on the component. Before cleanup or fixture deletion, query the component at its current exact path and require native `generating: false` and `cleaning: false`. A rename can change the component path and trigger a separate editor refresh. A previous terminal operation is not current-idle proof; a refused busy cleanup did not empty resources.

The operation store is process-local, bounded to 64 active leases and 128 completed operations with a 30-minute completed TTL. It is not a durable job service. The gateway may answer a long call after about 27 seconds with a `task.state` of `running` or `queued`; retrieve its string MCP task ID with `manage_tools.get_task_result`. Do not pass the numeric engine `taskId` or PCG `operationId` to that API. Gateway-result retention is separate from PCG operation retention. Transport deadlines still apply; in particular, the WebSocket tracker has a five-minute absolute cap.

`get_pcg_component_results` is a current managed-resource inventory, not a completion signal. It counts related managed ISM/HISM components and instances, managed actors and owned child-actor descendants. It does not scan global tags or load unresolved soft actor references; `unresolvedResourceReferences` records those omissions. `offset`/`limit` page components, actors, and root-output summaries (maximum 100 rows per category); `sampleLimit` is 0–16 and defaults to two first/last world-transform samples. Output summaries contain pin/class/tags/counts, not full point arrays; root-output point counts are not counts of all intermediate graph data. A failed operation's complete inventory may include preexisting or partial products. Inventory work is limited to 8,192 resource/object-reference visits, 8,192 output entries, 80,000 encoded result characters and a cooperative 100 ms budget; emitted tags are limited to 32 per output and 1,024 characters each. A query that cannot produce complete totals returns `PCG_SNAPSHOT_UNAVAILABLE`, not partial counts. An operation receipt may omit unavailable resource inventory; observation success is separate from inventory availability. Native resource enumeration has no public breakable overload, so its remaining callbacks become no-ops after refusal but the native call itself cannot be interrupted.

## Read root datasets and selected point values

The read-only data actions are `list_pcg_data`, `get_pcg_data_schema`, `create_pcg_data_view`, `read_pcg_data`, and `release_pcg_data_view`. They inspect root outputs for the exact supported component; they do not expose arbitrary intermediate graph/subgraph/cache data or profiling. `get_pcg_component_results` remains a managed-resource summary, not a typed point reader.

Point/Data and Param data-domain readers support selected bounded rows and native scalar metadata types. Point ranges are reacquired for each read; no full point table is copied. Metadata Attributes are distinct from graph-parameter Property Bags. Param `Elements` uses metadata-entry key space, while `Data` projects a single default value; there is no universal row index for every domain. Soft object/class references remain paths and are not loaded. Unsupported types/domains are refused or reported unavailable, not silently returned as empty or coerced.

The bounded data contract includes:

- At most 256 source rows per projection and 64 rows per page.
- At most 60,000 encoded characters per page; the handler result is limited to 80,000 characters.
- A projection limit of 262,144 encoded characters, 16,000 per row, and 4,096 values.
- A 60-second process-local view lifetime, up to 64 retained handles and 16 per principal, with bounded total row characters.
- Cooperative time checks; one native call cannot be preempted. A page token is not a hard real-time or total-memory guarantee.

A view is an immutable historical JSON projection with weak root membership and known/unknown generation provenance; it is not a fresh whole-world snapshot or a cold-load ID. Native object UID, row count, and graph revision alone do not identify mutable data contents. Release the caller's view and root handles when finished; releasing a root cascades to its views.

The tested Spatial slice supports discovery of a `PCGPrimitiveData` root and its Data-domain schema/default. It does not provide Spatial Elements, sampling, or typed point conversion. UE 5.8 generic Byte/Text/Enum/Struct/Object/SoftObject/Class/SoftClass descriptors and Array/Set/Map value reads are not all supported; descriptor discovery or a safe refusal is not positive value support. These data APIs are editor-local CPU reads on supported UE 5.7+ components and do not enable execution inspection, save assets, start generation, perform GPU transfer, or assert current world-input freshness.

## Permissions and compatibility

Read, Write, and Destructive are exact scope sets: Write does not imply Read. PCG cleanup/cancellation, disconnect, deletion, and destructive batches require explicit consent. The consent grant is an execute-envelope sibling, never a handler parameter. Folded family selectors retain their existing permission boundaries; existing leaf action names remain callable.

Component addresses such as `Map.Map:PersistentLevel.Actor.PCGComponent` are validated as full subobject paths before their owning package is checked against allowed content prefixes. A valid prefix cannot rescue a malformed suffix, traversal, or host-filesystem path. This is package/folder confinement, not a per-actor ACL; the handler still resolves the exact current-world component. Supported `/Content` and separator aliases use the shared canonical form at authorization and resolution. Legacy short-name execution calls retain their existing behavior.

Graph-parameter and completion-observation implementations require native APIs available in UE 5.7 or later; older supported engine branches return `NOT_SUPPORTED` for those operations. PCG remains optional. The completion flow explicitly does not certify partitioned/runtime-managed generation or GPU validation. PIE, World Partition/HiGen, GPU, packaged-runtime, Mac/Linux, and other untested engine/platform combinations must not be inferred from an editor CPU result.

## Validation

For exact test commands, identity requirements, manual cold-load setup, and known validation limits, see the [PCG validation guide](pcg-validation.md). The integration cases below require a live, explicitly selected isolated editor; running a static audit or mock smoke does not execute PCG in Unreal.
