# 📋 Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## 🏷️ [Unreleased]

<details>
<summary><b>✨ Added</b></summary>

- **`inspect.describe_view`**: what the level viewport's camera sees, as text: each actor's share of the frame, screen box, brightness and distance, plus the background share and the frame's brightness.
- **`inspect.capture_passes`**: aligned id (PNG with a legend), depth (EXR, cm) and normal (PNG) passes of the view, each with a summary.
- **`inspect.check_mesh`**: whether a static mesh is closed, with its holes, seams, inside-out and degenerate faces, parts, area and volume.
- **`inspect.raycast_world`**: up to 256 line traces against collision, each with its hit point, normal, distance, actor and component.
- **`manage_asset` `export_mesh`**: a static or skeletal mesh (or an animation) to glb, gltf, fbx or obj, without the exporters' dialogs.
- **`process_asset` `mesh_collision`**: box, sphere, capsule, complex or no collision on a static mesh asset and its placed copies.
- **`manage_geometry` `split_mesh`**: one static mesh per separate part or material slot, with `dryRun`.
- **Material Parameter Collections**: `create_material` `kind: parameter_collection` and the `CollectionParameter` node.
- **`system_control` `list_save_games` / `edit_save_game`**: read, edit, create and delete SaveGame slots.
- **`play_sound` `measure`**: plays a sound and answers how loud it was (`peakDb`, `averageDb`, `silent`, a wave's `wavePeakDb`).
- **`get_transform` `motion` `points`**: bone, socket or component positions per sample with ground heights and planted and slip stats; samples carry rotation.
- **`validate_assets` `dataValidation`** runs the project's and engine's validators; **`run_build` `tests`** answers each test's result.
- **`manage_tools` `get_task_result`**: reads a call that answered "still running"; the stdio server also answers so at 27 s.
- **`get_stats` `worldTickMs`**: the game world's own tick time while Play In Editor runs.
- **`add_function` overrides a parent function** such as `OnKeyDown`; **`add_variable` `exposeOnSpawn`**.
- **`set_property` `objectPaths`** writes one value on many objects, and `watch` samples the written property by default; **`call_actor_function` `componentName`** calls a component's function.
- **A Blueprint compile warns** about a sound played at a location with no attenuation.
- **`query_asset` `text`** finds graph nodes by their title.
- **Widgets**: `add_widget_tree` `material` paints an Image or Border; `add_animation_keyframe` `keys` writes a whole animation in one call.
- **Audio**: `create_metasound` `oneShot: false` makes a continuous source; distance attenuation sets air absorption (`lpf*`) and reads back what it holds.
- **Sequences**: `add_keyframe` `keys`, `set_section`, key `interpolation` and `lookAt`, `remove_keyframe` `frames`, particle keys (`particleKey`) and Niagara `moduleEnabled`.
- **Movie renders** capture gameplay (`gameModeOverride: level`, a `ui` pass, `audioOutput`), take `renderTimeoutMs`, and `create_render_job` `control: remove` clears jobs.
- **`advance_simulation`** `reset` previews an effect at an exact age, and the reply gives its `age` and `complete`.

</details>

<details>
<summary><b>🔄 Changed</b></summary>

- **Lean replies on both transports**: no log-only fields or repeated values; batches list only steps with something to say; a screenshot's bytes travel once over stdio.
- **`describe` states where a call runs**: the calls that test a running game list `pie` and `simulate`; Niagara `activate`, `deactivate` and `reset` work in the level editor too.
- **Vector arrays coerce only when complete and numeric**: two numbers no longer become `{x, y}` with a default `z`; rotation arrays read `[pitch, yaw, roll]`.
- **A missing parameter names the keys sent in its place**, and a reply names what ran when a call used another name.

</details>

<details>
<summary><b>🛡️ Security</b></summary>

- **MCP SDK 1.32.1 and proxy-addr 2.0.8** clear GHSA-6qxp-vccf-f47h and GHSA-jqcg-44mw-7w3h.

</details>

<details>
<summary><b>🔧 Fixed</b></summary>

- **Blueprint graphs**: batches call custom events they create, check expressions before any step and name an unknown edit; math expressions name what is wrong and the pins they made.
- **Pins and nodes**: `connect_pins` grows Sequence, Make Array and Switch on Int to the named pin, matches names without spaces and warns on a second Target object; `create_node` makes FlipFlop, DoOnce, DoN, Gate, typed `GetAllActorsOfClass` and native Break/Make nodes.
- **Blueprint assets**: an unknown parent class is refused; class defaults reach placed actors and derived Blueprints; a removed root promotes its children; SCS adds say where a component went and what it set.
- **Properties**: writes go through component setters, fail when put back, reach subobject owners and say whether they saved; `markDirty: false` stays clean; paths reach components by name and any mount; bools answer without `b`; struct reads keep zero fields.
- **Actors in a running game**: `delete` and `delete_by_tag` remove them, `call_actor_function` reaches the GameInstance, role names work in every lookup, spawns keep their mesh, and `spawn_batch` variables land before BeginPlay.
- **Placement**: a moved actor is solid where it stands; checks skip triggers, bodiless parts and invisible volumes and treat water as a surface; `fix_coplanar` parts a flush row in one pass; collision changes hold on placed copies; floaters float.
- **Lists and reads**: `list` reads struct members and `near` skips debug and editor-only bounds; multi-property reads keep dotted paths apart; the mesh view follows a placed actor; a missing asset reads as not found.
- **Widgets**: create by `widgetPath`, a tree root fills its canvas, a removed widget frees its names, new animations play from the graph at once, a key on an occupied frame replaces it, and previews show what the game shows.
- **Input**: simulated keys reach a focused menu button and release on time while paused; widget input warns when a minimized editor holds widgets still.
- **Materials**: enum settings take the Details panel's names and are checked before anything is made; Custom node pins wire like any node's; compile errors come after the shaders compile; batch steps take `nodeKind`; `set_material_parameter` says whether it saved.
- **Niagara**: static switches read and set, standalone ribbons draw, renderer and mesh materials get their usage flags, objects support undo, bindings link the input, linked inputs show their source, parameter writes answer success, and `autoActivate: false` stays off.
- **Audio**: MetaSound edits are heard at once and batches check classes and pins first and save once; placed sounds play in game; attenuation shapes take the radius on every axis; `measure` reads a wave's own level.
- **Sequences**: keys land as authored (sections span the range, auto tangents, Visibility tracks, transform defaults from the actor); `play` and `pause` seek with `startTime`; skeletal tracks bind the root mesh; spawnables make their own template; full-path create and duplicate save.
- **Renders and retargets**: a hung render frees the queue; `add_render_pass` names what it added; retargets keep pelvis motion and `create_ik_rig` builds from a mesh.
- **Captures**: screenshots after a camera move, full-window captures and captures right after an edit show the current state.
- **Editor**: console commands reach the running game; reflected calls run in the editor world; a restored window stays up; `step_frame` names its speed; paused motion runs end at once and `startWhen` reads component properties; `read_log` `livecoding` reads this editor.
- **Search and receipts**: text search finds assets picked on pins and keeps to its paths; graph lookup names the reader for non-Blueprint assets; Enhanced Input, foliage, folder moves, legacy mappings and deletes name what they changed; `save_all` names what it left.
- **Environment**: water bodies come out complete with varied waves; `create_sdf` names a too-thin shape; spline reads give both point spaces; `create_primitive` warns about a taken label.
- **Timing**: Fab library and media waits count real time; an accepted file delete counts as done; a refused call's text summary no longer reads `success: true`.
- **Builds**: the plugin compiles again on UE 5.0-5.4, on Fab's MSVC and clang toolchains, and without the Geometry Script plugin.

</details>

---

## 🏷️ [0.6.0-beta-c] - 2026-10-02

> [!NOTE]
> **Beta.** Published as a semver prerelease (`0.6.0-beta-c`) under the npm `beta` dist-tag, so `npm install` keeps serving the newest stable release, **`0.5.30`**. This section is everything on `dev` since the `v0.6.0-beta-b` tag, written from the code diff.

> [!IMPORTANT]
> ### 🔍 The editor checks its own work, and long calls stay alive
> Spawns, Blueprint component edits and mesh conversions measure where the result landed and name what is sunk, buried, floating or z-fighting; `sample_motion` plays and times a game over game time without the OS cursor; and a call that runs past the client's time limit answers that it is still running instead of failing. New one-call authoring covers smooth SDF shapes, polygon cages, text textures, whole widget layouts, palettes of material instances and several properties at once; Epic's 5.8 editor toolsets run behind the gateway's scopes and consent, and Fab imports run in the background. Contract changes are listed under **⚠️ Migration**.

<details>
<summary><b>✨ Added</b></summary>

- **`server/discover` answers on both transports.** The MCP 2026-07-28 discovery method (missing, it failed six conformance checks in #614) returns the protocol versions `initialize` negotiates, the same capabilities, instructions and server identity, and an hour-long public cache hint, with no session or handshake. The 2026-07-28 revision itself is not served, so a client on it falls back to `initialize`.
- **Epic's 5.8 editor toolsets run through the gateway, behind its scopes and consent.** `control_editor.list_editor_toolsets` lists the tools of Epic's Toolset Registry (StateTree, PCG, GAS, Niagara, UMG, MVVM and more, as their plugins are enabled), each with an effect judged from its name, or returns one tool's input schema (`toolName`). `call_editor_tool` runs one and answers with its output; a tool named for deleting or resetting is refused with `DESTRUCTIVE_EDITOR_TOOL` and runs through `call_editor_tool_destructive` (destructive scope, elevated consent), and a tool that would run scripts or console code is never called (`EDITOR_TOOL_BLOCKED`), nor is any tool of `ConfigSettingsToolset`, which writes any settings section, this bridge's own included (project settings stay on `system_control`). Before 5.8 they answer `EDITOR_TOOLSETS_UNAVAILABLE`.
- **`inspect.raycast_mesh` finds a mesh's surface.** Up to 256 rays at a static mesh asset in its own local space answer with the point each one meets, the surface normal (facing back along the ray), the material slot and the rotation a decal needs to print upright there, from the source triangles (full detail on a Nanite mesh), with no placed actor or collision. A part, print or decal can be put exactly on a curved surface.
- **Text becomes a texture.** `create_texture` with `kind: "text"` (`create_text_texture`) draws words with any Font asset and face (`fontFamily`, `typeface`, default Roboto Bold), `letterSpacing`, `justification` and `padding`, at the largest font size whose lines (letter spacing included) fit the texture, in `color` over `backgroundColor` (default transparent, so the alpha channel is the text mask). The reply lists the font's faces and the pixel box the letters cover; a face the font lacks is refused with the faces it has.
- **A palette of material instances takes one consent.** `create_material_instance` `instances` (up to 64 `{name, parentMaterial, savePath, parameters, save}`) makes each one as a single call would, with the call's own `parentMaterial`, `savePath` and `save` as every entry's defaults; the reply lists each under `instances` with `created`, and the call fails naming any that were not made. Seven instances of one parent took seven describes and seven consents. Retuning them is one consent too: `set_material_parameter` `assets` (up to 64 `{assetPath, parameters}` or `{assetPath, parameterName, value}`) writes each as a single call would and the receipt lists every instance it changed.
- **A whole widget layout is one call.** `add_panel_widget` `widgetKind: widget_tree` builds a nested `tree` of panels, borders, text, images and bars with their looks (size, face, letter spacing, outline and shadow on text; rounded corners and an outline on a border) and their slots, refuses an unknown field instead of ignoring it, and lists every widget it made. A HUD cost an add call per widget and a style call per look.
- **Every material slot of an actor in one call.** `set_material` `materials` puts entry i into slot i (`""` leaves a slot alone) in one undo step: an SDF part with six slots took six calls.
- **Several properties in one call.** `set_property` `properties` (`{name or dotted path: value}`) writes each as a single call would and reports them under `properties`; `get_property` `propertyNames` reads several, answering `properties` and naming any that do not resolve under `missingProperties`.
- **`spawn` takes a mesh path alone.** The handler placed a mesh actor for a bare `meshPath`, but the schema refused the call without a class.
- **A server and plugin from different releases say so, and name the fix.** The plugin reports its version in the bridge handshake; when it is not the server's (or a plugin too old to report one), the server logs a warning and every failed `execute` carries `versionMismatch`: both versions, the release page of the matching plugin, and the `npx -y unreal-engine-mcp-server@<plugin version>` pin that matches the server to the plugin instead.
- **`unreal-engine-mcp-server proxy` keeps a native session across editor restarts.** A stdio server in front of the plugin's `/mcp` (`UNREAL_MCP_URL`, default `http://127.0.0.1:3000/mcp`): while the editor is down every call answers `NOT_CONNECTED`, and the first call after it is back opens a new upstream session and runs, so the client never reconnects. It sends the capability token the stdio server would find, relays progress notifications as they stream, and caches the editor's tool list and instructions (`UNREAL_MCP_PROXY_CACHE`) for sessions that start before the editor.
- **`control_actor.sample_motion` watches an actor over game time in PIE.** It returns location, velocity and `propertyNames` (`"Component.Property"` ok) every `intervalSeconds` for `durationSeconds` (max 30 s, 400 samples) with `start`, `end`, `min`, `max` and `endedBecause`; the editor world gives `NOT_SIMULATING`. `inputs` (up to 32 `{key, atSeconds, holdSeconds}`) press keys at game-time offsets, `startWhen` holds the run until another actor's property takes a value, `maxRealSeconds` (default 25, max 50) caps wall time, and a start that never fires, keys that did not move the pawn or a slow frame rate warn. A minimized editor is shown without focus for the run (`windowRestored`) with the background CPU throttle off, then minimized again. Also `get_transform`'s `motion` read.
- **`control_actor.list` finds actors by place and kind.** `near` (`[x, y, z]` or `{x, y, z}`) sorts nearest first, then smaller bounds, and adds each row's `distance` to the actor's bounds; `radius` keeps those within it. `tag`, `className` (class or any parent, by name or path, `_C` optional) and `folder` (that folder and below, `"(none)"` for the root) filter, and every filter combines with `summary` and with each other; `propertyNames` reads `"Component.Property"`, unmatched names under `missingProperties`.
- **`set_transform` moves by an offset.** `offset` `[dx, dy, dz]`, also per item in `actors`, moves relative to the current location; with `location` it is refused with `INVALID_ARGUMENT`.
- **`control_actor.rename` renames an actor.** It sets the label (`INVALID_NAME` if refused) and, with `renameObject`, the object name too (made valid and unique); the reply has `oldLabel`, `label`, `oldObjectName`, `objectName` and `actorPath`.
- **`audit_placement` finds z-fighting and `fix_coplanar` fixes it.** `audit_placement` reports `coplanarFaces` per finding (kind `coplanar`, `byKind.coplanar`, read from each mesh's LOD0 triangles, also for actors tagged `mcp.placement.ok`) and takes `kinds` to limit problem kinds. `fix_coplanar` moves each face `distance` (0.1-20, default 1) in up to 4 passes as one undo step (`dryRun` is one pass); parts of one Blueprint are fixed in the Blueprint itself (compiled and saved, every placed copy fixed; `blueprintsFixed`), and `skipped` holds only the pairs left alone, with a reason.
- **`componentNames` narrows a component listing** on `control_actor` `get_components`, `inspect` `get_components` and `inspect_cdo`: it keeps the named components (case-insensitive) and lists misses under `missingComponents`. A mesh component's row names its `staticMesh` and `materials` on a world actor too, not only in a Blueprint.
- **Every spawn says where the actor ended up.** `spawn`, `spawn_blueprint` and each `spawn_batch` row report what `set_transform` reports: `placementWarning` (sunk into the surface under it, floating, intersecting other actors or nothing below), `groundZ`, `groundClearance`, `overlappingActors` and `suggestedLocation`, where a mesh whose pivot is its bounds centre spawned half under the floor behind a bare success (`spawn_blueprint` never checked, and `spawn_batch` dropped what each item's spawn found). A batch measures each item after its tags, so `mcp.placement.ok` silences a deliberate placement; `placementWarnings` counts the flagged items (a warning is not a failure) and `report: "failures"` keeps them. `suggestedLocation` is the actor's own location raised by the depth it is sunk, whatever its pivot (it was the bounds centre, which moved a corner-pivot mesh sideways).
- **Blueprint parts that sink into each other are named.** Every `edit_scs` edit that adds, moves, re-meshes, re-parents or sets a property on a component measures the mesh parts it touched against the rest of the actor, on their real triangles in a preview instance (the level is untouched), and returns `partWarnings` with a `warnings` sentence each: `kind` `buried` (inside `otherComponent`, with `depth` in cm and `insideShare`) or `sunk` (below a Character's capsule bottom). `convert_to_static_mesh` and `convert_to_nanite` onto an existing path check the loaded Blueprints that draw the replaced mesh the same way, and `audit_placement` with `blueprintPath` lists every part of a Blueprint (`minSeverity` is the tolerance, default 0.5 cm; the level filters `kinds`, `nameFilter` and `maxTilt` are refused there with `INVALID_ARGUMENT`). A component tagged `mcp.placement.ok` is a deliberate embed and is left out; `mcp.placement.ok:<component>` accepts it against that one part only.
- **`spawn_batch` names the actors it could not label.** `unnamedActors` lists the unique actor name of each item without `actorName`, in batch order (`""` for a failed one), under any report mode, and that item's `results[].name` is the unique name instead of the mesh label.
- **The running game's objects have names.** During PIE, `GameInstance`, `GameMode`, `GameState`, `PlayerController`, `PlayerPawn`, `PlayerState` and `HUD` (any case) work as `objectPath` for `inspect` `get_property`/`set_property` and as `actorName` in every `control_actor` action that takes one (not `GameInstance`, which is no actor); a real actor of that name wins. A live widget goes by the name `simulate_input` `widget_list` reports (`WBP_HUD_C_0` or `WBP_HUD`), so its widgets read and write as property paths (`CoinBox.RenderTransform`).
- **Class lookups take a Blueprint's short name.** `BP_Door` and `BP_Door_C` resolve to the Blueprint of that name (through the `/Game` asset registry when unloaded, also during play) for `find_by_class`, `spawn`, SCS, graph-node and widget creators and about 30 other actions.
- **`control_actor` `find` asks which actors use an asset.** `findBy: mesh` (`meshPath`) lists the actors with a static mesh component, an instanced or foliage one too, that draws the mesh, and `findBy: material` (`materialPath`) the actors with a component using the material in any slot, an override or the mesh's own, looking through a dynamic instance made from it. Each row names the matching components (`instances` for an instanced one, `slots` for a material), `limit` (default 200, at most 1000) cuts a long list with `truncated` and `totalCount`, and a path that loads nothing is `MESH_NOT_FOUND` or `MATERIAL_NOT_FOUND`.
- **`set_anim_graph_node_value` finds the node it was handed and stores what it was given.** `nodeName` takes the node's object name (`AnimGraphNode_SequencePlayer_0`) or its GUID (`nodeId` too, as `add_state` and `create_node` report it), looks in the AnimGraph and in the state machines and states under it, lists the nodes of a miss (`NODE_NOT_FOUND`) and the candidates of a title two nodes share (`AMBIGUOUS_NODE`; it took the first). A Sequence Player's `Sequence` (a Blend Space Player's `BlendSpace`) takes the animation's asset path through the node's own setter and fails when the node did not take it, and a setting of the node struct such as `PlayRate` is written where the node keeps it and read back in `value`; both answered success and stored nothing. `inspect_graph` `info: "node"` on an animation node now shows what it plays (`animationAsset`, `animationAssetClass`) and its editable `settings` (play rate, loop, start position ...), which its pins never showed; `graphName` is the AnimGraph or the state whose graph holds the node.
- **`get_foliage_instances` counts by type.** `byType` (`[{foliageType, count}]`) and a total `count`; `summary` drops the instance list, `limit` caps it with `truncated`. A level with no foliage actor gives `count: 0` without `byType`. With one type, each instance's `scale` is `{x, y, z}` (it was the X axis alone as a number, so a non-uniform scale such as a tall thin tree read as uniform).
- **`get_scs` finds every Blueprint with a kind of component.** A folder `path` plus `componentClass` (name, `/Script` path or parent such as `PrimitiveComponent`) scans up to 300 Blueprints (`scanned`, `blueprintCount`, `truncated`) for matching components, owned or inherited. `componentClass` also narrows one Blueprint's `components`/`inheritedComponents`, which now include parent Blueprints' components with their `ownerClass`, not only the native parent's defaults.
- **`manage_blueprint` `get` reads a component's default.** `propertyName` `Component.Property` reads it from the SCS template or native parent (`CharacterMovement.JumpZVelocity`) instead of `PROPERTY_NOT_FOUND`, and a name that still misses lists the Blueprint's variables.
- **`delete_node` removes many nodes at once.** `nodeIds` (1-500) are resolved first (`NODE_NOT_FOUND` lists misses, `PROTECTED_NODE` a function's entry or result node), then removed in one transaction and save; the reply has `removed` and `removedCount`.
- **`edit_graph` `arrange_nodes` tidies a graph.** The listed `nodeIds` move beside the nodes they are wired to (right of what runs them, below-left of what reads them), each to the nearest free slot; the nodes left out stay put and anchor them, and a listed node with nothing to anchor it stays where it was (`unmoved`). The reply lists each move under `moved`.
- **`set_property` can watch what a write sets off.** `watch` ({`propertyName`, `objectPath`, `durationSeconds`, `intervalSeconds`}) samples a property right after the write, in the same call, and the reply lists each value it took with its time: a HUD pop or fade runs on real time and is over before a later call could read it. The watched object passes the same reflection boundary as `get_property`, and a watch that does not resolve is refused before anything is written.
- **A read-only pin takes a literal.** `set_pin_default_value` and `build_graph` `pinDefaults` on a const-reference pin create (or reuse) a `MakeLiteral` node wired into the pin and return `literalNodeId` instead of `PIN_REQUIRES_CONNECTION`. Text, string, name, bool, int, int64, byte (not enum) and real work; struct, enum and container pins still refuse, and deleting the target also removes its literals.
- **`build_graph` declares functions, events and dispatchers.** A batch takes `add_function` (`functionName`, `inputs`, `outputs`, `pure`, `isPublic`; `$id` is the entry node, `$id_return` the return node; reply `nodeGuid`, `resultNodeGuid`), `add_event` and `add_event_dispatcher` (`dispatcherName`, `parameters [{name, type}]`), so members and graph go in one call; their names count as declared for the pre-check, and a function or event added earlier in the batch is found before any compile. `add_function` returns its GUIDs also for a function that already exists, and a member step lists its node's pins as a `create_node` step does (an event's parameters included); an `add_event` step that names no position takes a free slot like a created node instead of the graph origin. `create_node` also builds `CallDelegate` (`memberName`: a dispatcher), `Message` (`memberClass`: an interface, `memberName`: its function) and `AsyncTask` (`memberClass`: the async-action class, `memberName`: its static factory; event graphs only).
- **`add_event` binds a component's or widget's event.** `componentName` with `eventName` finds inherited components, ones added since the last compile and, in a Widget Blueprint, widget variables (`{componentName: "CreditsButton", eventName: "OnClicked"}`); a non-assignable delegate gives `DELEGATE_NOT_ASSIGNABLE`, a miss lists known components, and `nodeGuid` finds the node.
- **`inspect_graph` reads what an event does in one call.** `info: "node"` with `followExec` (1-50) adds `chain`: every node the exec wires leaving the node reach, depth-first (a Branch's then side before its else), each with `via` (the exec output that led there) and its inputs as values or `<- Title.Pin` sources.
- **`rename_variable` renames components too.** The SCS node and its graph getters are renamed together (the reply says `kind` component or variable), and `newName` is checked by the editor's name validator (`NAME_CONFLICT`) for both; the release answered `NOT_FOUND` for a component.
- **`describe` says which variants read each parameter.** A parameter read by only some variants of a folded family carries `variants`, on both doors' full and single-parameter describe (199 families; `add_content_widget` has 22). A path a handler reads under either spelling is declared by every variant (`assetPath`/`materialPath` on the material info reads and `compile_material`, `assetPath`/`systemPath`/`system` on `get_niagara_info`, `assetPath`/`systemPath` on the nine `edit_niagara_system` and the 21 `add_niagara_module` variants, `behaviorTreePath` beside `assetPath` on the `edit_behavior_tree` graph route), so no call is told it ignored the one it used, one of the two is what such a variant requires, and a bare `systemPath` on `get_niagara_info` reads the system instead of running `validate`. `get_tree` already declared both spellings on its one tree variant.
- **A box child has a size rule.** `set_widget_layout` `set_size` on a HorizontalBox, VerticalBox or ScrollBox child takes `sizeRule` (`Auto` or `Fill`, any case) and `fillValue` (>= 0; alone means Fill), read back by `get_widget_info`/`get_widget_slot_info`; canvas children keep `size {x, y}` and refuse them.
- **`set_widget_layout` `style` sets sounds, justification and fonts.** A Button takes `hoverSoundPath` and `pressSoundPath` (empty clears), text-layout widgets `justification` (`left`, `center`, `right`); other classes give `STYLE_FIELD_UNSUPPORTED`. A TextBlock also takes `fontFamily`, `typeface` (refused with the list of faces the font has), `letterSpacing` and `copyStyleFrom` (font, colour and shadow from another TextBlock).
- **`reparent_widget` places the widget.** `index` (0 = first; the same parent reorders) sets the position, every shared slot property (padding, alignment, size, canvas layout) is kept, and a parent inside the widget's own subtree gives `INVALID_PARENT`. The reply returns `index` and the slot layout; a move no longer warns of a phantom duplicate.
- **`duplicate_widget` copies a widget with its whole subtree.** Copies are named `<name>_Copy` (unique) unless `newName` names the top one, and go after the original, last under `newParent`, or at `index`. The reply has `slotName`, `newParent`, `index` and `copiedWidgets`.
- **`preview_widget` returns a picture.** It draws the Widget Blueprint offscreen in design mode (`resolution` `WxH`, default 1280x720, project DPI scale) and returns `imageBase64` (PNG) with `width`, `height` and `sizeBytes`; the editor opens only with `openEditor: true`. It needs only the `Read` scope.
- **`get_summary` says whether the level is saved.** Both readers (a level by path and `current_level`) add `unsaved` (also true for a dirty loaded World Partition actor package), `unsavedPackages` (first 100, levels first) and `unsavedPackageCount`; an unloaded level gets none.
- **`save_all` can save just what you changed.** `assetPaths` saves only those packages and reports the rest as `leftDirtyCount`; omitted or empty still saves everything.
- **`restart_editor` can close without relaunching.** `relaunch: false` ends PIE and closes under the same unsaved-package check; `discardUnsaved` marks dirty packages clean first so no save prompt opens. `validateOnly` reports the close case too.
- **`move` and `rename` accept a folder.** As `sourcePath`, a folder moves every asset under it with sub-folders, fixes redirectors, deletes the emptied source and reopens an open level under it at the new path (`PIE_ACTIVE` and `UNSAVED_CHANGES` refuse).
- **Deleting a folder is quick and says what it left.** `delete` removes a folder's assets in one engine pass (a 391-asset import took minutes with the editor frozen, past the client's timeout), and an asset it could not remove, unsaved ones included, is listed in `failedToDeletePaths` instead of counted as deleted.
- **`exists` and `bulk_rename` take lists.** `exists` `assetPaths` answers `existsByPath` (path to bool) and `exists` (all present); `bulk_rename` `renames` (`[{sourcePath, newName}]`) renames each asset in one call, with missing sources or taken names under `skipped`.
- **`maintain_content` `refresh_blueprints` refreshes many Blueprints.** It refreshes every node of each Blueprint under `folderPath` or `assetPaths`, then compiles and saves each; the reply has `refreshedCount`, `savedCount` and `compileErrors` (`COMPILE_FAILED` on any).
- **`query_asset` searches text.** `lookup: "text"` (`find_text`) finds a string in Blueprint pin literals and comments (with `nodeId`), variable and component defaults, widget texts, DataTable rows, String Table entries and, with `includeLevel` (default true), the open level's actors; `caseSensitive` matches case. A reference to another asset matches by its path, so searching a sound's name finds the component that plays it. `search` and `by_tag` say how many assets they found ("No assets matched." instead of "Assets found.").
- **`inspect_asset` lists what uses an asset.** `lookup: "dependencies"` with `referencers: true` returns the using packages under `referencers`. The dependencies list is sorted, and an object path (`Pkg.Asset`) is reduced to its package for both directions.
- **A screenshot can start from a camera position and skip the disk.** `control_editor` `screenshot` in `editor_viewport` mode takes `location` and `rotation` to move the viewport camera first (while PIE runs, the ejected view; `PIE_VIEW_NOT_EJECTED` until the player is ejected), `keepFile: false` returns the image without writing `Saved/Screenshots`, and `window` takes the list index as a number. A reply counts the shader jobs still compiling (`shadersCompiling`, with a warning above 0), `waitForShaders: true` waits for them first (at most 25 s; `shaderWait`), and the `inspect` editor-state reads carry the same count.
- **`configure_editor` `window` can minimize the editor.** `minimize: true` puts the main frame away without taking focus and turns the background CPU throttle back on (saved; `unthrottle` is ignored); the reply has `wasMinimized`, `minimized` (read back from the window), `restored` and `throttleOff`. A restore's `unthrottle` now lasts the editor session instead of being saved, so a later launch no longer runs flat out in the background. An editor launched with `-McpStartMinimized` puts itself away the same way as soon as its main window is on screen.
- **`system_control` lists and deletes its output files.** `list_output_files` pages newest first (`root`, `extension`, `limit` default 100 max 500, `offset`) with `total`, `returned`, `hasMore` and `nextOffset`; `delete_output_file` takes `path` or `paths`, answers per-path `results` (`PARTIAL_DELETE`) and names every deleted path in the receipt. Both stay inside `Saved/Screenshots`, `Saved/unreal-mcp`, `tmp/unreal-mcp` and `temp/unreal-mcp`.
- **`read_log` `filter` takes alternatives.** It splits on `|` and matches a line (message or category; for build, livecoding and previous sources also the file tail) containing any alternative, case-insensitively; plain text, not a regex. `source: "previous"` counts only the editor's own runs: the Fab browser's logs were counted too, so a `runsBack` could land on a browser log.
- **`create_nav_modifier` can spawn a volume.** Without `blueprintPath` it spawns a NavModifierVolume (`actorName`, `location`, `extent`, `areaClass`); with one it adds the component. `create_nav_modifier_component` takes `actorName` to add an instance component to a placed actor.
- **MetaSound graphs can be read and pruned (UE 5.5+).** `get_metasound_graph` returns nodes (`nodeId`, `name`, `className`, `kind`, inputs with literals, outputs), edges by pin name, `graphInputs` and `graphOutputs`. `edit_metasound` `remove_metasound_node` (`nodeId`/`nodeIds`, all-or-nothing; graph inputs and outputs refused) and `disconnect_metasound_nodes` (by target input, source output or exact pair) also run as `build_metasound` steps (`$id` ok in `nodeIds`).
- **`manage_audio` `stop_sound` stops, pauses and resumes sounds.** `play_sound` spawns a tracked component that `stop_sound` stops (only `soundPath`'s sound when given); `all: true` stops every audio device of the editor and game. The reply has `stopped` and `allStopped`. `soundOp` `pause` or `resume` pauses or resumes the same sounds instead (`all: true` covers every playing sound, game music included; `paused`, `resumed`).
- **Cutscenes can have music.** `add_track` with `trackType: "Audio"` and then `add_section` with `soundPath` puts a sound on the sequence, sized to the sound unless `end` is given; a missing sound is `ASSET_NOT_FOUND` and `soundPath` on another track `INVALID_ARGUMENT`.
- **Fab imports run in the background and can be followed or cancelled.** `import_marketplace_asset` `fab_listing` replies when Fab accepts the download (`operationId`, format, quality tier, file, size) instead of waiting minutes for the import; `query_marketplace` `fab_import_status` reports `phase` (queued, resolving, downloading, importing, cancelling, done, failed), `downloadPercent` from Fab's own notification, then `importedRoot`, `sampleAssetPaths` (meshes first) and `saved`, or `failureCode` with the lines Fab logged; a failure Fab logs after some assets have landed lets the import settle, then reports `IMPORT_PARTIAL` naming it. One import runs at a time and later adds queue behind it (8 at most, then `QUEUE_FULL`); a repeat add of a listing still importing gets that import back, or `ADD_ALREADY_RUNNING` when it asks for a different `quality`, `combineMeshes`, `destinationPath` or `assetName`. `fab_cancel` drops a queued add, presses Cancel on a pack download or cancels an Interchange import (a source-format download is `NOT_CANCELLABLE`, which `cancellable` says beforehand). The packages an import creates are saved once Interchange is done with them and again 15 and 60 seconds later; `saved`, `savedCount` and `unsavedPackages` say how it went.
- **The Fab add takes `combineMeshes`, `quality`, `destinationPath` and `assetName`.** `combineMeshes: false` imports each mesh of a file as its own asset, and a single mesh file of 50 MB or more is refused with `LARGE_SCENE_FILE` until it is chosen. `quality` (`raw`, `high`, `mid`, `low`) takes exactly that Megascans tier (`QUALITY_NOT_AVAILABLE` names the tiers offered). `destinationPath` and `assetName` move and name what the import created (`SM_<name>`, `MI_<name>`, `T_<name>_*`) through the rename `asset.move` uses, and never touch `/Game/Fab/Materials`, `MaterialFunctions` or `Textures`; a taken name stops it before anything moves. A complete project is refused (`COMPLETE_PROJECT`), as is a MetaHuman listing (`METAHUMAN_FORMAT`).
- **`fab_listing_details` says what adding the listing would do.** `canAddToProject` is false, with `addBlockedCode` and `addBlockedReason`, for a complete project, a MetaHuman listing and a listing with no importable format. `addFormat`, `versionName`, `engineVersions`, `supportsRunningEngine`, `engineMatch`, `downloadFile`, `qualities`, `downloadBytes` (left out, with `downloadSizeKnown` false, when Fab publishes no size), `formats` and `addWarnings` describe the download; `category`, `averageRating`, `ratingCount`, `licenseNames`, `isCc0`, `price`, `currency`, `isFree` and `publishedAt` describe the listing. `attributionRequired` says a Creative Commons Attribution license needs a credit line, and `isCc0` (in search rows too) is read from the license names: Fab sets its own CC0 flag on CC-BY licenses, so a pack that needs credit read as needing none.
- **`fab_search` rows carry the publisher, category, rating, price and formats,** and `seller` and `listingType` narrow the search.
- **`fab_library` syncs an empty table itself** (`syncTriggered`, `syncWaitedSeconds`) instead of asking for a console command.
- **`process_asset` sets the materials in a mesh asset's slots.** `process: "mesh_materials"` (`set_mesh_materials`) takes `assetPath` and `materials` (`[{slot, materialPath}]`, `slot` a 0-based index or a slot name) for a static or skeletal mesh, applies the valid entries in one rebuild and saves the mesh (`save` defaults to true; engine content is never saved, `saveSkippedReason` says so). The reply lists every slot with the material it holds and whether this call `changed` it, and each refused entry under `refused` (index out of range, unknown slot name, a material that does not load, never swapped for a default material); any refusal fails the call (`MATERIAL_SLOTS_PARTIAL`, or `MATERIAL_SLOTS_REFUSED` when none applied) while the valid entries stay applied.
- **`inspect_object` with `objectKind: "mesh"` says where each material slot's geometry is.** Every `materialSlots` row carries `triangles` and `sections` (what LOD0 draws with the slot) and `bounds` (`origin`, `extent`, `min`, `max`, `size`, `radius`, in mesh space) for a static mesh, and for a skeletal mesh in its reference pose, so a slot can be matched to a part of the mesh in one call. When the render data keeps no CPU copy of the vertices, `slotBoundsAvailable` is false and `bounds` is left out. A Nanite mesh reports its fallback mesh.
- **`create_primitive` `primitive: "sdf"` models smooth organic shapes in one call.** `shapes` (1-64 spheres, ellipsoids, boxes, capsules, cylinders, tori or cones, each with `center`, `rotation`, its sizes, an `operation` of union, subtract or intersect, and a `blend`: the radius of the soft fillet where it meets the shapes before it) become one signed distance field, meshed at `resolution` (16-256). Each shape's surface is its own polygroup carrying its `materialId` (listed under `parts`), cut cleanly along each fillet where ownership changes, so a carved visor or a painted band takes its own material slot. `mirror` adds a shape's reflections and `repeat` lays copies in a row or a ring (at most 1024 shapes in all). A subtract as the first shape is refused, shapes that leave no volume answer `SDF_EMPTY`, a mesh over 500,000 triangles answers `POLYGON_LIMIT_EXCEEDED` (lower the resolution), and shapes spanning more than 100,000 units are refused (`INVALID_ARGUMENT`).
- **`edit_dynamic_mesh` `append_polygons` builds a polygon cage in one call.** `vertices` (`{x, y, z}` points, at most 20000) and `faces` (three or more corner indices each, at most 20000; a larger or concave face is ear clipped) become a manifold surface, each face its own polygroup unless `faceGroups` says otherwise, with `faceMaterials` ids. A bad index, a repeated corner, an edge shared by three faces or two faces wound the same way along an edge is `INVALID_POLYGONS` naming the face, as are faces that come to more than 500,000 triangles (append them in several calls), and the mesh is left as it was. The reply has `verticesAdded`, `facesAdded`, `trianglesAdded`, `vertexCount`, `triangleCount` and `groupCount`.
- **`subdivide` takes a `scheme`.** `catmull_clark`, `loop` and `bilinear` join `pn` (the default, as before); `iterations` is the level, 1-6. `catmull_clark` and `bilinear` read every polygroup as one face of a cage (an `append_polygons` cage, or `create_box`, which has one polygroup per face), keep polygroup and material ids through the subdivision, and refuse a mesh with no usable cage as `INVALID_CAGE` with how to build one; `loop` refines the triangles and needs no cage. The reply adds `scheme`, `level`, `trianglesBefore`, `trianglesAfter` and `cageFaces`. These three schemes need the MeshModelingToolset plugin that GeometryScripting enables (`SUBDIVISION_UNAVAILABLE` without it).
- **The face operators take a `region`.** `extrude`, `inset`, `outset`, `offset_faces`, `bevel`, `chamfer` and `poke` accept `region` instead of `triangleIndices` (both is refused): a `box` the triangle centroids lie in, a `normal` with `normalAngle` (default 30), `groupIds` and `materialIds`, ANDed, in the mesh's local space. A region that matches nothing is `REGION_EMPTY` with the mesh's local bounds, and each reply says `trianglesSelected`.
- **`edit_dynamic_mesh` `set_material_id` sets material slots.** `materialId` (0-255) goes on every triangle, on `triangleIndices`, or on a `region`; the reply lists `materialIdsInUse`.
- **`convert_to_static_mesh` and `convert_to_nanite` take `materials` and `collision`.** `materials` is one asset path per material id (entry i is slot i, `""` keeps the default; a path that does not load as a material, or a list longer than the slots, is `INVALID_MATERIALS` before anything is created), and the asset gets the highest id + 1 slots. `collision` is `box` (the default, as before), `complex` (the render triangles) or `none`. The reply lists `slots` and `collision`.
- **`optimize_mesh` `morphology` offsets a mesh through a voxel grid.** `operation` `dilate`, `contract`, `close` (the fillet where unioned parts meet) or `open`, `distance` in cm (default 2% of the longest side) and `voxelCount` (grid cells along the longest side, default 128, 16-256). The mesh is kept when the result is empty or over the triangle cap; UVs, material ids, polygroups and vertex colours are rebuilt, and the reply says so.
- **`edit_dynamic_mesh` `bake_vertex_colors` bakes masks for materials.** R is ambient occlusion from `aoRays` (default 32, 8-256) hemisphere rays up to `aoDistance` (default 15% of the bounds diagonal), G is 1 minus the convex-edge mask, B is 1 minus the concave-cavity mask (both scaled by `curvatureScale`, default 1), A is height up the mesh's local Z, with `blurIterations` (default 1) neighbour averaging. White means no effect for every channel but height, and the values reach the static mesh unchanged, so a material's VertexColor node reads what was baked (UE 5.1 and later).

</details>

<details>
<summary><b>🔄 Changed</b></summary>

- **HUD pieces from `add_game_widget` are real, styled widgets.** `health_bar`, `ammo_counter`, `crosshair`, `minimap`, `compass`, `damage_indicator`, `interaction_prompt`, `objective_tracker` and `quest_tracker` honour `percent`, colour, text, size, `texturePath`, `keyLabel`, `items`, `maxVisibleObjectives` and `positionX`/`positionY`/`sizeX`/`sizeY`, give an empty tree a `RootCanvas`, list what they created under `widgets`, and refuse a taken `slotName` (`SLOT_EXISTS`) or missing parent (`PARENT_NOT_FOUND`); the damage indicator adds a `<slot>_Flash` animation.
- **`create_widget_template` builds ten screens as new Widget Blueprints** (`main_menu`, `pause_menu`, `settings_menu`, `loading_screen`, `hud`, `dialog`, `inventory`, `radial_menu`, `credits`, `shop`) named `name` in `path`; an existing asset gives `ALREADY_EXISTS`, a bad setting is refused before anything is created, and the reply lists every Button for `bind_widget` `on_clicked`.
- **The stdio door runs capabilities exactly like the native door.** `execute` forwards `{action, ...params}` to the record's parent tool and the plugin routes it; the TypeScript action layer is gone and only `manage_tools` runs in process. Both accept only declared parameters and their declared aliases (a spelling only that layer converted, such as `path` for `blueprint.create`'s `savePath`, is refused with the declared name), and parameters that layer dropped now arrive. The plugin has one dispatch table: each parent tool routes its sub-actions explicitly, with no fallback that offered an unmatched action to every handler.
- **Gateway refusals say what to do next.** `UNKNOWN_ACTION` for an action owned by one other tool names it with a `nextCall` to its describe (or, when no suggestion shares the verb, a search on the action's words); a conflicting folded selector answers `INVALID_PARAMETER_VALUE` naming both values, with an executable `nextCall`; an undeclared parameter never suggests `action` and says when a capability declares none; a parameter only another variant of a folded family reads is named in the receipt's `warnings` (`pinName is read only when info is pins; this info=node call did not use it.`). A cancelled stdio `execute` answers `MCP_REQUEST_CANCELLED`, not `TOOL_EXECUTION_FAILED` or `TOOL_TIMEOUT`.
- **Capability guidance is complete.** Folded families keep every variant's `whenToUse` and `whenNotToUse` lines, not just the first six (labelled `control=play: ...` when only some variants hold them, bare when all do; search ignores the labels; `struct.edit_struct` lists 13 of each), and drop `whenNotToUse` lines that send a capability to itself. 54 more asset, material, inventory, texture, DataTable, enum, interaction and struct records carry authored lines instead of filler.
- **Reader capabilities say what they return.** Summaries of `get_audio_info`, `get_texture_info`, `get_character_info`, `get_ai_info`, `get_level_structure_info`, `list_plugins`, `read_log` and other readers name their reply fields and topics ("sound length", "pixel format", "walk speed"); `get_mesh_info` has no material info, `analyze_trace` no timing counters, `get_stats` no draw calls.
- **Record text matches the handlers.**
  - `get_sessions_info` reads local player count, PIE state, split-screen setting and layout, and each PIE instance's net mode and URL (`pieInstances`), and drops the online fields (`currentSessionName`, `isLANMatch`, `maxPlayers`, `voiceChatEnabled`, `isHosting`, `connectedServerAddress`, `activeVoiceChannels`); `get_game_framework_info` reads the game mode and its default classes.
  - `list_plugins` `filter` is case-insensitive; `get_volumes_info` `filter` matches the actor label and `volumeType` the class name; `asset.list` `recursive` defaults to true and the list family says when to use `assets`, `content_sources` and `material_instances`; `get_input_info` lists every key mapping of a context.
  - `setup_ik` creates a Control Rig Blueprint for a skeleton (`name`, `skeletonPath` required; folder in `path`); `set_section_timing` moves a section's start.
  - `control_editor.play` names the values of `control`; `close_asset` points to `restart_editor`; `set_widget_layout` `alignment` and `sizeRule` name box, overlay and scroll-box slots; `variableType` lists every `add_variable` type; `get_dependencies` returns direct packages only.
  - `manage_audio` and `manage_networking` guidance reads "Use it to ..." (104 lines said "Use when ...") and 32 empty "Do not use when ..." lines are gone; widget guidance reads as English.
  - The session instructions open with "The only tool is `unreal`; it reaches the whole editor" and quote no capability count.
- **Search finds the capability a plain request names.** On both doors a record whose action names, family, domain or topics cover every word of the request ranks ahead of one that matches fewer (summary prose does not count); a leading read word (get, list, show, what, which, where) prefers readers, a leading delete prefers destructive ones, a topic that holds the whole request wins ("tag an asset" is `set_metadata`), and folded families keep every member's topics. Nearly 300 everyday requests across every domain ("list sounds", "copy button", "add fog", "make a cutscene", "add a checkbox", "stream in a sublevel", "mute all sounds") now rank their capability first.
- **Screenshots return the picture by default.** `control_editor` and `system_control` screenshots return the image inline (`returnBase64` true), fitted to 1600x900 without `resolution`; `returnBase64: false` keeps the full-size file only. A resampled `game_viewport` or `full_editor_window` image reports its size in `viewportWidth`/`viewportHeight`.
- **More edits save by default.** `save` defaults to true (`save: false` keeps the change in memory) for `modify_scs` (which now declares `compile`, applies a `location`, `rotation`, `scale`, `meshPath` or `materialPath` given at the top level or on an operation (an `add_component` given only a mesh or material used to keep neither), and names one it cannot apply in `warnings`), Game Framework, DataTable and struct writes, `manage_asset` `import`, `set_two_sided`, material `delete_node`, Niagara graph edits, `create_metasound`, enum edits (`create_enum` included) and the `manage_inventory` edits; a `parameters` list on `set_material_parameter` or `create_material_instance` passes the call's `save` to every entry (each saved the asset whatever the call said). GAS ability and effect edits always compile and save (`COMPILE_FAILED`, `SAVE_FAILED`).
- **Level and volume edits report whether the level is saved.** Streaming, World Partition, data layer, level blueprint and volume edits save only with `save: true` (a level save also writes its other unsaved changes), answering `saved: true` or `SAVE_FAILED` with the edit kept in memory; otherwise `saved: false` and the reply says the level is not saved yet. `configure_hlod_layer` saves its layer unless `save: false`.
- **Setting a game mode class no longer changes the project's game mode.** `set_default_pawn_class`, `set_player_controller_class`, `set_game_state_class`, `set_player_state_class` and `set_hud_class` change only the named game mode and refuse a class the slot cannot take; `makeDefault: true` also writes `GlobalDefaultGameMode` to DefaultEngine.ini (`CONFIG_WRITE_FAILED`); the release made the game mode the project default and the open level's GameMode Override on every call. Replies report `madeDefault`, `openLevelGameMode` and `effectiveInOpenLevel`, and say when the open level runs another game mode.
- **Scalar property writes are strict.** Integer, byte and int64 properties refuse non-integral and out-of-range values instead of truncating, and enums refuse hidden and `_MAX` entries (display names work).
- **`create_blend_space` creates a 2D blend space** (Direction -180..180 x Speed 0..600); `create_blend_space_1d` makes one axis.
- **Actions that echoed their input or refused now do the work.**
  - Skin weights: `copy_weights` (nearest vertex of another skinned mesh, bones mapped by name or to the nearest parent), `mirror_weights` (across `axis`, swapping left and right bones by name; `direction`), `prune_weights` (influences below `threshold`) and `import_morph_targets` (nearest vertex) rewrite the mesh's source data and reply with per-bone vertex counts; a rebuild that would drop render-only morph targets is refused with `RENDER_ONLY_MORPHS` unless `dropRenderOnlyMorphs: true`.
  - `set_retarget_chain_mapping` (`setup_ik` `kind: "chain_mapping"`) maps source to target chains (`None` clears, `autoMap` fills the rest) and reads them back; `create_pose_library` (`create_animation_asset` `kind: "pose_library"`) makes a Pose Asset from `sourceAnimationPath`, one pose per key frame (`poseNames`, `additive`).
  - `create_ao_from_mesh` bakes ambient occlusion with the engine baker in `uvChannel` (`rayDistance` 0 = unlimited; `minValue`, `meanValue`, `maxValue`; `AO_BAKE_FLAT`); `poke` splits triangles at their centroid (`distance` along the normal) and `quadrangulate` groups triangle pairs into quad polygroups; `revolve` turns the given `profile` of `{radius, height}` points (`angle`, `steps`, `cap`, `name`, `location`/`rotation`/`scale`) instead of the same small vase at the origin, and says so (`usedDefaultProfile`) when no profile is given.
  - `create_level_instance` assigns and loads the world asset (`levelAssetPath`; `loaded`, `childActorCount`); `create_packed_level_actor` runs the Packed Level Blueprint bake (UE 5.3+) and refuses a bake with no instances. Both take `label`, `location`, `rotation`, `blueprintPath` and `spawn`, not `levelInstanceName`, `instanceLocation`/`instanceRotation`/`instanceScale`, `packedLevelName`, `bPackBlueprints` or `bPackStaticMeshes`.
  - `set_doppler_effect` (`edit_sound_cue` `edit: "doppler"` by `assetPath`, no longer on `configure_sound_attenuation`) inserts or updates a Doppler node (`dopplerIntensity`, `smoothing`; `DOPPLER_NEEDS_SOUND_CUE`, `CUE_EMPTY`).
  - `manage_networking`: `configure_lan_play` writes `[URL] Port` to DefaultEngine.ini, `configure_session_interface` `DefaultPlatformService` (needs the online subsystem plugin), `configure_split_screen`/`set_split_screen_type` a layout's player counts and `configure_voice_settings` the voice cvars and VoIP settings (`micInputGain`, `noiseGateThreshold`, `silenceDetectionThreshold`, `sampleRate`), each read back; `join_lan_server` client-travels a PIE instance to `serverAddress`; `configure_player_start` tags a PlayerStart (`playerStartTag`, or `teamIndex` for TeamN; undoable); `add_network_prediction_data` adds a variable replicated only to the owning client.
- **`apply_baseline_settings` moves every scalability group** via `Scalability::SetQualityLevels` (performance 0/Low, balanced 2/High, quality 3/Epic) plus `r.VSync`, no longer touches `r.AllowHDR`, motion blur, depth of field, bloom, shadow or anisotropy, and adds `qualityLevel` to the reply.
- **Enhanced Input triggers and modifiers resolve by class.** `set_input_trigger` and `set_input_modifier` take any concrete trigger or modifier class (`InputTrigger`/`InputModifier` prefix optional) and keep an instance already there (`alreadyPresent`); names only the old list knew are refused (`INVALID_TRIGGER_TYPE`, `INVALID_MODIFIER_TYPE`) instead of becoming another class: `DoubleTap` (was Tap, or RepeatedTap on 5.6+), `SwizzleInputAxis` (the class is `SwizzleAxis`), `SmoothDelta` before 5.4 (was Smooth) and `ScaleByDeltaTime` before 5.1 (was Scalar).
- **Paths report themselves.** An invalid asset path is reported instead of resolving a same-named asset under `/Game`, `/Engine` or `/Script`; `create_landscape_grass_type` uses its `path` (the release hard-coded `/Game/Landscape`); MCP-created landscapes no longer carry `MCP_Landscape*` tags; `stream_level`/`unload` answer `STREAMING_LEVEL_NOT_FOUND` for a level the editor world does not stream.
- **Replies drop constant or duplicated fields.** Gone: `validationResult` (`validate_niagara_system`), `maxTailSize` (`write_snapshot`/`send_snapshot`), `navMeshPresent`/`bHasNavMesh` (navigation; `hasNavMesh` stays), and `commandExecuted`, `deletedViaFileFallback`, `editorDeletionSkippedForMap`, `deleteAssetFailed` (`delete_level`). An existing interactable answers `ASSET_ALREADY_EXISTS`.
- **`add_section` takes display-rate frames** for `start` and `end` (they were raw ticks), like the rest of `manage_sequence`; an empty or reversed range is `INVALID_ARGUMENT`, and the record declares `actorName` and `bindingId`.
- **`set_playback_speed` works without Sequencer open.** It sets the play rate of every level sequence actor playing the sequence plus the open Sequencer's speed (`speed`, `levelSequenceActors`, `sequencerUpdated`); neither existing is `PLAYBACK_TARGET_NOT_FOUND`, and a non-finite speed is refused.
- **`create_volume` refuses a parameter its class would ignore** (`sphereRadius` on a BlockingVolume, `damagePerSec` on a TriggerBox) with `INVALID_ARGUMENT` naming it, creating nothing. A `create_*` volume given `actorPath` attaches to that actor, like `add_*`.
- **`start_unreal_insights` opens the viewer by default** (`launchViewer: false` skips it), also when a trace is already running, and answers `VIEWER_LAUNCH_FAILED` if it cannot start; the record declares `channels`, `connectionType`, `traceFile`, `host`, `port` and `overwrite`.
- **Declared names win over legacy spellings in Blueprint edits.** Graph edits read `posX`/`posY` before `x`/`y` and `blueprintPath` before `assetPath`; SCS edits take the first non-empty of `blueprintPath` then `name`, and an SCS add its class from `componentClass`, then `componentType` (a payload naming only `componentType` was treated as an add and then answered "Component class not found"); `edit_scs` declares the class and attach parameters (`componentClass`, `componentType`, `attachTo`, and `parentComponent` for the adds) for every variant that reads them, so a top-level add through `edit=modify` no longer draws a receipt warning that the call "did not use" a parameter it used.
- **Widget layout and binding replies put their readback at the top.** Layout setters answer `applied`, `saved` and `appliedFields`; bind actions `slotName`, `property`, `functionName`, `generatedGetter`, `bindings` and `saved`.
- **Compensation hints point at live capabilities.** `describe` attaches seven inverse hints (spawn, duplicate, `create_light`, `create_landscape`, `open_asset`, `enable_tools`, `enable_category`) and a manual-cleanup note for `create_render_job`, all keyed by a live capability; the release's hints for `start_recording` and `start_session` pointed at stop capabilities that had been folded away.
- **A Fab pack is imported at the build for the running engine.** The add takes the highest build at or below the running engine, else the lowest above it (`engineMatch` says exact, older or newer), where it took the first one listed (the 5.0 pack on a 5.8 editor).
- **A Fab page that is still loading is waited for,** up to 15 seconds, instead of answering `PAGE_NAVIGATING`; a refused download names the step that refused (`NO_DOWNLOAD_URL`: license, claim or download-info) with Fab's status and words.
- **`create_light` says what it took.** `details.applied` lists the settings that went in, `details.refused` (`name: why`) those that did not (an unknown key, one that does not fit the light such as `outerConeAngle` on a point light, a wrong type, anything in `properties` on a sky light) and `details.adjusted` a value replaced for being out of range; before, they were dropped or only logged. The descriptions list every light type and `properties` key, a color is linear 0-1 stored as sRGB (`{1, 0.7, 0.42}` reads back as 255, 218, 173), and "add spot light intensity color" finds it on both doors. `manage_level.create_light` and `build_environment.create_light` run one handler; `manage_effect.create_dynamic_light` is the one that spawns into a running game.
- **`remesh_voxel` is a real voxel wrap.** It samples the mesh into a grid (`targetEdgeLength` is the cell, else `voxelCount` cells along the longest side, else `targetTriangleCount` sets it from the surface area, default 128 cells) and rebuilds one watertight, smooth-shaded surface through the engine's solidify; open shells are thickened first. It was a uniform remesh plus a hole fill. UVs, material ids, polygroups and vertex colours are rebuilt, and the mesh is kept when the result is empty or over the triangle cap.

</details>

<details>
<summary><b>🗑️ Removed</b></summary>

- **97 actions that only echoed their input, faked success or answered `NOT_SUPPORTED`** (the record count is unchanged by the removal):
  - `animation_physics` (1): `normalize_weights`
  - `manage_audio` (1): `enable_audio_analysis`
  - `manage_blueprint` (4): `create_game_screen` (its credits and shop screens are in `create_widget_template`), `create_property_binding`, `set_animation_loop`, `set_widget_binding` (`bind_widget` makes property bindings)
  - `manage_character` (12): `add_custom_movement_mode`, `configure_footstep_fx`, `configure_sprint`, `map_surface_to_sound`, `setup_character_ability`, `setup_climbing`, `setup_footstep_system`, `setup_grappling`, `setup_mantling`, `setup_sliding`, `setup_vaulting`, `setup_wall_running`
  - `manage_combat` (25): `apply_damage`, `configure_aim_down_sights`, `configure_combo_system`, `configure_damage_execution`, `configure_hit_reaction`, `configure_hitscan`, `configure_impact_effects`, `configure_muzzle_flash`, `configure_recoil_pattern`, `configure_shell_ejection`, `configure_spread_pattern`, `configure_tracer`, `configure_weapon_sockets`, `configure_weapon_trails`, `create_damage_effect`, `create_hit_pause`, `create_melee_trace`, `create_shield`, `heal`, `modify_armor`, `set_weapon_stats`, `setup_ammo_system`, `setup_parry_block_system`, `setup_reload_system`, `setup_weapon_switching`
  - `manage_effect` (1): `configure_event_payload`
  - `manage_gas` (9): `add_ability_task`, `add_tag_to_asset`, `configure_cue_trigger`, `configure_gameplay_cue`, `create_ability_set`, `grant_ability`, `set_ability_targeting`, `set_attribute_clamping`, `set_cue_effects`
  - `manage_geometry` (1): `triangulate`
  - `manage_interaction` (11): `add_destruction_component`, `add_interaction_events`, `configure_destruction`, `configure_destruction_damage`, `configure_destruction_effects`, `configure_destruction_levels`, `configure_interaction_widget`, `configure_trigger_events`, `configure_trigger_filter`, `configure_trigger_response`, `setup_destructible_mesh`
  - `manage_inventory` (20): `add_crafting_component`, `add_equipment_functions`, `add_inventory_functions`, `configure_equipment`, `configure_equipment_effects`, `configure_equipment_visuals`, `configure_inventory`, `configure_inventory_events`, `configure_inventory_slots`, `configure_inventory_weight`, `configure_loot_drop`, `configure_pickup`, `configure_pickup_effects`, `configure_pickup_interaction`, `configure_pickup_respawn`, `configure_station_recipes`, `create_equipment_component`, `create_inventory_component`, `create_pickup_actor`, `define_equipment_slots`
  - `manage_level_structure` (1): `configure_level_bounds`
  - `manage_networking` (11): `configure_local_session_settings`, `configure_net_serialization` (custom net serialization is a C++ `NetSerialize` on a USTRUCT, which no Blueprint setting reaches), `configure_push_to_talk`, `configure_round_system`, `configure_scoring_system`, `configure_spawn_system`, `configure_team_system`, `disable_input_action`, `set_voice_attenuation`, `set_voice_channel`, `setup_match_states`
- **`import_level` of a `.t3d` source** (the release ran `MAP IMPORTADD`) answers `NOT_SUPPORTED`.
- **MCP resources and capabilities nothing needed, on both doors.** `ue://level`, `ue://version` and the `ue://object/{objectPath}` template are gone; `ue://knowledge/{engineVersion}/{topic}` is `ue://knowledge/{topic}` and its reply no longer carries `engineVersion`, `ue://asset/{assetPath}` replies without `kind`, and completion no longer offers `engineVersion` or `objectPath`. The server no longer advertises `resources.subscribe` (resource update notifications) or `tasks` (`tasks/list`, `tasks/cancel`, task-augmented `tools/call`).
- **Prompt argument checks.** `prompts/get` no longer applies the per-kind path, identifier and engine-version checks; the stdio door still refuses a value over 512 characters or outside its `allowed` list, the native door neither.
- **Tooling nothing used.** The `test:native-parity`, `normalization:*`, `policy:*`, `migration:check`, `primitives:check`, `security:check`, `workflow:check` and `native:*` npm scripts, `.env.production`, and the `rimraf` and `js-yaml` dependencies; `headers:generate` and `headers:check` are new.
- **Settings and endpoints nothing read:** the environment variables `MCP_AUTOMATION_WS_PORTS`, `MCP_AUTOMATION_SERVER_LEGACY`, `MCP_AUTOMATION_CLIENT_MODE`, `MCP_AUTOMATION_MAX_AUTOMATION_REQUESTS_PER_MINUTE`, `MCP_ROUTE_STDOUT_LOGS` and `MCP_DEFAULT_CATEGORIES`; the `MCP_METRICS_PORT` Prometheus endpoint; the Project Settings `LogVerbosity`, `bApplyLogVerbosityToAll`, `bEnableSocketTelemetry` and `HeartbeatIntervalMs`, and the client-mode `EndpointUrl` and `ClientPort`; the plugin's WebSocket client mode and user-defined action aliases (`handler-aliases.json`); the Blueprint-callable `IsBridgeActive`, `GetBridgeState` and `SendRawMessage` and the `OnMessageReceived` delegate on the bridge subsystem; the `configSchema` and default `createServer` exports of the package entry (`dist/index.js`; the CLI still starts the server); and the raw-socket bare action names, which neither MCP door used.

</details>

<details>
<summary><b>🔧 Fixed</b></summary>

- **Volumes block and overlap.** A `BlockingVolume`, `TriggerVolume`, `KillZVolume`, `PainCausingVolume` or `PhysicsVolume` from `create_volume` (or resized by `set_volume_extent`, or an audio volume) had the right bounds and no collision, so a pawn walked through a blocking volume and a trigger never fired. The brush now gets its BSP and collision as the editor's own volume placement builds them.
- **A post-process call that names a missing volume gets one.** `configure_post_process` with an `actorName` that matched no volume failed `ACTOR_NOT_FOUND`, while the same call without a name used the level's unbound volume or made one; a name that matches nothing now resolves the same way, and a volume it creates takes that name.
- **`create_framework_class` takes a short parent class name** (`GameModeBase`, `BP_MyGameMode`), as documented; it answered `NOT_FOUND` for anything but a full path.
- **A Set node's value pin works as a link source.** `connect_pins` (and `build_graph`) given a Set node's variable-named pin as the source, which is the node's input, now links the node's matching output (`Output_Get`) instead of failing.
- **A math expression that does not parse is refused.** `set_node_property` `Expression` on a Math Expression node answered success while an expression it cannot parse (`-1`: there is no unary minus) left the node with no pins but the orphaned old output, which still counted as one; it now fails `EXPRESSION_INVALID` and the node keeps its previous expression and its links.
- **A palette's receipt lists the instances it made.** `create_material_instance` `instances` answered with an empty `changes`.
- **A misspelled function gets near names.** `VInterpToConstant` was answered with a bare "not found"; the suggestion now ignores underscores (`VInterpTo_Constant`), and without a `memberClass` it also searches the stock libraries the lookup itself tries.
- **A class path in the wrong folder names where the asset is.** `find` by class answers `CLASS_NOT_FOUND` with the `/Game` assets of that name, so `/Game/X/Gameplay/BP_Enemy` points to `/Game/X/Enemies/BP_Enemy`.
- **A function with two return nodes lists its outputs once.** `get_blueprint_details` listed every output once per return node.
- **`connect_pins` says which link it replaced.** A pin that holds one link (an exec output, a data input) dropped its old link without a word; the reply (and a batch step) now lists it under `replacedLinks`.
- **A class default write reaches the placed copies that kept it.** `set_property` on a Blueprint's class defaults or a component template also writes every copy placed in an editor level that still held the old default, as the details panel does (`instancesUpdated`); the compile had kept their old value as if they overrode it. A copy that overrides the value keeps its own.
- **`create` sets a component's members.** A dotted name in `properties` (`CharacterMovement.MaxWalkSpeed`, or a native component's object name, `CollisionCylinder.CapsuleRadius`) reaches the member of a component or struct on the class defaults; it failed as no such property.
- **A Blueprint asset path reaches its class defaults.** `get_property` and `set_property` given `/Game/X/BP_Foo` as `objectPath` looked the property up on the Blueprint asset itself ("not found in scope 'Blueprint'"); a property that is not the asset's own now goes to its class defaults, as `blueprintPath` does.
- **A write to a component of a Blueprint's class defaults is saved.** `set_property` on `CharacterMovement.PerchRadiusThreshold` of a `Default__` path compiled the change, then answered `saved: false` ("a running-game or transient object") with no game running and described the component the compile had discarded; it now saves the Blueprint and reads the value back off the new defaults.
- **`sample_motion` and `list` find a component through the actor property that holds it.** `propertyNames` `CharacterMovement.MaxWalkSpeed` came back under `missingProperties`, because only component names (`CharMoveComp`) matched; the names `get_property` takes work too.
- **The low-frame-rate warning names the real cause.** A slow `sample_motion` run blamed background throttling, which the run itself had switched off; it now points at the render cost (video memory) and at `fixed_delta_time` and `r.ScreenPercentage`.
- **A payload nested up to 24 levels deep is checked.** The pre-queue console-command and path scans stopped at 8 levels and refused anything deeper (`COMMAND_BLOCKED`), so a widget `tree` four panels deep could not be sent. The node budget (4096) and the refusal of a scan that runs out of it are unchanged.
- **A batch step that fails is reported once.** Its failure is in the batch's own result (`missingProperties`, the step's `error`); it also came back under `warnings` as an "Automation request failed" log line.
- **`fast-uri` 3.1.8.** The URI parser `ajv` validates with is past GHSA-hrr3-gc8f-f4qj (inconsistent host case normalization), so the production dependency audit is clean again.
- **A mesh converted over an existing asset keeps its collision.** `convert_to_static_mesh` and `convert_to_nanite` onto a path that already held a mesh left it with collision that never cooked, so pawns went straight through it. The physics data is rebuilt every time, and `collision: "box"` is now one box the size of the bounds. A converted mesh that cannot be loaded back fails with `ASSET_CREATION_FAILED`; it answered success with no material slots and no collision.
- **A generated texture that could not be saved says so.** `create_texture` (every kind) answered `saved: true` when all five save attempts failed, as long as an older version was still on disk. The first overwrite of an existing texture in an editor session was one such failure: the editor refused the new pixels ("bulkdata with an invalid payload"). The existing texture is now loaded and refilled, so that save goes through, and an asset of another type at the path is refused rather than replaced.
- **`set_material` fills every slot of an SDF or other dynamic mesh.** Its component starts with one slot however many material ids the triangles carry, so slot 1 and up answered `MATERIAL_SLOT_NOT_FOUND`; the ids the mesh uses now count as slots.
- **The plugin compiles on UE 5.0 and 5.1.** Sequencer code called track functions that exist only from 5.2; it now uses the right names on every version, and removing a sequence-level track works on 5.0 and 5.1 too.
- **WebSocket clients see why they were closed.** The bridge sends the close frame (4001-4005, 4008, 1002, 1009) with its reason, not a bare 1006.
- **Native `/mcp` no longer leaks memory per request.** A long-running editor stops accumulating a telemetry entry per request.
- **Automation cannot read or rewrite the bridge's own settings.** `get_project_settings` and `set_project_setting` refuse them (`SETTING_NOT_PERMITTED`); `set_preferences` skips them (`PREFERENCES_NOT_APPLIED`), so `bRequireCapabilityToken` cannot be turned off.
- **Replies stop redacting harmless text.** Slash-joined choice lists (`fill/left/center/right`) use commas, a `.uasset`/`.umap` host path becomes its package path, and a boolean under a secret-named key or a `TokensSaved`-style quantity reads back; a content mount registered in the editor (`/MoverExamples`, a plugin or Fab pack) stays readable while host paths and a root nothing mounted stay hidden. A host path ends at its file name, so the words after it are kept (the rest of the sentence up to the next comma was redacted with it).
- **A refused content path says why.** About a hundred actions answered "contains traversal sequences" (or only "Invalid assetPath") for any refusal. The reply now names the field, the path and the reason: a `..` segment, an absolute filesystem path, a root that is not mounted (named, with the mounted roots listed and the ones starting like it first) or a character a package name cannot carry. A mounted plugin or Fab-pack root is a folder like `/Game`: bare (`/MoverExamples`), slash-ended, as an object path, or shorter than four characters.
- **Receipt `dataDigest` covers the whole payload.** Payloads that differ only after a non-integer (motion samples, transforms) no longer hash alike on the native door.
- **Images get their own size budget.** `imageBase64` (up to 6 million characters) counts on top of the reply's 100k on both doors, for every capability; a larger image is refused.
- **Receipts name what a call changed.**
  - A reply that states `changedAssets`, even empty, is believed, so `preview_widget` and audio playback claim no asset change.
  - Widget replies list the Widget Blueprint once by package path; `set_style`/`set_clipping` carry `widgetPath`, `edit_graph` names its Blueprint, `manage_level` `save`/`save_as` set `savedAssetPath`, `modify_scs` sets `changedAssets`, `blueprintPath` is a receipt handle, `build_metasound`, a material graph batch, every material graph edit and `set_material_parameter` with `parameters` reply with `assetPath`, `compile` lists only the Blueprint; widget animation track and key replies name their Widget Blueprint, variable and Blueprint metadata writes name the Blueprint they saved, and a running-game object (`GameInstance`, a live widget) no longer lists `/Engine/Transient` as a changed asset.
- **Folded legacy names mean what they meant.** `stop_pie` pins `control=stop`, `single_frame_step` `control=step_frame`, `set_game_view_target` `cameraOp=view_target`, `create_blackboard_asset` `kind=blackboard`.
- **The gateway accepts parameters the handlers read.**
  - `create_light` `lightClass`/`properties`; `rename_level` `destinationPath` (it or `newName` is required); `set_light_channel` `enabled`/`channels`; `set_actor_light_channel` `enabled`; `configure_ray_traced_ao` `intensity`; `manage_level_structure` `create_level` `loadAfterCreate`; level-blueprint node actions `levelPath` (loaded sub-level).
  - Blueprint: `create_node` `pure`, `add_node` `targetClass`, `get_pin_details` `pinName`, `add_component` `location`/`rotation`/`scale`/`meshPath`/`materialPath`, `add_scs_component` `location`/`rotation`/`scale`/`properties`, `create` `blueprintType`, `create_blueprint` `properties`, `ensure_exists` `createIfMissing`.
  - Data layers: `assign_actor_to_data_layer` `actorName`/`actorPath`; `create_data_layer` `dataLayerAssetPath` and `bIsPrivate` (refused before UE 5.3).
  - Animation: `get_skeleton_info`, `list_bones`, `list_sockets` take `skeletonPath` or `skeletalMeshPath`; `list_physics_bodies` `physicsAssetPath` or `skeletalMeshPath`; `add_physics_body` `length`/`width`/`depth`/`height`/`rotation`; morph-target actions `lodIndex`.
- **A box slot takes a numeric alignment.** `set_widget_layout` `alignment` `{x: 0.5, y: 0.5}` works on HorizontalBox, VerticalBox, Overlay and Border children, snapping to Left/Top below 0.25, Center below 0.75, else Right/Bottom; canvas slots keep the numeric pivot.
- **Calls on the running game report what is true.** `set_property` on a PIE object answers `saved: false` with `saveSkippedReason` (it lasts until PIE stops); `control_actor` lookups answer at once when the actor is not in the PIE world (`worldName`); a missing object inside an existing package is null, and the not-found text lists the running-game names (`PlayerPawn`, `GameMode`, ...). `eject` leaves the pawn for a free camera and answers once the view has switched (it did nothing), `set_camera` and a screenshot's `location`/`rotation` move that ejected view and say so (`view`), and `game_viewport` photographs it; until the player is ejected they answer `PIE_VIEW_NOT_EJECTED`, naming the fix, instead of moving a hidden viewport and reporting success. `focus_actor` and `set_game_view` refuse the same way, while `set_view_mode` and `set_camera_fov` reach the running game's own view and camera (`method`). `possess` takes the pawn named by `actorName`, or without one brings an ejected player back to its pawn (`returnedFromEject`), and answers once the player is back in the game, or `POSSESS_FAILED` when the player controller ends up holding no pawn; it possessed nothing and said it had.
- **A write to a Blueprint's defaults sticks, and the reply says where it landed.** `set_property` on a Blueprint CDO (`blueprintPath`, or a `Default__` `objectPath`) wrote to the default object the compile it then ran replaced: a variable the Blueprint declares got its old value back, while the reply described a `REINST_` copy in `/Engine/Transient` (`saved: false`) and said success. The variable now keeps the value as its default, the reply is read back off the recompiled default object and the Blueprint is saved; a value the compile did not keep fails with `PROPERTY_SET_FAILED`, and a copy a compile left behind is refused with `STALE_TARGET`.
- **Asset lookups work during Play.** Every asset read goes through the asset registry, so playing a sound, resolving a Blueprint class, `exists`, `ensure_exists`, `probe_handle`, `query_asset` `exists` and `inspect_asset` dependencies and graph no longer report a real asset missing while the game runs; creating a Game Framework class twice answers "already exists" instead of crashing the editor; a working class lookup adds no engine warnings.
- **Blueprint graph edits.**
  - A refused `connect_pins` answers `CONNECTION_FAILED` with both pins' names, directions and categories, the schema's message and the source's output pins; `build_graph` connect steps use it. `GRAPH_NOT_FOUND` names the graph holding the event, else lists the graphs.
  - `get_node_details`, `get_pin_details`, `delete_node` and `set_node_property` accept `nodeGuid` beside `nodeId` (one is required), and a reply's `nodeId` is the full GUID; `set_node_property` reads `propertyValue` (`value` stays for `build_graph` steps) and refuses a call with none.
  - `inspect_graph` `filter` matches a node's title or name, or any pin's default, text or object path, ignoring case and spaces ("Spawn System" finds `SpawnSystemAtLocation`), and each node row carries its position (`x`, `y`).
  - A `build_graph` create step on a taken slot moves to the nearest free slot (the overlap guard's `suggestedPosition`, found in any direction; it walked right along a crowded row to the graph's far edge) or, failing that, the auto grid (it stopped after 6 tries), and a node placed without a position moves beside the node it is wired to once every step ran, a pure node beside the node that reads it, on that node's side when the spot is taken (it stayed on a grid right of the whole graph); a `MakeStruct` or `BreakStruct` node takes `structPath` and is refused without it (it came out pinless, as "Make <unknown struct>") and `placementWarning` reports a moved position; a step whose `pinDefaults` fail removes its node and literals and publishes no alias.
  - Reading a graph never compiles it; a single edit leaves the Blueprint saved after compiling, and a `build_graph` batch saves once.
  - `create_node` accepts `GetVariable`, `SetVariable` and `K2Node_` forms in any case, CallFunction reads `functionName` beside `memberName`, and `memberClass` (a function, variable or event node's) and a Cast's `targetClass` resolve `BP_X_C` and `/Game/.../BP_X`; a function that `memberClass` lacks is answered with the classes that declare it, component and actor classes included (`SetScalarParameterValueOnMaterials` asked of `PrimitiveComponent` came back a bare "not found": only function libraries were searched).
- **Blueprint variables, functions and events.**
  - `add_variable` refuses a name the parent class, a component, a function or an event already has (`VARIABLE_NAME_CONFLICT` names the inherited property) and a default that does not fit (`DEFAULT_NOT_APPLIED`); `get_blueprint` reports a bool default as a JSON boolean.
  - `add_function` sets access from `isPublic`; `ensure_exists` checks the place `name` plus `savePath` would create; `set_metadata` with `propertyName` writes to that variable, and without it a Blueprint with no generated class answers `BLUEPRINT_NOT_COMPILED`.
  - `add_event` and `remove_event` honour `graphName` (`GRAPH_NOT_FOUND`); `remove_event` takes `eventName`.
- **SCS edits reach placed actors.**
  - Collision edits on a component template (profile, enabled state, object type, responses) reach placed actors for `edit_scs`/`modify_scs` and `set_scs_property`; `edit_scs` `set_transform` and `set_property` update placed copies too.
  - Each change reports `instancesUpdated` (0 included) and `updatedInstances` (at most 3 paths); a mesh or material on an invisible or hidden-in-game component adds a hint to the summary and `warnings`.
  - `remove_scs_component` with `componentNames` removes them all, then compiles and saves once.
- **Property paths resolve the same way everywhere.** A bare name that is not a component property resolves to the one struct member at any depth that carries it (several is an error naming the candidates), in `get_component_property`, `set_component_property(ies)`, `add_component`, `set_scs_property`, `modify_scs` and `get`/`inspect` `get_property`/`set_property`; collision keys use the component's collision setters, and a `BodyInstance.*` write on a live component recreates physics state. A name that is not there lists up to 8 similar properties of that scope. `set_property` on an object inside a material or material function (a material expression such as a `TextureObjectParameter`) rebuilds that material (`materialRebuilt`), so a new `ParameterName` or default `Texture` shows on its material instances at once instead of after some other call recompiled it.
- **`add_component` and Blueprint `create` report what they could not apply.** `create` writes `properties` the way `set_property` does (a JSON object fills a struct) and lists `appliedProperties` and `failedProperties`, naming the misses in its message. `add_component` shares `set_component_properties`' property bag, warns when `meshPath` does not load or the component takes no mesh, and answers `PROPERTY_CONVERSION_FAILED` (nothing applied) or `PARTIAL_FAILURE` while keeping the component; an unapplied `Mobility` is reported too.
- **`convert_to_nanite` makes Nanite meshes, and `nanite_rebuild_mesh` sticks.** From UE 5.1 `convert_to_nanite` baked plain static meshes while replying `naniteEnabled: true`; it now turns Nanite on and reads the flag back from the mesh (`NANITE_NOT_ENABLED` if it is still off). `nanite_rebuild_mesh` rebuilt the mesh on 5.7+ only, never saved it, echoed its request and reset the mesh's position precision and fallback settings; it now changes only Nanite and `trianglePercent`, rebuilds and saves on every version (`SAVE_FAILED` otherwise) and replies with the mesh's own `naniteEnabled` and `trianglePercent`, the same over both routes.
- **Long calls say what Unreal is doing instead of failing.** A call still open after 27 s answers with `task.state` `running` while the work goes on, where it used to hit the client's time limit in silence. Through the stdio server only a call with an `idempotencyKey` and no `timeoutMs` answers that way, since resent with the same key once the work is done it answers with the real result; any other call waits for its result. On the editor's own MCP server the answer also names what the editor is working on and how far it has got (`queued` if it has not started, with `progress`); until then it hears every 8 s what the editor is doing, and thumbnail and LOD progress now reach it too. `EDITOR_BLOCKED` names the work holding the editor, its last progress and the shaders still compiling. `asset.delete` on a folder reports as it goes and removes unloaded assets that nothing outside the delete references straight from disk instead of loading them first (a 9,000-asset folder held the editor for over 15 minutes without a word).
- **Object and class references can be cleared, and a wrong one is refused.** Wherever a property value is written (`edit_scs` `properties`, `set_scs_property`, `set_property`, `set_component_property`, Blueprint `create` and the rest), `null`, `""` or `"None"` clear an object, class or soft reference (`null` and `"None"` were refused, so a child actor's class could not be emptied), and a path to an object of the wrong class is refused with the class the property holds, where it used to be stored as-is.
- **Actor calls tell the truth.** The placement check re-checks a `set_transform` batch against the finished layout, no longer takes what an actor holds up for its floor, confirms a box overlap on the collision shapes, and reports an actor held from the side as `mountedOn` instead of floating (`set_transform`, `spawn`, `audit_placement`); `audit_placement` rows list every actor they intersect (`overlappingActors`), and its `nameFilter` (and `fix_coplanar`'s) matches the object name it reports. `delete`, `delete_by_tag`, `spawn`, `spawn_blueprint`, `spawn_batch`, `set_material`, `set_visibility` and `set_actor_collision` are one undo step each with `undo` in the reply, and an incomplete `delete` answers `DELETE_PARTIAL` (or `NOT_FOUND`) with `deleted` and `missing`. `set_visibility` and `set_actor_collision` take `actorNames` as `add_tag` does, with misses under `missing` (`ACTOR_NOT_FOUND` when none matched) and changed actors under `affectedActors`, which the receipt lists as changes with actor handles, as it now does for the batch calls. `find_by_tag` walks the running game during PIE, `call_actor_function` runs Blueprint custom events on editor actors, a `set_blueprint_variables` whose every name misses answers `PROPERTY_NOT_FOUND` listing the actor's variables, `find_by_name` offers `similar` labels, any `ACTOR_NOT_FOUND` hands back that find as its `nextCall`, and replies name clashing labels by object name.
- **`control_editor.undo` and `redo` work.** They undo or redo the last transaction ("Undid: <title>") and answer `NOTHING_TO_UNDO`, `NOTHING_TO_REDO`, `UNDO_FAILED` or `REDO_FAILED`.
- **Fab opens its own tab.** Fab calls answered `FAB_NOT_READY` until someone opened Window > Fab: the auto-open never ran the menu item's action. It does now.
- **Saves and deletes never open a dialog.** Asset and level saves run unattended, and `bulk_delete` never shows the delete confirmation, so no prompt blocks the editor.
- **Widget edits.**
  - `bind_widget` writes real property bindings (text, color, enabled, visibility; `bind_percent` for Progress Bar, Slider, SpinBox) from `bindingSource`, a pure function or a variable (read through a generated `Get_<Widget>_<Property>` getter); refusals are `SIGNATURE_MISMATCH`, `SOURCE_NOT_FOUND`, `NOT_BINDABLE`, and `BINDING_INVALID` removes one that does not compile.
  - `bind_on_clicked` and `bind_on_value_changed` (function named by `bindingSource`) and `bind_on_hovered` (`onHoveredFunction`, `onUnhoveredFunction` for pointer-leave) hang a call to the function off the widget's event, creating it when missing (`BIND_FAILED` if the event already runs other nodes).
  - Widget adds compile the Widget Blueprint (except during PIE), so a new, copied or renamed widget is usable in a graph at once; `VARIABLE_NOT_FOUND` says whether Is Variable is off or the class is stale.
  - A widget name that would break the Blueprint is refused (`NAME_CONFLICT`, with a free-name suggestion); reusing the root's name no longer overflows the stack, and a refused add never removes a widget.
  - `add_widget_component` takes `slotName` and `parentSlot` (`PARENT_NOT_FOUND` if not a panel); `system_control` `add_widget_child` forwards to it, registers the widget, gives a lone leaf a `RootCanvas` and applies `name`, `text` and `parentName`.
  - Typed adds apply what they declare: `isEnabled`, `add_button`'s dark `<slotName>_Text` label, `add_image` `brushSize` (also on the ready-made screens; `ASSET_NOT_FOUND`), rich text `fontSize`, `inputType`, list/tree orientation, grid columns/rows, `wrap_box` `wrapWidth`, `positionX`/`positionY`/`sizeX`/`sizeY`, `maxDesiredWidth`/`maxDesiredHeight`, border `contentColorAndOpacity`.
  - `set_anchor`, `set_position`, `set_size` and `set_z_order` refuse a non-canvas slot (`INVALID_SLOT`, naming `set_alignment` or `set_padding`); `set_anchor` keeps the corner not given and refuses an unknown preset; `set_size`/`set_z_order` refuse an empty call; `set_padding` works on any margin slot and keeps unsent sides; `set_render_transform` starts from the current one; `set_visibility`/`set_clipping` refuse misspellings.
  - `set_style` saves the Widget Blueprint (`saved`, `saveSucceeded`, `appliedFields`), edits a TextBlock, a Button's child label or a RichTextBlock, declares `propertyName`/`value` for reflected properties (a dotted path such as `Font.OutlineSettings.OutlineSize` outlines text), and refuses an empty call or `outlineColor`/`outlineWidth` without `cornerRadius`; `set_font` keeps the size when only `font` is sent and refuses an unloadable font.
  - `add_animation_track` finds or creates the RenderOpacity, ColorAndOpacity or RenderTransform track for `trackType` (`trackCreated`, `bindingCreated`, `propertyName`, `saved`); `create_widget_blueprint` resolves `parentClass` by name or path and refuses a non-UserWidget (`INVALID_PARENT_CLASS`).
- **Character and interaction edits.**
  - Inventory: a loot entry or recipe ingredient added after a removal gets a new key instead of overwriting one; `set_item_icon` and `configure_item_stacking` fail with `PROPERTY_NOT_FOUND` when the item has no such property; `set_inventory_replication` keeps a RepNotify function; `get_inventory_info` declares the `lootEntryCount` it sends.
  - The `configure_*` character actions save after compiling (`SAVE_FAILED`); `configure_crouch`, `configure_capsule_component` and `configure_camera_component` write only fields sent and reply with the Blueprint's values (`NOT_A_CHARACTER` on the first two).
  - `set_walk_speed`, `set_jump_height`, `set_gravity_scale`, `set_ground_friction` and `set_braking_deceleration` require their value (`NOT_A_CHARACTER`); `configure_movement_speeds` declares the jump, air-control, gravity and friction fields.
  - `create_character_blueprint` derives from `parentClass` (a Character subclass, else `INVALID_PARENT_CLASS`) and answers `SAVE_FAILED` if it cannot save.
  - `configure_door_properties`, `configure_chest_properties` and `configure_switch_properties` write only fields sent; `create_door_actor` declares `locked`; `create_switch_actor` stores `SwitchType`.
- **Animation edits.**
  - `set_bone_parent` reads `parentBoneName` (`PARENT_NOT_FOUND`), `add_bone` before `parentBone`; `set_bone_key` reads `location`/`rotation`/`scale` arrays; `set_sequence_length` takes `numFrames` or `length` in seconds, keeps the frame rate and refuses under one frame.
  - `add_physics_constraint` refuses an existing pair (`CONSTRAINT_EXISTS`); `set_physics_constraint` edits (either order) or creates the constraint, changes only the axes named, and requires `bodyA` and `bodyB`.
  - `auto_skin_weights` recomputes LOD 0 weights with GeometryScripting on UE 5.5+, elsewhere `NOT_SUPPORTED`.
  - `create_montage` loads `animationPath` as the first slot segment, takes its skeleton when `skeletonPath` is omitted (`SKELETON_MISMATCH`) and reuses an existing montage; `create_animation_blueprint` derives from `parentClass` (`INVALID_PARENT_CLASS`) and takes the skeleton from `skeletalMeshPath` if `skeletonPath` is empty.
  - `add_layered_blend_per_bone` applies `layerSetup[].branchFilters` (`BONE_NOT_FOUND`, `INVALID_LAYER_SETUP`); `add_blend_node` builds TwoWayBlend, BlendListByBool, BlendListByInt or LayeredBoneBlend, else `UNKNOWN_BLEND_TYPE`; `set_interpolation_settings` maps `interpolationType` onto every axis (`INVALID_INTERPOLATION_TYPE`).
  - `add_blend_sample` places `sampleValue` `[x, y]`; `set_blend_in`/`set_blend_out` change only what is sent (`INVALID_BLEND_OPTION`); `set_additive_settings` refuses an unloadable base pose; `play_montage` applies its rate (`MONTAGE_PLAY_FAILED`); `configure_vehicle` accepts only `WheeledVehicle4W` or `WheeledVehicle`; `animationPath` is no longer copied into `assetPath` for the sample and slot actions.
  - The `animation_physics` records declare `save`, `positionX`/`positionY`, state-machine `states`/`transitions`, transition settings, blend-space axes, `groupName`, `numFrames`, `assetType`, `createIfMissing`, notify `time`/`trackIndex` and `create_control_rig` `skeletalMeshPath`/`modularRig`.
  - Skeleton, mesh, physics-asset, cloth, morph-target and skin-weight edits honour `save: false` (default true; `remove_socket`, `remove_physics_body`, `set_physics_asset` always save).
- **Levels.**
  - `manage_level` `save` with `savePath` saves as that path; `save`/`save_as` answer `LEVEL_NOT_LOADED` when `levelPath` is not the open level, `add_sublevel` `PARENT_LEVEL_NOT_LOADED`.
  - `get_summary` of a loaded level reads `assetClass` and tags from the map asset, and its `actorCount` (also `get_current_level`) counts the actors in it, not the empty slots a Blueprint recompile leaves (36 extra after one); `load` with `streaming: true` adds the level as a sublevel; `load` of the level that is already open reloads nothing and says so (`alreadyLoaded: true`, `reloaded: false`, and its `unsaved` state: its unsaved changes are kept, where it answered "Level loaded" and `dirtyWorldPackagesBeforeLoad: 0`), the dirty counts are read in every mode, and `saveDirtyPackages` saves in an interactive editor too (it was ignored there). Opening another level over unsaved level changes is refused with `DIRTY_PACKAGES`, which lists them in `unsavedPackages`: `load`, `create_level`, `control_editor` `open_level` and `manage_level_structure` `create_level` with `loadAfterCreate` dropped them without a word in an interactive editor. `load` takes `discardUnsaved` to drop them on purpose.
  - `create_level` takes `levelPath` (`savePath` is an alias; a last segment equal to `levelName` is the full path, else the parent folder), drops `template`, and `saveDirtyPackages` saves blocking dirty packages first (`DIRTY_PACKAGES`).
  - `.t3d` `export_level` exports the world `levelPath` names; `import_level` onto an existing destination without `overwrite` answers `DESTINATION_EXISTS`; `set_metadata` refuses a missing metadata object.
  - `create_sublevel` and `configure_level_streaming` apply `streamingMethod` (Blueprint or AlwaysLoaded, else `INVALID_ARGUMENT`); `configure_level_streaming` changes visibility, block-on-load and distance flags only when sent.
  - `enable_world_partition` applies `bUseExternalActors` (reply `usesExternalActors`); `bEnableWorldPartition: false` on a partitioned level is `NOT_SUPPORTED`.
  - `create_minimap_volume` reads `location` and `extent` (a TriggerBox `boxExtent`, else `extent`), post-process volumes `bEnabled`, an attached physics volume `priority`. `create_volume` declares `priority`, `killZHeight`, `postProcessSettings`; `set_volume_properties` applies `priority`, `blendWeight`, `bEnabled` on a PostProcessVolume and declares `bPainCausing`, `damagePerSec`, `reverbVolume`, `fadeTime`.
- **Data assets.**
  - `duplicate_struct` treats `destinationPath` as a folder when `destinationName` is given, it ends in `/` or it names a folder (the copy was named after the folder); `export_struct` member metadata survives `import_struct`; `create_enum` and `create_struct` report `saved` from the save itself; `get_metadata` no longer sends `debug_has_meta`.
  - `set_struct_as_row_struct` binds the struct to the table like `set_data_table_row_struct` (`dataTablePath`, `rowStructPath`, `migrateExistingRows`, `clearExisting`; reply `migratedCount`, `invalidRows`, `saved`).
  - DataTable errors name the missing field or unloadable path, `delete_data_table_row` answers `ROW_NOT_FOUND`, and row writes reply `saved` (`rowsCleared` for a clear).
  - Enum and instanced-struct failures come back as failures (`ASSET_NOT_FOUND`, `MISSING_PARAMETER`, `NOT_FOUND`, `DELETE_FAILED`); `split_enum` `index` copies every value from that position on.
  - `add_struct_member` applies `tooltip`/`metadata` and takes `members[]`; `import_struct` reads a JSON `sourcePath` and can create the struct; `create_struct` takes `structPath`, `rename_struct` `destinationFolder`/`newStructPath`, `delete_struct` `force`; `set_instanced_struct_property` honours `save: false`.
  - `asset` `delete` keeps an asset referenced from outside the delete unless `force: true`, listing it in `referencedPaths` (`ASSET_REFERENCED` when nothing was deleted), and a still-loaded asset whose file stayed on disk says so; `source_control_checkout`/`submit` read `paths`; `get_source_control_state` honours `recursive` and `assetPaths`; `search_assets` `classNames` takes any loaded short class name; `generate_lods` takes `lodCount` over `numLODs` and saves a dynamic mesh bake to `outputPath` (default `/Game/GeneratedMeshes/<actor>_LOD`, not `/Game/MCPTest`).
- **Renames keep the project working.** `rename`, `move` and `bulk_rename` re-point native class defaults (default map, game mode, input mapping contexts, per-map editor views), `MapsToCook` and, for a moved folder, `DirectoriesToAlwaysCook`, save the changed config, roll back on failure and name hard references under `blockingReferences`. `bulk_rename` answers `renamed: 0` with `notRenamed[]` (`BULK_RENAME_FAILED`), `rename` `DESTINATION_EXISTS`; `fixup_redirectors` and `bulk_delete`'s redirector pass no longer open the engine's report dialog.
- **Materials and textures.**
  - Texture edits, resize, `channel_pack`, `combine_textures` and `create_normal_from_height` read single-channel G8 textures such as `channel_extract` output (other formats fail naming the format), and `resize_texture` and `combine_textures` keep the source's sRGB setting.
  - `get_material_info` reads a material instance (`assetType`, `parent`, `baseMaterial`, `parameterOverrides`), declares `filter`, `nodeId`, `nodeIds`, and lists texture parameters beside the scalars and vectors (each with its default `texture` path and `samplerType`; a runtime virtual texture parameter its virtual texture), which it skipped; `find_node` matches a texture parameter by its parameter name.
  - `set_two_sided` honours `value: false`; `add_math_node`, `add_panner`, `add_rotator`, `add_noise` and `add_voronoi` apply their knobs and refuse one the node lacks (`add_noise` `octaves` is gone).
  - `find_node` reads `nodeName` and matches node ids; `delete_node` takes `nodeId` or `nodeIds`, lists `notFound` and fails `NOT_FOUND` when none match; `get_material_node_details` picks by `expressionIndex`; `build_material_graph` `add_scalar_parameter` accepts `name`; a Custom node's `additionalOutputs` rebuild its pins so they wire at once; `update_custom_expression` on its own compiles and saves the material and replies `compiled`, `compileErrors` and `saved` as `compile_material` does (it left the material unsaved and said nothing of a compile error); `disconnect_nodes` fails `PIN_NOT_FOUND` when no pin matched (it answered success) and unplugs a Custom node or function-call input by label; `connect_nodes` and `disconnect_nodes` take a Cloth material's pin names on the material output (`Cloth`, `FuzzColor`) and the engine's `CustomData0`/`CustomData1` (only the clear-coat names worked), `disconnect_nodes` reads `Base Color`-style spellings as `connect_nodes` does, and an unknown output pin lists every accepted one; a batch (`build_material_graph` or `add_material_node` `nodeKind: batch`) compiles the material once, at its end: its steps no longer recompile the half-built graph, so no step's compile warning (`Custom material X missing input 20 (OP)` before the wire feeding it) reaches `warnings` or `engineWarnings`, for a saved material as well as a new one; a step that fails compiles what was applied before it.
  - `create_decal_material`, `create_landscape_material` and `create_post_process_material` take `materialDomain`, `blendMode`, `shadingModel`, `twoSided`, `save`; `add_static_switch_parameter` declares `defaultValue`/`group`, `add_vector_parameter` `group`, `add_texture_sample` `parameterName`, `set_material_parameter` `texturePath`.
  - `create_noise_texture` reads `noiseType` (Perlin, FBM, Ridged, Billow; else an error) and the generators declare `outputPath` and their knobs; `set_streaming_priority` sets `neverStream` only.
- **Landscape and foliage.**
  - `create_landscape` builds the size it is given: `quadsPerSection` (or `sectionSize`) accepts only 7, 15, 31, 63, 127 or 255 with 1 or 2 sections.
  - `add_landscape_layer` takes the material in `materialPath`, puts the layer info in its folder (or `path`) and replies `layerInfoPath` and `saved`; a physical material that fails to load errors first.
  - Landscape calls take `landscapeActorPath` beside `landscapeName`/`landscapePath`; `sculpt` uses `tool`/`radius`/`falloff` and refuses tools other than Raise, Lower and Flatten; `paint_landscape(_layer)` paints the disc at `location` and `radius` (or the whole landscape) and keeps other weights.
  - `configure_landscape_lod` fails when nothing applies, `configure_landscape_material` declares only the material, and `create_procedural_terrain` labels itself `ProceduralTerrain` by default and errors on a material that does not load.
  - `add_foliage` with a foliage type or any of `locations`, `count`, `radius` scatters instances (a lone `meshPath` only makes the type); `add_foliage_instances` forwards `meshPath`; `remove_foliage` errors on nothing named, a missing type or a malformed `area`; `get_foliage_instances` refuses an unknown type (`FOLIAGE_TYPE_NOT_FOUND`).
  - `configure_foliage_*` fails on a mesh that does not load, reads `minScale`/`maxScale` independently, sets collision from `collisionEnabled`, answers `CONFIGURATION_FAILED` on a failed setting and names every property it wrote in `configuredProperties` (it listed only the `settings` entries, so a mesh change answered count 0 and an empty list); a mesh change adds `previousMesh`, `placedInstances` and `instancesShowingNewMesh` (the placed instances whose component now draws it); `create_procedural_foliage` labels its volume from `volumeName` and answers `RESIMULATION_FAILED` when nothing was placed.
- **Environment, lighting and rendering.**
  - With no name, the sky atmosphere, sky light, directional-light atmosphere, height fog, volumetric cloud, wind and sun-position calls edit the level's first actor of that class and spawn one only when none exists.
  - `cubemapPath` alone sets a sky light's source (`CONFIGURATION_FAILED` / `ASSET_NOT_FOUND` when it does not load), and `create_light` of type sky applies `intensity` and `color`.
  - `configure_sky_color_curve` and `configure_light_color_curve` treat `curvePath` as the curve asset, replacing its `keys` or creating it; water settings land on the body just created or found (`CONFIGURATION_FAILED`). Unread inputs (sky atmosphere `intensity`, cloud and rain `density`, `set_time_of_day` `propertyValue`, `configure_water_waves` `speed`) are no longer declared.
  - `configure_shadows` sets `sg.ShadowQuality`, `r.Shadow.DistanceScale`, `r.ContactShadows` and `r.RayTracing.Shadows` and errors when nothing is supplied; `set_exposure`, `set_ambient_occlusion` and `setup_global_illumination` apply what they report (`GI_METHOD_NOT_APPLIED`).
  - Single-value post-process variants write only the keys passed, lens variants answer `NO_SETTING_SUPPLIED` on an empty call, a `settings` object where no key is an engine field fails naming the closest fields (it answered success with every key under `unsupportedSettings`), `blendWeight` and `infiniteUnbound` are declared; reflection captures and planar reflection read `name` before `actorName`; `configure_capture_source` rejects an empty `captureSource`; `configure_indirect_lighting_cache` applies `enabled`.
  - `create_spline_actor` and the spline templates place `points` (`position` or `location`, tangents, rotation, scale) and read `name` before `actorName`; `add_spline_point` applies its tangents.
- **Sequencer.**
  - `play` applies `startTime` (seconds) and `loopMode` (`once` or `loop`; `EDITOR_NOT_OPEN` if no Sequencer opened); `currentFrame`, `playbackStart` and `playbackEnd` are display-rate frames.
  - Cinematic tracks take `startFrame`, `durationFrames`, `endFrame`, `rowIndex` and `bindingGuid`; particle and material tracks resolve their binding from `actorName` or `bindingGuid`; camera shake binds to the `cameraName` camera; `startFrame` is a material parameter key's time.
  - `add_shot_track`, `add_subsequence` and `configure_shot_settings` take the master sequence from `path`; `configure_shot_settings` finds the shot by `sectionIndex`, then `shotSequencePath`, then `name`, and changes only the start or length sent.
  - `add_camera` with `spawnable: true` creates a sequence-owned camera and replies `bindingGuid` (`SPAWNABLE_CREATION_FAILED`); `create_cine_camera_actor` declares the lens, filmback and focus fields; `configure_camera_settings` needs `actorName` or `cameraName`.
  - `list` searches its `path` (default `/Game`; outside the mounted roots is `INVALID_ARGUMENT`); Movie Render Queue jobs are picked by `jobId` or `renderJobName`, `create_render_job` declares the output settings, and `start_render` declares `onlyJob` and `timeoutMs` (max 300000).
  - Take Recorder applies `takePresetPath` (`TAKE_PRESET_NOT_FOUND`), validates `duration` (0-86400 s) and stops after the countdown plus `duration`; replay, media and metadata records declare `replayName`, `additionalOptions`, `key`, `value`; `create_media_sound_component` requires `actorName` and `mediaPlayerPath`.
- **Audio.**
  - `location`, `rotation` and `size` are `[x, y, z]` arrays, coerced by the gateway from `{x, y, z}` and `{pitch, yaw, roll}`; `play_sound_attached` attaches a new component to the named component or socket (`ATTACH_POINT_NOT_FOUND`), applies volume and pitch and plays it.
  - `create_audio_component` applies `volume` and `pitch`, activates unless `autoPlay: false` and answers `ACTOR_NOT_FOUND` for an unknown `actorName`; `create_ambient_sound` applies the attenuation and concurrency it loads; the fade actions read `fadeInTime`/`fadeOutTime` before `fadeTime`, and `fade_sound_out` uses `targetVolume`.
  - `create_reverb_zone` loads `reverbEffect` first, rejects a non-positive `size` and builds the box brush at half the size (default 500 per axis).
  - `set_class_parent` reads `parentClass` (`PARENT_NOT_FOUND`, `CYCLIC_PARENT`) and keeps `ChildClasses` in step, and `create_sound_class` applies it; `configure_occlusion`, `configure_spatialization` and `configure_distance_attenuation` read their declared names, refuse unknown values, and change the curve only when `distanceAlgorithm` is sent.
  - `create_submix_effect` builds a Reverb, EQ or Dynamics preset (`UNSUPPORTED_EFFECT_TYPE`); `create_dialogue_wave` fills its first context from `wavePath` and `speakerPath`; `create_dialogue_voice` refuses an unknown gender or plurality; `add_mix_modifier` writes the fade times; `configure_mix_eq` applies flat band fields; `create_source_effect_chain`, `add_source_effect`, `create_submix_effect` and the MetaSound edits honour `save` (default true); `set_metasound_default` reads `defaultValue` first.
  - An unloadable `wavePath`, attenuation, concurrency, effect preset, reverb effect, dialogue speaker or target voice is refused before anything is created; `set_audio_occlusion` without `soundPath` is `INVALID_ARGUMENT`.
  - `create_metasound` takes the `assetPath` the other MetaSound edits take (or `name` and `path`) and refuses a MetaSound already at that path with `ASSET_EXISTS`: it rebuilt a loaded one in place, and replaced one on disk at save.
  - `add_metasound_node` on an unknown class answers `NODE_CLASS_NOT_FOUND` with `candidateNodeClasses` and `acceptedNodeTypes`; `connect_metasound_nodes` `EDGE_FAILED` says which side is wrong (no pin, type mismatch, loop) and lists only the two nodes' pins under `sourceOutputs` and `targetInputs` (frontend V2 builds).
- **Effects and GAS.**
  - `activate_effect` finds the Niagara component by `assetPath` and the lifecycle actions read `actorName` before `systemName`; `set_niagara_parameter` with `assetPath` and no actor sets and saves the system's user-parameter default; `set_parameter_value` takes vector and colour arrays and a `parameters` list (`[{parameterName, parameterValue}]`, up to 200): every entry is written, the system saved once, each entry reported with `applied` or its `error`, and `PARAMETER_BATCH_INCOMPLETE` names any that failed (the others stay written); `parameters` beside a `parameterName` is refused.
  - Niagara authoring returns `TEMPLATE_NOT_FOUND` before creating a system, `NIAGARA_DI_CREATE_FAILED` when a data interface added nothing, `ASSET_NOT_FOUND` for a renderer material or mesh that does not load, and `UNSUPPORTED_PROPERTY` for any `set_emitter_properties` key but `enabled`.
  - `add_light_renderer_module` writes `RadiusScale` and `create_dynamic_light` `intensity` only when sent; `debug_shape` `color`/`boxSize` and `niagara`/`spawn_niagara` `scale` take a number, an array or `{x, y, z}`.
  - `set_activation_policy`, `set_instancing_policy`, `configure_asc` `replicationMode` and `set_effect_stacking` parse enums first and answer `INVALID_ARGUMENT` with nothing changed; an unregistered tag is `GAMEPLAY_TAG_NOT_REGISTERED`; `set_modifier_magnitude` writes SetByCaller magnitudes and refuses a negative `modifierIndex` and other types (`UNSUPPORTED_MAGNITUDE_TYPE`).
  - The GAS records declare the spellings the handlers read first (`activationPolicy` LocalOnly, LocalPredicted, ServerOnly or ServerInitiated, `instancingPolicy`, `modifierOperation`, `modifierMagnitude`, `cancelAbilitiesWithTag`, `blockAbilitiesWithTag`); `create_gameplay_effect` declares `duration` and `period`.
- **Editor and system.**
  - `full_editor_window` without a `window` selector always captures the main editor frame (a minimized frame is restored without focus and minimized again after the capture, else `EDITOR_WINDOW_MINIMIZED`; `mainWindow` says which was captured); every screenshot mode honours `path`.
  - `create_bookmark` and `jump_to_bookmark` drive the viewport's numbered slots (`id` required, else `INVALID_ARGUMENT`; an empty slot is `BOOKMARK_NOT_FOUND`); `set_editor_mode` answers `MODE_NOT_ACTIVATED` when the mode is not active.
  - `show_stats`/`hide_stats` toggle only when the stat is not already in the wanted state; `set_immersive_mode` fails with `NO_VIEWPORT` when nothing applied; `start_recording` needs PIE, verifies the demo driver and applies `frameRate`, `durationSeconds`; `close_asset` answers `EDITOR_NOT_OPEN` and reports `editorsClosed`.
  - `set_project_setting` saves a per-user settings class (such as `EditorPerProjectUserSettings`) to the user's config (`configFile`); a raw key with no settings property reaches DefaultEngine.ini and is read back; `get_project_settings` has category `packaging` and a keyed read returns only that key.
  - `inspect` `get_property`/`set_property` take `propertyPath` alone and name the missing target; `get_material_details`, `get_mesh_details` and `get_texture_details` return `TYPE_MISMATCH` for the wrong type; `find_by_class` without a class is `INVALID_ARGUMENT`.
  - `set_scalability` requires `level`; `start_profiling` stops after `duration`; `set_fullscreen` reads `windowed` as the inverse of `enabled`; `configure_occlusion_culling` applies `slop` and `minScreenRadius`.
  - `show_widget` honours `duration` (0.5-60 s) and only supports `notification`; `create_widget` refuses an unknown `widgetType` and an existing non-widget asset (`ASSET_TYPE_MISMATCH`); `validate_assets` loads every asset in a folder; `read_log` refuses `category`/`minVerbosity` for non-editor sources.
  - `capture_insights_trace` always writes a file and refuses a network `connectionType` (`INVALID_CONNECTION_TYPE`); `write_snapshot` resolves `snapshotPath` before the trace-file aliases.
  - `control_editor.play` replies once the play world has begun play (20 s cap, `PIE_START_TIMEOUT`) and names it in `pieWorld`.
  - The plugin builds on UE 5.8: the Fab adapter counts a module as present only when its `<Module>.Build.cs` exists, and PCG links normally on 5.8+ (UE 5.2-5.7 keep the delay-load).
- **Networking, input, AI, combat and geometry.**
  - `set_replication_condition`, `set_net_dormancy`, `set_net_role` and `set_inventory_replication` answer `INVALID_ARGUMENT` listing the valid names; `set_property_replicated` applies `condition`; `set_net_role` refuses `ROLE_Authority` and answers `NOT_SUPPORTED` for a non-Actor Blueprint; `set_owner` errors on an unmatched owner; `set_autonomous_proxy` and `configure_push_model` fail with no replicated variable; `configure_replication_graph` writes only fields sent; `create_rpc_function` accepts only Server, Client or NetMulticast.
  - `configure_client_prediction`, `configure_server_correction` and `configure_movement_prediction` refuse a non-Character Blueprint (`NOT_SUPPORTED`), and the last applies `networkSmoothingMode`; `configure_net_driver` writes the game net driver's class defaults and `DefaultEngine.ini` (`PERSIST_FAILED`); networking CDO edits refuse an uncompiled Blueprint.
  - `configure_replication` declares `isAutonomousProxy`, `spatiallyLoaded`, `netLoadOnClient` as booleans and `maxClientRate`, `maxInternetClientRate`, `netServerMaxTickRate` (like `configure_prediction`'s `networkMaxSmoothUpdateDistance` and `networkNoSmoothUpdateDistance`) as numbers.
  - `configure_game_rules` requires `bDelayedStart` (`NOT_SUPPORTED` for GameModeBase); `set_respawn_rules` refuses a non-AGameMode child; `configure_spectating` errors on an unloadable `spectatorClass`; `create_*` framework actions refuse an unloadable or unrelated `parentClass`; `create_game_mode` validates class overrides first; `get_game_framework_info` answers `NOT_FOUND` for a missing Blueprint.
  - `enable_input_mapping` answers `PIE_NOT_RUNNING` outside PIE and verifies the context was added; `mute_player` answers `NOT_SUPPORTED` when no voice system muted; `host_lan_server` prefixes `?` to `travelOptions` and hosts nothing without `executeTravel`; `add_mapping` checks trigger and modifier classes first.
  - `add_decorator` attaches to the composite or task `parentNodeId` names (`PARENT_NOT_FOUND`) and builds any concrete `BTDecorator_<Type>`; `configure_bt_node` sets a Blackboard key selector from a plain key name after checking the key and its type.
  - `configure_nav_area_cost` writes only fields sent, saves a Blueprint NavArea and declares `fixedAreaEnteringCost`; `set_ai_perception` applies `dominantSense`; `configure_slot_behavior` answers `GAMEPLAY_TAG_NOT_REGISTERED`; `create_behavior_tree` prefers `path` over `savePath`.
  - `setup_hitbox_component` attaches to `hitboxBoneName` under the Blueprint's skeletal mesh (`NO_SKELETAL_MESH`, `INVALID_ARGUMENT` for an unknown `hitboxType`) and keeps the size not given; `create_weapon_blueprint` and `configure_weapon_mesh` declare `weaponMeshPath` and refuse one that does not load; the other combat handlers answer `SAVE_FAILED` when a save fails.
  - `generate_collision` refuses an unknown `collisionType`; `simplify_collision` works on a copy so the render mesh keeps its triangles; `convert_to_static_mesh` saves the collision body it adds; modeling records declare the knobs their handlers read; rounded bevels are refused before UE 5.4.
  - PCG node edits declare `settings` and `save`, and an explicit `title` wins over the `nodeName` used to find the node.
- **Handlers that did the opposite of what they described now do it.** `activate_ragdoll` reaches its handler; `create_render_target` maps `RG8` to `PF_R8G8`; `add_state_tree_state` finds a parent at any depth; `inspect_struct` resolves a bare struct name; `inspect_class` returns the class's properties, defaults and ancestors (it failed `OUTPUT_SCHEMA_VIOLATION`); a widget animation key or track that is refused leaves no binding or empty track behind; `enable_gpu_simulation` applies the flags it reports; `create_animation_asset` refuses an existing asset of another class (`ASSET_TYPE_MISMATCH`); `set_loot_quality_tiers` stores the tiers it reports; `configure_grid_size` on a RuntimeHashSet world answers `INVALID_ARGUMENT` for a missing grid unless `createIfMissing`.
- **Registry installs no longer pin the server's settings.** `server.json` gave its environment variables fixed values (a 30 s timeout for every capability, port 8091, a placeholder project path); they are now defaults and placeholders, `UE_PROJECT_PATH` is marked required, and the capability-token and path-prefix variables are listed.
- **Plugin and game-feature content reachable without configuration.** The plugin reports the editor's mounted content roots in `bridge_ack` and whenever a mount comes or goes, and the path gate allows them alongside `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara` and `MCP_ADDITIONAL_PATH_PREFIXES`. `/ShooterCore/...` used to fail as a `SECURITY_VIOLATION` unless the mount was listed by hand, and create paths under a mount were rewritten to `/Game/...`. Arguments treated as file paths (including `outputPath`, even where it names an asset) never get the reported mounts and allow the static list plus `/Saved` and `/tmp`; the `sourcePath` of `import` and `import_struct`, the screenshot and `export_heightmap` `path`, `heightmapPath`, `externalPath` and a file `exportPath` are now treated as file paths, so they also gain `/Saved` and `/tmp`. A `/`-rooted path starting with two separators (`//host/share`, `/\host\share`) is refused.

</details>

<details>
<summary><b>⚠️ Migration</b></summary>

- `set_*_class` on `manage_networking`: pass `makeDefault: true` to keep making the game mode the project default; set the level's GameMode Override in World Settings.
- `edit_widget_blueprint` with `edit: "preview"`: call `preview_widget` instead.
- `control_actor.list` with `summary: true`: `byClass`, `byTag` and `byFolder` are arrays of `{name, count}` rows sorted by name, no longer `{name: count}` objects, and root actors count under `"(none)"` (and `inspect` `get_metadata` `folderPath` is `""`), not `"None"`.
- Widgets: `create_widget_template` creates a new asset (`name` required, folder in `path`) and refuses an existing one; `widgetPath`, `folder` and `gridSize` are refused, and `inventory_ui` is `inventory`. `add_game_widget` takes `positionX`/`positionY` (not `x`/`y`) and `parentSlot` (not `parentName`), sizes with `sizeX`/`sizeY`, and refuses `angle`, `brushSize` and `promptFormat`. `add_widget_component` takes `slotName` and `parentSlot` (not `componentName`/`parentName`).
- Send the parameter names `describe` lists: a stdio call that relied on the removed TypeScript conversions is refused with the declared name in its message.
- A removed action answers `UNKNOWN_ACTION` with a suggestion.
- `execute` `options` takes `idempotencyKey`, `expectedCatalogRevision`, `expectedRevisions` and `timeoutMs`; `preview` (`false` was accepted, `true` answered `UNSUPPORTED_PREVIEW`) is refused like any other unknown option, with `UNSUPPORTED_OPTION`.
- The typed Niagara module adds (`add_spawn_rate_module` and the rest of `add_niagara_module`) insert the stock module with its default inputs and no longer take value settings (`spawnRate`, `burstCount`, `count`, `burstTime`, `spawnPerUnit`, `lifetime`, `mass`, `acceleration`, `forceStrength`, `color`, `colorMode`, `sizeMode`, `uniformSize`, `collisionMode`, `restitution`, `friction`, `dieOnCollision`, `killCondition`, `cameraOffset`); tune the module afterwards with `set_parameter_value`.
- `bulk_delete` no longer takes `showConfirmation`: the consent grant confirms the delete.
- Edits that now save by default keep the old behaviour with `save: false`; the GAS Blueprint edits take no `save` and always compile and save.
- `manage_sequence`: send the master sequence as `path` to `add_shot_track`, `add_subsequence` and `configure_shot_settings` (`shotSequencePath` now picks a shot); `add_section` `start` and `end` are display-rate frames, not ticks; `set_playback_speed` writes the play rate into the level sequence actors that play the sequence.
- Partial results are reported as failures. `manage_asset` `delete` keeps an asset that something outside the delete still references and lists it under `referencedPaths`; it fails with `ASSET_REFERENCED` only when nothing was deleted (pass `force: true` to delete it anyway). A `control_actor` `delete` that removes only some actors answers `DELETE_PARTIAL`; an `add_component` whose properties or `meshPath` partly fail answers `PARTIAL_FAILURE`, or `PROPERTY_CONVERSION_FAILED` when none applied. The deleted actors and the added component stay, and the reply lists them.
- Parameters that were declared and never read are refused as `UNDECLARED_PARAMETER`:
  - Blueprints: `connect_pins` `linkedTo`; `probe_handle` `operations`, which never ran (batch graph edits with `edit_graph` `batch`).
  - Levels: `manage_level` `save` `levelName`, `save_as` `destinationPath` and `add_sublevel` `parentPath`; `create_level` `template`; `get_level_structure_info` `levelPath`; `open_level_blueprint` `save`.
  - Animation: `targetSkeleton` on the asset creators (use `skeletonPath`), `assetPath` on the AnimGraph edits (use `blueprintPath`), `path` on `create_state_machine` and `create_blend_tree`, `meshPath` on `create_ik_rig` (use `skeletalMeshPath`), `skeletalMeshPath` on `setup_ragdoll`, `length` on `set_section_timing` and `assetPath` on `setup_ik` (use `path`; the `chain_mapping` kind still takes `assetPath`).
  - Sequencer: `masterSequencePath` (use `path`), `path` and `cameraActorName` on `configure_camera_settings`, `path` and `save` on the camera rigs, `skeletalMeshPath` on the skeletal animation track, `path` on `create_media_sound_component`, `url`, `streamUrl` and `urls` on the media creators and `play_media` (network media was always refused).
  - Effects: `path` and the debug-shape options on `particle`, `path` on `niagara` and `create_volumetric_fog`, `reset` on `reset`, `name` on `add_niagara_module`, `emitterName` on `create_niagara_emitter`.
  - Audio: the free `properties` object on `add_cue_node`, `add_source_effect`, `configure_mix_eq`, `create_reverb_effect`, `create_sound_class`, `create_sound_mix` and `set_class_properties` (send the named fields); `sourceNode`, `sourcePin`, `targetNode` and `targetPin` on `connect_metasound_nodes` (send `sourceNodeId`, `sourceOutputName`, `targetNodeId` and `targetInputName`).
  - GAS: `attributeType` on `add_attribute`.
  - Materials, textures, assets and structs: `add_static_switch_parameter` `value` (use `defaultValue`), `add_noise` `octaves` (use `levels`), `set_streaming_priority` `streamingPriority`, `analyze_graph` `maxDepth`, `get_dependencies` `recursive` and `maxDepth` (direct packages only), `list_structs` `searchScope`.
  - Landscape, environment and lighting: `import_heightmap` `path` (use `heightmapPath`), `path` on `configure_landscape_lod`, the landscape `generate_lods`, `create_sky_sphere` and `create_fog_volume`, `set_time_of_day` `propertyValue`, `intensity` on `configure_sky_atmosphere`, `density` on `configure_volumetric_cloud` and the rain and snow particles, `speed` on `configure_water_waves`, `create_fence_spline` `spacing`, `configure_shadows` `cascadedShadows`, `build_lighting_quality` `settings`, `configure_screen_percentage` `actorName`.
  - Editor, inspect and system: `bookmarkName` and `description` on bookmarks, `metadata` on `control_editor` `start_recording`, `path` on `open_asset` and `close_asset`, `actorName` on `set_camera`, `type` on `start_profiling` and `show_stats`, `channels` on `subscribe` and `unsubscribe`, `widgetPath` on `show_widget`, `mergeActors` and `enableInstancing` on `merge_actors`, `tracePath` and `connectionType` on `capture_insights_trace`, `objectPath` on `inspect_cdo`, and the target parameters on `get_level_details`.
  - Networking and game framework: `replicationPolicy` on `configure_replication_graph`, `predictionThreshold` and `correctionThreshold` on the prediction actions, `allowSpectating` and `spectatorViewMode` on `configure_spectating`, `priority` on `create_input_mapping_context`, `serverName`, `sessionName` and `serverPassword` on `host_lan_server`.
  - Geometry: `maxHullVerts`, `maxVerticesPerHull` and `hullPrecision` on `generate_collision`, which never shaped the hulls (use `maxHullCount`).
  - PCG: `graphPath` on `set_pcg_partition_grid_size`.
  - Records that folded a removed action no longer declare the parameters only that action read (`configure_weapon` and `configure_damage`, `configure_game_mode`, `configure_session` and `configure_voice`, `configure_crafting` and `configure_loot`, `configure_ability`, `configure_interactable`, `configure_character`).
- Newly required parameters and narrowed values:
  - Blueprints and widgets: `set_node_property` requires `propertyValue`; `bind_widget` requires `slotName`; `create_widget_template` `settingsType` is `all`, `graphics`, `audio` or `controls` (`video` and `gameplay` are refused); `create_widget_blueprint` refuses a `parentClass` that is not a UserWidget class (`INVALID_PARENT_CLASS`); widget animation `trackType` no longer accepts `material` (use `opacity`, `color` or a transform type).
  - Character and animation: `set_walk_speed`, `set_jump_height`, `set_gravity_scale`, `set_ground_friction` and `set_braking_deceleration` require their value; `set_bone_parent` requires `parentBoneName`; `create_montage` needs `skeletonPath` or `animationPath`; the AnimGraph edits require `blueprintPath`; `parentClass` must be a Character (`create_character_blueprint`) or an AnimInstance (`create_animation_blueprint`).
  - Networking: `configure_game_mode` requires `gameModeBlueprint` (a call with only `blueprintPath` is refused).
  - Levels and volumes: `streamingMethod` no longer takes `Disabled`; `create_volume` refuses a parameter its volume class ignores.
  - Sequencer: `play` accepts `loopMode` `once` or `loop` only (not `pingpong`).
  - Effects: `set_emitter_properties` needs `emitterProperties` holding only `enabled`.
  - GAS: `set_ability_costs` needs `costEffectPath` and `set_ability_cooldown` `cooldownEffectPath`; `activationPolicy` takes LocalOnly, LocalPredicted, ServerOnly or ServerInitiated (OnInputPressed and the other old values are refused).
  - Editor and system: `set_scalability` requires `level`; `create_bookmark` and `jump_to_bookmark` require a numeric `id`; `control_editor` `start_recording` and `enable_input_mapping` need a running PIE session; `generate_collision` accepts only box, sphere, capsule, convex or convex_decomposition.
- Calls that reported success without doing what was asked now fail:
  - Levels: `import_level` onto an existing destination without `overwrite` (`DESTINATION_EXISTS`) or from a `.t3d` file (`NOT_SUPPORTED`), `save` or `save_as` with a `levelPath` that is not the open level (`LEVEL_NOT_LOADED`), `add_sublevel` with a `parentLevel` that is not open (`PARENT_LEVEL_NOT_LOADED`), `enable_world_partition` with `bEnableWorldPartition: false` on a partitioned level (`NOT_SUPPORTED`).
  - Animation: `add_physics_constraint` on a pair that already has a constraint (`CONSTRAINT_EXISTS`; use `set_physics_constraint` or `configure_constraint_limits`).
  - Materials and textures: a material node knob the node does not have, `delete_node` where no id matches, `create_noise_texture` with an unknown `noiseType`.
  - Landscape and environment: `sculpt` with a tool other than Raise, Lower or Flatten, `create_landscape` with a section size the engine cannot build, `remove_foliage` naming nothing, a post-process variant with no value to write, `configure_capture_source` without `captureSource`.
- `fab_search` rows no longer carry `rawIsFree`: `isFree` is derived from the price, because the listing's own flag disagrees with it.

</details>

<details>
<summary><b>👥 Contributors</b></summary>

- @theonlymankai-hash for making the plugin build on UE 5.8 (the Megascans probe and the PCG delay-load, #648).
- @SoloGorilla for letting the content mounts the editor reports through the path checks, so plugin and Fab-pack roots are reachable (#652).

</details>

<details>
<summary><b>📊 Change Statistics</b></summary>

| Metric | Count |
|--------|-------|
| Diff range | `v0.6.0-beta-b..v0.6.0-beta-c` |
| Commits | 489, the version bump and this release entry included |
| Files changed | 3,108 (3,053 hand-written, not counting line-ending-only changes) |
| Insertions / deletions | 104,422 / 492,733 (hand-written: 76,076 / 271,870) |
| Capability records | 393 |
| Callable `{tool, action}` pairs | 1,501 |
| C++ domain directories | 55 |

</details>

---

## 🏷️ [0.6.0-beta-b] - 2026-09-25

> [!NOTE]
> **Beta.** Published as a semver prerelease (`0.6.0-beta-b`) under the npm `beta` dist-tag, so `npm install` keeps serving the newest stable release, **`0.5.30`**. This section is everything on `dev` since the `v0.6.0-beta-a` tag, written from the code diff.

> [!IMPORTANT]
> ### 🧱 Fewer calls, honest replies, and a game you can drive from the tool
> Batch forms turn a Blueprint graph, a material graph, a MetaSound, a whole level layout or a row of actors into one call each, and a Play-In-Editor game can be played, timed and inspected without the OS cursor. A long list of handlers that answered success for work they never did now report what actually happened. Two small contract changes are listed under **⚠️ Migration**.

<details>
<summary><b>✨ Added</b></summary>

#### One call instead of many

- **`manage_blueprint.build_graph`** — runs up to 200 graph edits (`create_node`, `connect_pins`, `set_pin_default_value`, `set_node_property`, `create_reroute_node`, `add_variable`) through the ordinary single-step handlers, with their replies captured instead of sent. A step names its node with `id` and later steps refer to it as `"$id"`; `from`/`to` take `"$id.pin"` shorthand; a create step can carry `pinDefaults`; `"$entry"` is the graph's own function-entry node (a Construction Script's start). Nodes without a position fill a grid to the right of the existing graph, and an overlap is retried at the guard's own suggestion. Before any step runs, every function and variable a step names is resolved, so a misspelled name fails the batch with nothing applied; any other failure stops at that step and returns the node ids of the steps that were applied. The Blueprint compiles and saves once at the end.
- **`manage_material_authoring.build_material_graph`** — adds a material's nodes, wires them and sets blend mode, shading model, domain and two-sidedness in one consented call, then recompiles and saves once through `compile_material` and reports its errors. Only additive edits are batched; deleting and disconnecting keep their own consent. Auto-placed nodes are stacked by their reported height, so tall vector-parameter swatches no longer overlap.
- **`manage_audio.build_metasound`** — MetaSound `add_node`, `connect`, `set_default`, `add_input` and `add_output` steps in one call, with the same `$id` aliases.
- **`control_actor.spawn_batch`** — up to 500 actors per call, each item merged over shared `defaults` and run through the ordinary spawn path (mesh, class or Blueprint, per-item `variables`, folder, tags). A failed item is reported and the rest still spawn; `report: "failures"` returns counts and failed items only.
- **List forms of existing actions**, each item running the single-item path and each reported, with a batch that fails naming only the items that did not apply: `set_transform` `actors` (each with its own location, rotation or scale, as `{x,y,z}` objects or arrays), `set_blueprint_variables` `actors` (each with its own values), `set_material` and `add_tag` `actorNames`, `delete_by_tag` `tags`, `remove_scs_component` `componentNames`, `set_variable_metadata` `variableNames`, `set_material_parameter` and `create_material_instance` `parameters`, and `remove_foliage` `areas`. One consent grant covers the whole list.
- **An omitted fold selector is inferred** — a call that leaves out a family's selector (for example `nodeKind`) but sends parameters that only one variant declares now runs that variant on both gateways; parameters pointing at different variants still run the default.

#### Reading what is already there

- **`control_actor.list`** pages with `offset` and `nextOffset`, carries each actor's location, rotation and scale, reads named variables on every row with `propertyNames` (a name the class lacks is listed under `missingProperties` instead of vanishing), and with `summary: true` counts actors by class, tag and outliner folder instead of listing them.
- **`inspect_graph`** filters and pages a large graph, and a pin's links name the linked node's title.
- **`get_blueprint`** with `propertyName` returns just that value (read off the class default object when the property is inherited) instead of the whole summary; Blueprint `get_components` gives each component's location, rotation, scale, visibility, mesh, materials and whether it is authored, inherited or native.
- **`get_niagara_info`** lists every emitter's module inputs with their current values.
- **`manage_asset.list`** narrows by name with a string `filter` (it used to be ignored).
- **`system_control.read_log`** returns the recent editor log without a subscription, filtered by text, category or minimum verbosity. `source` reads the UnrealBuildTool log of the last compile (where "Live coding failed" keeps its compiler errors, with the file name kept readable), the Live Coding console log, or a previous editor run's log; `runsBack` reaches past the last restart, and `logFile` names the file read. `console_command` returns the lines the command logged.

#### Driving and testing a running game

- **`simulate_input` drives Enhanced Input** — `inputAction` (an asset path), `value` and `holdSeconds` inject the action itself, which a raw key never reaches; the reply names `injectedAction`. Holds are measured in game seconds and all end when the editor shuts down; `key_tap` presses and releases, keeping the key down for at least two game frames. `widget_list` names every live UMG widget in a PIE session and `widget_click` presses a Button, toggles a CheckBox or sets a Slider by name, without moving the OS cursor or taking focus.
- **The game clock acts on the running game** — `set_game_speed` and `set_fixed_delta_time` apply to the PIE world (a fixed step is switched off again when play stops), and `step_frame` steps the requested number of frames in the plugin and reports them.
- **`control_editor.restore_editor_window`** restores a minimized editor window without activating it, and by default turns off the background CPU throttle that pins a minimized editor's PIE at about 3 fps.
- **`control_editor.restart_editor`** relaunches the editor on the same project, with `validateOnly`, `delaySeconds`, and `discardUnsaved` (unsaved packages are refused otherwise).
- **`system_control.launch_build`** smoke-runs the game that `package_project` produced (offscreen by default, 5 to 120 seconds) and `package_status` reports the maps it loaded, its error count and the tail of its log.

#### Authoring

- **MetaHuman Creator in `manage_character`** — `metahuman_status`, `create_metahuman`, `rig_metahuman` (Epic's cloud auto-rig, non-blocking by default), `build_metahuman` and `export_metahuman` (geometry, materials or DNA).
- **Sequencer keys** — `list_track_keys` and `remove_keyframe` in `manage_sequence`, resolving tracks the same way every other track edit does; `onlyJob` is honoured when a Movie Render Queue render starts.
- **Animation Blueprints** — `set_transition_rules` builds the transition condition from `conditionVariable`, `conditionComparison` and `conditionValue`; `delete_transition` removes one; transitions honour `crossfadeDuration` (alias `blendTime`), `priorityOrder`, `automaticRule` and `bidirectional`; `add_state` wires the state's entry and, with an animation, a sequence player into its result.
- **`animation_physics.skin_mesh_to_skeleton`** skins a static mesh (or a donor skeletal mesh) to a skeleton through GeometryScripting; retargeting builds real IK Rig definitions and a retarget pipeline.
- **Blueprint graphs** — `set_node_property` reaches a node's reflected asset and object fields; `add_variable` stores object defaults and keeps `isPublic: false`.
- **Fab** is driven through its own API by reflection: the tab opens without the web page, an already-open Fab window is reused, listings are claimed before download, and `obj` and `usdz` downloads are accepted alongside `unreal-engine`, `gltf`, `glb` and `fbx`.
- **FBX animation import** — `manage_asset.import` brings in the animation an FBX carries as an AnimSequence on a named skeleton.

#### Finding the right capability

- Plain phrasings rank the intended capability first, native `search` honours the `effect` and `tool` filters, and `create_material`, material instances, widget clicks and map cooking each have search topics of their own.

</details>

<details>
<summary><b>🔧 Changed</b></summary>

- **Native requests are timed from their last progress, not their start.** While the game thread is alive, every open request gets a "still working" progress notification every 20 seconds, `MaxLifetimeSeconds` still ends a request that never answers, and progress percentages never go backwards. Long imports and builds are no longer killed and reported as failures.
- **A too-large result names the parameters that would narrow it** on the native gateway as well, and `summary` counts as a narrowing parameter on both.
- **A world edit made during Play-In-Editor carries a receipt warning** that it landed in the PIE world and is discarded when play stops.
- **Text summaries pair a record's display name with its path**, asset path, object path or id.
- **The `.env` file** is read from `MCP_ENV_FILE` when it is set, otherwise from the package root, otherwise from the working directory, using Node's own loader.
- **A client's capability profile** comes from the capabilities the client declares, not from a lookup table of known client names.
- **The console command policy** compiles each rule's pattern once instead of on every command, and every command-queue failure is logged.
- **Lighting and geometry records declare what their handlers read** — `configure_lumen` takes `quality`, `indirectLightingIntensity` and `bounces`; `configure_shadows` takes the shadow quality, cascade, distance, contact, ray-traced and virtual-shadow-map settings; `configure_exposure` takes `method`; `enable_volumetric_fog` honours `enabled: false`; the geometry primitives take their own dimensions (box `height`, cone `baseRadius` and `topRadius`, capsule `length`, plane `width` and `depth`, stairs `floating`, arch `angle`, ramp `width`, `length` and `height`), and the deform, operation and optimize records accept `actorName` or `targetActor`.
- **`set_project_setting`** also finds a setting by its console-variable name and reports it persisted only when the key is really in the config file on disk; **`set_preferences`** writes to the settings object its category names, not only to console variables.
- **The plugin's integer `Version`** in the `.uplugin` now follows the semver (`600` for 0.6.0), and `bump-version` writes it with `VersionName`.

</details>

<details>
<summary><b>🛡️ Security</b></summary>

- **Dangerous engine commands are refused on both doors** — a console command whose first token is `debug`, `exec`, `crash`, `gpucrash`, `check`, `gpf`, `ensure`, `ensurealways`, `fatal`, `bufferoverrun`, `crtinvalid`, `stall`, `hitch`, `renderhitch`, `softlock` or `eatmem` answers `DANGEROUS_ENGINE_COMMAND`.
- **Traversal detection decodes instead of matching spellings** — percent-encoding is decoded to a fixed point (at most four rounds, and malformed encoding is refused), so mixed forms such as `%2e.`, `.%2e` and `..%2f` are caught; the asset handlers use the same shared predicate; mount roots match case-insensitively.
- **Plugin content roots pass the native path canonicalizer** while host filesystem roots (`/home`, `/Users`, `/etc`, `/proc`, `/var`, `/tmp` and the like) are still refused.
- **Identity keys are redacted before a log line or error leaves the editor** (`UserId`, `AccountId` and `LoginId`, however they are spaced), `console_command` output and log text pass the per-line sanitizer, and the log tail `launch_build` returns gets the same treatment as `read_log`. A mount root named in plain prose is no longer redacted as if it were a path.
- **`launch_build` only starts a game inside the project directory**; Fab only adopts a browser that is on fab.com; MetaHuman export keeps `externalPath` contained.
- **Batches are bounded and stay on their own asset** — every actor list form refuses more than 500 actors, and a step of `build_graph`, `build_material_graph` or `build_metasound` cannot point at an asset other than the batch's.
- The unused loopback media-URL settings were removed from the plugin settings.

</details>

<details>
<summary><b>🛠️ Fixed</b></summary>

#### Replies that said success for work that did not happen

- `set_project_setting`, the Niagara system and Sequencer section handlers, `set_vector_parameter_value` (an `{x,y,z,w}` object or an `[r,g,b(,a)]` array used to be written as white), `set_texture_parameter_value` and `set_material_parameter` on a parameter the material does not have, `set_property` (it now saves the asset it writes and says when it did not), `add_scs_component` whose transform did not apply, and an asset delete whose `existsAfter` was hard-coded to false.
- A `delete` that left the `.uasset` behind to reappear on the next start, and a `duplicate` whose copies were never saved.
- `add_node` built every new Set as an assignment from its own Get, hung it off whichever event it found first, accepted variable nodes for names the Blueprint does not own, and accepted input-action nodes it could not bind; a missing graph node now says that node names work and where to list them.

#### Blueprints

- Component-template edits reach actors already placed in a level, including inherited native components, and a value that did not hold on an instance is named instead of counted.
- SCS nesting no longer duplicates nodes, an `edit_scs` batch names its failed operations and takes `reparent`, and `add_component`'s verification reflects the properties applied after the add.
- Struct pins take JSON vectors, pure Cast nodes are created as pure, an event node takes the name the editor shows, a library function named on the wrong library still resolves, and `"execute"` reaches a latent or macro node's own exec input.

#### Materials

- Wires carry channel masks, `compile_material` reports the real compiler errors, `update_custom_expression` can change inputs without cutting wires, and an `FColor` property takes 0-1 channels.
- The material input visitor covers refraction, anisotropy, tangent, pixel depth offset and clear coat; `SurfaceThickness` is exposed from 5.2 and `Displacement` from 5.3.

#### Animation and Sequencer

- Procedural bone tracks start from each bone's reference pose, so untouched channels no longer collapse the skeleton, and frames take `rotationDelta`.
- `add_state` no longer creates empty, entry-less states or clears a pose when a link is refused; `add_transition` applies the requested settings to an existing transition, and a missed state name lists the states that exist.
- Transform keyframes now evaluate, keyframe and range edits are saved, camera cut tracks can be found, removed and saved, the camera cut binding is checked before a section is created, and a Movie Render Queue range starting on the sequence's first frame renders whole with `onlyJob`'s enable toggles restored afterwards.
- IK Rig creation needs UE 5.6 or later, the batch retarget is built on 5.8, and skinning needs GeometryScripting on 5.5 or later; each answers with a clear message where its engine support is missing.

#### Audio, effects and world

- MetaSound literals take their input's type, asset inputs take object literals and refuse the wrong class, nodes resolve by name before they are added, and a connect into an already-connected input succeeds and saves.
- Niagara spawn, lifecycle and parameters work during PIE, `set_parameter_value` writes emitter module inputs, and spawn can attach to an actor.
- Foliage removal clears the rendered instances, bare locations honour `minScale`, `maxScale` and `randomYaw`, and `paint_foliage` drops an exact `count` over a box `area` onto the ground.
- `audit_placement` finds the floor under an actor past whatever is struck first, ignores something resting on the actor and the gameplay debugger's replicator; the sun's angles point the right way and unknown environment settings are named; post-process volumes apply their exposure and blend settings.
- Geometry deformers and primitives honour their declared parameters; `inspect_object` reads the component it is given; a widget of any class can be added by name; `duplicate` with a new name into a new folder keeps the folder; an FBX import clears a target named after the source file and replaces an existing asset only when nothing references it.

#### Editor and capture

- Screenshots bring the Level Editor tab forward and redraw before capturing instead of returning a black frame, and screenshots and camera moves use a viewport that is actually visible.
- `open_editor_tab` opens Fab; injected input rebuilds the key maps once per player instead of on every key; a spawn of a generated-class path no longer logs a registry miss.

#### GAS (#606)

- `manage_gas` mutations compile, verify on the compiled class and only then save: `create_gameplay_effect` applies duration and period, `set_effect_duration` refuses an unknown duration type, `add_effect_modifier` binds its attribute (`targetAttribute` or `attributeName`, the `AttributeSet.Attribute` form, and an error for an ambiguous name), `set_modifier_magnitude` verifies before saving, `set_ability_tags` checks all five tag containers before writing, and `add_attribute` applies its default value.

</details>

<details>
<summary><b>🗑️ Removed</b></summary>

- The TypeScript log reader, replaced by the native `read_log`.
- The migration translator and artifact modules the gateway never called, the unused tool-definition utility schemas with their stale `manage-asset/catalog.json`, and unused semantic-boundary helpers.
- The three Control Rig mutations `add_control`, `add_rig_unit` and `connect_rig_elements`, which no record published and which answered `NOT_SUPPORTED` on every path.
- The case-colliding `sublevelPath` alias of `manage_level`.
- The package's stale `module` field and its published sourcemaps, and the `dotenv` and `mcp-client-capabilities` dependencies.

</details>

<details>
<summary><b>⚠️ Migration</b></summary>

- **Coming from `0.5.30`, the current stable release?** Every step in the `0.6.0-beta-a` migration below still applies: the single `unreal` gateway tool is permanent, and a direct `tools/call` on a canonical tool name answers `DIRECT_TOOL_CALL_REMOVED` with an executable `nextCall`.
- **`manage_level`**: send `subLevelPath` (or `levelPath`) instead of `sublevelPath`.
- **`manage_geometry` primitives**: `create_plane`, `create_stairs`, `create_arch` and `create_ramp` no longer take `dimensions`; send the shape's own fields (see *Changed*).
- **`build_environment.configure_lumen`**: send `quality`, `indirectLightingIntensity` and `bounces` instead of a `settings` object.
- **`control_editor.set_fixed_delta_time`** needs a running PIE session and answers `NO_ACTIVE_SESSION` otherwise; it used to run a console command that changed nothing.

</details>

<details>
<summary><b>🧪 Tests & CI</b></summary>

- `type-check` also type-checks `tests/` and `scripts/`, `no-explicit-any` and `no-console` are lint errors in `src/`, the contract suites can fail, integration cases assert what the handlers answer, and a failed build stops the integration run (`UNREAL_MCP_ALLOW_TS_FALLBACK=1` runs the source instead).
- The dependency audit runs as its own job and blocks runtime advisories at moderate again; the MCP Registry publish waits for npm to serve the new version.
- Line endings were renormalized to LF, and generators sort with byte-order comparison so their output is identical on every machine.
- New plugin source-contract suites cover the batch forms, log and identity redaction, the game clock, window restore, key holds, native search filters and too-large guidance; the GAS verification work was split into `GAS/Authoring/` to stay within the 250-line and 25-files-per-folder gates.
- **`bump-version` can bump away from a prerelease.** Its rewrites of the `server-factory.ts` and `McpNativeTransport.h` fallbacks matched `X.Y.Z` only, so from `0.6.0-beta-a` both kept the old version, the workflow's own `version:check` failed and nothing was committed; both now take a prerelease suffix, and a test runs the workflow's own patterns. This release was bumped with it.

</details>

<details>
<summary><b>🔄 Dependencies</b></summary>

| Package | Change |
|---------|--------|
| `dotenv` | removed (Node's `process.loadEnvFile` reads `.env`) |
| `mcp-client-capabilities` | removed |

</details>

<details>
<summary><b>👥 Contributors</b></summary>

- @SoloGorilla for making `manage_gas` compile, verify and save what it authors (#606), and for pairing each record's display name with its address in text summaries (#621).

</details>

<details>
<summary><b>📊 Change Statistics</b></summary>

| Metric | Count |
|--------|-------|
| Diff range | `v0.6.0-beta-a..v0.6.0-beta-b` |
| Commits | 194, the version bump and this release entry included |
| Files changed | 917 (806 hand-written, not counting line-ending-only changes) |
| Insertions / deletions | 72,290 / 56,473 (hand-written: 22,002 / 31,994) |
| Capability records | 389 |
| Callable `{tool, action}` pairs | 1,566 |
| C++ domain directories | 67 |

</details>

---

## 🏷️ [0.6.0-beta-a] - 2026-09-18

> [!NOTE]
> **Beta.** Published as a semver prerelease (`0.6.0-beta-a`) under the npm `beta` dist-tag, so `npm install` keeps serving the newest stable release. `0.6.0a` is not valid semver — npm, the `bump-version` workflow and the version-consistency gate all reject it — so the release spells the beta out.

> [!IMPORTANT]
> ### 🚪 Single-Tool Gateway, Capability Catalog & Full Source Reorganization
> Everything on this branch since the `0.5.30` tag: the permanent cutover to a single public `unreal` gateway tool, a hand-authored capability catalog that now generates both the TypeScript and the C++ contract surfaces, cinematics/render/replay automation, and a top-to-bottom split of the TypeScript handlers and the C++ plugin into per-domain modules.
> **This release contains breaking changes** — see the **⚠️ Migration** section below.

<details>
<summary><b>✨ Added</b></summary>

- **Folded capability families** — a record can stand for a whole family of bridge actions. `routing.dispatchBy` maps one selector parameter's value to the existing handler action, and every former name stays callable as a folded legacy pair whose pins supply the value it implied; both gateways apply the same two steps (pins before validation, action after it). The folds are data (`records/folds/<parent>.folds.ts`, applied by `records/shared/fold.ts`): 244 fold families across 22 parents (222 of the resulting records dispatch through a `routing.dispatchBy` selector — 221 of them from the fold specs, plus `manage_level_structure.create_volume`, whose `volumeClass` dispatch is authored on the record itself — and the remaining 23 families are pure-alias folds; 246 of the 380 records carry at least one folded legacy pair, across 1,166 pinned pairs) take the catalog from 1,379 authored pre-fold action entries to **380 records**, while all 1,549 `{tool, action}` pairs (every shipped name plus 164 new family primaries) still resolve, describe and execute, the normalization audit total is unchanged at 1,341, and a fold whose primary is one of its members keeps the selector optional so every pre-fold call is unchanged. A consent grant may name the capability by any name it answers to (canonical id, alias, or a folded `tool.action` pair) on both doors; the native completion pool completes the old names too; the search index counts a name once per identifier field and stops re-scoring folded member ids as aliases. Per-action contract tests pin the authored, unfolded records (`records/unfolded.ts`), and the integration suites derive one twin case per family (`tests/fold-twins.mjs`) so every advertised primary and selector is exercised.

#### Gateway surface

- **`unreal` gateway tool with four operations** — `search`, `describe`, `execute`, and `configure` are now the entire public MCP surface on both transports. The routing engine lives in `src/server/gateway/` (26 TypeScript modules) and covers capability indexing and views, search filters, browse/capability describe modes, execute resolve → validate → policy → authorization → dispatch, receipt context, schema normalization, guidance, and availability probing.
- **Execute idempotency ledger** — repeat `execute` calls carrying the same idempotency key return the original receipt instead of re-running the action. Mirrored on both sides of the bridge: TypeScript (`src/server/gateway/idempotency-ledger.ts`, cap 1024) and C++ (`Private/Foundation/McpIdempotencyLedger`, cap 4096).
- **Compensation receipts and capability principals** — `Private/Foundation/McpCompensationReceipt` and `McpCapabilityPrincipal`/`McpCapabilityAuthorization` give the plugin its own authorization identity rather than trusting the caller's claim.
- **`DIRECT_TOOL_CALL_REMOVED` migration receipt** — a direct `tools/call` on a canonical parent name returns a bounded, executable receipt whose `nextCall` re-runs the same request through the gateway.
- **Execute option validation and refusal** — `timeoutMs` must be an integer in 1..600000 ms and `idempotencyKey` 1..128 characters — a malformed key previously bypassed the ledger entirely, so a retry re-ran the mutation with nothing reporting that dedup was off. `preview` is refused outright with `UNSUPPORTED_PREVIEW` (no dispatch path implements a dry run, and `behavior.supportsPreview` is deliberately not consulted, because honouring it would perform the real mutation and call it a preview), and an option that is accepted but not implemented (`savePolicy`, `validationLevel`, `taskPreference`) answers `UNSUPPORTED_OPTION` instead of being validated, echoed on a success receipt, and dropped. An `expectedCatalogRevision` that no longer matches answers `STALE_STATE`. A result that would exceed the transport budget is refused as `RESULT_TOO_LARGE` (100,000 characters, 6,000,000 for image-payload capabilities) on the failure path as well as the success path.
- **Bounded search** — `limit` defaults to 12 and is capped at 25, a page is additionally held to `maxBytes` (default 24,576, floor 512, ceiling 262,144) by dropping whole trailing rows, and `hasMore`, `truncated`, `truncationReason`, `nextCursor`, `coercions` and `budgetExceeded` report exactly what happened: an argument that was clamped is named, a page that could not fit even one row answers `budgetExceeded` instead of returning an identical cursor forever, and a truncated page says why. A capability id or alias that collides in the index fails closed with `GATEWAY_INDEX_CONFLICT` rather than resolving to an arbitrary record.

#### Capability catalog and contract generation

- **Hand-authored capability records as the single source of truth** — `src/tools/catalog/capabilities/records/**` holds **380 records** across the **23** canonical parents, authored with `buildCoreRecord()` declaring deltas only. `aggregate.ts` hard-asserts the record count and throws on mismatch.
- **Generation pipeline** — `scripts/generate-canonical-registry.ts` (+ the `scripts/canonical-registry/` modules) emits the generated capability shards, the orchestration routing index, the docs action reference, and the C++ side: `Private/MCP/Generated/` shards and `McpGeneratedParentRegistry*`. `scripts/generate-gateway-manifest.ts` (+ `scripts/gateway-manifest/`) emits `src/gateway/gateway-manifest.generated.{ts,json}` with content hashing and path policy.
- **New drift gates** — `registry:generate`, `registry:check`, `manifest:check`, `policy:generate`, `policy:check`, `normalization:check`, `normalization:audit`, `migration:check`, `primitives:check`, `security:check`, `eval:check`, `version:check`, `workflow:check`.
- **Deterministic ordering helper** — `src/utils/serialization/ordering.ts` provides byte-order comparison so generated shards are byte-identical across machines and locales (`localeCompare` is no longer used for ordering).

#### MCP protocol primitives

- **Resources, prompts, completions and progress** — new `src/server/mcp-primitives/` implements the primitive registry, wiring, handlers, notifications, catalog revision reader, and fallback pointers, with a prompt catalog and typed prompt errors, a completion provider (ranking, slots, sources, fixtures), and a progress reporter/token/sink registry.
- **MCP Tasks (`2025-11-25`)** — a per-session **bounded** task store implementing the MCP SDK's own `TaskStore` contract, so `tasks/get|list|cancel|result` are auto-registered and reachable from the wire. It deliberately does not reuse the SDK's `InMemoryTaskStore`, which is unbounded, drives expiry off real `setTimeout` timers, and ignores `sessionId` entirely. Task checkpoints ride alongside it.
- **Resource subscriptions and revisions** — a subscription store with a notification coalescer so bursts of editor changes collapse into one client notification, plus resource revision stamps for change detection.
- **Session capability profiles and `configure` state** — a client profile store, a session capability profile and a session configure store model the gateway's `configure` operation and per-client capability negotiation. The session resolver is exercised by tests only — nothing in the stdio path installs one yet.
- **Elicitation decision policy** — a decision policy (`isSafeToElicit`) that answers whether a field is safe to elicit. Elicitation itself is wired into the execute path: `dispatchAndValidate()` awaits `maybeElicitMissingArgs()`, which prefills only missing required primitive fields through the client's elicitation function, under a timeout and a `missing-params` fallback. It never elicits secrets, tokens, or credentials, and never a destructive-confirmation value. Mirrored natively for cross-transport parity.
- **Resource providers** — new `src/resources/` supplies capability resources, editor-state resources, knowledge resources, the resource catalog, read router, typed resource errors, and asset pagination.
- **Native primitive parity** — the plugin gained matching `Private/MCP/Primitives/`, `Resources/`, `Routing/`, `Execute/`, `Gateway/`, and `DynamicTools/` modules (task store and methods, subscription store, notification coalescer, completion pools and provider, prompt catalog/render/argument validation, client profile store, elicitation policy), audited by the `primitives:check` parity harness.
- **Protocol negotiation** — support for `2025-11-25` plus the native transport's `MCP-Protocol-Version` header guard: a present-but-unsupported version is refused with HTTP 400, while an absent header derives from the session's negotiated version and otherwise falls back to the `2025-03-26` default. Native accepts the three modern versions; the TypeScript SDK additionally accepts two legacy ones, from the pinned SDK's own supported set rather than from code in this repository.
- **Bounded task store and progress** — only `search` and `describe` may be task-augmented; any mutating operation, and any tool other than `unreal`, is refused with `TASK_CHECKPOINT_REFUSED` before any work runs. The per-session store is capped at 32 tasks with a 5 to 30 minute lifetime, refuses creation with a JSON-RPC `-32600` `TASK_STORE_AT_CAPACITY` while every retained task is still running, and permits exactly one terminal transition (`TASK_ALREADY_TERMINAL`). Progress notifications carry the client's own `_meta` token (without one the reporter is inert rather than inventing an id), are strictly monotonic, and are bounded to 64 notifications per operation with a 512-character message clamp and a 256-entry reporter map.
- **Resource surface** — `resources/templates/list` is registered with four templates (`ue://capability/{capabilityId}`, `ue://knowledge/{engineVersion}/{topic}`, `ue://object/{objectPath}`, `ue://asset/{assetPath}`) plus five static resources (`ue://capability/catalog`, `ue://project`, `ue://editor`, `ue://selection`, `ue://state/revisions`), every read is held to a 64 KiB budget with typed `RESOURCE_*` codes (`RESOURCE_TRAVERSAL_REJECTED` for host paths and traversal), and `ue://health` now also carries `readiness`, the telemetry `diagnostics` snapshot, `currentSession`/`previousSession` and `metricsExposition` alongside the capability and editor-state resources.

#### Cinematics, render and media automation

- Complete native and WebSocket **cinematics, Movie Render Queue, media, Take Recorder, and replay** automation coverage, with live-editor verification harnesses.

#### Blueprint authoring

- **`add_event` supports component-bound events** — pass `componentName` plus `eventName` (the delegate name) to wire a component's multicast delegate (e.g. `NearMissZone.OnComponentBeginOverlap`). Previously such requests fell through to the custom-event branch and produced an unbound `Event_<guid>` that never fired. The new branch resolves the SCS component, locates the multicast delegate property on its class (accepting both the bare name and the `__DelegateSignature` suffix), and creates a properly initialized `UK2Node_ComponentBoundEvent` with `ComponentPropertyName`, `DelegatePropertyName`, and `DelegateOwnerClass` set. Idempotent on repeat calls; returns `INVALID_ARGUMENT` / `COMPONENT_NOT_FOUND` / `COMPONENT_CLASS_UNRESOLVED` / `DELEGATE_NOT_FOUND` when inputs are missing or unresolvable. Guarded by `MCP_HAS_K2NODE_COMPONENTBOUNDEVENT` so the file still compiles on engine layouts where the header isn't reachable.
- **`create_node` / `add_node` sets the widget class on CreateWidget nodes** — a `K2Node_CreateWidget` previously fell through to generic instantiation that never assigned `WidgetType`, producing a generic `UUserWidget` `Class` pin and an untyped `Return Value`. A dedicated branch now reads `targetClass` (Widget Blueprint asset path such as `/Game/Widgets/WBP_HUD`, or a class name), resolves it, and assigns `WidgetType` before pin allocation so `ReconstructNode` builds the correct typed `Return Value`. Returns `INVALID_ARGUMENT` / `CLASS_NOT_FOUND`.
- **`create_node` / `add_node` sets the target class on DynamicCast nodes** — a `K2Node_DynamicCast` ("Cast To …") previously produced an unusable "Bad cast node" with only a wildcard `Object` pin and no typed `As <Class>` output. A dedicated cast branch reads `targetClass`, resolves it via `ResolveClassByName`, and assigns `UK2Node_DynamicCast::TargetType`.
- Factored the class-resolution and payload-reading logic shared by DynamicCast and CreateWidget into `ResolveTargetClassFromString` and `ReadTargetClassPayload`, so every node branch with a class pin accepts the same input forms (Blueprint asset path, generated-class path, native class name) and the same legacy field fallbacks (`memberClass` / `nodeClass` / `widgetType`, plus a `CastTo<Class>` prefix peel from `nodeType`).
- **`add_variable` applies `defaultValue`** — the handler read the field but never assigned it, so every variable was created with a zero/empty default (a float requested as `0.35` stayed `0`). The parsed default is now written to `FBPVariableDescription::DefaultValue` with type-aware formatting: booleans as lowercase `true`/`false`, integer/byte categories as whole numbers, floats/doubles via `SanitizeFloat`, and strings/struct literals passed through.
- **18 widget-authoring actions are now routable on both surfaces** — `add_quest_tracker`, `add_safe_zone`, `add_spacer`, `add_widget_component`, `add_widget_switcher`, `bind_localized_text`, `create_credits_screen`, `create_shop_ui`, `create_widget_style`, `delete_animation`, `get_widget_slot_info`, `remove_widget`, `rename_widget`, `reparent_widget`, `set_font`, `set_localization_key`, `set_margin`, and `set_widget_binding` were absent from both the TypeScript `WIDGET_AUTHORING_ACTIONS` set and the native `WidgetAuthoring()` routing array, so the handlers behind them could not be reached. Added to both, with matching capability-record property fragments for the widget path, layout, content, and panel inputs.
- **Promoted skeleton routes are named on both surfaces** — fifteen previously hidden native skeleton routes (`add_socket`, `modify_socket`, `set_physics_asset`, `modify_physics_body`, `remove_physics_body`, `set_physics_constraint`, `get_physics_asset_info`, `list_morph_targets`, `set_morph_target_value`, `get_bone_transform`, `list_virtual_bones`, `remove_socket`, plus the `delete_*` spellings) now carry explicit route names on the TypeScript `SKELETON_ACTIONS` and the native `Skeleton()` routing array, with twelve promoted to canonical capability records (marked `post-migration` so the pre-gateway audit total stays truthful). The remaining three `delete_*` spellings (`delete_socket`, `delete_morph_target`, `delete_virtual_bone`) stay hidden pending a retrieval-IDF budget fix.

#### Plugin capabilities

- **`MCP_NATIVE_PORT` environment variable** — overrides the native MCP HTTP/SSE port at startup without editing committed ini, so several editors can run at once on distinct ports. Falls back to the `Native MCP Port` project setting when unset or invalid.
- **`IKRigEditor` optional module** — declared for the `create_ik_rig` path so IK Rig creation works without a hard dependency on the editor module being present.
- **Pre-queue capability gate** — every automation request is authorized before it reaches the editor queue (`Private/Core/Security/McpPrequeueGate`), resolving the demand with the same `NormalizeAction` the dispatchers use, so a payload cannot authorize one capability and execute another. The refusal order is fixed (scope, consent, project, paths, path coverage, console command, consent nonce, quota), a refused request performs no editor work at all (the in-handler checks remain as post-queue defence), an unknown or misspelled action fails closed to `Admin`, an ambiguous one takes the strictest scope, and the recursive console-command scan (depth 8, 4096 nodes) treats a truncated scan as `COMMAND_BLOCKED`.
- **Scoped capability tokens, quotas and single-use consent** — a scoped token carries its profile, scopes, allowed path prefixes, allowed projects and per-minute request/tool-call quotas (a scoped `Admin` entry is invalid and ignored), and the `bridge_ack` `authority` block reports the effective identity without ever carrying the token, its paths or its limits. A presented-but-unresolvable token is refused even where a token is not required, and a project-restricted principal is refused at the handshake rather than per request. The quota ledger is shared by both transports, keyed by principal rather than socket or session, bounded to 256 tracked identities with least-recently-seen eviction, charged only after every other refusal, and `QUOTA_EXCEEDED` is the one refusal marked retryable — so a reconnect cannot reset a budget. A consent grant's nonce is burned after all authorization refusals and before the quota charge, so a replay answers `CONSENT_REUSED` without spending budget; nonce-less grants from older clients keep capability-match-only behaviour.
- **New state and refusal codes** — `STALE_STATE` and the `UNDO_UNAVAILABLE_*` family (`_DURABLE_WRITE`, `_EXTERNAL_PROCESS`, `_ASYNC_PIPELINE`, `_NO_TRANSACTION_BUFFER`, `_NON_TRANSACTIONAL_OBJECT`, plus `UNDO_BROKEN_BY_DURABLE_WRITE` from a package-saved witness) join the shared strings, `EDITOR_BLOCKED` reports a game-thread stall over 15 s, and `EDITOR_STATE_MISMATCH` reports a preview/state mismatch.
- **Cancellation and deferred replies are attributed to their transport** — a cancel from a socket that does not own the request is refused, and `McpRequestOriginRegistry` (cap 512) records the admitting transport per request id. That fixes native `/mcp` deferred replies which used to fall back to the WebSocket path, get dropped, and surface to the caller as an untyped 300 s `TIMEOUT`.
- **Editor policy surfaces** — the plugin settings carry the Movie Render Queue caps (dimension 8192, 33,554,432 px aggregate, `MaxMovieRenderAggregateWork`, executor/burn-in/Take-Recorder class allowlists) and the native session limits (600 requests and 120 tool calls per minute per client).
- **Bounded native sessions** — at most 16 active sessions (`MaxActiveSessions`) with a 120 s idle reclaim, and a 32-connection ceiling (`MaxConcurrentConnections`) that answers 503 beyond it.

#### Replies that report what actually happened

A write that lands is not a write that is correct, and several actions used to answer a bare `success` for a result the caller would only discover by looking at the editor. These replies now carry the finding itself:

- **Placement feedback on `control_actor` spawn and `set_transform`** — the reply names what the actor interpenetrates (`overlappingActors[]` with `penetrationDepth`), whether it is sunk into or floating above the surface beneath it (`groundZ`, `groundClearance`), and a `suggestedLocation` that rests on that surface. A Character's location is its capsule **centre**, so reusing a mesh's feet-relative Z buries it to the waist — the exact bug this was written for. Volume/trigger/light actors and subsystem debug-draw proxies (whose bounds span the level) are excluded, and slabs bedded into each other are read as floor layering rather than penetration, so the findings are the real ones.
- **`control_actor.audit_placement`** — the same check swept over a whole level for placements nobody will call back into. Findings come worst-first with a severity in world units, a `byKind` tally (`sunk`/`floating`/`overlapping`/`unsupported`) and `minSeverity`/`nameFilter`/`limit` to narrow, so one sweep of a 776-actor level answers within the transport budget instead of being refused as `RESULT_TOO_LARGE`. A geometric test cannot tell a mistake from a composition — a keep beds its towers into its platform, an island is meant to hang in the air — so an actor tagged `mcp.placement.ok` drops out as both subject and overlap target, letting the flagged count reach zero and mean something instead of training the caller to ignore it.
- **`audit_placement` also catches an actor lying on its side** — overlap and ground checks both pass for a building tipped onto its face: it is inside nothing, and its now-horizontal bounds still rest on the floor. Eighteen shop houses in one level stood on their gable ends with the sweep reporting nothing, because the ±90 meant to turn them to face the street had been written into `pitch` instead of `yaw`. Lean is now measured as the angle between the actor's up vector and world up, so yaw never counts and a fully inverted actor reads 180 rather than wrapping back to 0. Severity stays in world units — the distance the actor's top travelled from upright — so a toppled house outranks a tipped pebble instead of tying with it at "90", and `maxTilt` (default 30°) keeps the few degrees of lean that make a prop look hand-placed from reporting. Only actors that render a mesh are judged: rotation is the whole point of a light, a camera or a decal.
- **Re-using a `slotName` edits that widget instead of duplicating it** — re-adding is how a caller changes an existing widget ("the text block called `Txt_Health` now reads 0"), and `ConstructWidget` re-initialises the object already holding that name rather than making a second one. That re-initialisation clears the widget's `Slot`, so `GetParent()` answered null while the parent panel's slot list still pointed at the widget, and the add appended a second slot for the same widget — one widget listed twice under one parent, with the graph's variable binding to whichever the compiler reached first. Three HUDs were corrupted this way before the cause was found. The detach now asks the panels which of them lists the widget, because a parent's slot list survives the re-initialisation that erases the widget's own back-pointer, and it carries the slot's layout and Z-order across the re-seat — otherwise editing a label's text silently moved it to the panel's default corner. Geometry passed in the call still wins.
- **A whole-graph pin read reports pin literals, like the single-node one does** — `inspect_graph` with `info: "graph"` and `includePins` emitted each pin’s type, direction and `linkedTo` but never its `defaultValue`, while `info: "pins"` on the same pin did. Nothing said the field was being withheld, so an absent `defaultValue` read as “this pin is empty”: a branch comparing the level name against `"L_Hub"` looked like it compared against `""`, and a `bShowMouseCursor` that was set looked unset. Both readings were wrong and both sent a live debugging session down the wrong path. The graph-level view now emits `defaultValue`/`defaultTextValue`/`defaultObjectPath` on the same terms as the per-node view.
- **`create_node` seeds a Get Subsystem node with its subsystem type** — the `UK2Node_GetSubsystem` family (`GetSubsystem`, `GetSubsystemFromPC`, `GetEngineSubsystem`, `GetEditorSubsystem`) keeps its type in a `CustomClass` UPROPERTY that the editor palette seeds via `Initialize()` before the node is placed, not on a pin. Spawned by class name the property stayed null, so the node came back with an untyped result pin and the blueprint stopped compiling with "Node Invalid Subsystem Type must have a class specified" — and nothing could repair it, because the visible `Class` pin is only promoted into `CustomClass` during node reconstruction and `set_node_property` cannot reach the property. `targetClass` was accepted and silently ignored, so the only evidence was a compile error on a later call. The node is now seeded from `targetClass` before its pins are allocated, a non-`USubsystem` class is rejected, and a request without `targetClass` is refused rather than answered with a node that can never compile. Found wiring `AddMappingContext` into a live player controller.
- **`set_node_property` names the properties it accepts** — an unsupported name answered `Unsupported node property 'X'` and nothing else, so a caller could not tell a misspelling from a property that is simply not settable there, and had to guess. The refusal now lists the supported set and says that a node is moved with `NodePosX`/`NodePosY` and that node-class fields such as a cast target are set at creation via `create_node` `targetClass`.
- **`delete_node` refuses a `pinName` it would have ignored** — `break_pin_links` folds into `delete_node` under `deleteScope: "pin_links"`, so sending `pinName` without that scope meant "operate on this pin" and "delete the whole node" at once, and the node won, silently. It cost a live Branch node and the whole death branch hanging off it before the cause was clear. A destructive default must not resolve a contradiction in its own favour: the call is now refused with `CONTRADICTORY_SCOPE`, naming the scope that does what the pin was obviously meant to do. The refusal runs above the transaction, so a rejected request leaves no empty undo entry.
- **`manage_asset` `edit_struct` reports `saved`** — and warns via `persistenceWarning` when `save` was omitted, because struct members added in memory survive the session, pass `list_struct_members`, expose Break-struct pins, accept DataTable imports, and then vanish on the next editor start. `save_all` never rescues them: the package is not dirty.
- **`import_rows` reports `fieldsDefaulted`** — with a `dataLossWarning` naming the columns the entry omitted, because the import builds each row from a default-constructed struct. A 5-field import over a 20-field row used to reset the other 15 and still answer `imported: n, invalidRows: []`.
- **A throttled save no longer reports a write that did not happen** — `SaveLoadedAssetThrottled` skipped saves inside its window and returned `true` regardless, so a caller editing one Blueprint in a burst was told `saved: true` for every edit while only the first reached disk, and lost the tail of the burst on the next editor start. It now refuses to skip a package with unsaved work; a clean package is still skipped, because there is nothing to write.
- **`manage_blueprint compile` marks the recompiled asset unsaved** — a compile regenerates the class in memory without dirtying the package, so `control_editor.save_all` answered "0 dirty" and the caller concluded everything had persisted while the asset on disk still carried the previous bytecode. Compiling from the editor UI marks the asset unsaved; the action now does the same and reports `pendingSave` with a `persistenceHint`, so the ordinary edit → compile → save_all workflow ends with the work actually on disk.
- **`CONSENT_REQUIRED` hands back the grant that satisfies it** — the native refusal now spells out the exact `consent` sibling to re-send, matching what the TypeScript gateway already returned. It previously named only the gap and pointed at `describe`, so a caller's first use of any consent-bearing capability cost a full contract round-trip to learn two strings the refusal already held. The re-send still names the capability, which is what the gate is for.

#### Services and tests

- **Telemetry and readiness services** — `src/services/` gained a telemetry registry, observation and schema modules plus a readiness probe.
- **New test tiers** — `tests/eval/` (own Vitest config, run by `eval:check`), `tests/audits/`, `tests/harness/`, and `tests/fixtures/`, alongside `scripts/qa/` adversarial, cross-transport matrix, and capability-metadata audits.
- **Readiness vs. health** — `/health` now answers 503 while the server is not ready instead of reporting `ok` unconditionally, and a new `/ready` probe reports the same state without the health payload.
- **Capability-aware timeouts** — operation timeouts derive from the record's declared cost tier: `resolveActionTimeoutMs()` reads the generated `capability-cost-index.generated.ts` for the `tool::action` pair and maps its latency and resources onto `CAPABILITY_TIMEOUT_TIER_MS` (15 s to 20 min), with a 15 s floor, a 120 s fallback for unclassified pairs, and an `MCP_REQUEST_TIMEOUT_MS` override — replacing one flat default for every action.
- **Asset-listing cache invalidation** — the listing is invalidated after every successful mutation (`invalidateAssetCacheForMutation()` runs straight after dispatch, with 26 listing-neutral actions exempted and everything else treated as a mutation, fail-safe). The `/Content/...` alias is now mapped before path sanitization, which fixes a deleted asset lingering in the cache for the full TTL because sanitization threw on the unaliased path and the catch swallowed it. A diagnostics snapshot reader with a 64 KiB cap and an allowlist projection (tokens, paths and session ids dropped) joins the automation client, though nothing on the production path reads it yet.

#### Handlers, catalog and native routing

- **User-defined action aliases** — a project can add its own action names in `handler-aliases.json` (schema `Resources/MCP/custom-handler-aliases.schema.json`): version 1, at most 128 aliases and 64 KB, `lower_snake_case` names only, resolved through three search paths. An alias pointing at another alias, or at the protected `inspect`/`manage_tools` actions, is rejected, and an alias whose target is not registered yet stays pending until the target arrives.
- **Geometry dynamic-mesh authoring** — `manage_geometry` gained procedural-mesh authoring through one `edit_dynamic_mesh` family: `create_procedural_mesh`, `append_vertex`, `append_triangle`, `delete_vertex`, `delete_triangle`, `get_vertex_position`, `set_vertex_position`, `set_vertex_color`, `set_uvs`, `split_normals`, `translate_mesh` and a boolean `difference`.
- **AI authoring routes** — `set_ai_perception`, `create_nav_modifier` and `set_ai_movement` join behaviour tree, blackboard, EQS query, MassEntity, SmartObject, StateTree, navigation and nav-actor configuration as named actions instead of only through the behaviour-tree umbrella.
- **Native search matches words, and pages** — the native gateway search matcher scores word-level matches and accepts `actionOffset` and `maxBytes` so a large action list can be paged.

#### Fab asset-store bridge

- **`McpAutomationBridgeFab` delay-loaded module** — a second editor module that bridges the Fab asset store through the Fab plugin's own browser widget and download API (`McpFabBrowserBridge`, `McpFabSearchOperation`, `McpFabDetailsOperation`, `McpFabAddToProject`, `McpFabImportWatcher`). Its Fab and Megascans engine dependencies are declared optional and listed in `PublicDelayLoadDLLs` on Win64, so the module compiles away when they are absent. The store actions themselves are dispatched by the main module's `Private/Domains/AssetWorkflow/`: `search_fab_listings`, `get_fab_listing_details`, `add_fab_asset_to_project`, `download_fab_asset`, `list_fab_downloads`, `list_fab_library`, `list_megascans_library`, and `import_megascans_asset`. Two further actions, `list_content_sources` and `migrate_assets`, are generic content ingestion (engine templates, engine and plugin content, downloaded Bridge packs) rather than Fab-specific, and a migrate request never carries a filesystem path: it names a root token plus a relative id, both resolved against a fixed root table.
- **Diagnostics snapshot store** — `Foundation/Diagnostics/` ships a bounded, crash-tolerant singleton that records request admission, pre-dispatch, refusal, terminal, handshake, disconnect, and session events to `<Project>/Saved/MCP/diagnostics/`. Atomic file writes with temp+rename, previous-session rotation, corrupt-file tolerance with one-shot bounded warnings.
- **Reflected function invocation** — `Foundation/Reflection/McpReflectedInvoke` provides a shared RAII parameter-block marshalling primitive for arbitrary UFunction invoke, gated behind `effect: destructive` + `consent: elevated` on both `control_editor.invoke_reflected_function` and `control_actor.call_actor_function`.
- **`native-gates.ps1`** — PowerShell script for local native compile and smoke gates (`npm run native:compile`, `native:smoke`, `native:smoke:core`, `native:smoke:fab`, `native:check`), so a non-compiling C++ security control can never pass CI.

#### Source control and project setup

- **A project can be put under revision control from the tool** — `source_control_checkout` and `source_control_submit` shipped, but the step that has to happen first had no action at all, so on a project that had never been committed both answered `SOURCE_CONTROL_DISABLED` and the only way forward was the editor's Revision Control login dialog: the exact UI an automation caller is replacing. `source_control_init` creates the repository, writes an Unreal `.gitignore`, makes the first commit and selects the Git provider; `source_control_commit_all` snapshots everything after that. `source_control_enable` had been dispatched by the plugin since the source-control handlers were written, but no capability record ever published it, so the gateway rejected the action name outright — an implemented, registered, documented handler nothing could call. Both new actions run git on a worker thread and hop back to the game thread to answer, because `git add` over a full Content tree takes minutes and `ExecProcess` blocks its caller: on the game thread the editor stops pumping and the bridge socket looks dead. The `.gitignore` is not cosmetic — without it the stage walks roughly 10 GB of `Intermediate/`, `Saved/` and `Binaries/`. "Nothing to commit" reports `alreadyClean` rather than an error so a caller snapshotting on a timer sees no spurious failures, and every git invocation is returned in `steps[]` so a failure names the command that failed.

#### Widget styling and receipts

- **`set_style` can round a widget's corners** — every UMG panel this tool could author was a hard-edged rectangle and nothing on the published surface could change that, so a request to polish a UI had no answer short of hand-editing the asset. `cornerRadius` (plus optional `outlineColor` and `outlineWidth`) now applies to whichever brush the widget actually draws with: an Image's `Brush`, a Border's `Background`, or **all four** of a Button's normal/hovered/pressed/disabled brushes — rounding one alone makes the corners snap square under the cursor. A widget with no brush refuses with `STYLE_FIELD_UNSUPPORTED` and names the three classes that have one, instead of falling through to the generic reflection path that would answer success without changing anything.
- **Every receipt reports the world the request ran against** — an actor mutation reported success for whichever world was current at that instant, so if a level load then replaced that world the actor was unreachable and the receipt gave no hint anything had changed underneath it; the failure looked like the mutation never happened. Receipts now carry `worldName`.
- **Transport diagnostics name the dispatched capability** — `tools/call` logged only the parent tool, so an editor death left behind `tool=control_editor`, one of twenty possible actions, with no way to attribute the crash. The dispatched capability and at-cap session evictions are now logged.

</details>

<details>
<summary><b>🔧 Changed</b></summary>

- **Static `unreal` gateway tool replaces the 23-tool public surface.** The TypeScript stdio and native MCP transports now permanently expose a single `unreal` tool. The 23 canonical parents (`manage_asset`, `control_actor`, …) are internal and reachable only through `search`, `describe`, `execute`, and `configure`. This reduces client context pressure and eliminates hallucinated tool/action calls.
- **C++ plugin reorganized into per-domain modules** — `Private/` is now split into `Core/` (errors, requests, security, subsystem), `Domains/` (**66** domain directories), `Foundation/` (blueprint, bridge helpers, handler utils, capability authorization, idempotency, compensation), `MCP/` (transport, routing, execute, gateway, primitives, resources, dynamic tools, generated shards, tools), `Safety/`, and `Transport/`. The former per-tool `McpTool_*.cpp` files and the `McpNativeTransport.{h,cpp}` monolith are gone, replaced by generated registries and `Private/MCP/Transport/`.
- **`Private/Safety/` split into per-operation headers** — asset save, level save, map load, folder delete (assets/verify), animation delete, delete quiesce/compilation, world delete, package tools, material, and classification each have their own header; `McpSafeOperations.h` survives only as a short umbrella that includes them.
- **TypeScript tools reorganized** — the monolithic `src/tools/handlers/*-handlers.ts` files were replaced by **37** per-domain directories (**203** files, up from 64 flat ones at `v0.5.30`), and `src/tools/` now holds `catalog/` (capability records), `definitions/` (tool schemas), `orchestration/` (the canonical dispatcher, routing, and generated routing index), `dynamic/`, and `handlers/`. The former `src/tools/editor/` and `src/tools/level/` class modules had no live caller and are gone; the live code is `src/tools/handlers/editor/` and `src/tools/handlers/level/`. Types were split into `src/types/handlers/` and `src/types/tools/`; utilities were regrouped into nine `src/utils/` areas (`commands`, `paths`, `responses`, `validation`, `collections`, `serialization`, `logging`, `config`, `interaction`).
- **Automation bridge split into focused modules** — `src/automation/` now separates the client, config, frame codec, state, status, connection lifecycle, request dispatcher, request context/correlation, cancellation errors, capability-token provider, log redaction, the gateway consent/correlation/timeout/expected-revisions contexts, the read-only diagnostics snapshot reader, and the natural-timeout cancellation path.
- **`control_actor` spawn is transactional** — a requested `meshPath` that can't be applied no longer leaves a misconfigured actor in the level. It fails `MESH_NOT_FOUND` before spawning if the mesh can't load, or rolls back (`Destroy()` + `MESH_APPLY_FAILED`) if a resolved mesh can't be applied.
- **Console-command validation is generated** — the allow/deny model now lives in `src/utils/commands/console-command-policy*.ts` with a generated policy artifact and a `policy:check` drift gate, instead of a hand-maintained validator.
- **Minimum Node.js raised to `>=20.19.0`** (was `>=18`).
- **Structural scale of the reorganisation** — `src/` grew from 154 files to 832, the plugin's `Private/` tree from 133 `.cpp`/`.h` files in one flat list to 1,530 files under `Core/`, `Domains/`, `Foundation/`, `MCP/`, `Safety/`, `Tests/`, `Transport/` and `UI/`, and `src/tools/handlers/` from 64 flat files to 203 files in 37 domain directories.
- **`MCP_DEFAULT_CATEGORIES` is inert** — setting it now logs a warning and changes nothing: the public surface is the single gateway tool, so there is no category listing left to filter.
- **ESLint enforces the Node floor** — `eslint-plugin-n`'s node-builtins and ES-syntax rules run at `warn` (and therefore fail CI's `--max-warnings=0`), so an API above Node 20.19 cannot land unnoticed.
- **`manage_tools.list_tools` / `get_status` contract change** — the `description` field is dropped from the row shape, and `catalogRevision` plus `catalogStateRevision` are reported instead of hardcoded counts.

</details>

<details>
<summary><b>🛡️ Security</b></summary>

- **Scopes are exact-set membership with an `Admin` wildcard, not rank-based** — `Write` does **not** imply `Read`, and an unresolvable capability demands `Admin`.
- **Consent rides as an `automation_request` envelope sibling, never a handler param**, and is re-validated plugin-side. It is never inferred from loopback, a prior call, idempotency, or preview.
- **Capability-token auth is on by default** — tokens compare in constant time and are never logged; the plugin re-enforces every check the TypeScript layer performs.
- **Path handling routed through a shared canonicalizer** — paths are limited to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara` plus sanitized additions, with the `/Content` alias handled in one place instead of re-implemented per handler.
- **Render/media output hardening** — continuous local output-path validation against symlink replacement, and network-backed media URLs disabled because redirect destinations cannot be pinned.
- **Native transport hardening** — client-scoped rate limits retained across native MCP session rotation, strict native `manage_tools` argument validation, and sanitized streamed log payloads.
- **An `action`/`subAction` mismatch can no longer authorize one capability and execute another.** `AuthorizeAutomationRequest` normalizes any payload that declares both with different values (overwriting `action` from the authoritative `subAction`), the native execute stage stamps `subAction` from the server-resolved action, and the pre-queue gate resolves its demand with the same `NormalizeAction` the dispatchers call, so the gate and the dispatcher cannot disagree.
- **Scoped tokens are narrower than the legacy token by construction** — a scoped token may list only `Read`/`Write`/`Destructive` (never `Admin`), and a scoped token colliding with the legacy token wins because the narrower grant applies.
- **Reflected property access cannot reach the plugin's own settings** — `McpSafeReflectionTarget` refuses every `/Script` target with `OBJECT_NOT_ADDRESSABLE`, deliberately distinct from `OBJECT_NOT_FOUND`, from the array, element, insert and append property handlers, so a path- or project-restricted principal cannot reach the settings that govern the transport itself: a `write`-scoped principal could otherwise switch off `bRequireCapabilityToken` through `inspect.set_property`, and a `read`-scoped principal could read the Admin token out. `/Game`, `/Engine`, spawned actors and Blueprint CDOs are unaffected.
- **URL-looking arguments are refused outright** — handler URL validation rejects every URL form, including loopback and `file:` URLs, rather than trying to allow a safe subset.
- **Parameter gates use own-property lookups** — `hasOwn()` replaces an inherited-property check, so `__proto__`, `constructor` or `toString` cannot slip past an `additionalProperties` or dispatch gate, matching the native `TMap` lookup.
- **Token resolution and log redaction on the TypeScript side** — the bridge re-reads the token file on every `bridge_hello` and fails closed when it cannot resolve one, and `AutomationLogger` redacts tokens, paths and handshake metadata rather than trusting callers to keep them out.

- **A refused call no longer eats the caller’s consent grant** — the pre-queue gate burned a single-use consent nonce BEFORE the handler ran, so a call the handler then refused (a misspelled component name, a path that resolved to nothing) spent the grant on work that never happened; the retry came back `CONSENT_REUSED` and the caller had to re-run describe for a nonce, for a call that changed nothing. The burn now registers against the request and the single response funnel hands it back on failure and forgets it on success, so replay protection is unchanged: a grant that actually did something stays spent.
- **The param-scoped describe mints a consent grant too** — `describe {tool, action}` returned a contract plus a single-use `consentGrant.nonce`, while `describe {tool, action, param}` returned the per-parameter schema and no grant at all, even though it names the same capability under the same consent policy.
- **A cold-boot session cannot be rehydrated without the capability token** — `ValidateSession` rehydrated a session id predating the current transport instance without checking any credential, so a session id surviving a transport restart was accepted on its own. The plugin is the sole authority for auth and re-enforces it on this path as on every other.
</details>

<details>
<summary><b>🛠️ Fixed</b></summary>

#### Component trees and struct members

- **`attachTo` in a batched `edit_scs` actually attaches.** The parent search matched the requested name against the *exported text* of an `FSubobjectDataHandle` — an opaque id that never contains a component name — so it always fell through to the first handle it had, the root. Fourteen body parts landed on the collision cylinder while every op reported success. The parent is now resolved by name after the node exists, and an `attachTo` that cannot be resolved fails that op with a reason instead of being dropped.
- **Inherited components are addressable from the batch path.** A Blueprint's own SCS is only half its component tree: anything inherited from a native parent (ACharacter's `Mesh`, `CapsuleComponent`, `CharacterMovement`) lives on the CDO with no `USCS_Node`. `modify_component` answered "Component not found or template missing" for components the Blueprint plainly has, and `reparent_scs_component` answered `SCS_PARENT_NOT_FOUND` for a reparent the editor does with a drag. Both now resolve through the CDO, and a node parented to a native component records it the way the editor does.
- **`add_struct_member` honours the `members` array its own contract declares.** Only the single `memberName`/`memberType` pair was ever read, so the array form was refused with `MISSING_PARAMETER` and a ten-field struct cost ten calls. The array is validated as a whole — a bad entry refuses the batch rather than half-building a struct, because a partially applied member list is worse than none.

#### Editor capture, actor search and graph authoring

- **`control_editor.screenshot` can photograph a minimized editor again.** The window enumeration filtered minimized windows out entirely, so once the editor minimized itself (it does so on launch and after some PIE cycles) the main frame was absent from `windows[]` and unaddressable by index *or* title: `full_editor_window` answered `EDITOR_WINDOW_NOT_FOUND` with an empty window list and no in-tool way back. Minimized windows are now listed with `isMinimized`, and a minimized capture target is restored with `SW_SHOWNOACTIVATE` + `SWP_NOACTIVATE` before the capture so it never steals the user's focus or cursor; the response reports `windowRestored`.
- **`control_editor.take_screenshot` with `mode: "game_viewport"` no longer answers `NOT_IMPLEMENTED`.** The UI handler gates on the payload's own `subAction`, which still carried whichever alias the caller used, so the published `take_screenshot` spelling fell past the screenshot branch. The forward now names the canonical action.
- **`control_actor.find_actors_by_class` refuses a class name that does not resolve** instead of reporting "Found 0 actors". A Blueprint short name such as `BP_Thing_C` read as an empty level rather than as the typo it was; the refusal is `CLASS_NOT_FOUND` and names the generated-class path form that works.
- **`add_node` resolves the StandardMacros aliases that `create_node` already did.** `ForEachLoop`, `ForLoop`, `DoOnce`, `Gate` and friends are Blueprint macros, not `UK2Node_*` classes, so the same `nodeType` answered `UNSUPPORTED_NODE` on one action and succeeded on the other.
- **An unknown container type says how containers are spelled.** `TArray<Text>` and `Text[]` — the spellings a C++ author reaches for — produced a bare `Unknown type`, with no hint that the resolver takes `Array<T>`, `Set<T>` and `Map<Key,Value>`.
- **`set_widget_layout` applies every canvas-geometry field the call carries.** `layoutProperty` selects which one names the variant; `position`, `size` and `zOrder` sent together used to have two of the three silently dropped, so a widget landed in the right place at the default size behind everything else. The response lists what was `applied`.

#### Cinematics, render and replay

- Replay seek and killcam responses now wait for measured completion instead of returning optimistically.
- Movie Render Queue ownership is preserved through cancellation and held until executor settlement.
- Take Recorder panel/source state is restored after asynchronous start failures.
- Render limits are validated before queue mutation rather than after.
- Render output proof is token-aware, so tokenized output filenames verify correctly.

#### Engine compatibility

- **Source compatibility restored across the supported UE 5.0–5.8 range.** Several engine APIs and relocated headers were used without guards, and several existing guards named the wrong engine boundary, so the plugin failed to compile on parts of the range it advertises. Header selection now probes with `__has_include` instead of hard-coded version numbers wherever the engine moved a header, and the remaining guards were corrected against the engine source. Affected areas: the StructUtils headers, `FAssetCompilingManager::FinishCompilationForObjects`, `UWidgetBlueprint::WidgetVariableNameToGuidMap`, `CreateNewIKRigAsset`, `FString::RightChopInline`/`LeftInline`, and `PhysicsEngine/SkeletalBodySetup.h`. A redundant `UObject/StrProperty.h` include was dropped (`FStrProperty` comes from the already-included `UObject/UnrealType.h`). Three further shims were added for APIs that differ across the supported range (`MCP_SET_ENUMS`, `MCP_HAS_GET_OBJECTS_FLAGS`/`MCP_GET_OBJECTS_NO_NESTED`, `MCP_DISALLOW_SHRINKING`), and the `MCP_HAS_IKRETARGETER_SET_IKRIG_ENUM` guard was reversed because the boundary it named sat on the wrong minor.
- **JSON key-type change handled across the plugin** — 14 files move from `FJsonObject::Values` lookups with an `FString` key to `HasField()`, the ambiguous `TEXT("ReadOnly")` comparison is qualified, and the diagnostics filename no longer trips C2084.
- **Fab API conformance** — the adapter follows the engine's current Fab API surface, and `bCompileForEdit` (a member added in 5.6, absent on 5.5) is guarded in the shared Niagara stack-issue collector.
- **Render console handler** — use `FJsonObject::HasField()` instead of `Values.Contains(FString)`, following the `FJsonObject::Values` key-type change.
- **Asset soft-path fallback returned the wrong string shape** — `MCP_ASSET_DATA_GET_SOFT_PATH` used `PackageName` (`/Game/Foo`) where callers expected an object path (`/Game/Foo.Foo`). It now uses `FAssetData::ObjectPath`, the equivalent of the `GetSoftObjectPath()` used in the other branch.
- **Clean build fixed** — the memreport scan passed `256` as a seventh argument to `IFileManager::FindFilesRecursive`, but that parameter is `bClearFileNames`, not a result ceiling, so the call did not compile. The bound was dropped rather than reworked: truncating is also wrong here, since picking the newest of an arbitrary subset can miss the actual newest report. A real traversal bound would need `IterateDirectoryStatRecursively`.
- **Last source warning cleared** — `FLinearColor ColorValue;` left its channels uninitialized in the material-parameter track handler, and the only writer runs on one branch, so the compiler could not correlate the write with the guarded use and warned C4701. Seeded to opaque black, matching `ReadLinearColor`'s own defaults.

- **UE 5.8 string-literal compilation in the Fab module** — `McpFabAddToProject.cpp` failed to compile against UE 5.8’s stricter string-literal handling. Contributed by [@punal100](https://github.com/punal100) in [#639](https://github.com/ChiR24/Unreal_mcp/pull/639).

#### Handlers and routing

- **Asset listing accepted the wrong field name** — the asset-listing resource read `directory` where its schema declares `path`; the declared name now works.
- **`validate_niagara_system` reports real errors** — it previously hard-coded `isValid=true`. It now builds a full Niagara system view model and harvests stack issues (e.g. "The module has unmet dependencies.") across the system and emitter stacks. A data-processing-only view model cannot be used because `UNiagaraStackModuleItem::RefreshIssues()` emits no per-module issues in that mode.
- **IK Rigs created on the `NewObject` fallback path are registered with the asset registry** — `FAssetRegistryModule::AssetCreated()` is now called explicitly on that branch, which the static factory does for us on engines that have it. Without it the rig existed on disk but was unregistered, so it never appeared in the Content Browser until an unrelated rescan happened to pick it up: the asset looked lost even though creation had reported success.
- **Widget GUID registration logs a truthful no-op** — `RegisterWidgetGuid`, `UnregisterWidgetGuid`, and `RegisterAnimationGuid` each logged "registered"/"unregistered" on engine versions that have no `WidgetVariableNameToGuidMap`, claiming work they had not done. On those versions the engine owns the widget variable's GUID in `UBlueprint::NewVariables[].VarGuid`, and writing our own would overwrite a value existing bindings resolve through — so a no-op is correct, it just has to say so.
- **Bare `remove_variable` / `rename_variable` match on the native transport** — the Blueprint variable removal/rename handler matched only the `blueprint_`-prefixed forms, so the bare action names fell through unhandled. Both the snake_case (`remove_variable`, `rename_variable`) and alphanumeric-lowered (`removevariable`, `renamevariable`) bare forms are now accepted alongside the prefixed ones.
- **Every texture call was failing** — `action` is injected by the consolidated routing layer (`WithPayloadSubAction`) as the legacy dispatch verb, but it is not a client parameter and was absent from the handlers' `ValidParams` allowlists, so schema-valid texture calls were rejected with `TEXTURE_ERROR: Invalid parameter: action`. Added to all five affected handlers (gradient, noise, normal, pattern, resize).
- **`ListenPorts` drop warning** — when multi-listen is on and a partial `ListenPorts` override omits a default bridge port (8090/8091), a warning is logged instead of the drop being silent. The user's ports stay authoritative.
- **TS and native responses share one frame shape** — `normalizeAutomationFrame()` gives the TypeScript bridge the native `structuredContent` envelope, which fixed closed-output-schema failures across the whole 379-record catalog.
- **Blueprint macro nodes and material roots resolve correctly** — `ForLoop`, `ForLoopWithBreak`, `WhileLoop` and `ForEachLoop` were removed from the `K2Node_*` alias map (`ForEachLoop` was wrongly aliased to `K2Node_ForEachElementInEnum`), so the bare names reach the bridge and `TryCreateMacroNode` builds a real `K2Node_MacroInstance`. Material root targets (`root`, `output`, `materialoutput`, `materialgraphnoderoot`, `…_Root_<n>`) canonicalize to one sentinel with normalized output-pin casing for `connect_nodes`, and `propertyValue` is accepted as an alias of `value`.
- **Domain fixes surfaced by the sweep** — a property conversion that cannot coerce now answers `PROPERTY_CONVERSION_FAILED` with `partial: true` for the fields it did apply, unknown World Partition actions answer `UNKNOWN_ACTION` instead of falling through, `modify_scs` reaches the property-applying implementation, `advance_simulation` advances `steps` ticks once instead of `steps` squared, `simplify_mesh` no longer divides by zero on an empty mesh, and the pipeline status report states what it measured instead of a hardcoded value.

#### Live-editor sweep (2026-09-16 to 09-18)

- **A compile no longer pins a dead world** — the editor died with "Fatal World Leaks" several calls *after* the compile that caused it: compiling a Blueprint reinstances its live instances, the originals become garbage, and anything of them still sitting in the transaction buffer keeps the owning world alive. The first full GC — which the game itself triggers on its first `OpenLevel` — then took the whole editor down. The buffer is now cleared at the compile, but only when that Blueprint is actually pinned (the undo buffer references it, or it had live instances). Losing undo history beats losing the editor, and the receipt says which happened and why.
- **`add_node` no longer wires unrelated nodes into `Event Tick`** — a graph-wide exec-link sweep ran after *every* `add_node`, walking every node in the graph and connecting any `VariableSet` or `CallFunction` with a free exec input to the graph’s "preferred event". Adding one node could silently hang nodes the caller never mentioned off `Event Tick` or `Event PreConstruct`; the only trace was an `execLinked` boolean the gateway projects away. In a live project this put an `Add to Viewport` with a null target on `Event Tick`, logging a Blueprint runtime error every frame in PIE.
- **A synthetic click actually presses the button** — `mouse_click` reached the right widget and reported `handledBySlate: true` while the button never fired. Slate recomputes hover every frame from the *real* cursor, so a hover set by a separate `mouse_move` call was gone before the next request arrived, and `SButton` only raises `OnClicked` when the release lands on a widget it still considers hovered. The click now carries its own move in the same dispatch and borrows the hardware cursor for the press/release, putting it straight back where the person left it — a single-frame blip rather than parking the cursor on the target, which is what made automation unusable alongside other work.
- **`add_scs_component` routes through one implementation** — it had THREE payload readers (the batch `operations[]` path, `HandleScsAddComponent`, and a third copy inside `HandleBlueprintScsWrappers` that sat earlier in the route table and so answered every call). The wrapper read only `parent_component`/`parentComponent`, so `attachTo` was dropped and a component asked for `attachTo: "Mesh"` still landed on the collision cylinder, reported as success. Verified live: `attachTo: "Mesh"` on a Character now reports parent `Mesh`/`CharacterMesh0`.
- **Attaching to inherited components works** — parent resolution searched SCS nodes only, so on a Character every spelling of the inherited capsule and mesh failed. "Attach a weapon, light or camera to the character’s skeletal mesh" is the most common Blueprint task there is and it had no reachable path.
- **`MacroInstance` is refused instead of building a broken node** — `nodeType: "MacroInstance"` is the node *class*, not a macro; it fell through to the generic path, spawned a `UK2Node_MacroInstance` with no macro graph attached, and reported "Node created." on a Blueprint that no longer compiled. The spellings that actually resolve (`ForEachLoop`, `ForLoop`, `WhileLoop` and friends) are now named, and nothing is built otherwise.
- **`CreateWidget` nodes carry their class** — both authoring paths wrote a `WidgetType` UPROPERTY that `UK2Node_CreateWidget` does not have, so the reflection write did nothing while the call answered "Node added"; the Blueprint then failed to compile with "Spawn node Create Widget must have a class specified". The class lives on the node’s `Class` input pin, which is now written and reconstructed.
- **`GetVariable` nodes bind their member** — `add_node` with `nodeType: "GetVariable"` reported success and a `nodeGuid` and produced a node with ZERO pins, because the schema publishes `memberName` and the handler read `variableName`; any later `connect_pins` then failed with `PIN_NOT_FOUND`, pointing at the wrong problem.
- **Behavior Tree authoring no longer crashes the editor** — `SpawnMissingNodes()` was called on an already-populated graph, which the engine only ever does from `OnCreated()`; a BTGraph with no nodes (exactly what authoring a tree over the bridge produces) also hard-asserted.
- **Landscapes are created with components** — `create_landscape` produced an `ALandscape` with ZERO components: no geometry, bounds, collision or surface to sculpt, paint or stand on. `ALandscape::Import()` is what allocates the `ULandscapeComponent`s; `SetHeightData()` only writes into components that already exist.
- **Volumes are created with real extents** — the box brush was built on a volume whose `UModel`/`UPolys` had never been allocated, so every volume came out with bounds `{0,0,0}`: a `NavMeshBoundsVolume` enclosing no navigable area, a `PostProcessVolume` affecting nothing.
- **Writes that persist instead of echoing** — `set_world_settings` wrote the transient `WorldGravityZ` cache and enabled the override without touching `GlobalGravityZ`, discarding the requested value *and* pinning world gravity to 0; `create_interactable` accepted a full door/chest behaviour spec and stored none of it; interaction widget/component settings, switch and trigger config, `edit_blackboard.add_key`’s `baseObjectClass`, `create_skeleton`’s `name` and `paint_foliage_instances`’ `radius`/`density` were all accepted and dropped.
- **Struct and DataTable round-tripping** — every struct authored over MCP carried a permanent junk `MemberVar_0` that appeared in every row built on it; `update_row` replaced instead of merging, so a call setting two fields silently wiped the other thirteen, from an action whose name promises the opposite.
- **Closed output contracts stopped hiding handler data** — a field a handler emits but its record does not declare is projected away in silence, which accounted for a whole class of "the data is missing" findings where the data was never missing: sequence `add_actors`/`remove_actors` results and counts, `blueprint.get_scs` inherited components, `blueprint.connect_pins` pin names/types and `saved`, material node placement telemetry (including `overlappingNodes` and `placementWarning`, previously wired to three handlers out of sixteen), and `invoke_function`’s resolved target and `value`.
- **Success is no longer reported over a no-op** — `configure_volume` with properties the class does not carry, a `.t3d` import that imported nothing, a legacy input mapping removal that matched nothing, `add_foliage`’s `scatter` variant (which creates a `UFoliageType` and places zero instances), and UMG animation looping (which has no persisted setting at all — looping is a `PlayAnimation()` argument) each answered success while doing nothing.
- **`inspect` reads Blueprint variable defaults from the CDO** — `FBPVariableDescription::DefaultValue` is a legacy string that stays empty for every variable whose value was written to the CDO, which is where the Blueprint actually stores it.
- **Niagara module stack errors reach `warnings[]`** — `add_niagara_module` reported status `success` with an empty `warnings[]` while parking `stackErrors: ["The module has unmet dependencies."]` in details.
- **Texture create actions accept the folded `kind` discriminator** — the handlers validate against an explicit allowlist and `kind` was missing from all five, so the variant discriminator every caller of a folded action must send was rejected.
- **`save` is published on the struct and DataTable write actions** — both handlers had always read it; no capability record declared it, so no caller could reach working native support.
- **Mojibake repaired in source comments** — the same Windows-1252 round-trip that mangled `CHANGELOG.md` left double-encoded em dashes in 24 places across 13 files.

- **The bridge reports its target, and resolves the project’s own port** — a `NOT_CONNECTED` failure did not say what it had tried to reach, and a project that does not pin `MCP_AUTOMATION_PORT` was not consulted for its own setting. `readProjectListenPort()` now reads the first `ListenPorts` token from the project config (the plugin binds every configured token in order and a busy port silently drops out of the set, so the first token is the one to trust), and `describeBridgeFailure()` classifies the cause from structured transport codes rather than message text, which a peer controls. Contributed by [@punal100](https://github.com/punal100) in [#640](https://github.com/ChiR24/Unreal_mcp/pull/640).

#### Fab asset store

- **Fab search no longer hides results** — the search call was pinned to `channels=unreal-engine`, which hid the whole Megascans library; the pin is gone, so the public catalog is discoverable and importability is settled at add time instead.
- **`add_fab_asset_to_project` claims the listing first** — it performs a real `POST /add-to-library` (the same change Fab's own UI makes when you press Add to Project) because Fab answers 404 for a download the account does not own; it then picks the first importable format, and success is decided by the asset registry rather than by Fab's response.
- **Typed Fab failures** — `FAB_NOT_READY`, `FAB_REJECTED`, `NO_IMPORTABLE_FORMAT` and `IMPORT_TIMED_OUT` replace generic refusals, so a caller can tell a missing Fab plugin from a rejected download, an unimportable format, or an import that ran out of time.

</details>

<details>
<summary><b>🗑️ Removed</b></summary>

- **Gateway-mode opt-outs and the legacy 23-tool listing (permanent single-tool cutover).** The TypeScript `MCP_GATEWAY_MODE` env var and the native **Enable Native Gateway** (`bEnableNativeGateway`) project setting are both gone. The private 23-parent dispatch and legacy action mappings stay inside `unreal.execute`. `MCP_AUTOMATION_CLIENT_MODE` is unaffected — it remains a separate WebSocket client/server topology control.
- **Superseded TypeScript modules** — the root-level `src/tools/consolidated-tool-handlers.ts`, `property-dictionary.ts`, `tool-definition-utils.ts`, `src/tools/editor.ts`, `src/tools/level.ts`, `src/tools/schemas/core-tools.ts`, and the monolithic `src/tools/handlers/*-handlers.ts` set, all replaced by the catalog and per-domain directories. `src/tools/orchestration/consolidated-tool-handlers.ts` survives as the bootstrap/export facade, and `src/tools/dynamic/dynamic-tool-manager.ts` is still live: gateway availability and the execute static stage both gate on `isToolEnabled()`, `configure` mutates it, and it backs the local `manage_tools` category state rather than a per-tool public listing.
- **Superseded plugin sources** — the per-tool `McpTool_*.cpp` definitions, `McpDynamicToolManager.cpp`, `McpConsolidatedActionRouting.h`, and the `McpNativeTransport.{h,cpp}` monolith.
- **Dead code sweep (2026-09-05)**: the legacy `src/tools/editor/` and `src/tools/level/` class modules (only their own unit tests imported them; the live paths are `src/tools/handlers/editor` and `src/tools/handlers/level`), the orphan `src/tools/handlers/niagara/` handler (Niagara authoring is served by `effect/effect-niagara-actions.ts`), the stale `material-authoring-types.ts` copy of `material-authoring-common.ts`, the unused `src/types/index.ts` and `src/utils/index.ts` barrels, 33 exported functions and constants with no callers, the security PoC harness under `tests/unit/_poc_security/`, the orphan `evidence-aggregator.mjs`, the `.jules/` sentinel notes for code that no longer exists, and the `lint:c` / `lint:csharp` npm scripts. The generated `McpNativeGatewayManifest.h` (298 KB, never included by any translation unit since the native gateway moved to the generated registry) is no longer emitted; `generate-gateway-manifest.ts` now writes only the TypeScript and JSON manifests.
- **Deep cleanup continuation (2026-09-06)**: `UnrealCommandQueue` lost its unused `retryPolicy` recovery path (no caller ever passed one; a failed command is never re-run), the retired `route:effect:shadowed_stubs` disposition and its `EFFECT_MODULE_ROUTING` evidence path are gone (76 non-public routes, 7 typed removals), the unused `UE_EDITOR_EXE` / `UE_SCREENSHOT_DIR` env keys and the `src/types/tools/tool-*.ts` type modules were dropped, `toFiniteNumber` moved into `type-coercion.ts`, record builders share `SCHEMA_URI`, `V5_0`, `V5_8_P1` and the `str`/`num`/`bool` schema props, and generator scripts share `writeManifestTargets`. `docs/handler-mapping.md` names the files that actually own the foliage, property and sequence-metadata handlers.
- **Second removal pass (2026-09-06)**: the never-wired semantic value grammars (`semantic/frame-time.ts`, `geometry.ts`, `pagination.ts`, their parse helpers and the schema-only test) are gone; `save-policy.ts` and `property-assignment.ts` keep only the schemas the envelope and execution-options modules actually import. Dead exports dropped: `resolveAlias`, `PrimitiveHandler`, `AutomationMessageSchema`, `VerbFamily`, the unused per-parent record aliases, three `z.infer` aliases in execution-options and the six unused `*Response` interfaces. `animation_physics.list_bones` now declares the bone objects the plugin really emits (name, index, parentIndex, parentName, location) instead of `string[]`, so the gateway no longer refuses its result.
- The `typescript@^6` `overrides` block in `package.json`, alongside the toolchain pinning described under *Dependencies*.
- **The experimental in-editor ACP assistant panel is gone.** The separate assistant plugin subtree was removed from this tree and ships in no release; it survives only on feature branches. External consumers that drove the editor through it must target the native `/mcp` surface or the TypeScript stdio `unreal` gateway instead.
- **Superseded repo files (2026-09-06 and later)** — `GEMINI.md`, the root `mcp-config-example.json` and `claude_desktop_config_example.json` (with their `.github/labeler.yml` globs), `tests/heartbeat-progress.test.mjs`, `tests/integration/get_ai_info_characterization.mjs`, `tests/unit/tools/level_security.test.ts`, the unreferenced `Public/Plugin_setup_guide.mp4`, and the local-only `docs/native-automation-progress.md` progress log.

</details>

<details>
<summary><b>⚠️ Migration</b></summary>

- **Direct canonical tool calls are breaking.** Any client calling a canonical name directly (`tools/call` with `name: "manage_asset"`, `name: "control_actor"`, …) now receives a `DIRECT_TOOL_CALL_REMOVED` receipt instead of a result. Update call sites to the `unreal` gateway: `search` to find capabilities, `describe` for the exact action/parameter contract, then `execute` with `tool`, `action`, and `params`. There is no opt-out — `MCP_GATEWAY_MODE` and **Enable Native Gateway** are removed and there is no legacy listing to restore. The receipt's `nextCall` is executable and re-runs the original request through the gateway.
- **`manage_post_process` is folded into `manage_render`.** The `Render/McpAutomationBridge_RenderPostProcess*.cpp` files dispatch through `manage_render`; any client calling `manage_post_process` directly now fails with `does not match prefix`. Switch to `manage_render` and pass the desired sub-action via `subAction`. The reflection-capture resolution setter was renamed from `configure_capture_resolution` to `configure_reflection_capture_resolution`; the scene-capture path keeps the original name. `McpAutomationBridge_RenderHandlers.cpp` is now a 74-line dispatcher, with per-concern handlers under `Render/McpAutomationBridge_Render*.cpp`.
- **`control_actor` spawn with an unresolvable `meshPath` now fails.** A request that previously still produced a spawned actor and a success response now returns `MESH_NOT_FOUND` and spawns nothing.
- **Node.js `>=20.19.0` is required.** Node 18 is no longer supported.

</details>

<details>
<summary><b>🧪 Tests & CI</b></summary>

- **CI gate order is now asserted** by `tests/unit/workflow_gate_order_contract.test.ts`. The pipeline runs: `eslint --max-warnings=0` → `type-check` → `test:unit` → `registry:check` → `normalization:check` → `manifest:check` → `policy:check` → `test:params` → `migration:check` → `primitives:check` → `security:check` → `eval:check` → `version:check` → `workflow:check`, then a blocking `npm audit --omit=dev --audit-level=high` and an informational full-tree audit. A second matrix job (Node 20.19.x + 26.x) adds `build` + `test:smoke`.
- **Source-contract tests** in `tests/unit/plugin/*contracts.test.ts` read the C++ as text and assert required and forbidden patterns: pure-line ceilings, resolvable `Mcp*` includes, absence of split artifacts, constant-time token comparison, and no non-loopback bind without `bRequireCapabilityToken`.
- **Plugin-failure detection is word-bounded and content-scoped** — the generic indicator list that still includes the word `unknown` is unchanged, but a separate hard short-circuit list (which does not) runs before it, and the broad list now scans only the message and error strings rather than the whole response body, so a legitimate dispatcher message such as `"Unknown subAction."` no longer reads as a plugin crash. Real plugin errors are already caught by `isError: true` / `structuredContent.success: false`.
- **The test runner propagates failures via `throw`, not `process.exit(1)`** — it still sets `process.exitCode = 1`, but the error is rethrown so wrappers catching via `try`/`catch` or `Promise.all` see the underlying failure. Consumers that relied on the runner terminating the process from a `catch` block should switch to the rethrow contract.
- Added a `scripts/ci/unreal-job-gate.mjs` job gate and `scripts/qa/` adversarial, cross-transport-matrix, and capability-metadata audits.
- **The integration harness drives the gateway** — `tests/test-runner.mjs` exports `toGatewayCall()`, which rewrites every legacy `{tool, action}` case into an `unreal.execute` call (gateway options lifted into `options`, `action`/`subAction`/`params`/`consent` stripped out of `params`, case-level consent attached as the execute envelope sibling, all timeouts clamped to a 600 s ceiling). Assertions and capture selection moved to `tests/test-runner-response-utils.mjs`, and a missing `${captured:...}` value now throws instead of substituting a placeholder.
- **Crash detection is bounded** — the runner's crash and connection-loss signals moved from substring lists to word-boundary and bounded regexes (`hasCrashConnectionSignal`), with the bare `1006` code dropped as a standalone indicator and explicit close-code and not-connected matches added.
- **New test tiers and gate composition** — `security:check` runs `tests/unit/security` plus `tests/unit/adversarial`, `migration:check` also runs the gateway migration doc contract, and the suite gained adversarial fuzz/shrink/soak harnesses, evidence oracles, engine certification and readiness records, live drivers, cross-transport checks including dist freshness, and an offline native-discovery harness that compiles the real `McpNativeGateway*` sources and generated shards against a minimal engine shim.
- **Doc claims are machine-checked** — `tests/unit/docs/docs-claim-contract.test.ts` audits every published doc and the unreleased section of both changelogs against stale-claim rules (retired public tool surface, the removed in-editor assistant panel, unsupported protocol versions, unbacked certification and engine-range claims, stale capability-record counts), each with a negative control that proves the rule can fail.
- **CI job topology** — an opt-in `package-plugin` job (gated on the engine-root repo variable, with `MCP_STRICT_DEPRECATIONS=1`) and an opt-in `live-matrix` job (which builds and runs the Unreal integration suite on a labelled runner) join the always-on `unreal-optional-status` job that announces which Unreal-dependent jobs were skipped and why; four workflow contract tests plus the release-archive contract guard the pipeline.
- **Packaging writes a SHA-256 manifest and hardens the archive** — `scripts/lib/package-manifest.mjs` writes `McpAutomationBridge-v<version>-UE<engine>-<platform>.manifest.json` beside the archive, the archive additionally excludes `.cache/` and `DerivedDataCache/` and prunes `*.pdb`, `*.debug`, `*.sym` and `*.dSYM`, and a post-archive check fails the build if a generated build directory slipped in.

- **The catalog-import case got a timeout that fits it** — "inspect_cdo is in the tool schema action enum" cold-imports the generated catalog (the consolidated tool definitions plus all 380 records) inside the test body. That import alone runs past the 10s default under full-suite load, so the case failed on timing rather than content: it passed whenever the file was run on its own and failed in `npm run test:unit` regardless of what the rest of the change touched. Raised to 30s with the reason recorded beside it.
</details>

<details>
<summary><b>📚 Documentation</b></summary>

- **21 `AGENTS.md` files (20 area guides plus the root workspace guide)** now cover the tree: catalog, tools, handlers, gateway, MCP primitives, server, automation, utils, resources, types, plugin scope, plugin core/domains/safety/native-MCP/foundation/transport, tests, and the two test-area guides (`tests/unit/plugin/`, `tests/unit/mcp-primitives/`).
- **Gateway migration and protocol docs** — the permanent single-`unreal` surface on both transports (the former `MCP_GATEWAY_MODE` and **Enable Native Gateway** toggles were removed, not merely defaulted off), the `DIRECT_TOOL_CALL_REMOVED` receipt, `2025-11-25` negotiation with the `MCP-Protocol-Version` header guard, and the manifest generate/`--check` workflow.
- Published a generated action reference, migration map, and capability support matrix from the capability records.
- **The `bump-version` workflow rewrites all seven version sources** — `package.json` and `package-lock.json`, `server.json`, `McpAutomationBridge.uplugin`, `Resources/MCP/server-info.json`, the `src/server/server-factory.ts` fallback and the `McpNativeTransport.h` `ServerVersion` literal, then verifies with `npm ci && npm run version:check`. Release and plugin archives exclude `Binaries/`, `Intermediate/`, `Saved/`, `.cache/` and `DerivedDataCache/`, prune debug symbols, and fail the build when a generated build directory survives into the archive.

</details>

<details>
<summary><b>🔄 Dependencies</b></summary>

| Package | Change |
|---------|--------|
| `@modelcontextprotocol/sdk` | `^1.25.0` → pinned `1.29.0` |
| `eslint` | `^10.0.2` → pinned `9.39.5` |
| `@eslint/js` | `^10.0.1` → pinned `9.39.5` |
| `@typescript-eslint/{eslint-plugin,parser}` | `^8.4x` → pinned `8.63.0` |
| `typescript` | `^6.0.2` → pinned `5.9.3` (and the `overrides` block removed) |
| `@types/node` | `^25.0.2` → `^26.0.1` |
| `github/codeql-action/{init,analyze,autobuild}` | `4.37.9` → `4.38.0` (Dependabot) |
| `eslint-plugin-n`, `js-yaml` | added (dev) |

</details>

<details>
<summary><b>✅ Verification</b></summary>

- **Supports Unreal Engine 5.0–5.8.** The range is a source-compatibility target: per-version build and live-editor results are not asserted here. See [docs/performance-and-evidence.md](docs/performance-and-evidence.md) for the engine matrix and what each version's record actually shows.
- **Live-editor acceptance is not claimed for the TypeScript gateway build.** Gateway behavior, `2025-11-25` negotiation, manifest generation, and parity/parameter audits are verified through source-contract tests and the build, not against a running Unreal Editor. The integration suite (`npm test`) requires a live editor plus the bridge plugin and runs only in the opt-in `live-matrix` CI job; it is skipped by default rather than excluded. Do not treat any unexecuted live-editor proof as verified.

</details>

<details>
<summary><b>👥 Contributors</b></summary>

Special thanks to everyone who shipped code in this release window, with author aliases collapsed. Contributors whose work merged after the `v0.5.30` tag but is already credited in the 0.5.30 section below are not repeated here.

- **Editor-correctness sweep (the largest body of work in this release):** @SoloGorilla for ~70 commits across the inspect, actor, blueprint, material, metasound, PCG and asset handlers. Highlights: calls that reported success while dropping the write (`set_component_property`, `set_camera` discarding the requested position, material vector values written as opaque white), engine-ensure and editor-crash guards, path refusals that name the rule instead of always blaming traversal, pin and array summaries that say where they were cut, and UE 5.5/5.8 build guards (clang, `SetEnums`, the deprecated `ForEachObjectWithPackage` overload, `bCompileForEdit`, IWYU include regroup).
- **UE 5.8 support and bridge configuration:** @alecray for the `FJsonObject::Values` key-type build fix, the `MCP_NATIVE_PORT` override, a warning when `ListenPorts` silently drops a default bridge port (8090/8091), transactional `control_actor` spawn that rolls back on mesh failure, and unmet-dependency detection in `validate_niagara_system`.
- **Blueprint variable and event authoring:** @mhsm555 for component-bound events in `add_event` (#483), `targetClass` on DynamicCast nodes (#478), and `defaultValue` actually applied when adding variables (#475).
- **Native transport stability:** @vladSirin for moving the SSE notification keepalive onto a dedicated thread so it survives GameThread stalls (#491), and the UE 5.8 `FJsonObject` shared-string key build fix (#574).
- **Blueprint node safety:** @Fl0p for preventing an editor crash when creating `ConstructObjectFromClass`/`SpawnActorFromClass` nodes (#500), and for routing bare `remove_variable`/`rename_variable` on the native transport (#590).
- **Bridge targeting and Fab:** @punal100 for reporting the bridge target and resolving the project's bridge port (#640), and UE 5.8 Fab string-literal compilation fixes (#639).
- **Engine and compiler compatibility:** @max-modum for guarding pre-5.4/5.5 APIs so the plugin builds on older engines, verified on 5.3 (#493), and @TerryRouse02 for the C4800 enum-to-bool conversion in `IsStructureValid` (#562).
- **Dependency and workflow updates:** @dependabot[bot].

</details>

<details>
<summary><b>📊 Change Statistics</b></summary>

| Metric | Count |
|--------|-------|
| Diff range | `v0.5.30..v0.6.0-beta-a` |
| Commits since the tag | 1,020 (849 non-merge) |
| Files changed | 3,174 |
| Insertions / deletions | 781,656 / 203,402 |
| Capability records | 380 |
| Folded families | 244 across 22 parents (222 selector-dispatched) |
| Callable `{tool, action}` pairs | 1,549 (1,379 shipped names, 164 new family primaries, plus package_project, package_status, audit_placement and the three source_control actions) |
| Canonical parent tools (internal) | 23 |
| Public MCP tools | 1 (`unreal`) |
| C++ domain directories | 66 |
| TypeScript handler domains | 37 |
| Gateway routing modules | 26 |
| `AGENTS.md` files | 21 (20 area guides plus the root guide) |

> Insertion counts are dominated by committed generated artifacts (`capabilities/generated/`, native shards, manifests) and are not a useful measure of hand-written change.

</details>

---

## 🏷️ [0.5.30] - 2026-06-05

> [!IMPORTANT]
> ### 🚀 Native MCP & Code-Backed Tool Parity Release
> This release covers the `v0.5.21` to `0.5.30` release diff, including the TypeScript MCP server, native bridge plugin, tests, scripts, docs, workflows, and dependency manifests. The summary below is based on code and test changes, not commit subjects alone.

<details>
<summary><b>✨ Added</b></summary>

- **Native MCP Streamable HTTP endpoint** — added an opt-in in-plugin `/mcp` server with JSON-RPC 2.0 initialize/tools/list/tools/call handling, POST/GET/DELETE routing, `Mcp-Session-Id` session tracking, SSE tool-result streaming, progress notifications, persistent notification streams, `notifications/tools/list_changed` broadcasts, CORS handling, loopback-first binding, capability-token checks, and an editor status-bar indicator.
- **Self-describing native MCP tools** — added C++ `FMcpToolRegistry`, `FMcpSchemaBuilder`, `MCP_REGISTER_TOOL`, canonical native tool filtering, cached schema generation, and native dynamic tool/category enablement for the 23 canonical parent tools.
- **PCG automation** — added `manage_pcg` TypeScript/native schemas and handlers for graph/subgraph creation, PCG node aliases, pin connections, reflected node settings, component/world execution, partition grid configuration, save/overwrite behavior, and PCG plugin availability errors.
- **Environment systems automation** — added build-environment coverage for heightmap import/export, landscape layer info/material/splines/LOD/streaming proxies, foliage type configuration/paint/remove flows, sky and volumetric-cloud setup, weather/wind/time-of-day systems, water bodies, water waves/material/collision, and buoyancy components.
- **Behavior Tree authoring and introspection** — added `add_subnode`, root-sentinel decorators, decorator/service validation, subnode-aware lookup, `FBlackboardKeySelector` assignment, and `get_tree` runtime hierarchy serialization with root decorators, edge decorators, decorator ops, services, key properties, subtree references, and a success-with-null-root contract for graphless trees.
- **Blueprint, property, and inspection tools** — added `inspect_cdo`, Class Default Object component/property export, SCS and inherited SCS component classification, typed Blueprint custom-event pins, Enhanced Input graph nodes, inherited variable/member-class graph node lookup, and property access for Blueprint-added SCS component templates.
- **Editor, world, and input capabilities** — added full editor-window screenshots, game viewport screenshot routing, image content responses, simulated keyboard/mouse input aliases, active camera reporting, PIE runtime inspection, native `get_current_level`, actor material/view-target native actions, spawn scale support, and create-plane height handling.
- **Material, audio, animation, and system actions** — added Material Function creation/editing/calls/info, FunctionInput/FunctionOutput graph support, source-effect chains and source-effect presets, `force_rebuild_blend_space`, legacy/per-key input mapping edits, project setting writes, native asset validation, and `execute_python` for inline or project-local Python files.

</details>

<details>
<summary><b>🛡️ Security</b></summary>

- **GraphQL attack surface removed** — deleted the GraphQL server, schema/resolver/loaders, GraphQL docs, GraphQL unit tests, and direct GraphQL runtime dependencies.
- **Native MCP exposure controls** — default native MCP binding stays loopback-only unless explicitly allowed; non-loopback hosts warn, sessions are validated, stale requests/streams are cleaned up, and native HTTP requests use explicit request-origin routing instead of socket inference.
- **Capability-token and dynamic-tool protections** — native MCP validates `X-MCP-Capability-Token` when required, while both TypeScript and native dynamic tool managers protect `manage_tools`/`inspect` and protected categories from accidental disablement.
- **Python execution hardening** — `execute_python` enforces code/file exclusivity, a 1 MB inline code limit, project-root path normalization, symlink escape checks, `__file__` setup for file execution, temp-file cleanup, and direct `PythonScriptPlugin` execution.
- **Path, command, log, and workflow hardening** — tightened UE path normalization, console-command validation, snapshot/log path handling, level save/load flows, image/log redaction, safe `tmp/` cleanup, sync-script argument parsing, and GitHub Actions interpolation by moving untrusted values into environment variables.

</details>

<details>
<summary><b>🔧 Changed</b></summary>

- **Release metadata** — updated `package.json`, `package-lock.json`, `server.json`, the `src/index.ts` fallback, and `McpAutomationBridge.uplugin` to `0.5.30`.
- **Canonical TypeScript tool surface** — kept the 23 parent tools but moved action lists into shared constants, grouped tools into `core`, `world`, `gameplay`, and `utility`, merged nested `params` into top-level arguments for constrained clients, centralized handler routing, and removed legacy per-domain tool files.
- **Dynamic tool listing** — `tools/list` now checks known client support for `tools.listChanged`; dynamic clients can receive category-filtered tools, while clients without dynamic loading still see the full compatible tool surface.
- **Automation bridge lifecycle** — refactored host/port parsing, multi-port WebSocket connection attempts, handshake metadata, request queueing, progress timeout extension, stale-progress detection, absolute timeout caps, rate/message-size boundaries, disconnect/error tracking, and image-payload redaction.
- **Native bridge runtime** — split request dispatch out of the subsystem, added explicit `ERequestOrigin`, queued requests through the game thread, converted captured engine errors into failed responses, pumped GameThread tasks during native transport shutdown, and exposed native transport session/tool counts to UI.
- **Response and schema handling** — improved response validation, summary text generation, image response content, scalar result promotion, safe JSON cleanup, schema reuse, action-specific parameter descriptions, and stricter error context on tool failures.
- **Plugin compatibility** — updated bridge metadata for UE 5.8 Preview and added PythonScriptPlugin, StructUtils, Synthesis, and PCG plugin declarations where the new handlers need them.
- **Scripts and workflows** — made smoke tests run through SDK `InMemoryTransport`, added native parity/parameter audit npm scripts, changed `clean` to remove `tsconfig.tsbuildinfo`, added Linux/macOS/Windows plugin packaging scripts, strengthened sync/cleanup scripts, and made CI/publish/release gates stricter.

</details>

<details>
<summary><b>🛠️ Fixed</b></summary>

#### Routing & Native Tool Parity

- Fixed native/consolidated action routing for validation, audio creation, material graph pins, editor simulation, `add_widget_child`, `get_current_level`, AnimBP graph discovery, lighting, SCS edits, native actor/editor actions, exact action matching, and nested `params` payloads.

#### Blueprint, Graph & Property Handling

- Fixed inherited UPROPERTY lookup for VariableGet/VariableSet with `memberClass`, stale `K2Node_EnhancedInputAction` title cache refresh, Blueprint SCS component introspection, SCS template get/set paths, typed custom-event pin reconstruction, transaction ordering, null pin checks, case-insensitive pin connections, graph allocation fallback, and Blueprint busy-state cleanup.

#### Editor, World & Gameplay Behavior

- Fixed PIE/game viewport screenshots, full editor screenshot capture, simulated input dispatch, active camera view-state reporting, PIE runtime inspection, spawn scale application, plane height fields, landscape bounds fallback, prompt-save return codes, actor list response handling, editor/world handler stability, and native actor/editor contract alignment.

#### Asset, Level, Animation, Niagara & Audio

- Fixed unloaded level info via AssetRegistry fallback, classNames-only recursive asset search, normalized level path validation, source audio persistence, source-effect routing, audio authoring saves, material expression aliases, material pin routing to main inputs, UMaterialFunction graph details, animation notify validation, BlendSpace grid rebuilds, Niagara crash paths, hollow getters, parameter aliases, FText and FText-array property serialization, and `execute_python` file mode/output capture.

#### Plugin Stability & Compatibility

- Fixed native plugin compatibility across UE versions, macOS Clang audio literal builds, plugin package output detection, bridge socket/runtime handling, JSON key normalization, request telemetry, native MCP session validation, safe operations includes, handler review findings, and optional module/plugin availability paths.

</details>

<details>
<summary><b>🧪 Tests</b></summary>

- Added/expanded Vitest coverage for automation bridge connection, handshake, message schema, request tracking, config defaults, resources, health/metrics services, response validation, command validation, log reading/redaction, safe JSON, type coercion, normalization, queues, elicitation, and consolidated handler routing.
- Expanded MCP integration suites across core/world/gameplay/utility tools, including PCG, Behavior Tree subnodes/get-tree, networking/sessions/input, control-editor screenshots/input, actor list handling, audio/source effects, assets/material functions, Blueprints/SCS, levels, geometry, GAS, combat, inventory, interaction, sequence, environment, and system-control Python/project-setting flows.
- Added static native MCP action parity auditing and strict parameter-combination auditing that compare TypeScript schemas, native C++ tool definitions, native canonical registration, and test coverage.
- Hardened the custom test runner with richer assertions, captured variables, live/static reports, fake-success detection, progress output, expectation utilities, and deterministic parameter audit behavior.

</details>

<details>
<summary><b>🧰 Maintenance</b></summary>

- Refreshed AGENTS/project guidance, README/setup content, handler maps, testing guide, native automation progress notes, Roadmap, MCP coverage notes, UE 5.8 support notes, native audio routing notes, plugin READMEs, issue templates, labels, gitignore rules, production env defaults, Context7 config, and release metadata.
- Removed obsolete GraphQL API docs and GraphQL security tests with the GraphQL implementation.
- Trimmed unused plugin helpers/includes, streamlined bridge build settings, normalized Node built-in imports, simplified startup cleanup, optimized server utilities/build caching, and improved package/sync/cleanup script safety.

</details>

<details>
<summary><b>🔄 Dependencies</b></summary>

- Removed direct runtime dependencies for `@graphql-tools/schema`, `dataloader`, `graphql`, and `graphql-yoga`.
- Refreshed the lockfile across npm dependency groups, including security/maintenance updates for transitive runtime and dev packages.
- Updated pinned GitHub Actions used by checkout, setup-node, CodeQL, Release Drafter, github-script, action-gh-release, stale, labeler, and dependency-review workflows.

</details>

<details>
<summary><b>📚 Documentation</b></summary>

- Refreshed root and plugin README content, MCP/native transport setup, handler mapping, editor plugin extension notes, Roadmap, testing guide, native automation progress, MCP coverage notes, UE 5.8 support, and native audio routing notes.
- Added and updated repository guidance files for root, TypeScript server/tools/handlers/automation/utils/tests, native MCP internals, and McpAutomationBridge areas.
- Removed obsolete GraphQL API documentation.

</details>

<details>
<summary><b>👥 Contributors</b></summary>

Special thanks to the contributors in this release window, with obvious author aliases collapsed.

- **Native MCP Streamable HTTP endpoint:** Thanks @Fl0p for the native HTTP/SSE transport work.
- **GitHub Actions command-injection hardening:** @google-labs-jules[bot]
- **Dependency and workflow version updates:** @dependabot[bot]
- **Editor/runtime behavior fixes:** @xqdd for native `get_current_level` routing, spawn scale, create-plane height, lighting routing, Blueprint SCS verification, PIE runtime reporting, active camera state, per-key input mapping edits, and prompt-save return handling.
- **Behavior Tree and Blueprint introspection:** @kalihman for `get_tree`, decorator/service subnodes, Blackboard key selector assignment, inherited Blueprint/SCS introspection, and classNames-only asset search behavior.
- **Blueprint graph and animation reliability:** @VictorZhang01 for inherited UPROPERTY graph nodes, stale K2 node title refresh, AnimBP graph routing, and `force_rebuild_blend_space`.
- **Material and Blueprint component support:** @nekwo for Material Function authoring support and Blueprint-added component template property get/set coverage.
- **Plugin packaging and inspection:** @azwjp for Windows plugin packaging fixes, and @6r0m for `inspect_cdo` Blueprint CDO inspection.
- **Platform/build and routing fixes:** @jenniferied for macOS Clang audio-handler build fixes, @Miriam-R-coder for exact Blueprint action routing, and @spencer-zaid for screenshot, Niagara, hollow getter, and parameter-alias fixes.
- **Python and level metadata fixes:** @zmarx for `execute_python` file-mode handling and Python plugin initialization guards, and @codeman101 for unloaded `get_level_info` AssetRegistry fallback.

</details>

<details>
<summary><b>📊 Release Statistics</b></summary>

| Metric | Count |
|--------|-------|
| Release diff | `v0.5.21..0.5.30` |
| Files changed | 406 |
| Insertions | 61,393 |
| Deletions | 30,957 |
| TypeScript canonical tools | 23 |
| Native canonical tools | 23 |
| Native action parity mismatches | 0 |

</details>

---

## 🏷️ [0.5.21] - 2026-04-03

> [!IMPORTANT]
> ### 🔒 Security, New Features & Major Crash Fixes
> This release adds custom content mount points, full audio authoring, project settings management, vehicle physics configuration, blend tree/procedural animation/state machine creation, sequencer improvements, and critical crash prevention for deleting animation/IK assets and folders.

<details>
<summary><b>🛡️ Security</b></summary>

- **Command Injection in bump-version action** – Sanitized `release-type` input ([#327](https://github.com/ChiR24/Unreal_mcp/pull/327))
- **Command Injection in editor console commands** – Mixed-context sanitization for `start_recording`, `set_camera_fov`, `set_game_speed` ([#322](https://github.com/ChiR24/Unreal_mcp/pull/322))
- **Path Traversal in `export_level`** – Added path validation ([#305](https://github.com/ChiR24/Unreal_mcp/pull/305))
- **Path Traversal in screenshot filename** – Sanitized filenames, blocked traversal patterns ([#314](https://github.com/ChiR24/Unreal_mcp/pull/314))
- **Synchronous fs Hardening** – Replaced blocking `fs.existsSync` / `fs.readdirSync` with async versions ([#318](https://github.com/ChiR24/Unreal_mcp/pull/318))

</details>

<details>
<summary><b>✨ Added</b></summary>

- **Custom Content Mount Points** – `MCP_ADDITIONAL_PATH_PREFIXES` to whitelist plugin mount points (`/ProjectObject/`, etc.) ([#326](https://github.com/ChiR24/Unreal_mcp/pull/326) – thanks @6r0m)
- **Full Audio Authoring** – Create sound waves, sound cues, sound classes, sound mixes, attenuation settings; success flags in responses.
- **Project Settings Management** – New `manage_project_settings` tool (get/set project settings via config).
- **Animation Authoring** – `create_blend_tree`, `create_procedural_anim`, `create_state_machine` (C++ implementations, not console commands).
- **Vehicle Physics Configuration** – `configure_vehicle` with wheels, engine, transmission, mass, drag coefficient.
- **Sequencer** – `set_tick_resolution`, `set_view_range` actions.
- **Widget Authoring** – New template widgets: main menu, pause menu, HUD, crosshair, ammo counter, health bar, compass, interaction prompt, objective tracker, damage indicator, inventory grid, dialog box, radial menu, credits scroll, shop UI, quest tracker.
- **Runtime Module Checks** – Verify GameplayAbilities, EnhancedInput, BehaviorTreeEditor, LevelSequenceEditor, NiagaraEditor, StateTree, SmartObjects, MassEntity are loaded before use (clear error messages when plugins missing).

</details>

<details>
<summary><b>🛠️ Fixed</b></summary>

#### Crash Prevention (UE 5.7+)

- **Animation/Rig asset deletion** – Completely rewrote `McpSafeDeleteFolder` and added `DeleteAnimationRigClusterOrdered` to prevent 0xFFFFFFFFFFFFFFFF crashes when deleting AnimBlueprints, IKRigs, IKRetargeters, ControlRigBlueprints, and AnimSequences.
- **Folder deletion** – Replaced `UEditorAssetLibrary::DeleteDirectory` with `McpSafeDeleteFolder` (proper world switching, package unloading, compilation quiesce).
- **Blueprint creation** – Added pre‑creation checks in `CreateControlRigBlueprint` and widget blueprint creation to prevent engine assertion failures.
- **Widget creation** – Fixed widget crash ([#306](https://github.com/ChiR24/Unreal_mcp/pull/306)) by adding GUID registration (`RegisterWidgetGuid`) and safe tree replacement (`SafeAddWidgetToTree`).
- **AnimNotify/NotifyState** – Added abstract class validation and track existence checks.

#### Asset & Path Handling

- Improved asset loading reliability for newly created AI assets (removed stale `DoesAssetExist` checks).
- Resolved asset query parameter bugs and expanded `classNames` support ([#311](https://github.com/ChiR24/Unreal_mcp/pull/311)).
- Replaced custom asset directory checks with `UEditorAssetLibrary` to avoid stale cache issues.
- Fixed `searchText` filtering in `search_assets` action ([#308](https://github.com/ChiR24/Unreal_mcp/pull/308)).
- Added `offset` pagination to asset search.

#### Blueprint & Graph Editing

- Unified pin serialization across blueprint graph handlers ([#309](https://github.com/ChiR24/Unreal_mcp/pull/309)) – linked pins returned as objects with `nodeId` and `pinName`.
- Improved actor lookup to match subsystem behavior (checks both label and name).
- Aligned `get_ai_info` output with TypeScript schema ([#310](https://github.com/ChiR24/Unreal_mcp/pull/310)).

#### Performance & Console

- Delegated console command settings to C++ handler for better performance.
- Ensured successful execution of console commands (check `GEngine->Exec` return value).
- Added validation for required session parameters (interfaceType, controllerId, playerIndex, etc.).
- Removed redundant `AsyncTask` wrappers in `generate_thumbnail` and `generate_lods` (fixed 30‑second timeout).

#### Level Operations

- **`rename_level`** – Now uses `DuplicateAsset` + `DeleteAsset` to avoid modal “Find/Replace” dialog.
- **`duplicate_level`** – Validates source existence and deletes destination if already present.
- **`export_level`** – Added source level existence check before export.

#### Voice Chat & Sessions

- Improved `mute_player` – falls back to `BlockPlayers` when voice server not connected.
- Added validation for required parameters in all session actions.

#### Plugin Stability

- Used delay‑load for optional plugin modules to prevent missing dependency errors ([#317](https://github.com/ChiR24/Unreal_mcp/pull/317)).
- Refactored IK retargeter initialization using controller API (UE 5.7+) with backward compatibility fallback.
- Enhanced actor and component stability across subsystems.

#### Documentation

- Fixed rate limiting defaults and missing GraphQL heading ([#307](https://github.com/ChiR24/Unreal_mcp/pull/307)).

</details>

<details>
<summary><b>🔄 Dependencies</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `picomatch` | 4.0.3 → 4.0.4 | [#316](https://github.com/ChiR24/Unreal_mcp/pull/316) |
| Dependencies group | 9 updates | [#320](https://github.com/ChiR24/Unreal_mcp/pull/320) |
| `github/codeql-action` | 4.33.0 → 4.34.1 | [#319](https://github.com/ChiR24/Unreal_mcp/pull/319) |

</details>

<details>
<summary><b>👥 Contributors</b></summary>

- @google-labs-jules[bot] for all security fixes
- @kalihman for asset query, searchText, docs, and blueprint graph fixes
- @dependabot[bot] for dependency updates
- @6r0m for custom content mount points (first contribution)

</details>

---

## 🏷️ [0.5.20] - 2026-03-21

> [!IMPORTANT]
> ### 🛡️ Security Fix & UE 5.0 Compatibility
> This release includes a critical path traversal fix in export_asset, UE 5.0 compatibility improvements, and external actors support for World Partition.

### 🛡️ Security

<details>
<summary><b>🔒 Path Traversal in export_asset</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/5cf2a3c">5cf2a3c</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in `export_asset` action |
| **Fix** | Added path validation to prevent directory traversal attacks |

**Files Modified:**
- `McpAutomationBridge_SystemControlHandlers.cpp`

</details>

### ✨ Added

<details>
<summary><b>🌍 External Actors Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/51143c3">51143c3</a>)</summary>

| Feature | Description |
|---------|-------------|
| **External Actors** | Support for World Partition external actors in level structure handlers |
| **Streaming Reference** | Streaming reference creation for external actor packages |

**Files Modified:**
- `McpAutomationBridge_LevelStructureHandlers.cpp` (+127 lines)

</details>

### 🛠️ Fixed

<details>
<summary><b>🎮 UE 5.0 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/1057023">1057023</a>)</summary>

| Bug | Fix |
|-----|-----|
| `bIsWorldInitialized` API not available in UE 5.0 | Direct access to `bIsWorldInitialized` for UE 5.0 compatibility |

**Files Modified:**
- `McpAutomationBridgeHelpers.h`
- `McpAutomationBridge_LevelStructureHandlers.cpp`

</details>

<details>
<summary><b>🐛 Tick Task Manager Crashes</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/8c311d7">8c311d7</a>)</summary>

| Bug | Fix |
|-----|-----|
| Crashes from tick task manager during world operations | Added safety checks and proper cleanup in world management |
| World cleanup issues | Enhanced cleanup with `FlushRenderingCommands` safety |

**Files Modified:**
- `McpAutomationBridgeHelpers.h` (+36 lines)
- `McpAutomationBridge_LevelStructureHandlers.cpp` (+65 lines)
- `McpSafeOperations.h` (+16 lines)

</details>

<details>
<summary><b>🐛 Sublevel Creation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/bffb68c">bffb68c</a>)</summary>

| Bug | Fix |
|-----|-----|
| Sublevel creation path handling issues | Enhanced sublevel creation process with proper path handling |

**Files Modified:**
- `McpAutomationBridge_LevelStructureHandlers.cpp` (+201 lines)

</details>

<details>
<summary><b>🔧 UE 5.7 Build</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/295">#295</a>)</summary>

| Bug | Fix |
|-----|-----|
| Missing includes causing build failures on UE 5.7 | Added missing includes in `McpHandlerUtils.cpp` and `McpPropertyReflection.cpp` |

**Contributors:** @a2448825647

</details>

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| `release-drafter/release-drafter` | 7.0.0 | 7.1.1 | [#300](https://github.com/ChiR24/Unreal_mcp/pull/300) |
| `softprops/action-gh-release` | 2.5.3 | 2.6.1 | [#301](https://github.com/ChiR24/Unreal_mcp/pull/301) |
| `github/codeql-action` | 4.32.6 | 4.33.0 | [#299](https://github.com/ChiR24/Unreal_mcp/pull/299) |

</details>

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| `flatted` | 3.3.3 | 3.4.2 | [#304](https://github.com/ChiR24/Unreal_mcp/pull/304) |

</details>

### 🔌 Plugin

<details>
<summary><b>MCP Automation Bridge v0.1.3</b></summary>

Updated plugin version to 0.1.3 with all fixes and features from this release.

See [Plugin CHANGELOG](plugins/McpAutomationBridge/CHANGELOG.md) for details.

</details>

---

## 🏷️ [0.5.19] - 2026-03-18

> [!IMPORTANT]
> ### 🛡️ Security Hardening & Major Plugin Refactoring
> This release includes critical security fixes for command injection and path traversal vulnerabilities, a complete deep-level refactoring of 57 C++ handler files with centralized utilities, and removal of the WebAssembly integration.

### 🛡️ Security

<details>
<summary><b>🔒 Command Injection Prevention</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/288">#288</a>)</summary>

| Component | Change |
|-----------|--------|
| **sanitizeCommandArgument()** | Added semicolon sanitization to prevent command chaining attacks |
| **Physics Tools** | Sanitized constraint names, actor names, vehicle names, destruction names |
| **Animation Tools** | Sanitized state machine names, state names, transition conditions |
| **System Handlers** | Sanitized vehicle type, save paths, and all user-provided strings |

**Attack Vector Blocked:** Input like `"name;quit"` can no longer execute arbitrary commands.

</details>

<details>
<summary><b>🔒 Path Traversal Fixes</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/271">#271</a>, <a href="https://github.com/ChiR24/Unreal_mcp/pull/282">#282</a>)</summary>

| Component | Change |
|-----------|--------|
| **validateSnapshotPath()** | Fixed bypass where paths equal to CWD (without trailing separator) were incorrectly rejected |
| **Asset Handlers** | Added path sanitization to prevent traversal attacks |
| **Blueprint Creation** | Added savePath sanitization |

</details>

<details>
<summary><b>🔒 GraphQL CORS Hardening</b></summary>

| Component | Change |
|-----------|--------|
| **Default CORS** | Changed from permissive `'*'` to safe loopback origins |
| **Allowed Origins** | `localhost:4000`, `127.0.0.1:4000`, `localhost:3000`, `127.0.0.1:3000` |
| **Warning** | Added security warning when `'*'` is explicitly configured |

</details>

### 🔧 Changed

<details>
<summary><b>🏗️ Complete C++ Plugin Refactoring</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/280">#280</a>)</summary>

Deep line-by-line refactoring of 57 handler files and 8 infrastructure files:

| New Infrastructure File | Purpose |
|------------------------|---------|
| `McpHandlerUtils.h/cpp` | Standardized JSON response builders (1,900 lines) |
| `McpPropertyReflection.h/cpp` | Property reflection utilities (1,356 lines) |
| `McpSafeOperations.h` | Safe asset/level save for UE 5.7 (659 lines) |
| `McpVersionCompatibility.h` | UE 5.0-5.7 API compatibility macros (225 lines) |
| `McpHandlerDeclarations.h` | Forward declarations (844 lines) |
| `McpAutomationBridge_ConsoleCommandHandlers.cpp` | Batch and single command execution (302 lines) |

**Bugs Fixed During Refactoring:**
- EditorFunctionHandlers: use-after-free bug
- EffectHandlers: truncated condition + missing braces
- InventoryHandlers: duplicate TArray with undefined variables
- MaterialAuthoringHandlers: duplicate include + missing UE 5.0 fallback
- NavigationHandlers: case-sensitivity error
- SkeletonHandlers: duplicate verification + redundant code
- WidgetAuthoringHandlers: unreachable code block

</details>

<details>
<summary><b>⚡ Performance Improvements</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/283">#283</a>)</summary>

| Component | Change |
|-----------|--------|
| **Batch Console Commands** | New batch execution API for parallel command processing |
| **validate_assets** | Changed from sequential to `Promise.all` concurrent validation |
| **State Machine Creation** | States now added in parallel instead of sequentially |

</details>

<details>
<summary><b>🗑️ WebAssembly Integration Removed</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/240">#240</a>)</summary>

Removed WebAssembly (wasm-pack/Rust) integration:
- Deleted `src/wasm/` directory (874 lines)
- Deleted `wasm/` Rust crate (1,500+ lines)
- Removed WASM dependency from all handlers
- Native C++ handlers provide equivalent functionality

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Blueprint Inspect Crash</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/270">#270</a>)</summary>

| Bug | Fix |
|-----|-----|
| Blueprint inspect crashed when variable list exceeded buffer | Fixed truncated variable list handling |
| Function library blueprints not supported | Added function library blueprint support (#258) |

</details>

<details>
<summary><b>🐛 GAS Duplicate Effect Creation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/251">#251</a>)</summary>

| Bug | Fix |
|-----|-----|
| `create_gameplay_effect` assertion failure on duplicates | Prevented duplicate GameplayEffect creation |

</details>

<details>
<summary><b>🐛 Volume Handler Mobility</b></summary>

| Bug | Fix |
|-----|-----|
| Volume attachment failed for movable actors | Added mobility check for target actors in volume handlers |

</details>

<details>
<summary><b>🐛 UE 5.7 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/274">#274</a>)</summary>

| Bug | Fix |
|-----|-----|
| GeometryScript AppendCapsule compile error on UE 5.5+ | Added version guard for segment steps parameter |

</details>

<details>
<summary><b>🐛 Action Name Alignment</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/253">#253</a>)</summary>

| Bug | Fix |
|-----|-----|
| TypeScript action names mismatched C++ handlers | Aligned all action names with C++ handler expectations |

</details>

### 🗑️ Removed

<details>
<summary><b>Deprecated Tool Files</b></summary>

Removed deprecated standalone tool files (consolidated into handlers):
- `src/tools/audio.ts` → `src/tools/handlers/audio-handlers.ts`
- `src/tools/debug.ts` → consolidated into system handlers
- `src/tools/introspection.ts` → `src/tools/handlers/inspect-handlers.ts`
- `src/tools/materials.ts` → `src/tools/handlers/material-authoring-handlers.ts`
- `src/tools/performance.ts` → `src/tools/handlers/performance-handlers.ts`
- `src/tools/ui.ts` → consolidated into widget handlers
- `src/tools/input.ts` → `src/tools/handlers/input-handlers.ts`
- `src/tools/behavior-tree.ts` → consolidated
- `src/tools/engine.ts` → consolidated

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| hono | 4.12.0 | 4.12.7 | [#261](https://github.com/ChiR24/Unreal_mcp/pull/261), [#277](https://github.com/ChiR24/Unreal_mcp/pull/277) |
| express-rate-limit | 8.2.1 | 8.3.0 | [#269](https://github.com/ChiR24/Unreal_mcp/pull/269) |
| @types/node | Various updates | | Multiple PRs |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| release-drafter/release-drafter | 6.2.0 | 7.0.0 | [#286](https://github.com/ChiR24/Unreal_mcp/pull/286) |
| softprops/action-gh-release | 2.5.0 | 2.5.3 | [#287](https://github.com/ChiR24/Unreal_mcp/pull/287) |
| actions/setup-node | 6.2.0 | 6.3.0 | [#257](https://github.com/ChiR24/Unreal_mcp/pull/257) |
| github/codeql-action | 4.32.5 | 4.32.6 | [#266](https://github.com/ChiR24/Unreal_mcp/pull/266) |

</details>

### 📊 Statistics

- **Commits:** 55 non-merge commits
- **Files Changed:** 185 files
- **Lines Added:** ~30,280
- **Lines Removed:** ~20,440
- **New C++ Infrastructure:** 5 files (~4,900 lines)
- **Bugs Fixed:** 15+
- **Security Fixes:** 4 critical

---

## 🏷️ [0.5.18] - 2026-02-21

> [!IMPORTANT]
> ### 🔧 Installation, Documentation & Dependency Updates
> This release fixes npm install failures when downloading from GitHub releases, adds first-time project setup guidance, and updates dependencies.

### 🛠️ Fixed

<details>
<summary><b>🐛 npm install failure from release archives</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/215">#215</a>)</summary>

| Issue | Root Cause | Fix |
|-------|------------|-----|
| `npm install` fails with ESLint config error | Release archives excluded `eslint.config.mjs` and other build files | Added `-source` archives with complete build files |
| `prepare` script runs build unnecessarily | Checked only `dist/` existence, not build artifacts | Now verifies `dist/cli.js` and `dist/index.js` exist |
| Deprecated `--ext .ts` flag in lint | ESLint 9.x removed support for `--ext` flag | Removed flag, extensions configured in `eslint.config.mjs` |

**Files Modified:**
- `package.json` (prepare script, lint scripts, removed prebuild)
- `.github/workflows/release.yml` (added source archives, fixed plugin path)
- `README.md` (added Rust/wasm-pack prerequisites)

</details>

### 📚 Documentation

<details>
<summary><b>📖 First-time project open instructions</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/112df08">112df08</a>)</summary>

Added guidance for users opening Unreal projects for the first time:
- Explains UE prompt to rebuild missing modules
- Documents expected plugin load failure after first rebuild
- Recommends closing and reopening project to resolve

</details>

### ⬆️ Dependencies

| Package | From | To | PR |
|---------|------|-----|-----|
| hono | 4.11.7 | 4.12.0 | [#213](https://github.com/ChiR24/Unreal_mcp/pull/213) |
| ajv | 8.17.1 | 8.18.0 | [#210](https://github.com/ChiR24/Unreal_mcp/pull/210) |
| actions/stale | 10.1.1 | 10.2.0 | [#208](https://github.com/ChiR24/Unreal_mcp/pull/208) |
| actions/dependency-review-action | 4.8.2 | 4.8.3 | [#212](https://github.com/ChiR24/Unreal_mcp/pull/212) |

---

## 🏷️ [0.5.17] - 2026-02-16

> [!IMPORTANT]
> ### 🔧 World Tools Category Fixes & Security Hardening
> This release includes critical bug fixes, security hardening, and UE 5.7 compatibility improvements across all world-building tools (landscape, foliage, geometry, volumes, navigation).

### 🛡️ Security

<details>
<summary><b>🔒 Path Validation & Input Sanitization</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/207">#207</a>)</summary>

| Component | Change |
|-----------|--------|
| **SanitizeProjectRelativePath** | Rejects Windows absolute paths, normalizes slashes, collapses `//`, requires valid UE roots (`/Game`, `/Engine`, `/Script`) |
| **SanitizeProjectFilePath** | File operations with path traversal protection |
| **ValidateAssetCreationPath** | Combines folder + name validation for asset creation |
| **Actor/Volume Name Validation** | Blocks invalid characters, enforces length checks |
| **Snapshot Path Validation** | Prevents directory traversal attacks via snapshot paths |

**Files Modified:**
- `McpAutomationBridgeHelpers.h` (+326 lines of security helpers)
- `src/tools/environment.ts` (snapshot path security)
- `src/utils/path-security.ts` (path normalization)

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Landscape Handler Silent Fallback Bug</b> (McpAutomationBridge_LandscapeHandlers.cpp)</summary>

| Bug | Root Cause | Fix |
|-----|------------|-----|
| False positives on non-existent landscapes | Path matching compared `GetPathName()` (internal path) with asset path | Normalized both paths with `.uasset` stripping |
| Silent fallback to single landscape | `if (!Landscape && LandscapeCount == 1)` used any available landscape | Removed fallback, now returns `LANDSCAPE_NOT_FOUND` error |
| Wrong response path | Returned requested path instead of actual path | Now returns `Landscape->GetPackage()->GetPathName()` |

**Affected Handlers:** `HandleModifyHeightmap`, `HandlePaintLandscapeLayer`, `HandleSculptLandscape`, `HandleSetLandscapeMaterial`

</details>

<details>
<summary><b>🐛 Rotation Yaw Bug</b> (McpAutomationBridge_LightingHandlers.cpp:200)</summary>

| Bug | Fix |
|-----|-----|
| `Rotation.Yaw` read from `LocPtr` instead of `RotPtr` | Changed to `GetJsonNumberField((*RotPtr), TEXT("yaw"))` |

**Impact:** Incorrect rotation when spawning lights with rotation parameters.

</details>

<details>
<summary><b>🐛 Integer Overflow in Heightmap Operations</b> (McpAutomationBridge_LandscapeHandlers.cpp:631-635)</summary>

| Bug | Fix |
|-----|-----|
| `static_cast<int16>(CurrentHeights[i])` overflows for values > 32767 | Changed to `static_cast<int32>` |

**Impact:** Heightmap raise/lower operations now produce correct results for heights above midpoint.

</details>

<details>
<summary><b>🐛 set_curve_key Success Reporting</b> (McpAutomationBridge_AnimationHandlers.cpp:2139)</summary>

| Bug | Fix |
|-----|-----|
| `bSuccess` initialized `false`, only set `true` inside `if (bSuccess)` block (unreachable) | Moved success logic before the condition check |

**Impact:** `set_curve_key` now correctly reports success.

</details>

<details>
<summary><b>🐛 CraftingSpeed Truncation</b> (McpAutomationBridge_InventoryHandlers.cpp:2716)</summary>

| Bug | Fix |
|-----|-----|
| `int32 CraftingSpeed` truncated fractional multipliers (1.5 → 1) | Changed to `double` |

</details>

<details>
<summary><b>🐛 Invalid Color Fallback Not Applied</b> (McpAutomationBridge_LightingHandlers.cpp:277)</summary>

| Bug | Fix |
|-----|-----|
| `SetLightColor()` only called when `bColorValid == true`, but `bColorValid = false` for invalid colors | Removed guard, always call `SetLightColor()` after correcting invalid colors to white |

</details>

<details>
<summary><b>🐛 Double-Validation in Snapshot Path</b> (src/tools/environment.ts:253, 322)</summary>

| Bug | Fix |
|-----|-----|
| Redundant second `validateSnapshotPath()` call on already-resolved absolute paths | Removed redundant call |

</details>

<details>
<summary><b>🐛 Intel GPU Driver Crash Prevention</b> (McpAutomationBridgeHelpers.h)</summary>

| Bug | Fix |
|-----|-----|
| `MONZA DdiThreadingContext` exceptions on Intel GPUs during level save | Added `McpSafeLevelSave` helper with `FlushRenderingCommands` and retry logic |

</details>

### ✨ Added

<details>
<summary><b>🛤️ LOD Generation Enhancements</b> (McpAutomationBridge_GeometryHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **landscapePath support** | LOD generation now accepts single `landscapePath` or array `assetPaths` |
| **lodCount parameter** | Alternative to `numLODs` for specifying LOD count |
| **Path sanitization** | All LOD operations use `SanitizeProjectRelativePath` |

</details>

<details>
<summary><b>🌿 FoliageType Auto-Creation</b> (McpAutomationBridge_FoliageHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **Auto-create FoliageType** | When painting/adding foliage, FoliageType is automatically created from StaticMesh if missing |
| **Path validation** | All foliage operations use path sanitization |

</details>

<details>
<summary><b>🏔️ Landscape Layer Auto-Creation</b> (McpAutomationBridge_LandscapeHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **Auto-create layers** | When painting, landscape layers are auto-created if they don't exist (matches UE editor behavior) |

</details>

<details>
<summary><b>📊 Handler Verification</b> (Multiple Handler Files)</summary>

| Pattern | Description |
|---------|-------------|
| **AddActorVerification** | Returns `actorPath`, `actorName`, `actorGuid`, `existsAfter`, `actorClass` |
| **AddComponentVerification** | Returns `componentName`, `componentClass`, `ownerActorPath` |
| **AddAssetVerification** | Returns `assetPath`, `assetName`, `existsAfter`, `assetClass` |
| **VerifyAssetExists** | Verifies asset exists at path |

**Files Updated:** PropertyHandlers, LevelHandlers, EffectHandlers, GASHandlers, SequenceHandlers, SkeletonHandlers, and 30+ additional handler files

</details>

### 🔧 Changed

<details>
<summary><b>🎮 UE 5.7 Compatibility</b></summary>

| Component | Change |
|-----------|--------|
| **WebSocket Protocol** | `GetProtocolType()` (FName) replaces deprecated `GetProtocolFamily()` (enum) |
| **SCS Save** | `McpSafeAssetSave` replaces `SaveLoadedAssetThrottled` to prevent recursive `FlushRenderingCommands` crashes |
| **PostProcessVolume** | Conditionally compiled (removed in UE 5.7) |
| **Niagara Graph** | Initialize `GraphSource`/`NiagaraGraph` to prevent null graph crashes |
| **Landscape Edit** | `FLandscapeEditDataInterface` for UE 5.5+, deprecation suppression for 5.0-5.4 |
| **WorldPartition** | Support `RuntimeHashSet` in addition to `RuntimeSpatialHash` for UE 5.7+ |

</details>

<details>
<summary><b>📈 Performance Improvements</b></summary>

| Component | Change |
|-----------|--------|
| **Heightmap Modification** | Pass `false` to `FLandscapeEditDataInterface` to prevent 60+ second GPU sync delays |
| **Landscape Updates** | Use `MarkPackageDirty` instead of `PostEditChange` to avoid unnecessary rebuilds |
| **Geometry Operations** | Memory pressure checks and triangle limits to prevent OOM crashes |

</details>

### 📊 Statistics

- **Files Changed:** 70 files
- **Lines Added:** ~7,200
- **Lines Removed:** ~1,400
- **Bug Fixes:** 8 critical bugs
- **New Verification Helpers:** 4

---

## 🏷️ [0.5.16] - 2026-02-12

> [!IMPORTANT]
> ### 🚀 Major Feature Release: 200+ Action Handlers
> This release adds ~200 new C++ automation sub-actions across all domains, introduces progress heartbeat protocol for long-running operations, dynamic tool management, IPv6 support, and comprehensive security hardening.

### ✨ Added

<details>
<summary><b>🎮 200+ MCP Action Handlers</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/200">#200</a>)</summary>

| Domain | New Actions |
|--------|-------------|
| **AI** | 50+ actions for EQS, Perception, State Trees, Smart Objects |
| **Combat** | Weapons, projectiles, damage, melee combat |
| **Character** | Character creation, movement, advanced locomotion |
| **Inventory** | Items, equipment, loot tables, crafting |
| **GAS** | Gameplay Ability System: abilities, effects, attributes |
| **Audio** | MetaSounds, sound classes, dialogue |
| **Materials** | Material expressions, landscape layers |
| **Textures** | Texture creation, compression, virtual texturing |
| **Levels** | 15+ new sub-actions for level management |
| **Volumes** | 18 volume types |
| **Performance** | Profiling, optimization, scalability |
| **Input** | Enhanced Input Actions & Contexts |
| **Interaction** | Interactables, destructibles, triggers |
| **Misc** | System control, tests, logs |

**New Handler Files:**
- `McpAutomationBridge_CharacterHandlers.cpp` (337 lines)
- `McpAutomationBridge_CombatHandlers.cpp` (398 lines)
- `McpAutomationBridge_SystemControlHandlers.cpp` (324 lines)
- `McpAutomationBridge_MiscHandlers.cpp` (1010 lines)
- `McpAutomationBridge_WidgetAuthoringHandlers.cpp` (2404 lines)

</details>

<details>
<summary><b>💓 Progress Heartbeat Protocol</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/201">#201</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Progress Updates** | C++ sends `progress_update` WebSocket messages during long-running operations |
| **Deadline Extensions** | TS extends request deadlines on each update with deadlock safeguards |
| **Stale Detection** | Detects same percentage for 3 consecutive updates |
| **Absolute Cap** | 5-minute maximum extension limit |
| **Max Extensions** | 10 extensions per request |

**Timeout Changes:**
- Default request timeout: 60s → 30s (extensions handle slow ops)

</details>

<details>
<summary><b>🔧 Dynamic Tool Management</b></summary>

| Feature | Description |
|---------|-------------|
| **manage_tools MCP Tool** | Enables AI to enable/disable tools at runtime |
| **Protected Tools** | `manage_tools`, `inspect`, and core category cannot be disabled |
| **list_changed Notifications** | Tool registry sends MCP notifications when tools change |
| **Category Filtering** | Filter tools by category (core, world, authoring, gameplay, utility) |

</details>

<details>
<summary><b>🌐 IPv6 Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/194">#194</a>)</summary>

| Feature | Description |
|---------|-------------|
| **IPv6 Addresses** | Full support for IPv6 addresses in automation bridge |
| **Hostname Resolution** | DNS resolution via `GetAddressInfo` instead of fallback to 127.0.0.1 |
| **Address Family Detection** | Auto-detect IPv6 by checking for colons in address |
| **Zone ID Handling** | Strip zone IDs from IPv6 addresses for Node.js compatibility |
| **Fallback Support** | Re-create socket as IPv4 when IPv6 not available |

</details>

### 🛡️ Security

<details>
<summary><b>🔒 Security Hardening</b></summary>

| Function | Description |
|----------|-------------|
| **SanitizeProjectRelativePath** | Rejects Windows absolute paths, normalizes slashes, collapses `//`, requires valid UE roots |
| **SanitizeAssetName** | Strips SQL injection patterns, invalid characters, enforces 64-char limit |
| **ValidateAssetCreationPath** | Combines folder + name validation |
| **IsValidAssetPath** | Rejects `:` (Windows drive letters) and consecutive slashes |

**TypeScript Security:**
- `src/utils/path-security.ts`: Collapse `//` normalization
- `src/utils/validation.ts`: SQL injection detection

</details>

<details>
<summary><b>🔒 String Escaping Fix</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/202">#202</a>)</summary>

| Issue | Fix |
|-------|-----|
| Incomplete string escaping in path handling | Added proper escaping for special characters |

</details>

### 🔧 Changed

<details>
<summary><b>🎮 UE 5.7 Compatibility Fixes</b></summary>

| Component | Change |
|-----------|--------|
| **WebSocket** | `GetProtocolType()` (FName) replaces `GetProtocolFamily()` (enum) |
| **SCS Save** | `McpSafeAssetSave` prevents recursive `FlushRenderingCommands` crashes |
| **PostProcessVolume** | Conditionally compiled (removed in 5.7) |
| **Niagara** | Initialize `GraphSource`/`NiagaraGraph` to prevent null graph crashes |

</details>

<details>
<summary><b>⚡ Performance & Infrastructure</b></summary>

| Change | Description |
|--------|-------------|
| **Memory Detection** | Windows `GlobalMemoryStatusEx` replaces heuristic detection |
| **Rate Limit** | `MaxAutomationRequestsPerMinute` raised 120 → 600 |
| **Logging** | Improved request/response logging with action name and filtered payload preview |
| **Blueprint Handler** | Variable name collision generates unique suffix, type validation before loading |

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Various Fixes</b></summary>

| Fix | Description |
|-----|-------------|
| **~30 handlers** | Handlers that returned `nullptr` now return structured JSON |
| **Blueprint** | Unknown actions return explicit error instead of silent failure |
| **Level tools** | File existence checked before load, post-load path validation |
| **Eject handler** | Changed from stopping PIE to ejecting from possessed pawn |

</details>

### 📊 Statistics

- **Files Changed:** 83 files
- **Lines Added:** ~23,000
- **Lines Removed:** ~2,700
- **New Action Handlers:** ~200
- **New Handler Files:** 5

---

## 🏷️ [0.5.15] - 2026-02-06

> [!NOTE]
> ### 🌐 Network Configuration Release
> This release adds support for non-loopback binding in automation bridge settings, enabling LAN access configuration.

### ✨ Added

<details>
<summary><b>🌐 Non-Loopback Binding Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/193">#193</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Non-Loopback Binding** | Automation bridge can now bind to non-loopback addresses (e.g., `0.0.0.0`) for LAN access |
| **Allow Non-Loopback Setting** | New `bAllowNonLoopback` setting in plugin configuration |
| **TypeScript Support** | Added `MCP_AUTOMATION_ALLOW_NON_LOOPBACK` environment variable |
| **Host Validation Tests** | New test suite for bridge host validation |

**Configuration:**
```env
# Enable LAN access
MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true
MCP_AUTOMATION_HOST=0.0.0.0
```

**Security Note:** Only enable on trusted networks with appropriate firewall rules.

</details>

### 🔄 Dependencies

<details>
<summary><b>Dependabot Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.32.1 → 4.32.2 | [#189](https://github.com/ChiR24/Unreal_mcp/pull/189) |
| Dependencies group | 2 updates | [#190](https://github.com/ChiR24/Unreal_mcp/pull/190) |

</details>

### 📊 Statistics

- **Files Changed:** 8 files
- **Lines Added:** ~270
- **Lines Removed:** ~10

---

## 🏷️ [0.5.14] - 2026-02-05

> [!IMPORTANT]
> ### 🔐 TLS & Network Security Release
> This release introduces TLS/SSL support for secure WebSocket connections (`wss://`), per-connection rate limiting, loopback-only network binding enforcement, and authentication state tracking for the Automation Bridge.

### 🛡️ Security

<details>
<summary><b>🔒 Loopback-Only Binding & Handshake Enforcement</b> (<code>70c2745</code>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Loopback Binding** | Automation Bridge now only binds to loopback addresses (127.0.0.1 or ::1) |
| **Handshake Required** | Automation requests require completed `bridge_hello` handshake |

**C++ Plugin:**
- Rejects `0.0.0.0` and `::` bind attempts, falls back to `127.0.0.1` with warning
- Added `AuthenticatedSockets` tracking set in `McpConnectionManager`
- Unauthenticated sockets receive `HANDSHAKE_REQUIRED` error and connection close (code 4004)

**TypeScript Bridge:**
- Added `normalizeLoopbackHost()` to validate and enforce loopback addresses
- Non-loopback host values rejected with warning and fallback

</details>

### ✨ Added

<details>
<summary><b>🔐 TLS/SSL, Rate Limiting & Schema Validation</b> (<code>d2a94cf</code>)</summary>

| Feature | Description |
|---------|-------------|
| **TLS/SSL Support** | Full `wss://` WebSocket support with OpenSSL/TLS integration (TLS 1.2+) |
| **Rate Limiting** | Per-connection limits: configurable, defaults to disabled (0) for development |
| **Schema Validation** | New Zod schemas in `src/automation/message-schema.ts` for type-safe message parsing |

**New Plugin Settings:**
- `bEnableTls`, `TlsCertificatePath`, `TlsPrivateKeyPath` - TLS configuration
- `MaxMessagesPerMinute`, `MaxAutomationRequestsPerMinute` - Rate limit configuration

**C++ Implementation:**
- `InitializeTlsContext()`, `EstablishTls()`, `SendRaw()`, `RecvRaw()` - TLS-aware I/O
- Requires UE 5.7+ for native socket release; graceful fallback on older versions

**TypeScript Integration:**
- Added `rateLimitState` tracking with cleanup on connection close

</details>

### 🛠️ Fixed

<details>
<summary><b>🔧 TLS Memory Management</b> (<code>321206e</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Struct Initialization** | Fixed `FParsedWebSocketUrl` member initialization order (Port using uninitialized `bUseTls`) |
| **SSL Context Ownership** | Added `bOwnsSslContext` to prevent double-free of client contexts owned by `ISslManager` |

</details>

<details>
<summary><b>🔧 Thread Safety & TLS Error Handling</b> (<code>6fd1553</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Mutex Protection** | Added `SocketRateLimits` cleanup in `ForceReconnect` with proper mutex locking |
| **Declaration** | Moved `ShutdownTls()` declaration outside `WITH_SSL` guard for compilation compatibility |

</details>

<details>
<summary><b>🔧 Review Feedback Fixes</b> (<code>8987a3e</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Duplicate Call** | Fixed duplicate `ActiveSockets.Empty()` call in connection manager |
| **TypeScript Cleanup** | Added `rateLimitState` cleanup in `closeAll()` method |

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | 1.25.3 → 1.26.0 | [#187](https://github.com/ChiR24/Unreal_mcp/pull/187) |
| `mcp-client-capabilities` | Latest | [#186](https://github.com/ChiR24/Unreal_mcp/pull/186) |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.32.0 → 4.32.1 | [#185](https://github.com/ChiR24/Unreal_mcp/pull/185) |
| `actions/github-script` | 7.0.1 → 8.0.0 | [#184](https://github.com/ChiR24/Unreal_mcp/pull/184) |

</details>

---

## 🏷️ [0.5.13] - 2026-02-02

> [!IMPORTANT]
> ### 🛡️ Security & Compatibility Release
> This release includes multiple critical security fixes for command injection and path traversal vulnerabilities, along with full Unreal Engine 5.0 backward compatibility and WebSocket stability improvements.

### 🛡️ Security

<details>
<summary><b>🔒 Command Injection in UITools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/144">#144</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection via unsanitized user input in widget creation |
| **Fix** | Added `sanitizeConsoleString()` and applied `sanitizeAssetName()` to all user-provided identifiers |
| **Contributors** | @google-labs-jules[bot] |

</details>

<details>
<summary><b>🔒 Command Injection in LevelTools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/179">#179</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection via level names, event types, and game mode parameters |
| **Fix** | Added `sanitizeCommandArgument()` and applied to all console command parameters |
| **Contributors** | @google-labs-jules[bot] |

</details>

<details>
<summary><b>🔒 Path Traversal in Asset Listing</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/163">#163</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in `listAssets` via `filter.pathStartsWith` parameter |
| **Fix** | Applied `normalizeAndSanitizePath()` to GraphQL `listAssets` and asset handler `list` action |
| **Contributors** | @google-labs-jules[bot] |

</details>

### ✨ Added

<details>
<summary><b>🎮 Unreal Engine 5.0 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/183">#183</a>)</summary>

| Component | Description |
|-----------|-------------|
| **API Abstractions** | Version-guarded macros for Material, Niagara, AssetRegistry, Animation, and World Partition APIs |
| **Build System** | Made plugin dependencies optional with dynamic memory-based configuration |
| **Coverage** | 41 handler files updated with UE 5.0-5.7 compatibility |

**Compatibility Macros Added:**
- `MCP_GET_MATERIAL_EXPRESSIONS()` - Abstracts material expression access
- `MCP_DATALAYER_TYPE` / `MCP_DATALAYER_ASSET_TYPE` - Data layer type abstraction
- `MCP_ASSET_FILTER_CLASS_PATHS` - Asset registry filter abstraction
- `MCP_ASSET_DATA_GET_CLASS_PATH()` - FAssetData abstraction
- `MCP_NIAGARA_EMITTER_DATA_TYPE` - Niagara emitter abstraction

</details>

### 🛠️ Fixed

<details>
<summary><b>🔌 WebSocket Stability</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/180">#180</a>, <a href="https://github.com/ChiR24/Unreal_mcp/pull/181">#181</a>)</summary>

| Fix | Description |
|-----|-------------|
| **TOCTOU Race** | Fixed Time-of-Check-Time-of-Use race condition in ListenSocket shutdown |
| **Shutdown Hang** | Fixed WebSocket server blocking cook/package builds |
| **Version Compatibility** | Fixed `PendingReceived.RemoveAt()` API differences for UE 5.4+ |

**Contributors:** @kalihman

</details>

<details>
<summary><b>🔧 Resource Handlers</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/165">#165</a>)</summary>

- Fixed broken actors and level resource handlers
- Added missing actors and level resources to MCP resource list

**Contributors:** @kalihman

</details>

<details>
<summary><b>🔧 Other Fixes</b></summary>

| Fix | Description |
|-----|-------------|
| UE 5.7 | Resolved macro handling and ControlRig dynamic loading issues |
| UE 5.5 | Fixed API compatibility issues in handlers |
| UE 5.1 | Fixed `MaterialDomain.h` inclusion path |
| JSON | Refactored JSON handling in McpAutomationBridge |

</details>

### 🧪 Testing

- Added security regression tests for UITools, LevelTools, and asset handlers

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `release-drafter/release-drafter` | 6.1.1 → 6.2.0 | [#160](https://github.com/ChiR24/Unreal_mcp/pull/160) |
| `actions/checkout` | 6.0.1 → 6.0.2 | [#161](https://github.com/ChiR24/Unreal_mcp/pull/161) |
| `github/codeql-action` | 4.31.10 → 4.32.0 | [#168](https://github.com/ChiR24/Unreal_mcp/pull/168), [#170](https://github.com/ChiR24/Unreal_mcp/pull/170) |
| `google-github-actions/run-gemini-cli` | Latest | [#177](https://github.com/ChiR24/Unreal_mcp/pull/177) |

</details>

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | Latest | [#154](https://github.com/ChiR24/Unreal_mcp/pull/154) |
| `hono` | 4.11.4 → 4.11.7 | [#173](https://github.com/ChiR24/Unreal_mcp/pull/173) |
| `@types/node` | Various updates | [#158](https://github.com/ChiR24/Unreal_mcp/pull/158), [#162](https://github.com/ChiR24/Unreal_mcp/pull/162), [#175](https://github.com/ChiR24/Unreal_mcp/pull/175) |

</details>

---

## 🏷️ [0.5.12] - 2026-01-15

> [!NOTE]
> ### 🔧 Handler Synchronization Release
> This release focuses on synchronizing TypeScript handler parameters with C++ handlers and dependency updates.

### 🛠️ Fixed

<details>
<summary><b>🔧 TS Handler Parameter Sync</b> (<code>5953232</code>)</summary>

- Synchronized TypeScript handler parameters with C++ handlers for consistency
- Fixed parameter mapping issues between TS and C++ layers

</details>

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `release-drafter/release-drafter` | 6.1.0 → 6.1.1 | [#141](https://github.com/ChiR24/Unreal_mcp/pull/141) |
| `google-github-actions/run-gemini-cli` | Latest | [#142](https://github.com/ChiR24/Unreal_mcp/pull/142) |

</details>

<details>
<summary><b>NPM Package Updates</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/143">#143</a>)</summary>

| Package | Update |
|---------|--------|
| `@types/node` | Various dev dependency updates |

</details>

---

## 🏷️ [0.5.11] - 2026-01-12

> [!IMPORTANT]
> ### 🛡️ Security Hardening & UE 5.7 Compatibility
> This release includes multiple critical security fixes for path traversal and command injection vulnerabilities, along with UE 5.7 Interchange compatibility fixes.

### 🛡️ Security

<details>
<summary><b>🔒 Path Traversal in Asset Import</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/125">#125</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in asset import functionality |
| **Fix** | Added path sanitization and validation |

</details>

<details>
<summary><b>🔒 Command Injection Bypass</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/122">#122</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Command injection bypass via flexible whitespace |
| **Fix** | Enhanced command validation to detect and block bypass attempts |

</details>

<details>
<summary><b>🔒 Path Traversal in Screenshots</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/120">#120</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in screenshot filenames |
| **Fix** | Implemented filename sanitization and path validation |

</details>

<details>
<summary><b>🔒 Path Traversal in GraphQL</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/135">#135</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in GraphQL resolvers |
| **Fix** | Added input sanitization for GraphQL resolver paths |

</details>

<details>
<summary><b>🔒 GraphQL CORS Configuration</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/118">#118</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 MEDIUM |
| **Vulnerability** | Insecure GraphQL CORS configuration |
| **Fix** | Implemented secure CORS policy |

</details>

<details>
<summary><b>🔒 Enhanced Command Validation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/113">#113</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection bypasses |
| **Fix** | Enhanced validation patterns to prevent injection bypasses |

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 UE 5.7 Asset Import Crash</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/138">#138</a>)</summary>

| Fix | Description |
|-----|-------------|
| **Interchange Compatibility** | Deferred asset import to next tick for UE 5.7 Interchange compatibility |
| **Name Sanitization** | Improved asset import robustness and name sanitization |

**Closes [#137](https://github.com/ChiR24/Unreal_mcp/issues/137)**

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | 1.25.1 → 1.25.2 | [#119](https://github.com/ChiR24/Unreal_mcp/pull/119) |
| `hono` | 4.11.1 → 4.11.4 | [#129](https://github.com/ChiR24/Unreal_mcp/pull/129) |
| `@types/node` | Various updates | [#130](https://github.com/ChiR24/Unreal_mcp/pull/130), [#133](https://github.com/ChiR24/Unreal_mcp/pull/133), [#134](https://github.com/ChiR24/Unreal_mcp/pull/134) |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.31.9 → 4.31.10 | [#126](https://github.com/ChiR24/Unreal_mcp/pull/126) |
| `actions/setup-node` | 6.1.0 → 6.2.0 | [#133](https://github.com/ChiR24/Unreal_mcp/pull/133) |
| `dependabot/fetch-metadata` | 2.4.0 → 2.5.0 | [#114](https://github.com/ChiR24/Unreal_mcp/pull/114) |

</details>

---

## 🏷️ [0.5.10] - 2026-01-04

> [!IMPORTANT]
> ### 🚀 Context Reduction Initiative & Spline System
> This release implements the **Context Reduction Initiative** (Phases 48-53), reducing AI context overhead from ~78,000 to ~25,000 tokens, and adds a complete **Spline System** (Phase 26) with 21 new actions. ([#107](https://github.com/ChiR24/Unreal_mcp/pull/107), [#105](https://github.com/ChiR24/Unreal_mcp/pull/105))

### ✨ Added

<details>
<summary><b>🛤️ Spline System (Phase 26)</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/105">#105</a>)</summary>

New `manage_splines` tool with 21 actions for spline-based content creation:

| Category | Actions |
|----------|---------|
| **Creation** | `create_spline_actor`, `add_spline_point`, `remove_spline_point`, `set_spline_point` |
| **Properties** | `set_closed_loop`, `set_spline_type`, `set_tangent`, `get_spline_info` |
| **Mesh Components** | `create_spline_mesh`, `set_mesh_asset`, `set_spline_mesh_axis`, `set_spline_mesh_material` |
| **Scattering** | `create_mesh_along_spline`, `set_scatter_spacing`, `randomize_scatter` |
| **Quick Templates** | `create_road_spline`, `create_river_spline`, `create_fence_spline`, `create_wall_spline`, `create_cable_spline`, `create_pipe_spline` |
| **Utility** | `get_splines_info` |

**C++ Implementation:**
- `McpAutomationBridge_SplineHandlers.cpp` (1,512 lines)
- Full UE5 Spline API integration with `USplineComponent` and `USplineMeshComponent`

</details>

<details>
<summary><b>🔧 Pipeline Management Tool</b></summary>

New `manage_pipeline` tool for dynamic tool category management:

| Action | Description |
|--------|-------------|
| `set_categories` | Enable specific tool categories (core, world, authoring, gameplay, utility, all) |
| `list_categories` | Show available categories and their tools |
| `get_status` | View current state and tool counts |

**MCP Capability:**
- Server advertises `capabilities.tools.listChanged: true`
- Client capability detection via `mcp-client-capabilities` package
- Backward compatible: clients without `listChanged` support get ALL tools

</details>

### 🔧 Changed

<details>
<summary><b>📉 Context Reduction Initiative (Phases 48-53)</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/107">#107</a>)</summary>

| Phase | Description | Token Reduction |
|-------|-------------|-----------------|
| **Phase 48** | Schema Pruning - Condensed all 35+ tool descriptions to 1-2 sentences | ~23,000 |
| **Phase 49** | Common Schema Extraction - Shared schemas for paths, names, locations | ~8,000 |
| **Phase 50** | Dynamic Tool Loading - Category-based filtering | ~50,000 (when using filtering) |
| **Phase 53** | Strategic Tool Merging - Consolidated 4 tools | ~10,000 |

**Total Potential Reduction: ~91,000 tokens**

**Common Schemas Added:**
- `assetPath`, `actorName`, `location`, `rotation`, `scale`, `save`, `overwrite`
- `standardResponse` for consistent output formatting
- Helper functions: `createOutputSchema()`, `actionDescription()`

</details>

<details>
<summary><b>🔀 Tool Consolidation (Phase 53)</b></summary>

| Deprecated Tool | Merged Into | Actions Moved |
|-----------------|-------------|---------------|
| `manage_blueprint_graph` | `manage_blueprint` | 11 graph actions |
| `manage_audio_authoring` | `manage_audio` | 30 authoring actions |
| `manage_niagara_authoring` | `manage_effect` | 36 authoring actions |
| `manage_animation_authoring` | `animation_physics` | 45 authoring actions |

**Benefits:**
- Reduced tool count: 38 → 35
- Simplified tool discovery for AI assistants
- Backward compatible: deprecated tools still work with once-per-session warnings
- Action routing uses parameter sniffing to resolve conflicts

</details>

### ⚠️ Deprecated

- `manage_blueprint_graph` - Use `manage_blueprint` with graph actions instead
- `manage_audio_authoring` - Use `manage_audio` with authoring actions instead
- `manage_niagara_authoring` - Use `manage_effect` with authoring actions instead
- `manage_animation_authoring` - Use `animation_physics` with authoring actions instead

### 📊 Statistics

- **Files Changed:** 20
- **Lines Added:** 4,541
- **Lines Removed:** 3,555
- **Net Change:** +986 lines
- **New C++ Handler:** 1,512 lines (`McpAutomationBridge_SplineHandlers.cpp`)
- **New TS Handler:** 169 lines (`spline-handlers.ts`)
- **Common Schemas Added:** 50+ reusable schema definitions

### 🔗 Related Issues

Closes [#104](https://github.com/ChiR24/Unreal_mcp/issues/104), [#106](https://github.com/ChiR24/Unreal_mcp/issues/106), [#108](https://github.com/ChiR24/Unreal_mcp/issues/108), [#109](https://github.com/ChiR24/Unreal_mcp/issues/109), [#111](https://github.com/ChiR24/Unreal_mcp/issues/111)

---

## 🏷️ [0.5.9] - 2026-01-03

> [!IMPORTANT]
> ### 🎮 Major Feature Release
> This release introduces **15+ new automation tools** with comprehensive handlers for Navigation, Volumes, Level Structure, Sessions, Game Framework, and complete game development systems. ([#53](https://github.com/ChiR24/Unreal_mcp/pull/53))

### 🛡️ Security

<details>
<summary><b>🔒 Fix Arbitrary File Read in LogTools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/103">#103</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Arbitrary file read via `logPath` parameter |
| **Impact** | Attackers could read any file on the system by manipulating the `logPath` override |
| **Fix** | Validated that `logPath` ends with `.log` and is within `Saved/Logs` directory |

**Protections Added:**
- Enforced `.log` extension requirement
- Restricted to `Saved/Logs` directory (CWD or UE_PROJECT_PATH)
- Added path traversal and sibling directory attack protection

</details>

### ✨ Added

<details>
<summary><b>🛠️ New Automation Tools</b></summary>

| Tool | Description |
|------|-------------|
| `manage_navigation` | NavMesh configuration, Nav Modifiers, Nav Links, pathfinding control |
| `manage_volumes` | 18 volume types (Trigger, Blocking, Audio, Physics, Navigation, Streaming) |
| `manage_level_structure` | World Partition, HLOD, Data Layers, Level Blueprints |
| `manage_sessions` | Split-screen, LAN play, Voice Chat configuration |
| `manage_game_framework` | GameMode, GameState, PlayerController, match flow |
| `manage_skeleton` | Bone manipulation, sockets, physics assets |
| `manage_material_authoring` | Material expressions, landscape materials |
| `manage_texture` | Texture creation, compression, virtual texturing |
| `manage_animation_authoring` | AnimBP, Control Rig, IK Rig, Retargeter |
| `manage_niagara_authoring` | Niagara systems, modules, parameters |
| `manage_gas` | Gameplay Ability System (Abilities, Effects, Attributes) |
| `manage_character` | Character creation, movement, locomotion |
| `manage_combat` | Weapons, projectiles, damage, melee combat |
| `manage_ai` | EQS, Perception, State Trees, Smart Objects |
| `manage_inventory` | Items, equipment, loot tables, crafting |
| `manage_interaction` | Interactables, destructibles, triggers |
| `manage_widget_authoring` | UMG widgets, layout, styling |
| `manage_networking` | Replication, RPCs, network prediction |
| `manage_audio_authoring` | MetaSounds, sound classes, dialogue |

</details>

### 🔧 Changed

<details>
<summary><b>Build & Infrastructure Improvements</b></summary>

| Change | Description |
|--------|-------------|
| Bounded Directory Search | Replaced unbounded recursive search with bounded depth search (3-4 levels) |
| Property Management | Enhanced property management across all automation handlers |
| Connection Manager | Added `IsReconnectPending()` method to McpConnectionManager |
| State Machine | Improved state machine creation with enhanced error handling |

</details>

### 📊 Statistics

- **New Tools:** 15+
- **New C++ Handler Files:** 20+

---

## 🏷️ [0.5.8] - 2026-01-02

> [!IMPORTANT]
> ### 🛡️ Security Release
> Critical security fix for path traversal vulnerability and material graph parameter improvements.

### 🛡️ Security

<details>
<summary><b>🔒 Fix Path Traversal in INI Reader</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/48">#48</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in `getProjectSetting()` |
| **Impact** | Attackers could access arbitrary files by injecting `../` sequences into the category parameter |
| **Fix** | Added strict regex validation `^[a-zA-Z0-9_-]+$` to `cleanCategory` in `src/utils/ini-reader.ts` |

</details>

### 🛠️ Fixed

<details>
<summary><b>Material Graph Parameter Mapping</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/50">#50</a>)</summary>

| Schema Parameter | C++ Handler Expected | Status |
|------------------|---------------------|--------|
| `fromNodeId` | `sourceNodeId` | ✅ Auto-mapped |
| `toNodeId` | `targetNodeId` | ✅ Auto-mapped |
| `toPin` | `inputName` | ✅ Auto-mapped |

Closes [#49](https://github.com/ChiR24/Unreal_mcp/issues/49)

</details>

---

## 🏷️ [0.5.7] - 2026-01-01

> [!IMPORTANT]
> ### 🛡️ Security Release
> Critical security fix for Python execution bypass vulnerability.

### 🛡️ Security

<details>
<summary><b>🔒 Fix Python Execution Bypass</b> (<code>e16dab0</code>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Python execution restriction bypass |
| **Impact** | Attackers could execute arbitrary Python code by using tabs instead of spaces after the `py` command |
| **Fix** | Updated `CommandValidator` to use regex `^py(?:\s|$)` which correctly matches `py` followed by any whitespace |

</details>

### 🔧 Changed

<details>
<summary><b>Release Process Improvements</b></summary>

- Removed automatic git tag creation from release workflow
- Updated release summary instructions for manual tag management

</details>

### 🔄 Dependencies

<details>
<summary><b>Package Updates</b></summary>

| Package | Update | Type |
|---------|--------|------|
| `zod` | 4.2.1 → 4.3.4 | Minor |
| `qs` | 6.14.0 → 6.14.1 | Patch (indirect) |
| `github/codeql-action` | 3.28.1 → 4.31.9 | Major |

</details>

---

## 🏷️ [0.5.6] - 2025-12-30

> [!IMPORTANT]
> ### 🛡️ Type Safety Milestone
> This release achieves **near-zero `any` type usage** across the entire codebase. All tool interfaces, handlers, automation bridge, GraphQL resolvers, and WASM integration now use strict TypeScript types with `unknown` and proper type guards.

### ✨ Added

<details>
<summary><b>📐 New Zod Schema Infrastructure</b></summary>

| File | Description |
|------|-------------|
| `src/schemas/primitives.ts` | 261 lines of Zod schemas for Vector3, Rotator, Transform, Color, etc. |
| `src/schemas/responses.ts` | 380 lines of response validation schemas |
| `src/schemas/parser.ts` | 167 lines of safe parsing utilities with type guards |
| `src/schemas/index.ts` | 173 lines of unified schema exports |

**Total:** 981 lines of new type-safe schema infrastructure

</details>

<details>
<summary><b>🔧 Type-Safe Argument Helpers</b> (<code>d5e6d1e</code>)</summary>

New extraction functions in `argument-helper.ts`:

| Function | Description |
|----------|-------------|
| `extractString(params, key)` | Extract required string with assertion |
| `extractOptionalString(params, key)` | Extract optional string |
| `extractNumber(params, key)` | Extract required number with assertion |
| `extractOptionalNumber(params, key)` | Extract optional number |
| `extractBoolean(params, key)` | Extract required boolean with assertion |
| `extractOptionalBoolean(params, key)` | Extract optional boolean |
| `extractArray<T>(params, key, validator?)` | Extract typed array with optional validation |
| `extractOptionalArray<T>(params, key, validator?)` | Extract optional array |
| `normalizeArgsTyped(args, configs)` | Returns `NormalizedArgs` interface with accessor methods |

**NormalizedArgs Interface:**
- `getString(key)`, `getOptionalString(key)`
- `getNumber(key)`, `getOptionalNumber(key)`
- `getBoolean(key)`, `getOptionalBoolean(key)`
- `get(key)` for raw `unknown` access
- `raw()` for full object access

</details>

<details>
<summary><b>🔌 WASM Module Interface</b> (<code>d5e6d1e</code>)</summary>

Defined structured `WASMModule` interface replacing `any`:

```typescript
interface WASMModule {
  PropertyParser?: new () => { parse_properties(json, maxDepth) };
  TransformCalculator?: new () => { composeTransform, decomposeMatrix };
  Vector?: new (x, y, z) => { x, y, z, add(other) };
  DependencyResolver?: new () => { analyzeDependencies, calculateDepth, ... };
}
```

</details>

<details>
<summary><b>📝 Automation Bridge Types</b> (<code>f97b008</code>)</summary>

| Type | Location | Description |
|------|----------|-------------|
| `QueuedRequestItem` | `automation/types.ts` | Typed interface for queued request items |
| `ASTFieldNode` | `graphql/resolvers.ts` | GraphQL AST node types for parseLiteral |
| `ASTNode` | `graphql/resolvers.ts` | Typed AST parsing |

</details>

### 🔧 Changed

<details>
<summary><b>🎯 Tool Interfaces Refactored</b> (<code>d5e6d1e</code>)</summary>

**ITools Interface - Replaced all `any` with concrete types:**

| Property | Before | After |
|----------|--------|-------|
| `materialTools` | `any` | `MaterialTools` |
| `niagaraTools` | `any` | `NiagaraTools` |
| `animationTools` | `any` | `AnimationTools` |
| `physicsTools` | `any` | `PhysicsTools` |
| `lightingTools` | `any` | `LightingTools` |
| `debugTools` | `any` | `DebugVisualizationTools` |
| `performanceTools` | `any` | `PerformanceTools` |
| `audioTools` | `any` | `AudioTools` |
| `uiTools` | `any` | `UITools` |
| `introspectionTools` | `any` | `IntrospectionTools` |
| `engineTools` | `any` | `EngineTools` |
| `behaviorTreeTools` | `any` | `BehaviorTreeTools` |
| `logTools` | `any` | `LogTools` |
| `inputTools` | `any` | `InputTools` |
| Index signature | `[key: string]: any` | `[key: string]: unknown` |

**StandardActionResponse:**
- Changed `StandardActionResponse<T = any>` → `StandardActionResponse<T = unknown>`

**IBlueprintTools:**
- `operations: any[]` → `operations: Array<Record<string, unknown>>`
- `defaultValue?: any` → `defaultValue?: unknown`
- `propertyValue: any` → `propertyValue: unknown`

**IAssetResources:**
- `list(): Promise<any>` → `list(): Promise<Record<string, unknown>>`

</details>

<details>
<summary><b>🔷 GraphQL Resolvers Type Safety</b> (<code>f97b008</code>, <code>fa4dddc</code>)</summary>

All scalar resolvers now use typed parameters:

| Scalar | Before | After |
|--------|--------|-------|
| `Vector.serialize` | `(value: any)` | `(value: unknown)` |
| `Rotator.serialize` | `(value: any)` | `(value: unknown)` |
| `Transform.parseLiteral` | `(ast: any)` | `(ast: ASTNode)` |
| `JSON.parseLiteral` | `(ast: any)` | `(ast: ASTNode): unknown` |

**Internal interfaces typed:**
- `Asset.metadata?: Record<string, any>` → `Record<string, unknown>`
- `Actor.properties?: Record<string, any>` → `Record<string, unknown>`
- `Blueprint.defaultValue?: any` → `unknown`

</details>

<details>
<summary><b>🌐 Automation Bridge Type Safety</b> (<code>f97b008</code>)</summary>

| Location | Before | After |
|----------|--------|-------|
| `onError` callback | `(err: any)` | `(err: unknown)` |
| `onHandshakeFail` callback | `(err: any)` | `(err: Record<string, unknown>)` |
| `catch` block | `catch (err: any)` | `catch (err: unknown)` with type guard |
| `onMessage` handler | `(data: any)` | `(data: Buffer \| string)` |
| `queuedRequestItems` | inline type with `any` | `QueuedRequestItem[]` |

</details>

<details>
<summary><b>🔌 WASM Integration Type Safety</b> (<code>d5e6d1e</code>)</summary>

| Method | Before | After |
|--------|--------|-------|
| `parseProperties()` | `Promise<any>` | `Promise<unknown>` |
| `analyzeDependencies()` | `Promise<any>` | `Promise<unknown>` |
| `fallbackParseProperties()` | `any` | `unknown` |
| `fallbackAnalyzeDependencies()` | `any` | `Record<string, unknown>` |
| `globalThis.fetch` patch | `(globalThis as any).fetch` | Typed with `GlobalThisWithFetch` |
| Error handling | `(error as any)?.code` | `(error as Record<string, unknown>)?.code` |

</details>

<details>
<summary><b>📊 Handler Types Expanded</b> (<code>d5e6d1e</code>)</summary>

`src/types/handler-types.ts` expanded with 147+ lines of new typed interfaces for all handler argument types.

</details>

### 🛠️ Fixed

<details>
<summary><b>✅ extractOptionalArray Behavior</b> (<code>f97b008</code>)</summary>

- Now returns `undefined` (instead of throwing) when value is not an array
- Documented behavior: graceful fallback for type mismatches
- Allows handlers to use default behavior when optional arrays are invalid

</details>

### 📊 Statistics

- **Files Changed:** 70 source files
- **Lines Added:** 3,806
- **Lines Removed:** 1,816
- **Net Change:** +1,990 lines (mostly type definitions)
- **New Schema Files:** 4 (981 lines total)
- **`any` → `unknown` Replacements:** 100+ occurrences

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `actions/first-interaction` | 1.3.0 → 3.1.0 | [#38](https://github.com/ChiR24/Unreal_mcp/pull/38) |
| `actions/labeler` | 5.0.0 → 6.0.1 | Dependabot |
| `github/codeql-action` | SHA update | Dependabot |
| `release-drafter/release-drafter` | SHA update | Dependabot |
| Dev dependencies group | 2 updates | Dependabot |

</details>

---

## 🏷️ [0.5.5] - 2025-12-29

> [!NOTE]
> ### 📝 Quality & Validation Release
> This release focuses on **input validation**, **structured logging**, and **developer experience** improvements. WebSocket connections now enforce message size limits, Blueprint graph editing supports user-friendly node names, and all tools use structured logging.

### ✨ Added

<details>
<summary><b>🔌 WebSocket Message Size Limits</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/36">#36</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Max Message Size** | 5MB limit for WebSocket frames and accumulated messages |
| **Close Code 1009** | Connections close with standard "Message Too Big" code when exceeded |
| **Fragment Accumulation** | Size checks applied during fragmented message assembly |

**C++ Changes:**
- Added `MaxWebSocketMessageBytes` (5MB) and `MaxWebSocketFramePayloadBytes` constants
- Implemented size validation at frame receive, fragment accumulation, and initial payload
- Proper teardown with `WebSocketCloseCodeMessageTooBig` (1009)

</details>

<details>
<summary><b>🔷 Blueprint Node Type Aliases</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

User-friendly node names now map to internal K2Node classes:

| Alias | K2Node Class |
|-------|-------------|
| `Branch` | `K2Node_IfThenElse` |
| `Sequence` | `K2Node_ExecutionSequence` |
| `ForLoop` | `K2Node_ForLoop` |
| `ForLoopWithBreak` | `K2Node_ForLoopWithBreak` |
| `WhileLoop` | `K2Node_WhileLoop` |
| `Switch` | `K2Node_SwitchInteger` |
| `Select` | `K2Node_Select` |
| `DoOnce`, `DoN`, `FlipFlop`, `Gate`, `MultiGate` | Flow control nodes |
| `SpawnActorFromClass`, `GetAllActorsOfClass` | Actor manipulation |
| `Timeline`, `MakeArray`, `MakeStruct`, `BreakStruct` | Data/utility nodes |

**C++ & TypeScript Sync:**
- `BLUEPRINT_NODE_ALIASES` map in `graph-handlers.ts`
- `NodeTypeAliases` map in `McpAutomationBridge_BlueprintGraphHandlers.cpp`

</details>

<details>
<summary><b>🌳 Behavior Tree Generic Node Types</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

| Node Type | Default Class | Category |
|-----------|---------------|----------|
| `Task` | `BTTask_Wait` | task |
| `Decorator` / `Blackboard` | `BTDecorator_Blackboard` | decorator |
| `Service` / `DefaultFocus` | `BTService_DefaultFocus` | service |
| `Composite` | `BTComposite_Sequence` | composite |

Aliases for common BT nodes: `Wait`, `MoveTo`, `PlaySound`, `Cooldown`, `Loop`, `TimeLimit`, `Selector`, etc.

</details>

<details>
<summary><b>📊 show_stats Action</b></summary>

New `show_stats` action in `system_control` tool:
- Toggle engine stats display (`stat Unit`, `stat FPS`, etc.)
- Parameters: `category` (string), `enabled` (boolean)

</details>

### 🔧 Changed

<details>
<summary><b>📋 Structured Logging</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/36">#36</a>)</summary>

Replaced `console.error`/`console.warn` with structured `Logger` across all tools:

| File | Change |
|------|--------|
| `actors.ts` | WASM debug logging |
| `debug.ts` | Viewmode stability warnings |
| `dynamic-handler-registry.ts` | Handler overwrite warnings |
| `editor.ts` | Removed commented debug logs |
| `physics.ts` | Improved error handling with fallback mesh resolution |

</details>

<details>
<summary><b>🎯 Handler Response Improvements</b></summary>

| Handler | Change |
|---------|--------|
| `actor-handlers.ts` | Returns clean responses without `ResponseFactory.success()` wrapping |
| `blueprint-handlers.ts` | Includes `blueprintPath` in responses |
| `environment.ts` | Changed default snapshot path to `./tmp/unreal-mcp/` |

</details>

### 🛠️ Fixed

<details>
<summary><b>✅ Input Validation Enhancements</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

| Handler | Validation Added |
|---------|------------------|
| `editor-handlers.ts` | Viewport resolution requires positive numbers |
| `asset-handlers.ts` | Folder paths must start with `/` |
| `lighting-handlers.ts` | Valid light types: `point`, `directional`, `spot`, `rect`, `sky` |
| `lighting-handlers.ts` | Valid GI methods: `lumen`, `screenspace`, `none`, `raytraced`, `ssgi` |
| `performance-handlers.ts` | Valid profiling types with clear error messages |
| `performance-handlers.ts` | Scalability levels clamped to 0-4 range |
| `system-handlers.ts` | Quality level clamped to 0-4 range |

</details>

<details>
<summary><b>🔧 WASM Binding Patching</b> (<code>7cc602a</code>)</summary>

- Fixed TOCTOU (Time-of-Check-Time-of-Use) race condition in `patch-wasm.js`
- Uses atomic file operations with file descriptors (`openSync`, `ftruncateSync`, `writeSync`)
- Proper error handling for missing WASM files

</details>

### 🗑️ Removed

<details>
<summary><b>🧹 Code Cleanup</b></summary>

| Removed | Lines | Reason |
|---------|-------|--------|
| `src/types/responses.ts` content | 355 | Obsolete response type definitions |
| `scripts/validate-server.js` | 46 | Unused validation script |
| `scripts/verify-automation-bridge.js` | 177 | Unused functions and broken code |

</details>

### 📊 Statistics

- **Files Changed:** 28+ source files
- **Lines Removed:** 436 (cleanup)
- **Lines Added:** 283 (validation + features)
- **New Node Aliases:** 30+ Blueprint, 20+ Behavior Tree

---

## 🏷️ [0.5.4] - 2025-12-27

> [!IMPORTANT]
> ### 🛡️ Security Release
> This release focuses on **security hardening** and **defensive improvements** across the entire stack, including command injection prevention, network isolation, and resource management.

### 🛡️ Security & Command Hardening

<details>
<summary><b>UBT Validation & Safe Execution</b></summary>

| Feature | Description |
|---------|-------------|
| **UBT Argument Validation** | Added `validateUbtArgumentsString` and `tokenizeArgs` to block dangerous characters (`;`, `|`, backticks) |
| **Safe Process Spawning** | Updated child process spawning to use `shell: false`, preventing shell injection attacks |
| **Console Command Validation** | Implemented strict input validation for the Unreal Automation Bridge to block chained or multi-line commands |
| **Argument Quoting** | Improved logging and execution logic to correctly quote arguments containing spaces |

</details>

### 🌐 Network & Host Binding

<details>
<summary><b>Localhost Default & Remote Configuration</b></summary>

| Feature | Description |
|---------|-------------|
| **Localhost Default** | WebSocket, Metrics, and GraphQL servers now bind to `127.0.0.1` by default |
| **Remote Exposure Prevention** | Prevents accidental remote exposure of services |
| **GRAPHQL_ALLOW_REMOTE** | Added environment variable check for explicit remote binding configuration |
| **Security Warnings** | Warnings logged for unsafe/permissive network settings |

</details>

### 🚦 Resource Management

<details>
<summary><b>Rate Limiting & Queue Management</b></summary>

| Feature | Description |
|---------|-------------|
| **IP-Based Rate Limiting** | Implemented rate limiting on the metrics server |
| **Queue Limits** | Introduced `maxQueuedRequests` to automation bridge to prevent memory exhaustion |
| **Message Size Enforcement** | Enforced `MAX_WS_MESSAGE_SIZE_BYTES` for WebSocket connections to reject oversized payloads |

</details>

### 🧪 Testing & Cleanup

<details>
<summary><b>Test Updates & File Cleanup</b></summary>

| Change | Description |
|--------|-------------|
| **Path Sanitization Tests** | Modified validation tests to verify path sanitization and expect errors for traversal attempts |
| **Removed Legacy Tests** | Removed outdated test files (`run-unreal-tool-tests.mjs`, `test-asset-errors.mjs`) |
| **Response Logging** | Implemented better response logging in the test runner |

</details>

### 🔄 Dependencies

- **dependencies group**: Bumped 2 updates via @dependabot ([#33](https://github.com/ChiR24/Unreal_mcp/pull/33))

---

## 🏷️ [0.5.3] - 2025-12-21

> [!IMPORTANT]
> ### 🔄 Major Enhancements
> - **Dynamic Type Discovery** - New runtime introspection for lights, debug shapes, and sequencer tracks
> - **Metrics Rate Limiting** - Per-IP rate limiting (60 req/min) on Prometheus endpoint
> - **Centralized Class Configuration** - Unified Unreal Engine class aliases
> - **Enhanced Type Safety** - Comprehensive TypeScript interfaces replacing `any` types

### ✨ Added

<details>
<summary><b>🔍 Dynamic Discovery & Engine Handlers</b></summary>

| Feature | Description |
|---------|-------------|
| **list_light_types** | Discovers all available light class types at runtime |
| **list_debug_shapes** | Enumerates supported debug shape types |
| **list_track_types** | Lists all sequencer track types available in the engine |
| **Heuristic Resolution** | Improved C++ handlers use multiple naming conventions and inheritance validation |
| **Vehicle Type Support** | Expanded vehicle type from union to string for flexibility |

**C++ Changes:**
- `McpAutomationBridge_LightingHandlers.cpp` - Runtime `ResolveUClass` for lights
- `McpAutomationBridge_SequenceHandlers.cpp` - Runtime resolution for tracks
- Added `UObjectIterator.h` for dynamic type scanning
- Unified spawn/track-creation flows
- Removed editor/PIE branching logic

</details>

<details>
<summary><b>⚙️ Tooling & Configuration</b></summary>

| Feature | Description |
|---------|-------------|
| **class-aliases.ts** | Centralized Unreal Engine class name mappings |
| **handler-types.ts** | Comprehensive TypeScript interfaces (ActorArgs, EditorArgs, LightingArgs, etc.) |
| **timeout constants** | Command-specific operation timeouts in constants.ts |
| **listDebugShapes()** | Programmatic access in DebugVisualizationTools |

**Type System:**
- Geometry types: Vector3, Rotator, Transform
- Required-component lookups
- Centralized class-alias mappings

</details>

<details>
<summary><b>📈 Metrics Server Enhancements</b></summary>

| Feature | Description |
|---------|-------------|
| **Rate Limiting** | Per-IP limit of 60 requests/minute |
| **Server Lifecycle** | Returns instance for better management |
| **Error Handling** | Improved internal error handling |

</details>

<details>
<summary><b>📚 Documentation & DX</b></summary>

| Feature | Description |
|---------|-------------|
| **handler-mapping.md** | Updated with new discovery actions |
| **README.md** | Clarified WASM build instructions |
| **Tool Definitions** | Synchronized with new discovery actions |

</details>

### 🔧 Changed

<details>
<summary><b>Handler Type Safety & Logic</b></summary>

**src/tools/handlers/common-handlers.ts:**
- Replaced `any` typings with strict `HandlerArgs`/`LocationInput`/`RotationInput`
- Added automation-bridge connectivity validation
- Enhanced location/rotation normalization with type guards

**Specialized Handlers:**
- `actor-handlers.ts` - Applied typed handler-args
- `asset-handlers.ts` - Improved argument normalization
- `blueprint-handlers.ts` - Added new action cases
- `editor-handlers.ts` - Enhanced default handling
- `effect-handlers.ts` - Added `list_debug_shapes`
- `graph-handlers.ts` - Improved validation
- `level-handlers.ts` - Type-safe operations
- `lighting-handlers.ts` - Added `list_light_types`
- `pipeline-handlers.ts` - Enhanced error handling

</details>

<details>
<summary><b>Infrastructure & Utilities</b></summary>

**Security & Validation:**
- `command-validator.ts` - Blocks semicolons, pipes, backticks
- `error-handler.ts` - Enhanced error logging
- `response-validator.ts` - Improved Ajv typing
- `safe-json.ts` - Generic typing for cleanObject
- `validation.ts` - Expanded path-traversal protection

**Performance:**
- `unreal-command-queue.ts` - Optimized queue processing (250ms interval)
- `unreal-bridge.ts` - Centralized timeout constants

</details>

### 🛠️ Fixed

- **Command Injection Prevention** - Additional dangerous command patterns blocked
- **Path Security** - Enhanced asset-name validation
- **Type Safety** - Eliminated `any` types across handler functions
- **Error Messages** - Clearer error messages for class resolution failures

### 📊 Statistics

- **Files Changed:** 20+
- **New Interfaces:** 15+ handler type definitions
- **Discovery Actions:** 3 new runtime introspection methods
- **Security Enhancements:** 5+ new validation patterns

### 🔄 Dependencies

- **graphql-yoga**: Bumped from 5.17.1 to 5.18.0 (#31)

---

## 🏷️ [0.5.2] - 2025-12-18

> [!IMPORTANT]
> ### 🔄 Breaking Changes
> - **Standardized Tools & Type Safety** - All tool handlers now use consistent interfaces with improved type safety. Some internal API signatures have changed. (`079e3c2`)

### ✨ Added

<details>
<summary><b>🛠️ Blueprint Enhancements</b> (<code>e710751</code>)</summary>

| Feature | Description |
|---------|-------------|
| **Dynamic Node Creation** | Support for creating nodes dynamically in Blueprint graphs |
| **Struct Property Support** | Added ability to set and get struct properties on Blueprint components |

</details>

### 🔄 Changed

<details>
<summary><b>🎯 Standardized Tool Interfaces</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/28">#28</a>)</summary>

| Component | Change |
|-----------|--------|
| Tool Handlers | Optimized bridge communication and standardized response handling |
| Type Safety | Hardened type definitions across all tool interfaces |
| Bridge Optimization | Improved performance and reliability of automation bridge |

</details>

### 🔧 CI/CD

- 🔗 **MCP Publisher** - Fixed download URL format in workflow steps (`0d452e7`)
- 🧹 **Workflow Cleanup** - Removed unnecessary success conditions from MCP workflow steps (`82bd575`)

---

## 🏷️ [0.5.1] - 2025-12-17

> [!WARNING]
> ### ⚠️ Breaking Changes
> - **Standardized Return Types** - All tool methods now return `StandardActionResponse` type instead of generic objects. Consumers must update their code to handle the new response structure with `success`, `data`, `warnings`, and `error` fields. (`5e615c5`)
> - **Test Suite Structure** - New test files added and existing tests enhanced with comprehensive coverage.

### 🔄 Changed

<details>
<summary><b>🎯 Standardized Tool Interfaces</b> (<code>5e615c5</code>)</summary>

| Component | Change |
|-----------|--------|
| Tool Methods | Updated all tool methods to return `StandardActionResponse` type for consistency |
| Tool Interfaces | Modified interfaces (assets, blueprint, editor, environment, foliage, landscape, level, sequence) to use standardized response format |
| Type System | Added proper type imports and exports for `StandardActionResponse` |
| Handler Files | Updated to work with new standardized response types |
| Response Structure | All implementations return correct structure with `success`/`error` fields |

</details>

### ✨ Added

<details>
<summary><b>🧪 Comprehensive Test Suite</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/25">#25</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Test Coverage** | Added comprehensive test files with success, error, and edge cases |
| **GraphQL DataLoader** | Implemented N+1 query optimization with batching and caching |
| **Type-Safe Interfaces** | Added type-safe automation response interfaces for better error handling |
| **Utility Tests** | Added tests for core utilities (normalize, safe-json, validation) |
| **Real-World Scenarios** | Enhanced coverage with real-world scenarios and cleanup procedures |
| **New Test Suites** | Audio, lighting, performance, input, and asset graph management |
| **Enhanced Logging** | Improved diagnostic logging throughout tools |
| **Documentation** | Updated supported Unreal Engine versions (5.0-5.7) in testing documentation |

</details>

### 🧹 Maintenance

- 🗑️ **Prompts Module Cleanup** - Removed prompts module and related GraphQL prompt functionality ([#26](https://github.com/ChiR24/Unreal_mcp/pull/26))
- 🔒 **Security Updates** - Removed unused dependencies (axios, json5, yargs) from package.json for security (`5e615c5`)
- 📐 **Tool Interfaces** - Enhanced asset and level tools with security validation and timeout handling (`5e615c5`)

### 📦 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR | Commit |
|---------|--------|-----|--------|
| `actions/checkout` | v4 → v6 | [#23](https://github.com/ChiR24/Unreal_mcp/pull/23) | `4c6b3b5` |
| `actions/setup-node` | v4 → v6 | [#22](https://github.com/ChiR24/Unreal_mcp/pull/22) | `71aa35c` |
| `softprops/action-gh-release` | 2.0.8 → 2.5.0 | [#21](https://github.com/ChiR24/Unreal_mcp/pull/21) | `b6c8a46` |

</details>

<details>
<summary><b>NPM Package Updates</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/24">#24</a>, <code>5e615c5</code>)</summary>

| Package | Update |
|---------|--------|
| `@modelcontextprotocol/sdk` | 1.25.0 → 1.25.1 |
| `@types/node` | 25.0.2 → 25.0.3 |

</details>

---

## 🏷️ [0.5.0] - 2025-12-16

> [!IMPORTANT]
> ### 🔄 Major Architecture Migration
> This release marks the **complete migration** from Unreal's built-in Remote Plugin to a native C++ **McpAutomationBridge** plugin. This provides:
> - ⚡ Better performance
> - 🔗 Tighter editor integration
> - 🚫 No dependency on Unreal's Remote API
>
> **BREAKING CHANGE:** Response format has been standardized across all automation tools. Clients should expect responses to follow the new `StandardActionResponse` format with `success`, `data`, `warnings`, and `error` fields.

### 🏗️ Architecture

| Change | Description |
|--------|-------------|
| 🆕 **Native C++ Plugin** | Introduced `McpAutomationBridge` - a native UE5 editor plugin replacing the Remote API |
| 🔌 **Direct Editor Integration** | Commands execute directly in the editor context via automation bridge subsystem |
| 🌐 **WebSocket Communication** | Implemented `McpBridgeWebSocket` for real-time bidirectional communication |
| 🎯 **Bridge-First Architecture** | All operations route through the native C++ bridge (`fe65968`) |
| 📐 **Standardized Responses** | All tools now return `StandardActionResponse` format (`0a8999b`) |

### ✨ Added

<details>
<summary><b>🎮 Engine Compatibility</b></summary>

- **UE 5.7 Support** - Updated McpAutomationBridge with ControlRig dynamic loading and improved sequence handling (`ec5409b`)

</details>

<details>
<summary><b>🔧 New APIs & Integrations</b></summary>

- **GraphQL API** - Broadened automation bridge with GraphQL support, WASM integration, UI/editor integrations (`ffdd814`)
- **WebAssembly Integration** - High-performance JSON parsing with 5-8x performance gains (`23f63c7`)

</details>

<details>
<summary><b>🌉 Automation Bridge Features</b></summary>

| Feature | Commit |
|---------|--------|
| Server mode on port `8091` | `267aa42` |
| Client mode with enhanced connection handling | `bf0fa56` |
| Heartbeat tracking and output capturing | `28242e1` |
| Event handling and asset management | `d10e1e2` |

</details>

<details>
<summary><b>🎛️ New Tool Systems (0a8999b, 0ac82ac)</b></summary>

| Tool | Description |
|------|-------------|
| 🎮 **Input Management** | New `manage_input` tool with EnhancedInput support for Input Actions and Mapping Contexts |
| 💡 **Lighting Manager** | Full lighting configuration via `manage_lighting` including spawn, GI setup, shadow config, build lighting |
| 📊 **Performance Manager** | `manage_performance` with profiling (CPU/GPU/Memory), optimization, scalability, Nanite/Lumen config |
| 🌳 **Behavior Tree Editing** | Full behavior tree creation and node editing via `manage_behavior_tree` |
| 🎬 **Enhanced Sequencer** | Track operations (add/remove tracks, set muted/solo/locked), display rate, tick resolution |
| 🌍 **World Partition** | Cell management, data layer toggling via `manage_level` |
| 🖼️ **Widget Management** | UI widget creation, visibility controls, child widget adding |

</details>

<details>
<summary><b>📊 Graph Editing Capabilities (0a8999b)</b></summary>

- **Blueprint Graph** - Direct node manipulation with `manage_blueprint_graph` (create_node, delete_node, connect_pins, etc.)
- **Material Graph** - Node operations via `manage_asset` (add_material_node, connect_material_pins, etc.)
- **Niagara Graph** - Module and parameter editing (add_niagara_module, set_niagara_parameter, etc.)

</details>

<details>
<summary><b>🛠️ New Handlers & Actions</b></summary>

- Blueprint graph management and Niagara functionalities (`aff4d55`)
- Physics simulation setup in AnimationTools (`83a6f5d`)
- **New Asset Actions:**
  - `generate_lods`, `add_material_parameter`, `list_instances`
  - `reset_instance_parameters`, `get_material_stats`, `exists`
  - `nanite_rebuild_mesh`
- World partition and rendering tool handlers (`83a6f5d`)
- Screenshot with base64 image encoding (`bb4f6a8`)

</details>

<details>
<summary><b>🧪 Test Suites</b></summary>

**50+ new test cases** covering:
- Animation, Assets, Materials
- Sequences, World Partition
- Blueprints, Niagara, Behavior Trees
- Audio, Input Actions
- And more! (`31c6db9`, `85817c9`, `fc47839`, `02fd2af`)

</details>

### 🔄 Changed

#### Core Refactors
| Component | Change | Commit |
|-----------|--------|--------|
| `SequenceTools` | Migrated to Automation Bridge | `c2fb15a` |
| `UnrealBridge` | Refactored for bridge connection | `7bd48d8` |
| Automation Dispatch | Editor-native handlers modernization | `c9db1a4` |
| Test Runner | Timeout expectations & content extraction | `c9766b0` |
| UI Handlers | Improved readability and organization | `bb4f6a8` |
| Connection Manager | Streamlined connection handling | `0ac82ac` |

#### Tool Improvements
- 🚗 **PhysicsTools** - Vehicle config logic updated, deprecated checks removed (`6dba9f7`)
- 🎬 **AnimationTools** - Logging and response normalization (`7666c31`)
- ⚠️ **Error Handling** - Utilities refactored, INI file reader added (`f5444e4`)
- 📐 **Blueprint Actions** - Timeout handling enhancements (`65d2738`)
- 🎨 **Materials** - Enhanced material graph editing capabilities (`0a8999b`)
- 🔊 **Audio** - Improved sound component management (`0a8999b`)

#### Other Changes
- 📡 **Connection & Logging** - Improved error messages for clarity (`41350b3`)
- 📚 **Documentation** - README updated with UE 5.7, WASM docs, architecture overview, 17 tools (`8d72f28`, `4d77b7e`)
- 🔄 **Dependencies** - Updated to latest versions (`08eede5`)
- 📝 **Type Definitions** - Enhanced tool interfaces and type coverage (`0a8999b`)

### 🐛 Fixed

- `McpAutomationBridgeSubsystem` - Header removal, logging category, heartbeat methods (`498f644`)
- `McpBridgeWebSocket` - Reliable WebSocket communication (`861ad91`)
- **AutomationBridge** - Heartbeat handling and server metadata retrieval (`0da54f7`)
- **UI Handlers** - Missing payload and invalid widget path error handling (`bb4f6a8`)
- **Screenshot** - Clearer error messages and flow (`bb4f6a8`)

### 🗑️ Removed

| Removed | Reason |
|---------|--------|
| 🔌 Remote API Dependency | Replaced by native C++ plugin |
| 🐍 Python Fallbacks | Native C++ automation preferred (`fe65968`) |
| 📦 Unused HTTP Client | Cleanup from error-handler (`f5444e4`) |

---

## 🏷️ [0.4.7] - 2025-11-16

### ✨ Added
- Output Log reading via `system_control` tool with `read_log` action. filtering by category, level, line count.
- New `src/tools/logs.ts` implementing robust log tailing.
- 🆕 Initial `McpAutomationBridge` plugin with foundational implementation (`30e62f9`)
- 🧪 Comprehensive test suites for various Unreal Engine tools (`31c6db9`)

### 🔄 Changed
- `system_control` tool schema: Added `read_log` action.
- Updated tool handlers to route `read_log` to LogTools.
- Version bumped to 0.4.7.

### 📚 Documentation
- Updated README.md with initial bridge documentation (`a24dafd`)

---

## 🏷️ [0.4.6] - 2025-10-04

### 🐛 Fixed
- Fixed duplicate response output issue where tool responses were displayed twice in MCP content
- Response validator now emits concise summaries instead of duplicating full JSON payloads
- Structured content preserved for validation while user-facing output is streamlined

---

## 🏷️ [0.4.5] - 2025-10-03

### ✨ Added
- 🔧 Expose `UE_PROJECT_PATH` environment variable across runtime config, Smithery manifest, and client configs
- 📁 Added `projectPath` to runtime `configSchema` for Smithery's session UI

### 🔄 Changed
- ⚡ Made `createServer` synchronous factory (removed `async`)
- 🏠 Default for `ueHost` in exported `configSchema`

### 📚 Documentation
- Updated `README.md`, config examples to include `UE_PROJECT_PATH`
- Updated `smithery.yaml` and `server.json` manifests

### 🔨 Build
- Rebuilt Smithery bundle and TypeScript output

### 🐛 Fixed
- Smithery UI blank `ueHost` field by defining default in runtime schema

---

## 🏷️ [0.4.4] - 2025-09-28

### ✨ Improvements

- 🤝 **Client Elicitation Helper** - Added support for Cursor, VS Code, Claude Desktop, and other MCP clients
- 📊 **Consistent RESULT Parsing** - Handles JSON5 and legacy Python literals across all tools
- 🔒 **Safe Output Stringification** - Robust handling of circular references and complex objects
- 🔍 **Enhanced Logging** - Improved validation messages for easier debugging

---

## 🏷️ [0.4.0] - 2025-09-20

> **Major Release** - Consolidated Tools Mode

### ✨ Improvements

- 🎯 **Consolidated Tools Mode Exclusively** - Removed legacy mode, all tools now use unified handler system
- 🧹 **Simplified Tool Handlers** - Removed deprecated code paths and inline plugin validation
- 📝 **Enhanced Error Handling** - Better error messages and recovery mechanisms

### 🔧 Quality & Maintenance

- ⚡ Reduced resource usage by optimizing tool handlers
- 🧹 Cleanup of deprecated environment variables

---

## 🏷️ [0.3.1] - 2025-09-19

> **BREAKING:** Connection behavior is now on-demand

### 🏗️ Architecture

- 🔄 **On-Demand Connection** - Shifted to intelligent on-demand connection model
- 🚫 **No Background Processes** - Eliminated persistent background connections

### ⚡ Performance

- Reduced resource usage and eliminated background processes
- Optimized connection state management

### 🛡️ Reliability

- Improved error handling and connection state management
- Better recovery from connection failures

---

## 🏷️ [0.3.0] - 2025-09-17

> 🎉 **Initial Public Release**

### ✨ Features

- 🎮 **13 Consolidated Tools** - Full suite of Unreal Engine automation tools
- 📁 **Normalized Asset Listing** - Auto-map `/Content` and `/Game` paths
- 🏔️ **Landscape Creation** - Returns real UE/Python response data
- 📝 **Action-Oriented Descriptions** - Enhanced tool documentation with usage examples

### 🔧 Quality & Maintenance

- Server version 0.3.0 with clarified 13-tool mode
- Comprehensive documentation and examples
- Lint error fixes and code style cleanup

---

<div align="center">

### 🔗 Links

[![GitHub](https://img.shields.io/badge/GitHub-Repository-181717?style=for-the-badge&logo=github)](https://github.com/ChiR24/Unreal_mcp)
[![npm](https://img.shields.io/badge/npm-Package-CB3837?style=for-the-badge&logo=npm)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![UE5](https://img.shields.io/badge/Unreal-5.6%20|%205.7-0E1128?style=for-the-badge&logo=unrealengine)](https://www.unrealengine.com/)

</div>
