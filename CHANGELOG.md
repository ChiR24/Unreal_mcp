# 📋 Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## 🏷️ [Unreleased]

<details>
<summary><b>✨ Added</b></summary>

- **`get_scs` finds every Blueprint with a kind of component.** Given a folder (`path`) and `componentClass` ("TextRender", "PointLight", a /Script path, or a parent class such as "PrimitiveComponent"), it lists each Blueprint under the folder that has such a component, with the matching component names; `componentClass` also narrows one Blueprint's listing. Finding every enemy with a floating label took one `get_scs` per Blueprint.
- **`get_summary` says whether the level is saved.** Both of its readers (a level by path, or the one open in the editor) answer `unsaved` for that level, plus `unsavedPackages` and `unsavedPackageCount` for every level and asset with unsaved changes, the same set a level load would prompt for. "is the level saved" and "list unsaved changes" had only `restart_editor` with `validateOnly` to answer them, a capability that closes the editor.
- **A box child's size rule.** `set_widget_layout` size now sets a HorizontalBox or VerticalBox child to Auto or Fill (`sizeRule`) with a weight (`fillValue`), the setting get_widget_info already reported but nothing could change, so two buttons in a row can share it evenly. A canvas child keeps size {x, y}.
- **`control_actor.list` finds what is near a point.** `near` ({x, y, z} or [x, y, z]) lists the matching actors nearest first, each row with its distance to the actor's bounds, and `radius` keeps only those within it. It combines with every other filter, so a thing seen in a screenshot can be looked up by where it is. Of equally near actors the smaller one comes first, so the thing at the spot leads and the level-wide foliage actor, whose bounds contain every point, comes last.
- **describe says which variant reads each parameter.** A capability that stands for a family of variants (add_content_widget covers text, image, button, slider and nine more) lists every variant's parameters, 30 in that case, with nothing telling a slider's minValue from a progress bar's isMarquee. Each parameter only some variants read now carries `variants`, on the full describe and on the single-parameter one, on both doors. 201 of the 389 capabilities are such families.
- **`control_actor.list` filters by `tag`, `className` and `folder`.** `className` also matches subclasses (`Light` finds every light type) and takes a name or a path, a Blueprint's `_C` optional; `folder` matches that outliner folder and every folder under it, `"(none)"` the root. With `summary`, it shows what a `delete_by_tag` would remove before the delete runs.
- **`componentNames` on `get_components`** (`control_actor`, `inspect`) and `inspect_cdo` returns only the named components and lists a name that matches none under `missingComponents`; checking one component of a 27-component Blueprint used to return all 27.
- **`spawn_batch` names the actors it could not label.** An item without `actorName` is labelled after its mesh ("Cube" for every cube), so nothing in the reply could address it later. `unnamedActors` lists each one's unique actor name in batch order under either `report` mode, and a full report names it the same way.
- **`get_foliage_instances` counts by type.** Every reply carries `byType` counts; `summary` returns only the counts and `limit` caps the instance list, with `truncated` saying when it did. A level-wide call listed every instance: about 70 KB for a 573-instance meadow.
- **`control_actor.sample_motion` watches an actor over game time.** In Play-In-Editor, one call returns the actor's location, velocity and any `propertyNames` every `intervalSeconds` of game time for `durationSeconds`, with `start`/`end` and `min`/`max` extents and why it stopped (`duration`, `realTimeCap`, `sampleCap`, `actorDestroyed` when a death reloads the level, `worldEnded`). Proving a jump, a spring launch or a patrol used to take a sleep-and-poll loop whose samples fell wherever the editor's frame rate put them, so a death between two polls read as a teleport back to the start. It is the `motion` member of the `get_transform` family (`readMode: "motion"`), and it refuses the editor world, which never ticks, with `NOT_SIMULATING`.
- **`sample_motion` can drive the test it watches.** `inputs` presses keys at exact game-time offsets from the start of the run ({key, atSeconds, holdSeconds}), and `startWhen` holds the run until another actor's property takes a value ({actorName, propertyName, equals}; by default it has to change into it, so a platform already solid is waited out until it next appears). A jump timed over two calls landed 1-2 game seconds late, whatever the model, because the time the caller spent between the calls ran on the game clock; in one call it lands where it was aimed. The reply lists when each key went down and up (`inputsApplied`) and how long the start waited (`waitedSeconds`), and `endedBecause` is `startWhenTimeout` when the start never came.
- **`inspect_asset` lists what uses an asset.** `lookup: "dependencies"` with `referencers: true` returns the packages that use the asset (the Blueprints that spawn it, the levels that place it), the question to answer before deleting or replacing it; nothing answered it before. The list is sorted, and an object path (`/Game/FX/NS_Puff.NS_Puff`) now works as well as a package path. Searching "asset referencers" or "what uses this asset" finds it.
- **The running game's objects have names.** While PIE runs, `inspect` `get_property` / `set_property` take `GameInstance`, `GameMode`, `GameState`, `PlayerController`, `PlayerPawn`, `PlayerState` or `HUD` as `objectPath`. Their real paths are transient (`/Engine/Transient.UnrealEdEngine_0:BP_MyGI_C_3`), so a game-instance variable such as a best score could not be read or set at all.
- **Play tests reach the player by role.** `control_actor`'s transform actions and `sample_motion` take `PlayerPawn` (or PlayerController, GameMode, GameState, PlayerState, HUD) as `actorName` while PIE runs, so a test no longer has to find out that the pawn spawned as `BP_Hero_C_0`.
- **`sample_motion` watches components too.** `propertyNames` takes `"Component.Property"` (`"Visual.RelativeScale3D"`) beside actor properties, so a squash on landing, a spinning part or a light that flickers can be proven over game time in the same call; before, only the actor's own properties could be sampled.
- **A `startWhen` that never fires says why.** `endedBecause: startWhenTimeout` now comes with a warning naming the value last read. The common case (the value already held when the call arrived, so the default rising-edge wait never saw it change) is spelled out with `waitForChange: false` as the fix; the bare timeout had left the caller to guess.
- **Buttons get hover and click sounds.** `set_widget_layout` `style` takes `hoverSoundPath` and `pressSoundPath` (a SoundCue, SoundWave or MetaSound; an empty string clears it), written into the Button's style. A menu got UI sounds only by wiring every button's OnHovered/OnClicked to PlaySound2D; "button sound" and "click sound" find it in search.
- **`set_style` saves what it changes.** Its convenience fields (text, fontSize, colorAndOpacity, texturePath, cornerRadius, renderOpacity, the button sounds) marked the Widget Blueprint modified and stopped there, so the edit lived only in editor memory: a restart or a package build dropped it, while `set_clipping` and the `propertyName` path saved. They save now and report `saveSucceeded`.
- **`save_all` can save just what you changed.** `assetPaths` limits the save to the listed assets or levels and leaves every other dirty package as it is; the reply counts what it left (`leftDirtyCount`). Saving everything also wrote out unrelated work someone had open, and the contract told callers to "save individually" through a capability that did not exist. "save asset" finds it in search.
- **A box slot takes a numeric alignment.** `set_widget_layout` `alignment` `{x: 0.5, y: 0.5}` was refused for every VerticalBox, HorizontalBox, Overlay or Border child ("carried neither a usable x nor y"): a JSON number also reads as a string, so it was checked against the word list and matched nothing. Only canvas children could be centred by number; the documented example now works on every slot.
- **Error messages stop reading as redacted paths.** Messages that listed choices with slashes (`fill/left/center/right/top/bottom`, `castShadows/shadowBias/...`, `r/g/b/a`, `float/int/bool`, `cvar/key/command`, `target/property/value`) reached the caller as `fill[path redacted]`, because the response redactor rightly takes `a/b/c` for a path. They list the choices with commas now, so the hint that says what to send arrives whole.
- **Class lookups take a Blueprint's short name.** `find_by_class`, spawn and every other caller of the shared class resolver answered CLASS_NOT_FOUND for `BP_Door_C` or `BP_Door` and demanded the full `/Game/.../BP_Door.BP_Door_C` path. A short name now resolves to the loaded class, or through the asset registry to the Blueprint of that name when it is not loaded yet, during play as well.
- **`set_style` justifies text.** `justification` (`left`, `center`, `right`) aligns the text of a TextBlock or RichTextBlock; centring a title took a raw `propertyName: Justification` write that nothing pointed to. "center text" and "text alignment" find it in search.
- **A write to the running game says it lasts until PIE stops.** `set_property` on an actor of the Play-In-Editor world answered `saveSkippedReason: "level content is saved with its level"`, promising a save that stopping play throws away. It now says the change lasts until PIE stops, as a GameInstance write already did.
- **Every receipt digest covers its whole payload.** The native door rendered `data` for `receipt.dataDigest` without fractions, so the render stopped at the first non-integer and every reply carrying one (motion samples, transforms) hashed the same prefix: different payloads, identical digests.
- **`create_level` honours `savePath`.** It declared `savePath` but read only `levelPath`, so a level created with `savePath: /Game/Levels` quietly landed in `/Game/Maps`. `savePath` is an alias of `levelPath` now, a folder that already ends in the level name is taken as the full path, and the `template` it never applied is no longer declared.
- **Screenshots name their modes and can leave no file.** `mode` is an enum now: `editor_viewport`, `game_viewport` (which never contains UMG) and `full_editor_window` (the one that shows game UI); it used to be a shared string described as an editor mode. `keepFile: false` returns the image (with `returnBase64: true`) without leaving a file in `Saved/Screenshots`.
- **Files the MCP writes can be listed and removed through it.** `system_control` `list_output_files` shows the screenshots and environment snapshots the tool has written, and `delete_output_file` removes one; nothing outside `Saved/Screenshots`, `Saved/unreal-mcp`, `tmp/unreal-mcp` and `temp/unreal-mcp` can be reached. Cleaning up after a check used to need a shell reaching into the project.
- **`generate_lods` saves its bake where you say.** LODs on a dynamic-mesh actor need a static mesh, which was always written to a hard-coded `/Game/MCPTest` folder. It takes `outputPath` now and otherwise follows `convert_to_static_mesh` into `/Game/GeneratedMeshes`.
- **`set_transform` moves by an `offset`.** `offset: [dx, dy, dz]` moves an actor from where it stands instead of to an absolute `location` (giving both is refused), and each `actors` item takes its own, so a group shifts by one amount without reading every position first: raising a 16-part castle onto a plaza took 16 absolute locations.
- **An unknown action points at the tool that has it.** When the action exists under exactly one other tool, `UNKNOWN_ACTION` (execute and describe, both doors) says so in its message and its `nextCall` describes that tool's action: `manage_blueprint.set_blueprint_variables` used to suggest `edit_variable`, while the action is `control_actor`'s. When none of the suggestions shares the action's verb (a one-letter typo still counts as shared), `nextCall` searches the action's words instead, so `manage_asset.save_asset` leads to `control_editor.save_all` rather than to `move_asset`.
- **Ready-made HUD pieces are back, and they are built for real.** `add_game_widget` (`widgetKind`: `health_bar`, `ammo_counter`, `crosshair`, `minimap`, `compass`, `damage_indicator`, `interaction_prompt`, `objective_tracker`, `quest_tracker`) adds a styled piece to an existing Widget Blueprint. A `slotName` already in the tree is refused (`SLOT_EXISTS`) instead of duplicated, a missing parent is refused, an empty tree gets a canvas root, and `positionX`, `positionY`, `sizeX` and `sizeY` override the default placement. The settings the old actions declared and never read now take effect: the health bar's `percent`, fill colour and label, the text, size and colour of the counter and crosshair, a minimap or compass texture (refused when it does not load), the damage vignette colour with a `<slot>_Flash` animation lasting `fadeTime`, the prompt's key and text, and the trackers' title and rows (`items`, `maxVisibleObjectives`). The reply lists every widget created, so each part can be bound or restyled by name. "health bar", "crosshair" and "minimap" find it in search. These pieces and the screens below were in the Removed list and come back rebuilt, not restored: `add_game_widget`, `create_widget_template` and their 19 member names (`add_health_bar`, `create_main_menu`, …) are callable again, which brings the catalog to 382 records and 1,453 `{tool, action}` pairs. `create_game_screen`, `create_property_binding`, `set_animation_loop` and `set_widget_binding` stay removed; `bind_widget` makes property bindings.
- **Ready-made screens are back as new Widget Blueprints.** `create_widget_template` (`screen`: `main_menu`, `pause_menu`, `settings_menu`, `loading_screen`, `hud`, `dialog`, `inventory`, `radial_menu`, `credits`, `shop`) creates a new Widget Blueprint holding that screen. An existing asset is refused instead of having its tree wiped, and a bad setting is refused before the asset is created. The screens read at a glance: near-black backdrops, slate-blue rounded buttons with padded labels, a credits list that is actually visible (the old one was zero pixels wide, with white text on white), shop cards wide enough for their labels in a scroll area that stops at its content, and a Quality combo box whose text is dark on its light face. The settings menu holds a Quality combo box, Fullscreen and VSync check boxes and volume and sensitivity sliders, chosen by `settingsType`; `hud` starts with the pieces named in `elements`; the loading screen can fade in (`fadeTime`); `buttons`, `responseCount`, `columns`, `rows`, `segmentCount`, `entries` and `itemCount` set how many of each part it gets. The reply lists every Button created, ready to wire with `bind_widget` `on_clicked`.
- **Blueprints get component events.** `add_event` (and `add_function` with `kind: "event"`) takes `componentName` with `eventName` and builds the event the editor's component "+" button makes: `OnComponentBeginOverlap` on a BoxComponent, or `OnComponentHit` on a Character's inherited CapsuleComponent. The code path existed, but `componentName` was not declared, so the gateway refused the only way in. When a call did get through, it found only components the Blueprint added itself, and a component added since the last compile produced a node with no pins and no function name that was reported as success. A delegate that Blueprints cannot bind is refused with `DELEGATE_NOT_ASSIGNABLE`, a missing component lists the Blueprint's components, and the reply's `nodeGuid` finds the new node.
- **`create_light` spawns any light class.** `lightClass` (a class name or path, including a Blueprint light such as `/Game/Lights/BP_Lamp.BP_Lamp_C`) can replace `lightType`, and `properties` sets intensity, color, shadows, attenuation radius, cone angles and source size on the light component. The handler already read both, but the contract did not declare them, so the gateway refused them. `rename_level` also takes `destinationPath`, which moves the level instead of renaming it in place.
- **Navigation modifiers without a Blueprint.** Without `blueprintPath`, `create_nav_modifier` answered "Missing blueprintPath". It now places a NavModifierVolume in the level, using `location`, `extent`, `actorName` and `areaClass`. `create_nav_modifier_component` takes `actorName` and adds the component to that placed actor instead of to a Blueprint.
- **`list_output_files` pages and filters, and `delete_output_file` takes several paths.** In a live project, one call returned all 416 files (about 50k characters). The list now comes back newest first, 100 files by default (`limit` goes up to 500, and `offset` fetches the next page). It can be narrowed to one `root` folder or one `extension`. The reply carries `total`, `returned`, `hasMore` and `nextOffset` instead of `count` and `truncated`. `delete_output_file` now takes `paths` as well as `path`. Each path gets its own entry in `results`, and the call fails with `PARTIAL_DELETE` when any of them was not deleted.
- **Volumes take their own class's settings.** `create_volume` declares `priority` (physics and post-process volumes), `killZHeight` (sets a KillZVolume's height at the kill plane) and `postProcessSettings` (bloom, exposure bias, vignette, saturation, contrast, gamma), and an attached post-process volume now applies those settings too. `set_volume_properties` sets `priority`, `blendWeight` and `bEnabled` on a PostProcessVolume, where it used to answer `NO_PROPERTIES_APPLIED`, and declares the pain and reverb fields it already applied.
- **Blueprint actions take the fields their handlers read.** `create_node` takes `pure` (a Cast with no exec pins), `add_node` takes `targetClass` (Cast and CreateWidget), and `get_pin_details` takes `pinName` to report one pin. `add_component` and `add_scs_component` run the same handler, so both now take `location`, `rotation`, `scale`, `properties`, `meshPath` and `materialPath`. `create` and `create_blueprint` both take `blueprintType` and `properties`, and `ensure_exists` takes `createIfMissing` (with `false`, it only reports whether the Blueprint exists).
- **Level-structure edits take the targets their handlers read.** `assign_actor_to_data_layer` accepts `actorPath` in place of `actorName`. `create_data_layer` takes `dataLayerAssetPath` and `bIsPrivate` (UE 5.3 or later; refused on older engines). The level-blueprint node edits take `levelPath` to edit a loaded sub-level's blueprint instead of the persistent level's. `create_level` declares `loadAfterCreate`.
- **Skeleton reads take a skeletal mesh.** `get_skeleton_info`, `list_bones` and `list_sockets` take `skeletalMeshPath` in place of `skeletonPath`, and `list_physics_bodies` takes it in place of `physicsAssetPath` (the mesh's assigned physics asset). `add_physics_body` accepts `length`, `width`, `depth`, `height` and `rotation`, and `create_morph_target` and `set_morph_target_deltas` accept `lodIndex`. The handlers read all of these already; the gateway refused them as undeclared.
- **`edit_metasound` removes and disconnects.** `remove_node` (`nodeId` or `nodeIds`) takes nodes out with every link on them and refuses the graph's own inputs and outputs; `disconnect` removes the link into an input, every link out of an output, or one exact link. Both run as `build_metasound` steps too, where `$id` also works inside `nodeIds`. Re-voicing a sound used to leave the old oscillators behind as dead nodes. UE 5.5 or later.
- **`get_metasound_graph` reads a MetaSound as data.** Every node with its class and input literals, the links by pin name, and the graph inputs and outputs. Finding a node id for `set_default` meant reading a 15-40 KB document export through `inspect`. UE 5.5 or later.
- **`delete_node` deletes several Blueprint nodes at once.** `nodeIds` removes them under one consent with one transaction, one compile and one save, all or nothing (`NODE_NOT_FOUND` lists ids that matched nothing, `PROTECTED_NODE` the function entry and result nodes). Removing a 71-node dead chain took 71 consented calls.
- **`query_asset` finds text inside assets.** `lookup: "text"` (`find_text`) searches Blueprint graph literals (with their node ids) and comments, variable and component defaults, widget texts, DataTable rows, String Table entries and the actors of the open level. `search_assets` only matches asset names.
- **`reparent_widget` reorders.** `index` places the widget among the new parent's children (0 is first), so the same parent reorders its children, and the reply reads the slot layout back.
- **`restart_editor` can close the editor.** `relaunch: false` closes it under the same unsaved-package gate (a running PIE session ends first); `validateOnly` lists the unsaved packages without doing either.
- **`move` and `rename` take a folder.** A folder as `sourcePath` moves everything under it into `destinationPath`, keeping sub-folders; redirectors are fixed up, the emptied folder is removed, and a level open in the editor is reopened at its new path. Relocating a game folder of 207 assets took 207 calls.
- **`maintain_content` refreshes Blueprints.** `refresh_blueprints` refreshes every node of the Blueprints under `folderPath` (or `assetPaths`), then compiles and saves each and lists the ones that do not compile. After a class rename, casts kept their old output pin ("AsBP Mario Save"), which compiled into the bytecode under that name.
- **`rename_variable` renames components.** A component name renames the Simple Construction Script node and every graph getter, and a name the Blueprint already uses is refused (`NAME_CONFLICT`); it answered `NOT_FOUND` for components although `newName` promised them.
- **`exists` checks several paths.** `assetPaths` answers `existsByPath` in one call.
- **`audit_placement` finds z-fighting.** Two mesh faces in one plane that point the same way flicker between the two surfaces; the sweep reports them under `coplanarFaces` (kind `coplanar`), reading each mesh's own triangles so only faces a mesh really fills count. `kinds` narrows the report, e.g. `["coplanar"]` in a platformer full of platforms that float on purpose.
- **`fix_coplanar` fixes z-fighting in one call.** Every actor with a coplanar face moves a unit: a piece lying inside the other face (a door on a wall) comes forward, a piece sunk into it (a ramp in the floor) goes back, and a mesh whose two faces on one axis both flicker is resized. It repeats until nothing is left to move (at most four passes), previews with `dryRun`, undoes in one step, and lists pairs inside one Blueprint under `skipped`.
- **`control_actor.rename` renames a placed actor.** Searching "rename actor" found nothing: a label could only be given at spawn. `rename` sets the label, and `renameObject` also renames the actor object, which is the name a cooked level ships (the label is editor-only). Renaming a Blueprint class leaves its placed actors named after the old class (`BP_OldEnemy_C_6`), and the engine's own label edit no longer renames the object.
- **`stop_sound` stops what `play_sound` started.** A 2D sound played through `play_sound` (looping stage music) could not be stopped at all: it was fire-and-forget. `manage_audio` `stop_sound` stops the ones started from `soundPath`, or every one of them, and `all: true` silences every audio device of the editor and the running game.
- **`preview_widget` returns the widget as an image.** It opened and focused the Widget Blueprint editor and returned no picture. It now draws the widget offscreen for `resolution` (default 1280x720, with the project's DPI scaling) in design mode and returns a PNG the client shows; `openEditor: true` still opens the editor.
- **`duplicate_widget` copies a widget and everything under it.** A list row with its label, icon and button is copied beside the original, or under `newParent` at `index`, the copy named `newName`; building ten rows took ten full re-authorings.
- **`set_style` picks a font face and copies one label's look to another.** `fontFamily` (a Font asset), `typeface` (one of its faces; a face the font lacks is refused with the faces it has), `letterSpacing`, and `copyStyleFrom` (another text block's font, colour and shadow). There was no way to make text bold.
- **Screenshots from a given camera.** `screenshot` takes `location` and `rotation` and moves the editor viewport there before it captures, answering `cameraLocation` and `cameraRotation`; a view took a separate camera call first.
- **`bulk_rename` takes explicit renames.** `renames: [{sourcePath, newName}]` gives each asset its own new name in one call; a source that is missing or a name that is taken is listed under `skipped`. Only a prefix, suffix or replace pattern over a folder could be applied before.
- **`fix_coplanar` fixes pairs inside one Blueprint.** Two parts of one Blueprint actor in one plane (a brow on a face, a strut in its frame) were only listed for a hand `edit_scs` per Blueprint. The smaller part now moves a unit in the Blueprint itself (the other part when the smaller one is the root), so every placed copy is fixed at once, compiled and saved, and listed under `blueprintsFixed`.

</details>

<details>
<summary><b>🔄 Changed</b></summary>

- **A folded capability says which variant each guidance line is about.** Merged `whenToUse`/`whenNotToUse` lines now name their variant ("control=play: PIE is already running."); unlabelled, `play` read as if no control worked while PIE runs. Lines every variant shares stay bare, and search ignores the labels.
- **Motion sampling answers within 25 seconds by default.** `get_transform` motion stops at `maxRealSeconds`, which defaulted to 40, past the 30-second timeout some clients use, so a long run was cut off client-side while the editor kept sampling.
- **Read capabilities say what they return.** 41 readers described themselves as "metadata" or "information" (`get_audio_info`, `get_texture_info`, `get_character_info`, `get_ai_info`, `get_level_structure_info`, `list_plugins`, `read_log`, ...). Each summary now names the fields its handler answers with, and its topics add the words a caller types ("sound length", "pixel format", "walk speed", "list blackboard keys"). Seven summaries promised data that never comes back and now say what does: `get_mesh_info` material info, `get_enum` metadata, `analyze_trace` timing counters, `get_performance_stats` draw calls, `list_debug_shapes` "active shapes", `get_sessions_info` online sessions. Over 133 verb-led requests for these readers ("get sound length", "list plugins"), the reader ranks first 113 times on the stdio door (was 51) and 112 on the native door (was 74), together with the ranking changes below.
- **Search prefers the capability that names every word of the request.** A record whose names, family, domain or topics cover all the words of a request (summary prose does not count, since it also holds words like "one" and "another") ranks ahead of one that matches fewer words through its member names. "change button text" used to find `add_content_widget` (through `add_button` and `add_text_block`) and "how many actors in the level" found `inspect_object`. Both doors carry the rule. Button edits were the worst case and have their own words now: "copy button" finds `duplicate_widget`, "delete the quit button" `remove_widget`, "move the button" and "make the button bigger" `set_widget_layout` (which also answers to `hide_widget`), and the corpus pins them. On the stdio door a request that opens with a read word (get, list, show, which, where) prefers read capabilities by 80 points instead of 20, so "list blackboard keys" finds `get_ai_info` rather than `edit_blackboard`, whose member names carry both nouns. Over the 58 plain requests, first-place answers rose from 42 to 53 on the stdio door and from 47 to 54 on the native door; on the retrieval corpus, top-1 went from 0.967 to 0.984 and from 0.918 to 0.967.
- **A screenshot shows the picture by default.** `control_editor` and `system_control` screenshots answered with only a file path unless `returnBase64` was set, and a model client cannot open that file. The image now comes back inline, fitted to 1600x900 when no `resolution` is given; `returnBase64: false` keeps the old file-only, full-size capture.
- **Setting a game mode class no longer changes the project's game mode.** `set_default_pawn_class`, `set_player_controller_class`, `set_game_state_class`, `set_player_state_class` and `set_hud_class` used to make that game mode the project default on every call (written to the per-user Saved config, not DefaultEngine.ini), set it as the open level's GameMode Override and mark the level dirty. They now change only the class on the game mode you name. `makeDefault: true` makes it the project default in DefaultEngine.ini, as Project Settings does. Every reply reports `madeDefault`, `openLevelGameMode` (what the open level runs in play) and `effectiveInOpenLevel`, and the message says when the open level runs a different game mode.
- **Level edits say when they are not saved.** Level-structure and volume edits (streaming, World Partition, data layers, level blueprint nodes, volumes) still leave the level unsaved unless `save: true` is passed, because saving a level also writes every other unsaved change in it. The reply now carries `saved: false` and says the level is not saved yet, where it used to say nothing.
- **The stdio server runs every capability the way the native door does.** `execute` forwards `{action, ...params}` to the record's parent tool and the plugin's parent routing picks the handler; the TypeScript per-domain action layer is gone. Both doors now accept exactly the parameters a record declares: a spelling only that layer converted (`path` for `blueprint.create`'s `savePath`, a `Game/…` path without its leading slash) is refused with the declared name, while the aliases a record declares (`type` on `add_material_node`, `targetPath` on `import_level`/`duplicate_level`, `sourceNode`/`sourcePin`/`targetNode`/`targetPin` on `connect_metasound_nodes`, `emitter` on Niagara module actions, `actorName` on `set_niagara_parameter`) are read by the plugin.
- **One dispatch table in the plugin.** The fallback chain that retried every handler for an unmatched action is gone; each parent tool routes its sub-actions explicitly, `system_control`'s widget, screenshot, project-settings, sound and display actions included.
- **Scalar property writes are strict.** Integer properties refuse non-integral and out-of-range values instead of truncating them, and enums refuse hidden and `_MAX` entries (display names are accepted).
- **`create_blend_space` creates a 2D blend space** (Direction −180..180 × Speed 0..600), as its record describes; `create_blend_space_1d` makes one axis.
- **`apply_baseline_settings` moves every scalability group** through `Scalability::SetQualityLevels` (performance → Low, balanced → High, quality → Epic) plus `r.VSync`, instead of seven hand-picked console variables; it no longer touches `r.AllowHDR`.
- **Enhanced Input triggers and modifiers resolve by class**, so every trigger and modifier class works. The spellings only the old ladder knew are refused instead of silently becoming Tap, Smooth or Scalar: `DoubleTap` (use `RepeatedTap`, 5.6+), `SwizzleInputAxis` (use `SwizzleAxis`), `SmoothDelta` before 5.4 and `ScaleByDeltaTime` before 5.1.
- **Paths.** An invalid asset path reports itself instead of resolving a same-named asset under another root; landscape grass types land in their `path`; MCP-created landscapes no longer carry `MCP_Landscape*` tags; `stream_level`/`unload` refuse a level that is not a sublevel of the editor world instead of reporting a console command's success.
- **Refusal codes and reply fields.** Several refusals carry precise codes (`SKELETON_NOT_FOUND`, `ANIMATION_NOT_FOUND`, `ASSET_CREATION_FAILED`, `BLUEPRINT_NOT_FOUND`, `SCS_NOT_FOUND`, `PARAM_TYPE_MISMATCH`, `ASSET_ALREADY_EXISTS` for an existing interactable), and constant or duplicated reply fields are gone: `validationResult` on `validate_niagara_system`, the header dump on `analyze_trace`, `maxTailSize` on `write_snapshot`/`send_snapshot`, `navMeshPresent`/`bHasNavMesh` on navigation replies and three always-equal `delete_level` flags.
- **`add_section` takes display-rate frames.** `start` and `end` were used as tick-resolution values, so `end: 60` on a 24 fps sequence made a section a small fraction of one frame long. They are display-rate frames now, like every other frame `manage_sequence` takes; an empty or reversed range is refused, and the `actorName` track filter and the `bindingId` a camera cut section needs are declared.
- **`set_playback_speed` works without Sequencer open.** It changed only the open Sequencer editor and failed with `EDITOR_NOT_OPEN` whenever the sequence was not open, so the speed a game or PIE plays at could not be set. It now writes the play rate of every level sequence actor in the editor level that plays the sequence, and the open Sequencer's preview speed when there is one. The reply lists those actors (`levelSequenceActors`) and says whether Sequencer was updated (`sequencerUpdated`); `PLAYBACK_TARGET_NOT_FOUND` means neither exists.
- **Game Framework edits save by default.** Every `manage_game_framework` action read `save` but never declared it, so the gateway refused it, the handler's default of false always applied, and nothing was ever saved: a restart dropped the edit. Each action now saves its Blueprint unless you pass `save: false`. `set_default_pawn_class` and the other class setters now say in their reply that the game mode also becomes the project default and the open level's game mode override. They already did this, but silently.
- **`modify_scs` saves by default.** An SCS edit stayed in editor memory unless the caller asked for a save, so an editor restart dropped it. `save` now defaults to true (`save: false` opts out), and `compile` is declared. `modify_scs` also accepts a single component described at the top level (`componentName` with `location`, `rotation`, `scale`, `meshPath`, `materialPath` or `properties`), and a top-level location, rotation or scale now reaches the edit instead of being dropped.
- **`create_volume` refuses a parameter its class would ignore.** A `sphereRadius` on a BlockingVolume or a `damagePerSec` on a TriggerBox used to be dropped with a success reply. The call now fails with `INVALID_ARGUMENT`, names the parameter, lists what that volume class takes, and creates nothing. A `create_*` volume given `actorPath` now attaches to that actor, as the `add_*` spelling already did.
- **`start_unreal_insights` opens Unreal Insights by default.** It opened the viewer only with `launchViewer: true`, and never while a trace was already running. It now opens the viewer unless you pass `launchViewer: false`, including when a trace is already running, and it takes the same trace inputs as `start_session`. If the viewer fails to start, the call answers `VIEWER_LAUNCH_FAILED` instead of success.
- **Declared names win over legacy spellings in Blueprint edits.** In graph edits, `posX` and `posY` take precedence over `x` and `y`, and `blueprintPath` over `assetPath`. In SCS edits, `blueprintPath` takes precedence over `name`, and an empty `name` no longer hides `blueprintPath`.
- **Widget layout and binding replies put their readback at the top.** The layout setters declare `applied` and `saved`, and the bind actions declare `property`, `functionName`, `generatedGetter`, `bindings` and `saved`, so these fields arrive at the top of the result instead of inside `details`.
- **`setup_ik` describes what it does.** The `setup` kind creates a Control Rig Blueprint bound to a skeleton to host IK solvers; it was described as configuring IK on an Animation Blueprint. It requires `name` and `skeletonPath` and takes the folder as `path`. `set_section_timing` is described as moving a section's start (a section runs until the next one starts).

</details>

<details>
<summary><b>🗑️ Removed</b></summary>

- **117 actions that only echoed their input, faked success or answered `NOT_SUPPORTED`** (the catalog goes from 389 to 382 records; 1,453 `{tool, action}` pairs stay callable):
  - `animation_physics` (7): `copy_weights`, `create_pose_library`, `import_morph_targets`, `mirror_weights`, `normalize_weights`, `prune_weights`, `set_retarget_chain_mapping`
  - `manage_asset` (1): `create_ao_from_mesh`
  - `manage_audio` (2): `enable_audio_analysis`, `set_doppler_effect`
  - `manage_blueprint` (4): `create_game_screen`, `create_property_binding`, `set_animation_loop`, `set_widget_binding`
  - `manage_character` (12): `add_custom_movement_mode`, `configure_footstep_fx`, `configure_sprint`, `map_surface_to_sound`, `setup_character_ability`, `setup_climbing`, `setup_footstep_system`, `setup_grappling`, `setup_mantling`, `setup_sliding`, `setup_vaulting`, `setup_wall_running`
  - `manage_combat` (25): `apply_damage`, `configure_aim_down_sights`, `configure_combo_system`, `configure_damage_execution`, `configure_hit_reaction`, `configure_hitscan`, `configure_impact_effects`, `configure_muzzle_flash`, `configure_recoil_pattern`, `configure_shell_ejection`, `configure_spread_pattern`, `configure_tracer`, `configure_weapon_sockets`, `configure_weapon_trails`, `create_damage_effect`, `create_hit_pause`, `create_melee_trace`, `create_shield`, `heal`, `modify_armor`, `set_weapon_stats`, `setup_ammo_system`, `setup_parry_block_system`, `setup_reload_system`, `setup_weapon_switching`
  - `manage_effect` (1): `configure_event_payload`
  - `manage_gas` (9): `add_ability_task`, `add_tag_to_asset`, `configure_cue_trigger`, `configure_gameplay_cue`, `create_ability_set`, `grant_ability`, `set_ability_targeting`, `set_attribute_clamping`, `set_cue_effects`
  - `manage_geometry` (3): `poke`, `quadrangulate`, `triangulate`
  - `manage_interaction` (11): `add_destruction_component`, `add_interaction_events`, `configure_destruction`, `configure_destruction_damage`, `configure_destruction_effects`, `configure_destruction_levels`, `configure_interaction_widget`, `configure_trigger_events`, `configure_trigger_filter`, `configure_trigger_response`, `setup_destructible_mesh`
  - `manage_inventory` (20): `add_crafting_component`, `add_equipment_functions`, `add_inventory_functions`, `configure_equipment`, `configure_equipment_effects`, `configure_equipment_visuals`, `configure_inventory`, `configure_inventory_events`, `configure_inventory_slots`, `configure_inventory_weight`, `configure_loot_drop`, `configure_pickup`, `configure_pickup_effects`, `configure_pickup_interaction`, `configure_pickup_respawn`, `configure_station_recipes`, `create_equipment_component`, `create_inventory_component`, `create_pickup_actor`, `define_equipment_slots`
  - `manage_level_structure` (3): `configure_level_bounds`, `create_level_instance`, `create_packed_level_actor`
  - `manage_networking` (19): `add_network_prediction_data`, `configure_lan_play`, `configure_local_session_settings`, `configure_net_serialization`, `configure_player_start`, `configure_push_to_talk`, `configure_round_system`, `configure_scoring_system`, `configure_session`, `configure_session_interface`, `configure_spawn_system`, `configure_team_system`, `configure_voice_settings`, `disable_input_action`, `join_lan_server`, `set_split_screen_type`, `set_voice_attenuation`, `set_voice_channel`, `setup_match_states`
- **Settings and endpoints nothing read:** the environment variables `MCP_AUTOMATION_WS_PORTS`, `MCP_AUTOMATION_SERVER_LEGACY`, `MCP_AUTOMATION_CLIENT_MODE`, `MCP_AUTOMATION_MAX_AUTOMATION_REQUESTS_PER_MINUTE`, `MCP_ROUTE_STDOUT_LOGS` and `MCP_DEFAULT_CATEGORIES`; the `MCP_METRICS_PORT` Prometheus endpoint; the Project Settings `LogVerbosity`, `bApplyLogVerbosityToAll`, `bEnableSocketTelemetry` and `HeartbeatIntervalMs`; the plugin's WebSocket client mode; and the raw-socket bare action names, which neither MCP door used.

</details>

<details>
<summary><b>🔧 Fixed</b></summary>

- **`startWhen` waits for the value it was given.** `get_transform` motion with `startWhen.equals` 2 waited for "True" instead: every value was read as a bool first, so any number or string became "True" or "False" ("Walking" became "False"), and a run gated on `Phase` 2 never started. Each value is now read as its own JSON type.
- **A downscaled window capture says what size it was taken at.** `full_editor_window` screenshots now carry `viewportWidth`/`viewportHeight` when the image was downscaled, as editor-viewport captures already did; a 2560x1440 window returned at 1600x900 gave no hint of its real size.
- **`get_summary` names the map asset.** For a loaded level it reported `assetClass` Package with no tags: the bare package path found the in-memory package before the map inside it. The map asset is tried first now.
- **Three descriptions say what the handler does.** `get_volumes_info` `filter` matches the actor label (it claimed to filter by type; `volumeType` does that), `asset.list` `recursive` defaults to true, and `asset.list` gained guidance for each of its three listings.
- **Deleting an output file shows up in the receipt.** `delete_output_file` answered only `path`, so removing a screenshot returned `changes: []`. It now lists every file it deleted.
- **A preview or a played sound no longer claims to have changed an asset.** After preview_widget the receipt listed the Widget Blueprint under changes, and after play_sound the sound. A reply that states `changedAssets`, an empty one included, is now taken at its word; the actors it names still count.
- **An image gets its own size budget.** Only the two screenshot capabilities were exempt from the 100k reply budget, so a busy widget preview at 1280x720 came back RESULT_TOO_LARGE with advice to paginate an image. Any reply's `imageBase64` (up to 6 million characters) now counts on top of the 100k the rest of the reply is held to.
- **reparent_widget stops warning about a duplicate it never made.** Every move said the widget "was already in the tree" and to use a fresh slotName; the warning is kept for an add that re-uses a slotName.
- **Widget receipts list the Widget Blueprint once**, by its package path (/Game/UI/WBP_Menu) instead of the package path and the object path side by side.
- **inspect_graph's filter looks at pin values.** "Menu" found nothing in a graph whose Create Widget node builds WBP_MainMenu, because the class sits on a pin; a node now also matches on a pin's default value, text or object path.
- **Style, clipping and edit_graph batch receipts name the asset they saved**; they had no handle and no change.
- **A level save names the level it saved.** `manage_level` save and save-as answered only with fields the receipt does not read, so saving a level returned no handle and no change.
- **An edit_graph batch no longer stops on a crowded position.** A step whose position overlapped a node ran out of retries and stopped the batch; the node now moves to free space and the step says where in placementWarning.
- **A widget that was just added, copied or renamed can be used in a graph at once.** The Widget Blueprint is compiled after the edit (never during play), so edit_graph no longer says a flagged widget "is not marked as a variable"; when one really is missing, the error says whether the flag is off or the class is stale.
- **compile's receipt lists the Blueprint it compiled**, not the words "compiled" and "saved" beside it.
- **add_event binds a widget's event in a Widget Blueprint** ({componentName: "CreditsButton", eventName: "OnClicked"}), which failed with "Component not found".
- **`control_actor.list` propertyNames reads a component's property** as "Component.Property" (StaticMeshComponent.LDMaxDrawDistance), as sample_motion already did.
- **Record text that sent callers wrong:** duplicate_widget says copied children are named <name>_Copy; variableType lists every type add_variable accepts (class paths such as /Script/UMG.Widget, structs, enums, soft references, containers); the widget use-guidance reads as English ("A new text block widget must be added"); close_asset tells a caller who meant the editor to use restart_editor; the audio and input use-guidance reads "Use it to configure distance attenuation." instead of "Use when configure distance attenuation." (102 lines), and 34 records lose a line that said nothing; get_input_info says it lists every key mapping of an Input Mapping Context, so "list input mappings" finds it. set_widget_layout's alignment now says how a box, overlay or scroll box slot takes it (a number, or fill, left, center, right, top, bottom per axis; it read only "Widget alignment (0-1)"), and the size rule names ScrollBox children, which it also sizes.
- **WebSocket clients see why they were closed.** The bridge closes with 4004 (handshake required), 4005 (bad token), 4008 (rate limit), 1002/4001-4003 (protocol) and 1009 (too large), but never sent the close frame, so every client saw an abnormal 1006. The close frame now goes out first, code and reason included.
- **Native `/mcp` requests no longer leak a telemetry entry each.** The native reply path closed the metrics interval directly and never dropped the action recorded at dispatch; a long-running editor grew that map by one entry per request.
- **Automation can no longer read or rewrite the bridge's own settings.** `set_preferences category: "McpAutomationBridge"` could turn `bRequireCapabilityToken` off and `bAllowNonLoopback` on; `get_project_settings` could read the capability tokens back; `set_project_setting` refused the section only by its exact spelling. All three now check the resolved settings class and answer `SETTING_NOT_PERMITTED`.
- **`remove_foliage` with a malformed `area` removes nothing.** An area missing `min` or `max` fell through to removing every instance of the type (or, with `removeAll`, every type); it is now `INVALID_ARGUMENT`.
- **Character configure actions save.** configure_capsule_component, mesh, camera, movement, jump, rotation, nav movement, crouch and the single movement setters compiled the Blueprint and answered "configured", and the change was gone when the editor closed. They save now and answer `SAVE_FAILED` when they cannot.
- **Failures that reported success, or left a half-change behind:** `get_foliage_instances` refuses an unknown type (`FOLIAGE_TYPE_NOT_FOUND`); `configure_grid_size` refuses a missing grid without `createIfMissing`; `setup_global_illumination` says when the GI method did not take (`GI_METHOD_NOT_APPLIED`); `configure_nav_area_cost` and `configure_net_driver` check everything before writing, and `configure_net_driver` reads the ini back (`PERSIST_FAILED`); `configure_weapon_mesh` refuses a mesh that does not load; `setup_hitbox_component` keeps the size it was not given; `convert_to_static_mesh` saves the collision body it adds; `set_doppler_effect` no longer leaves an unlinked node that a retry then "updated"; the networking CDO edits refuse an uncompiled Blueprint instead of skipping it.
- **Widget templates:** two spec widgets with one name ("New Game", "NewGame") are refused instead of one widget listed twice; a failed HUD piece takes its root canvas and animation back out; a damage indicator whose flash could not be authored is refused; `duplicate_widget` with a `newName` a child copy had taken no longer overwrites that child.
- **Behaviour recipes** refuse an existing event dispatcher whose parameters differ from the declared ones (`VARIABLE_TYPE_CONFLICT`).
- **`sample_motion`** reads its watched properties by name each tick, so a Blueprint recompiled mid-run no longer leaves it reading freed memory.
- **Idempotency fingerprints** render integers past int64 exactly (1e19 and 2e19 were one fingerprint) and fail closed on a payload they cannot render.
- **`prompts/get`** refuses arguments over 512 characters and values outside a declared `allowed` list. A cancelled execute answers `MCP_REQUEST_CANCELLED`, not `TOOL_EXECUTION_FAILED`.
- **`preview_widget` needs only the Read scope.** It draws a transient copy of the widget and changes nothing, but it was folded into `edit_widget_blueprint` with the writes, so a read-only principal was refused a picture. It is its own read capability now (389 capabilities).
- **Combat actions say when the save failed.** Every weapon, projectile, damage-type and damage-execution action saved the Blueprint and answered success whatever the save returned; a read-only or source-controlled file now answers `SAVE_FAILED`.
- **`configure_level_streaming` checks `streamingMethod` first.** An unknown method was refused only after the level had been added to the world as a streaming level, which stayed.
- **`restart_editor` with `relaunch: false` closes without a save prompt.** With `discardUnsaved`, the close could still ask to save the packages the caller had chosen to drop, a dialog no remote caller can answer.
- **A collision edit on a component template reaches placed actors whole.** `set_scs_property` and `edit_scs` pushed only the key they were given, while a collision profile also sets the object type and every channel response; placed actors now take all of it.
- **describe names the right way back.** Most `compensation.inverse` hints were keyed by names that had been folded into other capabilities and never attached, and two named the capability itself as its own inverse.
- **The session instructions stop quoting a stale count.** Every client read "380 capabilities covering 1,500+ editor actions" at connect; the catalog holds 388 capabilities and about 1,450 actions, and the number went stale with every new record. The first line now says what the one tool reaches instead (actors, levels, Blueprints and their graphs, widgets, materials, audio, animation, effects).
- **Search answers a question with a reader.** "get actor location" ranked set_transform first on both doors, "show blueprint graph" edit_graph, and "read datatable rows" a row delete; a small model that takes the first row then changed what it meant to read. A query that opens with a read word (get, read, list, inspect, query, describe, find, count, show, what, which, where, who, how, is, does) now ranks the matching capabilities that only read ahead. It only reorders matches, and requests to change something ("set actor location", "show hidden actor") rank as before.
- **Search answers a delete with a delete.** "delete the spawned actor from the level" ranked spawn first on the native door, because "spawned" matched spawn. A query that opens with delete, remove, destroy or erase now ranks the matching capabilities that delete ahead, on both doors; it only reorders matches. The eval gate now measures the native door as well: its corpus top-1 went from 88.5% to 90.2% (the TypeScript door from 95.1% to 96.7%), and both must stay at 90% or better.
- **Search finds a capability by the words its variants were written with.** Folding a family into one capability kept only the family's own short topic list and threw away 229 phrasings written for its variants, so "press play" and "start the game" never reached control_editor.play, "create actor" never reached spawn, and "actor position" never reached get_transform. A family now keeps its variants' phrasings alongside its own. On 58 plain-worded requests, the first result is right 42 times on the native door (was 40) and 38 on the TypeScript door (was 35).
- **Search knows the phrasings a name cannot carry.** "where is the player", "how many actors", "what is in this folder", "compile widget blueprint", "on clicked", "connect nodes", "print string" and "material parameters" are now declared topics of the capability that answers them (get_transform, list, asset list, compile, bind_widget, edit_graph, get_material_info and more). With the fix above, 45 of the 58 plain-worded requests rank the right capability first on the native door and 41 on the TypeScript door. Those 58 requests are now an eval (tests/eval/plain-probe), which fails CI if either door answers fewer of them first; with the twins that answer the same request accepted, the floors are 47 native and 42 TypeScript.
- **Receipts name the Blueprint or MetaSound a call changed.** A `modify_scs` edit, a reply naming its asset as `blueprintPath`, and a `build_metasound` batch left the receipt's `changedAssets` empty, so nothing said what to save or check again.
- **SCS edits say briefly how many placed copies took the change.** `instancesUpdated` is always there now, 0 included, and `updatedInstances` names at most three; an edit to a Blueprint placed a hundred times listed every one, and "none" read as a missing field.
- **A mesh or material set on a hidden component says so.** `modify_scs` reported success with nothing to see; the operation's result now carries a hint, repeated in warnings, when the component is invisible or hidden in game.
- **`remove_scs_component` with `componentNames` compiles and saves once.** It compiled and saved the Blueprint after every name, so removing a dozen parts could outlast the client's timeout; the reply still reports each one.
- **An unknown MetaSound node class goes straight to its candidates.** `add_metasound_node` handed a class the registry does not hold to the builder, which logged an engine error for it, and the receipt reported the call as failed on top of the candidates list.
- **A refused MetaSound connection says why.** `EDGE_FAILED` named nothing; it now says whether the source node has no such output, the target no such input, the types differ (put a conversion node between them) or the edge would close a loop, and lists the pins under `sourceOutputs` and `targetInputs`, in `connect_nodes` and in `build_metasound` batches.
- **`add_widget_component` takes `slotName` and `parentSlot`.** Every other widget kind takes those names; this one read only `componentName` and `parentName`, which still work.
- **Deleting a still-loaded asset says its file stayed.** When the asset left the Content Browser but its package stayed loaded, `delete` listed it as failed with no reason, and the file came back on the next editor start; the reply now says so and what to do. A level whose file fallback answered false after the level was already gone is counted as deleted.
- **Renaming a map or folder keeps packaging settings pointing at it.** `MapsToCook` and `DirectoriesToAlwaysCook` hold package paths as plain text, so a renamed map or moved folder silently dropped out of every packaged build; a rename now rewrites them (a folder only when the whole folder moved).
- **Engine warnings about an asset keep its name.** Redaction replaced the whole disk path of a package file, leaving "[path redacted], Default Material will be used"; the path becomes the asset's package path (`/Game/Maps/M_Wall`) instead.
- **`set_project_setting` saves per-user settings where the editor reads them.** A class saved per user (EditorPerProjectUserSettings) was written to the project's Default ini, where the user's own saved value overrode it; it is saved to the user's config now, as the settings editor does.
- **The `play` summary names the values `control` takes.** It said "start, pause, resume, stop", but `start` is not one of them (`play` starts Play In Editor), so a caller going by the summary was refused.
- **The plugin builds on UE 5.8.** UBT failed with `Could not find definition for module 'MegascansPlugin'` because the Fab adapter probed for Megascans by folder name, and 5.8 ships `Engine/Plugins/MegascansPlugin` as a content-only folder; `McpAutomationBridgeFab.Build.cs` now counts a module as present only when its `<Module>.Build.cs` exists (Fab is still detected, Megascans reports unavailable). The link failed with `LNK1194` because 5.8 PCG exports a data symbol (`PCG::Private::UserParameterTagData`) and MSVC cannot delay-load a DLL that data is imported from; PCG is linked normally on 5.8+, UE 5.2–5.7 keep the delay-load so prebuilt packages still load where PCG is off, and `PCG` stays `Optional` in the `.uplugin` for UE 5.0–5.1.
- **Reusing a widget's root name no longer crashes the editor.** Adding a canvas under a `slotName` the root already had made the root its own child, and UMG recursed until the stack overflowed. A widget can no longer be seated inside itself or its own subtree, `add_widget_component` seats through the same path as every other add, and a refused add never removes an existing widget.
- **Creating a game-framework class twice during Play no longer crashes the editor.** The duplicate check could not see the existing Blueprint while PIE ran, so the second `create_hud_class` (or any `create_*` class) asserted in the engine; it now reads the asset registry and answers "already exists".
- **`system_control.set_quality` works on the native door**, and `play_sound` without `soundPath` plays the editor's compile-success cue; both existed only in the TypeScript layer.
- **`set_transition_rules` applies its condition over stdio.** The TypeScript layer dropped `conditionVariable`, `conditionComparison` and `conditionValue`, so the rule reported success without the condition; a variable the Animation Blueprint lacks is now reported.
- **`add_widget_child` goes through `add_widget_component`**, so it registers the widget, gives a lone leaf a `RootCanvas` root and applies `name` and `text`.
- **Folded legacy names mean what they meant.** `stop_pie`, `single_frame_step`, `set_game_view_target` and `create_blackboard_asset` pin the selector value of the operation they name instead of standing in for the family default.
- **`add_mapping` checks its trigger and modifier classes before mapping the key**, so a bad class no longer leaves a trigger-less mapping behind; **`activate_ragdoll`** reaches its handler; **`create_render_target`** accepts its own example format and maps `RG8` to `PF_R8G8`; **`add_state_tree_state`** finds a parent at any depth; **`inspect_struct`** resolves a bare struct name; **`enable_gpu_simulation`** applies the flags it reports; **`create_animation_asset`** refuses an existing asset of another class (`ASSET_TYPE_MISMATCH`) instead of reusing it; **`set_modifier_magnitude`** writes SetByCaller magnitudes and refuses the types it cannot write; **`set_loot_quality_tiers`** stores the tiers it reports.
- **A `set_transform` batch checks placement against the finished layout.** Each item was checked right after its own move, while the items after it still stood at their old spots: a coin moved into a new arc reported overlapping a coin that was about to move away.
- **`get_component_property` reads the bare name the write path takes.** `CollisionProfileName` answered `PROPERTY_NOT_FOUND` although `set_properties` accepts it, so a caller could not confirm what it had just written. A name that lives one struct deep (`BodyInstance.CollisionProfileName`) now resolves there when exactly one struct member carries it, and the reply names the path it read.
- **A class lookup that works no longer reads as a failure.** A Blueprint function call given `memberClass: "BP_MarioGI_C"` or the asset path `/Game/Mario/Blueprints/BP_MarioGI` built its node and compiled, but the reply carried four engine warnings (a "Short type name ... provided for TryFindType" callstack for each short name, "Failed to find object" for each asset path) raised by the lookups along the way. Class lookups by short name or asset path (graph class pins, `memberClass`, behavior-tree node classes, factory classes) no longer log them.
- **A refused pin connection says why.** `connect_pins` (and a `build_graph` connect step) answered only "Failed to connect pins (schema rejection)". It now gives each pin's direction and type, the graph schema's reason ("Directions are not compatible", a type mismatch...) and the source node's output pins, so naming a Set node's value input as the source points straight at its `Output_Get`.
- **Replies name actors so the name reaches them again.** Every reply that names an actor (`control_actor`'s transform, material, tags, attach, physics, snapshots, spawn, find, `audit_placement` findings and overlaps and `delete_by_tag`'s `deleted` list, and the actors that lighting, spline, Niagara, effect, environment, navigation, sequencer-camera, editor-camera, geometry-primitive and streaming-volume actions create or read) gave its label, and every unnamed spawn is labelled "Cube": the reply's handle then pointed at whichever cube the resolver met first, and a 253-actor delete answered "Cube" 200 times. A label that another actor shares is now replaced by the unique actor name; a unique label still reads as before.
- **An actor missing from the running game is reported as missing.** A lookup that missed in the Play-In-Editor world then asked the editor level through an API the engine refuses during play, and the refusal it logged turned the reply into "stop play, then retry", although the running world had been searched (the actor was in the level PIE had left). The miss is answered directly, and the reply's `worldName` names the world that was searched.
- **`inspect_graph` finds a node by `nodeGuid`.** Its contract takes `nodeGuid` in place of `nodeId`, but the node and pin reads looked only at `nodeId` and answered "Could not find node ''". Both names work, and the reply's `nodeId` is the node's full GUID whichever GUID, prefix or node name found it.
- **`sample_motion` says when its keys went nowhere.** When keys were pressed and the player's pawn never moved (a title or pause menu on top, or input the game had locked), the reply ends with a warning naming those causes and `widget_list` / `widget_click` as the way past a menu. Before, the samples read like a level that blocks the way.
- **A graph filter matches what the editor shows.** `inspect_graph` `filter: "Spawn System"` matched nothing because this reply titles the node `SpawnSystemAtLocation`; spaces are ignored on both sides now.
- **A missing object is reported as missing.** A path to an object that does not exist inside an existing package (a wrong instance number on a transient object, say) resolved to the package itself, so the next error blamed a property "not found on Package". The package is returned only when the path names it, and the not-found message lists the names that reach the running game's objects.
- **`get_dependencies` says what `recursive` and `maxDepth` do.** Both were documented as walking the dependency tree, but the handler lists direct packages only; their descriptions say so and point at `lookup: "graph"`, which does walk it. `query_asset`'s summary stopped calling its node-graph lookup a reference graph.
- **`control_editor.undo` and `redo` do something.** They ran the console text `Undo` / `Redo`, which the editor does not recognise (both are `TRANSACTION` subcommands), and answered "Undo executed" while nothing moved. They now drive the editor's transaction buffer, name the transaction they undid or redid, and answer `NOTHING_TO_UNDO` / `NOTHING_TO_REDO` when the buffer is empty.
- **A delete is one undo step.** `control_actor.delete` (including `actorNames`) and `delete_by_tag` run inside one editor transaction per call, so one `undo` restores every actor; each `DestroyActor` used to be its own transaction, so undoing a large clear took one undo per actor. The reply's `undo` block says whether the transaction recorded.
- **Saves never open a modal.** Asset and level saves run unattended, so a failed save is logged instead of opening a dialog that blocked the editor mid-call, and a `build_graph` batch saves its Blueprint once instead of once per step.
- **`get_material_info` reads a material instance.** It answered `ASSET_NOT_FOUND`; it now reports the instance's `parent`, `baseMaterial` and `parameterOverrides` (scalar, vector and texture).
- **`manage_blueprint` `get` reads a component default** given as `Component.Property` (`Shield.bVisible`) from the Blueprint's construction-script template, where it answered `PROPERTY_NOT_FOUND`.
- **A folder, tag or class named like a credential keeps its count.** `control_actor.list` `summary` answered `"Level/Stage/Secrets": "[REDACTED]"`, because receipt redaction reads a JSON key as a field name. The names now travel as values (see Migration); redaction is unchanged.
- **A read-only pin takes a literal.** `set_pin_default_value`, and `pinDefaults` in `build_graph`, refused a const-reference pin such as TextRender **Set Text**'s `Value` with `PIN_REQUIRES_CONNECTION`, which stopped a batch halfway through. The value now goes into a `MakeLiteral` node wired into the pin (text, string, name, bool, int, byte, float); the reply names it as `literalNodeId`, and setting the pin again updates that node. Struct and enum pins still refuse.
- **A failed `build_graph` step leaves nothing behind.** A `create_node` step whose `pinDefaults` failed kept its node in the graph while the error said only the steps before it were applied, so re-running the batch from that step stacked a duplicate. The node, and any literal node it got, is removed.
- **Reading a Blueprint graph never compiles it.** `inspect_graph` compiled any Blueprint it found dirty (after a failed batch, or an edit left uncompiled in the editor), and that compile cleared the editor's undo history.
- **A folded action's selector conflict says what to do.** Calling a folded name such as `set_component_property` with a selector value it does not pin (`edit: "set_properties"`, which `describe` lists) answered only "pins a selector value that conflicts with the one supplied". It now names the pinned and the sent value and returns an executable `nextCall`: the family's primary action with the same params. Both transports.
- **`read_log` `filter` takes alternatives.** `LoadMap|Bringing World` was read as one literal and matched nothing; `|` now separates alternatives and a line matches any of them. The contract now says the filter is plain text, not a regex, since escaped brackets silently match nothing.
- **`manage_blueprint` `get` reads a component the native parent creates.** `CharMoveComp.JumpZVelocity` and `CharacterMovement.JumpZVelocity` answered `PROPERTY_NOT_FOUND` on a Character Blueprint because only construction-script components were searched; the CDO's default subobject (by object name or by the property holding it) is searched too.
- **`folder: "(none)"` finds the root.** `control_actor.list` read a root actor's folder as `None`: the summary listed a folder named `None` and the documented `"(none)"` filter matched nothing. `inspect`'s actor query reported `folderPath: "None"` the same way; it is now empty.
- **`GetVariable` works as a node type everywhere.** `edit_graph` names it as a node alias, and `add_node` took it, but `create_node` and every `build_graph` step answered `NODE_TYPE_NOT_FOUND`. `VariableGet`, `GetVariable` and the Set forms now work in any case on all three.
- **Blueprint calls work during Play.** Every `manage_blueprint` call made while PIE ran logged the engine error "The Editor is currently in a play mode.", and `exists`, `ensure_exists` and `probe_handle` answered "not found" for a Blueprint that exists, because they asked the editor asset library, which refuses during play. They read the asset registry now, and `probe_handle` reports the `assetClass` it never found before.
- **`GRAPH_NOT_FOUND` says where to look.** A graph name that is really an event (`FoundSecret`, a custom event inside `EventGraph`) answered only "Could not find graph". The error now names the event graph that event lives in, and any other miss lists the Blueprint's graphs.
- **Search ranks the exact action above a folded one.** On the native door "delete blueprint graph node" ranked `edit_anim_graph` and `edit_graph` above `delete_node`, because a folded record's aliases score word by word. A record gets a bonus when the query names every word of its own action (two words or more) and every query word matches it.
- **A boolean named like a secret reads back.** A Blueprint default such as `bSecret: true` came back as `"[REDACTED]"`, because receipt redaction masks any value under a key that names a credential. A boolean can carry no credential, so it is no longer masked; strings, numbers, objects and arrays under such keys still are, on both transports.
- **"remove", "destroy" and "erase" find delete capabilities.** They matched only capabilities literally named `remove_*`: on the native door "remove actor" answered a Sequencer binding edit, and "remove node from blueprint graph" reached `delete_node` on neither door. Both doors now read all three as "delete" in queries and catalog text alike. `remove_tag` declares "remove tag from actor" as a topic so that phrasing still finds it. On the native door, an alias the query names word for word also earns the exact-action bonus.
- **A single graph edit leaves the Blueprint saved.** Single-step graph edits (`connect_pins`, `create_node`, `delete_node`, …) saved the Blueprint and then compiled it for the reply, and the compile marked it unsaved again, so every single edit left it dirty and the reply never said so. The reply now saves after that compile and reports `saved`.
- **A Blueprint named as `memberClass` resolves.** A CallFunction node with `memberClass` `BP_MarioGI_C` or `/Game/Mario/Blueprints/BP_MarioGI` answered "Function 'AddCoin' not found": the class lookup rejected a Blueprint's own names, and the error never said the class was the problem. Both spellings resolve now, and a `memberClass` that still resolves nothing is named in the error.
- **A Custom material node's extra outputs can be wired straight away.** `add_custom_expression` and `update_custom_expression` stored `additionalOutputs` but the node kept one output pin until the material was reopened, so connecting `"$node.Emis"` in the same batch failed with "Source output 1 does not exist". The pins are rebuilt when the outputs are set, and `additionalOutputs` now documents its `{name, type}` items.
- **A folded capability no longer sends you to itself.** A folded record kept its members' "use X" guidance, so `edit_graph` said "use create_node" and `inspect_graph` "use get_node_details", which are variants of the same capabilities. Those lines are dropped; guidance that points elsewhere stays.
- **An undeclared parameter never suggests `action`.** Every capability declares the envelope's `action`, so `playAction` drew "did you mean 'action'" for a field `execute` refuses inside params. Both doors leave `action` and `subAction` out of the hint and the allowed list.
- **Light and text edits are easier to find.** `edit_component` names a light's `Intensity` and `LightColor` and a TextRender's `Text` in its summary and topics, so "set light intensity" and "set text render text" list it among their first results; `get_material_info` answers "inspect material graph" and "material node details".
- **`add_node` over stdio keeps `memberName`.** The TypeScript server forwarded only `functionName`, so `add_node` with the declared `memberName` reached the plugin with no variable or function name.
- **`set_node_property` writes the value you send.** It read only `value`, which the contract does not declare and the gateway refuses, so every call wrote an empty value (clearing a comment, or moving the node to 0) and reported success. It now reads `propertyValue` as a string, number or boolean, and a call without a value fails with `INVALID_ARGUMENT`. `delete_node` and `set_node_property` accept `nodeGuid` like the other node edits, and every node-addressing action takes either `nodeId` or `nodeGuid`.
- **Nested property paths resolve the same way for reads and writes.** `set_component_property` looked a name up only on the component itself, so `BodyInstance.CollisionEnabled` or `LightmassSettings.bShadowIndirectOnly` answered "not found", even though `get_component_property` could read the same path. Reads and writes now share one resolver. It follows a dotted path to any depth, and it resolves a bare name that exactly one struct member carries, at any depth. When several members match, the error lists the full paths to choose from. The resolver is used by `control_actor`'s `set_component_property`, `set_component_properties`, `add_component` and `get_component_property`, by `manage_blueprint`'s `set_scs_property`, `modify_scs` and `get`, and by `inspect`'s `get_property` and `set_property`. Collision keys go through the component's collision setters everywhere, and a raw `BodyInstance` write on a live component now reaches its physics.
- **`add_component` reports what it could not apply.** A `meshPath` that did not load, or a mesh given to a component that takes none, was dropped without a word. A property that failed came back as success with the failure buried in `warnings`. The call now fails with `PROPERTY_CONVERSION_FAILED` (nothing applied) or `PARTIAL_FAILURE`, naming the component it added, which stays in place. `meshPath` is declared. On both `add_component` and `set_component_properties`, a `Mobility` value that did not apply is now reported.
- **`find_by_tag` sees the running game, and a partial `delete` fails.** During Play-In-Editor, `find_by_tag` asked the editor for its actors, the editor refused, and the reply said 0 found for tagged actors that were right there. It now searches the running world, as `find_by_name` and `find_by_class` do. `delete` reported a partial delete as success, with only a nested `success: false` saying otherwise, and a delete that found nothing dropped the list of missing names. The call now fails with `DELETE_PARTIAL` or `NOT_FOUND`, and both carry the `deleted` and `missing` lists.
- **Widget property bindings work.** `bind_widget` `text`, `color`, `enabled` and `visibility` had no handler at all, so every call answered `UNKNOWN_ACTION`. They now write a real UMG property binding to `bindingSource`: a pure function with no inputs is bound directly, and a variable gets a generated `Get_<Widget>_<Property>` getter that converts it (a bool drives Visible or Collapsed). `color` binds whichever tint the widget has: text or image colour, progress fill or border brush. A function with the wrong signature, a name that is neither a variable nor a function, and a widget that has no such property are refused, the last with the list of what it can bind; a binding that does not compile is removed again and refused.
- **Event bindings call the function you name.** `bind_widget` `on_clicked`, `on_hovered` and `on_value_changed` placed an event node that called nothing: the required `bindingSource` was never read. The event now calls `bindingSource` (`onHoveredFunction` and `onUnhoveredFunction` for hover) and creates that function with the event's inputs when it is missing, so a slider's new value arrives in it. `on_hovered` also binds OnUnhovered, which it promised and never did, and an event that already runs other nodes is refused with a pointer to `edit_graph` instead of being rewired. The reply lists, per event, the function, whether it or the event node was created, both node ids, and whether the Widget Blueprint was saved.
- **Character edits change only what they name.** `configure_crouch`, `configure_capsule_component` and `configure_camera_component` wrote every field with a built-in default, so setting the capsule radius reset the half-height to 96, setting `canCrouch` reset the crouch speed to 300 and the crouched half-height to 44, and setting the arm length turned camera lag off. Each field now changes only when sent (a new camera boom still starts from the third-person defaults), and the reply reports the values the Blueprint holds after the call. A Blueprint that is not a Character is refused with `NOT_A_CHARACTER` instead of answering success with nothing changed.
- **`set_walk_speed` and its siblings need their value.** `set_walk_speed`, `set_jump_height`, `set_gravity_scale`, `set_ground_friction` and `set_braking_deceleration` wrote the engine default over the Blueprint's own value when the value was left out, and changed nothing on a non-Character Blueprint, both with a success reply. The value is required now, and a non-Character Blueprint is refused with `NOT_A_CHARACTER`. `configure_movement_speeds` also accepts `jumpHeight`, `jumpHoldTime`, `maxJumpCount`, `airControl`, `gravityScale` and `fallingLateralFriction`, which it already applied but the gateway refused as undeclared.
- **Door, chest and switch configure calls change only what you pass.** `configure_door_properties`, `configure_chest_properties` and `configure_switch_properties` wrote a default for every field left out, so changing `openAngle` unlocked a locked door. Only the fields sent are written now, and only those are echoed back. `create_door_actor` declares `locked`, and `create_switch_actor` now stores `switchType` instead of only echoing it.
- **Gameplay Ability System edits survive a restart.** `set_ability_costs`, `set_ability_cooldown`, `set_activation_policy`, `set_instancing_policy`, `configure_asc`, `set_effect_stacking`, `set_effect_tags` and `add_effect_cue` changed the Blueprint's defaults and only marked it modified, so the change was gone after an editor restart. They compile and save the Blueprint now, and answer `COMPILE_FAILED` or `SAVE_FAILED` when a step fails instead of reporting success.
- **Niagara graph edits are saved.** `add_niagara_module`, `connect_niagara_pins` (including `autoConnect`) and `remove_niagara_node` only marked the system dirty, so the change vanished at the next editor start. They save unless `save: false` and report `saved`. `connect_niagara_pins` also declares the `fromNode`, `fromPin`, `toNode` and `toPin` it reads, so an explicit connection can be made through the gateway at all.
- **Skeleton and mesh edits honour `save: false`.** Every bone, virtual-bone, socket, skeleton, physics-asset, physics-body, constraint, cloth, morph-target and skin-weight edit saved the asset whatever `save` said. `save: false` now leaves the change in memory; the default is still to save.
- **Level-structure and volume edits save when asked.** `save: true` was declared on volume creates, adds, extent, bounds and property edits and removals, and on data-layer, streaming, grid, World Partition, minimap and level-blueprint edits, but none of them read it. It now saves the edited level and the reply reports `saved`. A save requested on an unsaved `/Temp` level, or a save that fails, answers `SAVE_FAILED`, and the edit stays in memory. `configure_hlod_layer` honours `save: false` and reports a failed save instead of hiding it.
- **`manage_level` saves act on the level you name.** `save` with `savePath` used to save the open level in place under its old name; it now saves the level to `savePath`. `save` and `save_as` refuse with `LEVEL_NOT_LOADED` when `levelPath` names a level other than the open one. `add_sublevel` refuses with `PARENT_LEVEL_NOT_LOADED` when `parentLevel` is not the open level; it used to add the sub-level to whatever level was loaded.
- **Asset operations honour `force`, `save`, `paths` and `recursive`.** `delete` reads `force`: without it, an asset something outside the delete still references is kept and listed in `referencedPaths` (it used to be deleted and its referencers broke silently). `import` saves the imported asset. `source_control_checkout` and `source_control_submit` read the declared `paths`, so a multi-asset call works; `get_source_control_state` reads `recursive` (every `/Game` package the assets depend on, up to 512) and takes `assetPaths`. `generate_lods` lets `lodCount` win over the older `numLODs`; `move` declares `newName`, `list` `includeMetadata`, `create_thumbnail` `outputPath`, `fixup_redirectors` `checkoutFiles`, `source_control_commit_all` `userName` and `userEmail`, `create_render_target` `renderTargetPath`, and `nanite_rebuild_mesh` `trianglePercent`.
- **Images keep their size.** `add_image` `brushSize` and every sized image in the ready-made screens and HUD pieces fell back to 32 by 32 once the asset was saved and reopened, because the size only reached the live preview. It is written into the image's brush now.
- **Typed widget adds apply what they declare.** `isEnabled` now applies to every typed add; `add_button` takes its label as a dark `<slotName>_Text` child; `add_image` sizes its brush and refuses a texture that does not load instead of adding a white image; `add_rich_text_block` applies `fontSize`; `add_text_input` picks single or multi line from `inputType`; list and tree views take `orientation`; `add_grid_panel` gives `columnCount` and `rowCount` equal-share columns and rows instead of ignoring them; `add_wrap_box` applies `wrapWidth` (giving one turns on the explicit wrap width). `add_widget_component` refuses a `parentName` that is not a panel instead of seating the widget at the root. Typed adds, spacers and panels declare `positionX`, `positionY`, `sizeX` and `sizeY`, size boxes declare their maximum width and height and borders their content tint, all of which the plugin already read. A size on its own no longer re-pins a centred canvas child to the top-left corner.
- **Layout setters report what they wrote and refuse what they cannot do.** `set_anchor` and `set_position` answered success for a widget in a box or overlay and wrote nothing; they now refuse a non-canvas slot and name the setter that fits. `set_anchor` takes a `preset` alone, keeps the corner it is not given and refuses an unknown preset; `set_size` and `set_z_order` refuse a call with nothing to set; `set_padding` works on every slot type that has padding and keeps the sides it is not given; `set_render_transform` starts from the widget's own transform, so setting only the angle keeps its translation and scale; `set_visibility` and `set_clipping` refuse a misspelled value instead of quietly resetting it. Every reply carries the widget read back (visibility, render transform, and the canvas anchors, position, size and z-order or the box padding and alignment) and whether the save landed.
- **`set_style` and `set_font` reach the text they are pointed at.** `set_style` `text` and `fontSize` changed only a TextBlock; on a Button or a RichTextBlock they were skipped and the call fell into read mode and answered success. A Button's label and a RichTextBlock's text and size change now, an outline without a `cornerRadius` is refused (the rounded brush draws it), `propertyName` and `value` are declared for any other style property, and a call that asks for nothing is refused. `set_font` keeps the current size when only a font is given (it reset every size to 24), refuses a font that does not load, and styles a RichTextBlock through its default text style instead of refusing it.
- **`add_animation_track` creates the track it names.** It read an undeclared `propertyName`, ignored `trackType`, created only the widget's row in the animation, and appended another copy of that row on every call. It now finds or creates the RenderOpacity, ColorAndOpacity or RenderTransform track for `trackType` on a single binding and says whether the track was added or already there.
- **`create_widget_blueprint` refuses a parent that is not a widget.** A `parentClass` that did not resolve, or was not a UserWidget class, quietly produced a plain UserWidget and reported success; it is refused now (`INVALID_PARENT_CLASS`), and a Widget Blueprint class path is accepted as a parent.
- **`create_node` CallFunction reads `functionName`.** Passing the declared `functionName` failed with "Function '' not found" because only `memberName` was read. Either name works now.
- **`add_variable` explains a name clash.** A name the parent class already uses is refused with `VARIABLE_NAME_CONFLICT`. The message names the inherited property, the class that declares it and its type, and points to `edit_variable` `set_default`. A name that collides with a component, function or event, which the compiler renames, is reported with the name the variable ended up with. Both cases used to answer only "Variable add verification failed".
- **`add_function` applies `isPublic`.** The flag was echoed back as applied but never set. `false` now makes the function private; leaving it out keeps the editor default, public.
- **`ensure_exists` finds the Blueprint it made.** It checked `/Game/<name>` (or any Blueprint with the same name) while creating under `savePath`, so a repeat call with another `savePath` never found what it had created. It now checks `savePath` plus `name`.
- **Blueprint `set_metadata` with `propertyName` writes to that variable.** The keys used to go on the class. With no `propertyName`, a Blueprint that has never been compiled now answers `BLUEPRINT_NOT_COMPILED`; before, it reported every key as set while writing nothing.
- **`add_event` and `remove_event` honour `graphName`.** Both always worked on the main EventGraph. A page name that does not exist now answers `GRAPH_NOT_FOUND` with a list of the Blueprint's event graph pages. `remove_event` declares `eventName`, so a custom event can be removed by name.
- **`create_character_blueprint` honours `parentClass`.** Every Blueprint derived from `Character` whatever was asked for, and the reply said `Character`. It now derives from the named class (a native Character subclass or a Character Blueprint, by short name or path), refuses anything that is not a Character with `INVALID_PARENT_CLASS`, and the reply names the real parent.
- **`set_bone_parent` re-parents to the bone you name.** It read only `parentBone` and `newParentBone`, which the contract does not declare, so the declared `parentBoneName` always arrived empty and the bone became a second root while the reply reported success. It reads `parentBoneName` now, requires it, and refuses a parent the skeleton does not have with `PARENT_NOT_FOUND`. `add_bone` takes `parentBoneName` over the older `parentBone` when both are sent.
- **One constraint per pair of bodies.** `set_physics_constraint` appended a new constraint on every call, stacking joints on the same two bodies; it now edits the pair's existing constraint, or creates one, and the reply says which (`created`). `add_physics_constraint` refuses a pair that already has one with `CONSTRAINT_EXISTS`. Limits on `add_physics_constraint`, `set_physics_constraint` and `configure_constraint_limits` change only the axes named: tightening one swing used to reset the other two axes to 45 degrees Limited. An angle sent without a motion makes that axis Limited. `limits` is now a declared object ({swing1LimitAngle, swing2LimitAngle, twistLimitAngle and a Free, Limited or Locked motion for each}) on all three, and `bodyA` and `bodyB` are required.
- **`auto_skin_weights` computes weights.** It only rebuilt the mesh's render data from the weights it already had, yet answered "Mesh rebuilt with recalculated skin weights". It now recomputes LOD 0 weights by smooth binding to the mesh's own skeleton, keeping normals, tangents and vertex order, on UE 5.5 and later with GeometryScripting, and answers `NOT_SUPPORTED` elsewhere (set weights with `set_vertex_weights` there).
- **`create_montage` builds from its animation.** `animationPath` was declared but ignored, so every montage came out empty. The sequence now becomes the first segment of the slot and supplies the skeleton when `skeletonPath` is omitted; a sequence on a different skeleton than `skeletonPath` is refused with `SKELETON_MISMATCH` before the engine could assert. Re-running with an existing name reuses the asset (`existingAsset: true`), as the sequence creators do, instead of creating a second asset over the first, and `slotName` names the slot.
- **`create_animation_blueprint` honours `parentClass`.** It was read and ignored, so every Animation Blueprint derived from `AnimInstance`. It now derives from the named AnimInstance subclass (native or an Animation Blueprint path), refuses any other class with `INVALID_PARENT_CLASS` and names the parent in the reply. `skeletalMeshPath` takes the skeleton of that mesh when the caller holds only the mesh.
- **`add_layered_blend_per_bone` applies its layers.** `layerSetup` was read and dropped, so the node had no branch filters and blended nothing, and the call still reported success. Each `layerSetup` entry now becomes a blend pose with its `branchFilters` ({boneName, blendDepth}), `boneName` is shorthand for one layer on that bone, a bone that is not on the Animation Blueprint's skeleton is refused with `BONE_NOT_FOUND`, and the reply counts `layerCount` and says when the node was created with no filters.
- **`add_blend_node` creates the node type you ask for.** `BlendListByBool` and `BlendListByInt`, both documented, came out as a TwoWayBlend, as did any misspelled type. Both are created now, `blendType` is an enum (`TwoWayBlend`, `BlendListByBool`, `BlendListByInt`, `LayeredBoneBlend`), and an unknown type is refused with `UNKNOWN_BLEND_TYPE`.
- **`set_interpolation_settings` sets the interpolation.** `interpolationType` was read and never applied, and every call reset the sample weight speed to 5. It now sets the input smoothing type (`Average`, `Linear`, `Cubic`, `EaseInOut`, `ExponentialDecay`, `SpringDamper`) and the new `interpolationTime` on every axis, changes `targetWeightInterpolationSpeed` only when sent, refuses an unknown type with `INVALID_INTERPOLATION_TYPE` (the old example's `Lerp` included), and reports the values the blend space now holds.
- **`set_bone_key` keys the rotation you send.** The contract types `rotation` as `[pitch, yaw, roll]`, but the handler read it as an object, got nothing, and wrote every key with no rotation while reporting success. The array is applied now, and `scale` is accepted.
- **`set_sequence_length` takes seconds and keeps the frame rate.** `length` was declared but never read, so a call with only `length` made the sequence 30 frames long, and the frame rate reset to 30 whenever the length changed. It now takes `numFrames`, or `length` in seconds, keeps the sequence's own rate unless `frameRate` is sent, refuses a length under one frame, and reports `numFrames` and `frameRate`.
- **Adding a sample or segment edits the right asset.** When `assetPath` was missing, `animationPath` was copied into it, so `add_aim_offset_sample` loaded the sample as the aim offset, and `add_montage_slot` with `montagePath` and `animationPath` edited the sequence. On `add_aim_offset_sample`, `add_blend_sample`, `add_montage_slot` and `create_montage`, `animationPath` is only the input now, and `montagePath` and the other asset aliases win over it everywhere else. A blend-space sample given as `[x, y]` is placed there instead of at 0, 0.
- **Montage blend edits change only what is sent.** `set_blend_in` and `set_blend_out` reset the curve to Linear on a `blendTime`-only call and the time to 0.25 seconds on a `blendOption`-only call. Each now changes only when sent, `blendOption` (`Linear`, `Cubic`, `Sinusoidal`) is declared, and an unknown option is refused with `INVALID_BLEND_OPTION`.
- **`set_additive_settings` refuses a base pose it cannot load.** A `basePoseAnimation` that did not load was skipped and the call reported success; it is refused with `ANIMATION_NOT_FOUND` before anything changes, and `basePoseAnimation` is now declared. `basePoseType` also takes the engine's own `AnimFrame` and `AnimScaled` spellings, which used to fall back to the reference pose.
- **`play_montage` plays at the rate it reports.** On a mesh without an AnimInstance the montage played at normal speed while the reply claimed `playRate`; the rate is applied now. With an AnimInstance, a montage the engine refused to play (another skeleton, say) answered success; it is refused with `MONTAGE_PLAY_FAILED`. The reply's `playMode` says which path ran.
- **`configure_vehicle` refuses a type it cannot build.** Any `vehicleType` was echoed back while a four-wheel movement component was added regardless. Only `WheeledVehicle4W` and `WheeledVehicle` are accepted now, anything else is refused with `UNSUPPORTED_VEHICLE_TYPE`, and the reply names the movement component class. The `wheels`, `engine` and `transmission` blocks it already applied are now declared.
- **Animation authoring accepts what its handlers read.** Calls the handlers supported were refused as undeclared: `save` across the family (default true), `positionX` and `positionY` on AnimGraph nodes, `states` and `transitions` on `create_state_machine`, the transition settings on `add_transition`, the axis names and ranges on the blend-space creators, `gridDivisions` on `set_axis_settings`, `groupName` on `add_slot_node`, `numFrames` on `create_procedural_anim` and `create_animation_sequence`, `assetType` (sequence or montage) on `create_animation_asset`, `createIfMissing` on `set_curve_key`, `time` and `trackIndex` on notifies, and `skeletalMeshPath` and `modularRig` on `create_control_rig`. `create_state_machine` no longer reads its `name` as the Blueprint path, which turned a missing `blueprintPath` into "AnimBlueprint not found" for the machine's name.
- **`manage_level` `load` with `streaming: true` streams the level in.** It adds the level to the open world as a sub-level, as `add_sublevel` does. The flag used to be ignored, and the load replaced the open level.
- **`create_level` can save unsaved work first.** Creating a level ends by loading it in place of the open one. With `saveDirtyPackages: true`, unsaved packages are now saved before that happens. If a save fails, the call fails with `DIRTY_PACKAGES` and creates nothing.
- **Level exports, imports and metadata report what happened.** A `.t3d` `export_level` now exports the level `levelPath` names; it used to export whatever level was open. `import_level` onto an existing destination without `overwrite` used to answer success while importing nothing, and now fails with `DESTINATION_EXISTS`. Level `set_metadata` with an empty `metadata` object used to answer success with nothing written, and is now refused.
- **Level streaming applies `streamingMethod`.** `create_sublevel` made every sub-level Blueprint-streamed, and `configure_level_streaming` echoed the method back without changing it. Both now apply Blueprint or AlwaysLoaded, and the reply reads the method back from the level. `configure_level_streaming` also changes only the flags you pass; any omitted flag used to overwrite the level's visibility, block-on-load and distance-streaming settings with defaults.
- **`enable_world_partition` applies `bUseExternalActors`.** The flag now moves the level's actors into one-file-per-actor packages, as the World Settings toggle does, and the reply reports `usesExternalActors`. Before, the flag was ignored and the reply said to run a commandlet. Turning World Partition off on a partitioned level now answers `NOT_SUPPORTED`; it used to answer success and change nothing.
- **Volume creates read their declared fields.** `create_minimap_volume` reads `location` and `extent` (it read only `volumeLocation` and `volumeExtent`). A TriggerBox honours `extent`; before, the `boxExtent` default hid it. Post-process volumes read `bEnabled`, and an attached physics volume applies `priority`.
- **`set_struct_as_row_struct` binds the struct to the table.** It read a `structPath` the contract never declared, so every call through the gateway answered "MISSING_PARAMETER: MISSING_PARAMETER", and even a direct call only compiled the struct and left the table untouched. It now does what `set_data_table_row_struct` does: it reads `dataTablePath` and `rowStructPath`, validates and compiles a user struct first, migrates or clears the existing rows (`migrateExistingRows` and `clearExisting`, now declared on both actions) and reports what it bound.
- **DataTable errors say what went wrong.** A refusal carried only its code ("MISSING_PARAMETER" was the whole message); each one now names the missing field or the path that did not load. `delete_data_table_row` refuses a row that does not exist (`ROW_NOT_FOUND`) instead of reporting it removed, every row write reports `saved`, and `clear_data_table_rows` reports how many rows it cleared.
- **Enum and instanced-struct failures come back as failures.** `manage_asset` answered every enum and FInstancedStruct action with `success: true`, so "Enum not found", a missing parameter and a refused `delete_enum` all read as successes. They now fail with their message and a code (`ASSET_NOT_FOUND`, `MISSING_PARAMETER`, `NOT_FOUND`, `DELETE_FAILED`), and `reorder_enum_values` naming a value the enum lacks is an error instead of a no-op success.
- **Struct edits apply what they declare.** `add_struct_member` applies its `tooltip` and `metadata` (both were dropped) and takes `members` to add several at once; `import_struct` reads `sourcePath`, a project JSON file holding a member array or what `export_struct` returns, and can create a new struct from `name` and `path`; `create_struct` takes a full `structPath`; `rename_struct` takes `destinationFolder` or `newStructPath`; `delete_struct` takes `force` to delete a struct that is still referenced. `set_instanced_struct_property` with `save: false` now skips the save; it saved regardless.
- **`split_enum` reads `index`.** Every value from that position on is copied into the new enum; pass `values` or `index`, not both. The index was declared and ignored, so a split by position was impossible.
- **`set_two_sided` honours `value: false`.** It read an undeclared `twoSided` that defaulted to true, so `value: false` turned two-sided on. It now recompiles the material, saves it (see Migration) and reports the flag the material actually holds.
- **Material node knobs take effect.** `add_math_node` applies `constA` and `constB`, `add_panner` `speedX` and `speedY`, `add_rotator` `speed`, `add_noise` `scale` and `levels`, and `add_voronoi` `scale`; all were declared and dropped. A knob the chosen node does not have (a `constA` on a Sine node, say) is refused with `INVALID_ARGUMENT` instead of ignored.
- **Material nodes can be found and deleted by what the contract names.** `find_node` reads `nodeName` (it read an undeclared `name`, so a search by name was impossible), also matches node ids, and no longer lets a type match override a name miss. `delete_node` and `remove_material_node` take `nodeIds` alone; ids that match nothing are listed under `notFound`, and a call where none match fails with `NOT_FOUND` instead of answering "Deleted 0 node(s)". `get_material_node_details` picks a node by `expressionIndex`, and `get_material_info` declares `filter`, `nodeId` and `nodeIds`.
- **Material creators and parameters take their settings.** `create_decal_material`, `create_landscape_material` and `create_post_process_material` accept `materialDomain`, `blendMode`, `shadingModel`, `twoSided` and `save` like `create_material`, a value overriding the preset. `add_static_switch_parameter` takes `defaultValue`, `add_vector_parameter` and the static switch take `group`, `add_texture_sample` takes `parameterName`, `set_material_parameter` takes `texturePath`, and `save` is declared on the material edits that read it.
- **`add_landscape_layer` puts the layer info beside the material.** It took `materialPath` as a folder, so every call created a folder named after the material; the layer info asset now lands in the material's folder, or in `path`. A physical material that does not load fails the call before anything is created (it was skipped behind a success), `hardness`, `physicalMaterialPath`, `noWeightBlend` and `save` are declared, and the reply carries `layerInfoPath` and `saved`. `create_landscape_layer_info` fails on a bad physical material the same way.
- **Texture generators do what they say.** `create_noise_texture` reads `noiseType` (Perlin, FBM, Ridged, Billow; anything else is refused) where one algorithm always ran, and the generators declare `outputPath` and the colour, shape and noise knobs they already read; resize, normal-from-height, channel pack, channel extract and combine declare their `path` and `name` fallbacks. `hdr` is no longer read: the pixel writer fills 8-bit data, so it produced an 8-bit texture labelled HDR. `set_streaming_priority` stops claiming a priority it never set (a texture has none) and reports `neverStream` only, and `configure_virtual_texture` stops echoing tile sizes it never applied.
- **`create_landscape` builds the size it is given.** `quadsPerSection` was stored as quads per component, so `sectionsPerComponent: 2` built components that disagreed with the imported heights, `sectionsPerComponent: 0` divided by zero, and UE 5.0-5.4 imported with component indices instead of the vertex region. A component now spans `sectionsPerComponent` sections of `quadsPerSection` quads, and sizes the engine cannot build are refused.
- **Landscape edits read the target, brush and layer they declare.** Every edit action finds its landscape by `landscapeActorPath` (the path `create_landscape` returns), which was declared and never read. `sculpt` lets `tool`, `radius` and `falloff` win over their older spellings and refuses a tool other than Raise, Lower or Flatten, which used to change nothing and report success. `paint_landscape` paints the disc of `radius` around `location` and leaves the rest of the layer as it was; with no brush and no region it paints the whole landscape. `paint_landscape_layer` uses the `layerInfoPath` asset for a new layer (refusing one whose layer name differs), and both report the region they painted.
- **Landscape configure calls fail when nothing applied.** `configure_landscape_lod` reported success with nothing applied or with errors listed; it now fails. `configure_landscape_material` declares only the material it assigns. `create_procedural_terrain` reads its declared `name` (defaulting to ProceduralTerrain instead of demanding `actorName`) and `spacing`, and fails on a `material` that does not load instead of skipping it.
- **`add_foliage` places foliage.** With a foliage type or a placement (`locations`, `position`, or `location` with `radius` and `count`) it scatters instances; it used to build only the type asset and report success. `meshPath` alone still makes the type. `add_foliage_instances` accepts its declared `meshPath`, and `remove_foliage` with nothing named, or with a type that does not exist, is an error instead of a success that removed nothing.
- **Foliage type settings apply or fail.** The `configure_foliage_*` calls fail on a mesh that does not load (the old mesh was kept behind a success), read both scale bounds (`maxScale` was skipped whenever `minScale` was given), set the placed instances' collision from `collisionEnabled`, stop reporting a spurious error for `cullDistance`, and fail when any setting fails. `create_procedural_foliage` names its volume from `volumeName`, puts its assets in `path`, and reports a simulation that placed nothing as a failure.
- **Sky, sun and weather calls configure the actor already in the level.** Unnamed, `configure_sky_atmosphere`, `configure_sky_light`, the sun and directional-light calls, height fog, volumetric cloud and wind edit the level's actor of that kind; matching the default label exactly spawned a second sun beside one labelled DirectionalLight. These calls now declare `actorName` and `location` (and `rotation` where azimuth and elevation do not set it), and `create_sky_sphere` no longer pushes one rotation or settings object onto all three rig actors.
- **Sky lights use their cubemap.** `configure_sky_light` and `create_sky_light` apply `cubemapPath` (switching the source to a specified cubemap) and fail on a path that does not load; `create_sky_light` needed an undeclared `sourceType` before it looked at the path. `create_light` with a sky light applies `intensity` and `color`, which were dropped.
- **Colour curves take their keys.** `configure_sky_color_curve` and `configure_light_color_curve` edit the curve at `curvePath` with the given `keys` ({time, color}), creating it there when missing; they wrote a flat white curve into a new folder named after the path on every call.
- **Water settings land on the right body.** Material and collision go on the water body the call just created or found, not the level's first ocean, and a setting or material that does not apply fails the call instead of hiding in the reply. Undeclarable no-op inputs (`intensity` on the sky atmosphere, `density` on clouds and weather particles, `propertyValue`, `path`, the water waves' `speed`) are gone from the records.
- **Lighting calls apply every setting they declare.** `configure_shadows` applies `shadowQuality`, `shadowDistance`, `contactShadows` and `rayTracedShadows` (the last only returned a note) and declares the per-light shadow keys it reads. `set_exposure` sets the override flags, without which its values never took effect, and applies `method`; `set_ambient_occlusion` applies `quality`; `setup_global_illumination` applies `quality`, `indirectLightingIntensity` and `bounces`. `ensure_single_sky_light` and `create_lightmass_volume` declare `name`, `recapture` and `size`.
- **Post-process variants write only what is passed.** An omitted amount no longer resets its setting (`set_exposure_min_max` reset the bound left out to 1.0, and SSAO, grain and chromatic aberration overwrote what `settings` had just set with a default), and a call that changes nothing fails with `NO_SETTING_SUPPLIED` instead of answering success. `blendWeight` and `infiniteUnbound` are declared on every volume variant and reported in `appliedSettings`; `configure_pp_blend` takes `enabled`; `configure_bloom`, `configure_exposure`, `configure_lens_flare`, `configure_ssao`, `configure_grain` and `configure_chromatic_aberration` declare what they read.
- **Captures and light channels read their inputs.** Reflection captures accept the declared `name`, `rotation` and `settings`, `configure_capture_source` requires `captureSource` (an omitted one changed nothing and reported success), `set_light_channel` and `set_actor_light_channel` take `enabled` and `channels`, `configure_indirect_lighting_cache` applies `enabled`, and `configure_ray_traced_ao` declares `intensity`.
- **Spline points land where they are sent.** Every spline action finds its actor by `actorPath` when `actorName` is omitted. `create_spline_actor` and the road, river, fence, wall, cable and pipe templates place points given as `position` (they landed at the origin) with their tangents, rotation and scale; `add_spline_point` declares `index` and applies its tangents; `set_spline_point_tangents` applies a distinct `leaveTangent` instead of logging and dropping it. The templates take `name` over `actorName` and declare `materialPath`, `width` and `closedLoop`, and the scatter, spacing and randomization calls declare the inputs they read.
- **`play` starts where and how it is asked.** `startTime` and `loopMode` were declared and ignored; `play` now starts from `startTime` (seconds) and sets Sequencer's loop mode to `once` or `loop`. The reply's `startTime` used to be tick frames divided by the display rate reported as seconds; it is seconds now, and the frame fields are display-rate frames.
- **Cinematic tracks take the range and binding they read.** Every `add_cinematic_track` kind declares `startFrame`, `durationFrames`, `endFrame` and `rowIndex`, which the section range was already built from, and `bindingGuid` where the track is bound. A particle track resolves its binding from `actorName` like the other bound tracks instead of demanding `bindingGuid`; a camera shake track is bound to the `cameraName` camera, as Sequencer does, where it was added unbound; a material parameter track declares the `value` it requires (a number, or an `{r, g, b, a}` colour) and its `startFrame` key time; and a shot track honours `rowIndex`.
- **Shots and subsequences load their master from `path`.** `add_shot_track`, `add_subsequence` and `configure_shot_settings` required `masterSequencePath`, which nothing read; the master comes from `path` now. `configure_shot_settings` picks the shot by `sectionIndex`, `shotSequencePath` or name, and moves only the range fields sent, so a rename no longer resets the shot to frames 0 to 100.
- **`add_camera` makes a spawnable camera.** `spawnable: true` was declared and ignored, so a camera actor was always placed in the level; it now adds a camera owned by the sequence that exists only while the sequence plays, and returns its `bindingGuid`.
- **Camera actions take the settings they apply.** `create_cine_camera_actor` declares the lens, filmback and focus fields it applies; `configure_camera_settings` targets a level camera by `actorName` or `cameraName` and no longer asks for a sequence `path` it never loaded; the camera rig actions drop the `path` and `save` they never read.
- **Sequence `list` searches the folder it is given.** It always searched the whole Game folder; `path` now narrows the search to that folder and everything under it, and a path outside the mounted roots is refused.
- **Movie Render Queue jobs can be picked by name.** Every job action selects its job by `jobId` or `renderJobName`, as the plugin always did, where the contract demanded `jobId`. `create_render_job` declares the output settings it validates and applies (the ones `configure_output_settings` takes), and `start_render` declares `onlyJob` and a `timeoutMs` capped at the 300000 ms the transport waits.
- **Take Recorder applies its preset and stops on time.** `configure_take_sources` declared `takePresetPath` and never read it; the preset now seeds the take (it cannot be combined with a sequence path or `recordInto`). `start_recording` stops the take `duration` seconds after the countdown, where the recording ran until `stop_recording`, and declares the source fields it configures inline; `configure_take_sources` and `configure_recorded_tracks` declare the `frameRate` and `recordInto` they apply.
- **Replay, media and metadata actions declare what they read.** `configure_demo_settings` declares `replayName`, `play_demo` and `start_killcam` declare `additionalOptions`, `create_media_sound_component` requires the `actorName` and `mediaPlayerPath` it needs instead of an asset `path` it never read, `get_metadata` declares `key`, and `set_metadata` declares `key` and `value`.
- **Sounds play where they are sent.** `play_sound_at_location`, `spawn_sound_at_location`, `create_ambient_sound` and `create_reverb_zone` read `location`, `rotation` and `size` only as arrays, while the gateway passed the `{x, y, z}` objects the contract described, so the sound played at the world origin. The contract declares them as `[x, y, z]` arrays now, both doors convert an object into one, and every handler reads either shape. A sound created without an actor also landed at the origin whatever `location` said; it is placed now.
- **`play_sound_attached` plays, where it was asked.** The attach point was resolved and then dropped, the component sat inactive on the actor's root and nothing started it, yet the reply said "Sound attached". The sound now attaches to the named component or socket (a component name wins), plays, applies `volume`, `pitch` and `componentName`, and reports `attachedTo` and `playing`; an attach point the actor lacks is refused with `ATTACH_POINT_NOT_FOUND`.
- **Audio components apply what they are given.** `create_audio_component` read `volume` and `pitch` as strings, so numbers never applied, started playing whatever `autoPlay` said, and on an unknown `actorName` spawned a stray actor and reported success; numbers apply now, `autoPlay: false` leaves it stopped, and an unknown actor is refused with `ACTOR_NOT_FOUND`. `create_ambient_sound` loaded `attenuationPath` and `concurrencyPath` and then dropped them; the component uses both. `fade_sound_out` honours `targetVolume`, the fades read the declared `fadeInTime` and `fadeOutTime` ahead of `fadeTime`, and `set_sound_mix_class_override` reads its declared `fadeTime`.
- **`create_reverb_zone` encloses something.** The spawned audio volume had no brush, so the zone covered nothing and `size` was never used. It is built as a box of `size` (default 500 on each axis; every axis must be positive), and a `reverbEffect` that does not load is refused before anything spawns instead of leaving a zone with no reverb.
- **Sound class parents stick.** `set_class_parent` read `parentPath` while the contract sends `parentClass`, so every call through the gateway cleared the parent, and a wrong path was ignored under success. It reads `parentClass` now (omitting it clears the parent), refuses a parent that does not load or would make a cycle, and updates both classes' child lists so a mix modifier's `applyToChildren` reaches the child. `create_sound_class` applies its `parentClass` the same way; it used to drop it.
- **Attenuation edits read their contract names.** `configure_occlusion` read only the engine field names, so `enable: false` never arrived and every call turned occlusion on; it reads `enable`, `occlusionVolumeScale` and `occlusionFilterScale` now. `configure_spatialization` read `spatializationAlgorithm` while the contract sends `spatialization`, so the algorithm never changed. `configure_distance_attenuation` reset the falloff curve to Linear whenever `distanceAlgorithm` was omitted; it leaves it alone now. An unknown algorithm is refused in both.
- **Audio authoring does what its parameters say.** `create_submix_effect` read `effectType` and dropped it, so every submix had no effect; it now adds a Reverb, EQ or Dynamics preset to the submix's effect chain and refuses any other type. `create_dialogue_wave` fills its first context from `wavePath` and `speakerPath`, `create_dialogue_voice` refuses an unknown `gender` or `plurality` instead of keeping the default, `add_mix_modifier` sets the mix's `fadeInTime` and `fadeOutTime` it used to read and drop, and `configure_mix_eq` applies the single band fields on top of `eqSettings` instead of dropping them when both are sent.
- **Audio saves follow `save`.** Every audio asset edit declares `save` (default true). `create_metasound` read it and never saved, so a new MetaSound lived in memory only; it saves now. The MetaSound node, connect and default edits, the source effect chain and the submix creators saved regardless of it; `save: false` now keeps the change in memory. For MetaSound defaults, `defaultValue` wins over the legacy `floatValue`, `intValue`, `boolValue` and `stringValue` fields.
- **Wrong audio asset paths are refused.** A `wavePath`, `attenuationPath`, `concurrencyPath`, parent class, source effect preset, reverb effect, dialogue speaker or target voice that does not load used to be skipped while the call reported success (a Sound Cue with no wave, a component with no attenuation); each is now refused with a not-found code before anything is created. `set_audio_occlusion` without `soundPath` configured a throwaway object nothing used and reported success; it is refused.
- **`activate_effect` finds an effect by its asset.** It declared `assetPath` but nothing read it; it now activates the Niagara component playing that system, on `actorName` or anywhere in the running (else the editor) world, and declares `reset`. On `activate`, `deactivate`, `reset` and `advance_simulation` the declared `actorName` now wins over the undeclared `systemName`, and `spawn_niagara` labels the actor with `actorName` rather than an undeclared `name`.
- **`set_niagara_parameter` sets a system asset's default.** Given `assetPath` and no actor, it fell through to the actor form and answered "Actor '' not found". It now sets the user parameter's default on the asset, as `set_parameter_value` does, and saves it. Vector parameter values in both actions also take the `[x, y, z]` array form.
- **Niagara authoring refuses what it could not do.** A `templateEmitterPath` that is not an emitter authored an empty system and reported success; it is refused with `TEMPLATE_NOT_FOUND` before any asset is created. Every data interface action but the static-mesh one answered success with `dataInterfaceAdded: false`; they fail now. A renderer `materialPath` or `meshPath` that does not load is refused instead of skipped, and `set_emitter_properties` refuses any key other than `enabled` instead of ignoring it under an "updated" reply.
- **Light effects keep engine defaults.** `add_light_renderer_module` rewrote the renderer's radius scale to 100 whenever `lightRadius` was omitted; it leaves it alone now. `create_dynamic_light` without `intensity` made a dark light (intensity 0); it keeps the engine intensity, no longer refuses a call without `location` (the origin by default), and takes `name` over the older `lightName`.
- **Effect colours and scales take both shapes.** `debug_shape` drew white for a colour given as an `{r, g, b, a}` object and ignored an object `boxSize`, and `niagara` and `spawn_niagara` ignored a `scale` given as `{x, y, z}`; objects and arrays both work now. The parameters these actions read are declared: `rotation` and `scale` on the spawners (`rotation` and `systemPath` on `particle`), the per-shape `debug_shape` fields (`thickness`, `boxSize`, `endLocation`, `direction`, `length`, `angle`, `halfHeight`), `templateEmitterPath` and `save` on the procedural creators, and `emitterName` on `add_emitter_to_system` and `set_parameter_value`.
- **GAS edits refuse what they cannot apply.** An unknown `activationPolicy`, `instancingPolicy`, `replicationMode` or stacking policy became a default (or left the field alone) while the reply echoed the caller's value as applied, and an unregistered gameplay tag in `set_effect_tags` or `add_effect_cue` was skipped under "Effect tags set" or "Cue added". Each is refused now with nothing changed (`GAMEPLAY_TAG_NOT_REGISTERED` for tags), and the reply reports the value actually written. An empty cost or cooldown effect path is refused instead of answering success with nothing assigned, `set_effect_stacking` without `stackLimitCount` keeps the current limit instead of resetting it to 1, and `set_modifier_magnitude` refuses a negative `modifierIndex`.
- **GAS parameters mean what `describe` says.** `activationPolicy` lists the real net execution policies (LocalOnly, LocalPredicted, ServerOnly, ServerInitiated) instead of input triggers the handler never knew; `create_gameplay_effect` declares the `duration` and `period` it applies; and the declared names (`activationPolicy`, `instancingPolicy`, `modifierOperation`, `modifierMagnitude`, `magnitudeCalculationType`, `cancelAbilitiesWithTag`, `blockAbilitiesWithTag`) win when a legacy spelling is sent beside them.
- **`full_editor_window` always captures the main editor frame.** With the main window minimized, the capture fell back to whichever window was usable and photographed the Message Log. Without a `window` selector, the capture is now always the main frame. A minimized frame is restored without taking focus or moving the cursor. When it cannot be restored, the call fails with `EDITOR_WINDOW_MINIMIZED` instead of capturing another window, and the reply says whether the captured window is the main frame (`mainWindow`). `path` was declared for every screenshot mode but honoured only by `game_viewport`. Every mode now saves into it, inside the project. `game_viewport` returned base64 by default against its own schema; like the other modes, it now returns it only when `returnBase64: true` is passed.
- **Bookmarks and editor modes do what they report.** `create_bookmark` and `jump_to_bookmark` sent console commands that do not exist, did nothing, and answered success. They now drive the level viewport's numbered bookmark slots. `id` is the slot number and is required. A non-numeric or out-of-range `id` is refused instead of silently meaning slot 0, and jumping to an empty slot answers `BOOKMARK_NOT_FOUND`. `bookmarkName` and `description` are gone, because Unreal bookmarks have no names. `set_editor_mode` ran `mode <X>`, which only sets grid and snap options, and reported "Editor mode set" for any word. It now activates the mode by id (a bare name such as `landscape` is also tried as `EM_Landscape`) and fails with `MODE_NOT_ACTIVATED` when the mode is not active afterwards.
- **`control_editor` stats, immersive mode, replay recording and `close_asset` stop reporting unapplied changes.** Stat commands toggle, so `show_stats` could hide a stat that was already on screen. `show_stats` and `hide_stats` now read `stat` and leave a stat that is already in the wanted state alone. `hide_stats` with no stat hides them all, and a missing viewport answers `NO_VIEWPORT`. `set_immersive_mode` with no level viewport reported success with `applied: false`; it now fails. `start_recording` answered "Recording started" in the editor world, which has no game to record. It now needs Play-In-Editor (`NO_ACTIVE_SESSION`) and fails with `RECORDING_NOT_STARTED` when no recorder started. `frameRate` sets the replay rate, `durationSeconds` stops the recording after that much game time, and the unread `metadata` is gone. `close_asset` loaded the asset just to close it and answered "closed" whether or not an editor was open. It now finds the asset in memory, answers `EDITOR_NOT_OPEN` when no editor was open, and reports `editorsClosed`. `simulate_input` declares `z` for Axis3D actions.
- **`inspect` property calls take `propertyPath` alone, and the typed detail actions check their target.** `get_property` and `set_property` required `propertyName`, so the schema refused the declared `propertyPath` before the handler, which already read it, ever saw the call. Either name now works. A call with no target is refused with a message naming `objectPath`, `actorName`, `name` and `blueprintPath`. `get_material_details`, `get_mesh_details` and `get_texture_details` used to answer any object with the generic `inspect_object` view; they now fail with `TYPE_MISMATCH` when the object is not the type they read. `find_by_class` with no `className` or `classPath` answered success with 0 objects; it is now `INVALID_ARGUMENT`. Parameters the handlers already read are now declared: `markDirty` on `set_property`, a `properties` bag on `set_component_property`, `detailed` and `propertyNames` on the detail actions, `format` and `outputPath` on `export`, `limit` and `offset` on `list_objects`, `actorNames` on `add_tag` and `delete_object`, and `componentName` and `propertyNames` on `get_actor_details`.
- **`system_control` performance and display settings apply what they report.** `set_scalability` ignored `category` and moved every group. It now sets one group, or all of them when no category or `Overall` is given, and it requires `level`. `show_stats` toggled, so `enabled: false` could turn an overlay on; it now sets the state, as `show_fps` does. `start_profiling` ignored `duration`; the capture now stops after that many seconds. `type`, which the capture cannot narrow, is gone. `set_fullscreen` reads `windowed` as the inverse of `enabled` and refuses the two when they contradict. `configure_occlusion_culling` declares `slop` and `minScreenRadius`, which it applies. On `configure_world_partition` and `optimize_draw_calls`, the declared `streamingDistance` and `enableInstancing` now win over the older spellings.
- **`system_control` widget, validation and log actions read what they declare.** `show_widget` now honours `duration` (0.5 to 60 seconds) instead of a fixed 4. It stops declaring `widgetPath`, since it can only show an editor notification. `create_widget` reads `widgetPath` for its name and folder. An unknown `widgetType` is now refused instead of silently becoming a UserWidget, and an existing asset that is not a Widget Blueprint is refused with `ASSET_TYPE_MISMATCH`. `validate_assets` dropped `path` when `assetPath` was also given, and passed a folder just for existing. It now validates both, and it loads every asset in a folder, listing the ones that fail. `read_log` refuses `category` and `minVerbosity` with a log-file source, where they were silently ignored. `play_sound` declares `startTime`. `screenshot` declares `keepFile` and `path`. `subscribe` and `unsubscribe` drop `channels`, since the stream always carries every category. `spawn_category` declares `enabled`: it shows or hides a Gameplay Debugger category.
- **Insights trace actions honour their inputs.** `capture_insights_trace` records to a file only: it quietly turned a network `connectionType` into a file trace, and it no longer declares `connectionType` or `tracePath` (a network trace starts with `start_session`; a direct plugin call with a network type is refused with `INVALID_CONNECTION_TYPE`). `write_snapshot` now reads its declared `snapshotPath` before the trace-file aliases. `start_session` declares `traceFile`, `overwrite` and `launchViewer`, and `send_snapshot` declares `host` and `port`.
- **Replication settings refuse what they cannot apply.** `set_replication_condition`, `set_property_replicated`, `set_net_dormancy` and `set_net_role` turned an unknown name into a default (COND_None, DORM_Never, ROLE_None) while the reply echoed the caller's value. They now answer `INVALID_ARGUMENT` and list the accepted names. `set_inventory_replication` does the same for `replicationCondition`, and it fails when the Blueprint has none of the inventory variables. `set_property_replicated` now applies its declared `condition`. `set_net_role` stored nothing, because a Blueprint has no role. It now says what it does: `ROLE_None` turns replication off, the two proxy roles turn it on, and `ROLE_Authority` is refused. `set_owner` no longer clears the owner when `ownerActorName` matches no actor. `set_autonomous_proxy` and `configure_push_model` fail on a Blueprint with no replicated variable, and turning autonomous proxy off now undoes only `COND_AutonomousOnly` instead of resetting every condition. `configure_replication_graph` forced `netLoadOnClient` on when it was omitted, and it only logged `spatiallyLoaded`. It now writes just the fields that were sent, and `replicationPolicy`, which nothing applied, is gone. `create_rpc_function` used to create a function that was not an RPC for any unknown `rpcType`; it now refuses anything but Server, Client or NetMulticast.
- **Prediction and net driver settings reach the objects that use them.** `configure_client_prediction`, `configure_server_correction` and `configure_movement_prediction` saved any Blueprint and answered success. They now refuse a Blueprint that is not a Character. `networkSmoothingMode` is applied, and the misapplied `predictionThreshold` and the unused `correctionThreshold` are gone. `configure_net_driver` wrote only to a running net driver, which the editor world normally lacks. It now writes the game net driver's class defaults and `DefaultEngine.ini`, plus the running driver when there is one.
- **Game Framework actions refuse what they cannot set.** `configure_game_rules` wrote nothing on a GameModeBase child, which has no `bDelayedStart`, and still replied success. It now answers `NOT_SUPPORTED`, requires `bDelayedStart`, and reads the value back after the compile. `set_respawn_rules` refuses a GameModeBase child the same way instead of only logging. `configure_spectating` fails when the spectator class does not load or cannot be set, and `allowSpectating` and `spectatorViewMode`, which nothing read, are gone. The `create_*` actions refuse an unloadable or unrelated `parentClass` instead of falling back to the default parent. `create_game_mode` checks every class override before creating anything, and a refused override is no longer reported as success. A class setter refuses a class that is not the kind its slot holds (an Actor as the HUD, say). `get_game_framework_info` answers `NOT_FOUND` for a Blueprint that does not load.
- **Enhanced Input and session calls stop reporting changes that did not happen.** `set_input_trigger` and `set_input_modifier` stacked a new instance on every call, so a second Negate cancelled the first. A trigger or modifier of that class that is already present is now kept, and the reply says `alreadyPresent`. `enable_input_mapping` answered `enabled: true` outside PIE with nothing enabled. It now refuses with `PIE_NOT_RUNNING` and checks that the context was actually added. `map_input_action` declares `triggerType` and `modifierType`, and `create_input_mapping_context` drops `priority`, which a mapping context does not have. `mute_player` wrote the mute to a set nothing read and called it success. With no voice system, it now answers `NOT_SUPPORTED`. `host_lan_server` drops the never-applied `serverName`, puts a `?` before `travelOptions`, and says that nothing is hosted without `executeTravel`.
- **Behavior Tree edits reach the node they name.** `add_decorator` could attach only to the tree root and refused `parentNodeId`. It now attaches to the composite or task that `parentNodeId` names, or to the root when none is given, and the reply reports `attachedTo`. It also builds the remaining AIModule decorator types (TimeLimit, ForceSuccess, ConeCheck and the others) by class. `configure_bt_node` now sets a Blackboard key selector from a plain key name. It checks that the key exists in the tree's Blackboard and is a type the node accepts. Before, a plain name like `TargetActor` failed.
- **Navigation, perception and smart-object settings apply what they report.** `configure_nav_area_cost` reset the cost to 1 when `areaCost` was omitted, and it called `fixedAreaEnteringCost` read-only. It now writes only the costs given, sets the entering cost, and saves a Blueprint NavArea. A native NavArea is reported as changed for this session only. An unknown `areaClass` on the nav modifier actions is refused instead of silently becoming the default area. `set_ai_perception` applies `Touch` and `None` as `dominantSense` and declares `hearingRange`, `enableDamage` and `dominantSense`. `set_ai_movement` declares the six movement fields it reads, and `set_nav_area_class` declares `componentName`. `configure_slot_behavior` dropped an unregistered activity tag and still succeeded. It now refuses with `GAMEPLAY_TAG_NOT_REGISTERED`, and `activityTags` is declared. `create_behavior_tree` now prefers the declared `path` over `savePath`.
- **Hitboxes bind to their bone, and weapons get their mesh.** `setup_hitbox_component` created the hitbox but never attached it to `hitboxBoneName`. It now hangs the hitbox under the Blueprint's skeletal mesh (its own or an inherited one, such as a Character's) at that bone. A bone on a Blueprint with no skeletal mesh is refused with `NO_SKELETAL_MESH`, and an unknown `hitboxType` is refused. `create_weapon_blueprint` declares `weaponMeshPath`. A path that does not load now fails the call, where before the weapon was created without its mesh.
- **Modeling actions declare the settings they read.** Knobs the handlers already applied could not be sent: outset `distance`, bevel and chamfer `distance` and `segments`, loft spline, `segments` and `cap`, sweep `steps` and `cap`, smooth and relax `strength`, the cylindrify, lattice and displace `axis`, `merge_vertices` `weldDistance`, remesh `targetTriangleCount`, `generate_collision` `maxHullCount`, the torus `angle` and the disc hole (`innerRadius`). `generate_collision` fell back to its default shape for an unknown `collisionType` while echoing it; it now refuses it. `simplify_collision` decimated the render mesh along with the collision; it now simplifies a copy. The declared names now win over their aliases (extrude `amount`, capsule `radialSegments`, plane `depth`, disc `radius`, `extrude_along_spline` `segments`). A rounded bevel on a face selection now gets its `segments`, and `segments` is refused before UE 5.4.
- **PCG node edits declare `save` and `settings`.** Every typed node action now declares `settings` and `save`, and the create and connect actions declare `save`, so the gateway no longer refuses them. `set_pcg_node_settings` declares `meshPath`, `actorClass`, `x` and `y`. An explicit `title` now wins over the `nodeName` used to find the node, which had renamed the node with its own lookup name. `set_pcg_partition_grid_size` no longer declares the `graphPath` it never read.
- **`call_actor_function` runs Blueprint events on placed actors.** A custom event on an actor in the editor world answered success and never ran, because the engine drops script calls there unless script execution is allowed; the call now runs under `FEditorScriptExecutionGuard`.
- **Widget names that would break the Blueprint are refused.** A `slotName` held by an inherited property (`DisplayLabel`), a variable, a function or a widget of another class answers `NAME_CONFLICT` with a free name, and nothing is built. It used to fail the compile and leave the widget in the tree. A widget that still breaks the compile is removed again, and an unnamed add next to a taken default name gets a free name instead of re-configuring the earlier widget.
- **`reparent_widget` keeps the slot layout.** Padding, alignment and size reset to defaults on every move; each layout field now carries over, and a parent inside the widget's own subtree is refused (`INVALID_PARENT`) instead of building a cycle.
- **`set_style` replies match its schema.** After a successful write the reply failed with `OUTPUT_SCHEMA_VIOLATION` because `applied` carried field names; those moved to `appliedFields`, and `applied` is the layout read back.
- **Quantities named after tokens are no longer redacted.** A `TokensSaved` variable read back as `[REDACTED]` in pin defaults and Blueprint defaults; a key ending in a quantity word (saved, spent, earned, balance, cost...) is a value, not a secret.
- **Material batch parameter steps accept `name`.** `add_scalar_parameter` in `build_material_graph` refused the `name` that describe documents.
- **Material `delete_node` saves.** It recompiled but left the material unsaved, so an editor restart brought the node back; it saves by default now (`save: false` skips it) and reports `saved`.
- **Renames no longer cancel themselves.** The engine asks OkCancel before renaming an asset a native class default still points at, and an unattended editor answers Cancel: a default map, a game mode, an input mapping context, or any map ever opened (the level editor keeps a camera per map) made `rename`, `move` and `bulk_rename` fail. Those references now follow the asset to its new path and the changed config files are saved; hard references are named under `blockingReferences`.
- **`bulk_rename` reports what happened.** A failed rename answered `renamed: 5` with the planned list, and `oldPath` showed the new path; it now answers 0 with the reason, and `rename` no longer blames "exists or locked" for every failure (`DESTINATION_EXISTS` when it is taken).
- **`play` answers when the game is running.** The session starts on a later editor tick, so the next call (`set_game_speed`, `simulate_input`) met no play world; the reply now waits until the play world has begun and names it (`pieWorld`).
- **`get_project_settings`** has a `packaging` category, finds a settings class that moved modules by its name (`/Script/UnrealEd.ProjectPackagingSettings`), and a keyed read answers only that key instead of the whole section.
- **`find_text` skips engine plumbing.** A one-letter search in a level returned 2,435 net-driver, collision-profile, sprite and folder hits; those properties are no longer searched.
- **Folder moves no longer crash the editor.** The redirector fixup went through the engine's `FixupReferencers`, whose report dialog asserts in an unattended editor; it now runs without UI, and `bulk_delete`'s redirector pass shares it.
- **Moves keep `MapsToCook`.** Its entries are plain package names, so a moved map silently dropped out of every packaged build; they now follow the rename.
- **`search_assets` takes any short class name** (`ObjectRedirector`, `InputMappingContext`), not only thirty listed ones.
- **Component template edits reach placed actors.** `edit_scs` `set_transform` and `set_property` changed only the Blueprint while placed actors kept the old value; every placed actor that still had the old value now takes the new one (`instancesUpdated`), as in the Blueprint editor.
- **Tool descriptions.** `move` and `rename` say they take folders, `edit_scs` documents its batch `transform` object, and `classNames` names both accepted forms.

</details>

<details>
<summary><b>⚠️ Migration</b></summary>

- `set_*_class` on `manage_networking`: pass `makeDefault: true` to keep making the game mode the project default. These actions never change the open level's GameMode Override any more; set it in the level's World Settings.
- `edit_widget_blueprint` with `edit: "preview"`: call `preview_widget` instead. The action name `manage_blueprint.preview_widget` works as before.
- `configure_net_serialization` is removed. It refused every call: custom net serialization is a C++ `NetSerialize` on a USTRUCT, which no Blueprint setting reaches.
- `control_actor.list` with `summary: true`: `byClass`, `byTag` and `byFolder` are arrays of `{name, count}` rows sorted by name, no longer `{name: count}` objects.
- Send the parameter names `describe` lists: a stdio call that relied on the removed TypeScript conversions is refused with the declared name in its message.
- A removed action answers `UNKNOWN_ACTION` with a suggestion; set a `PlayerStart`'s tag with `control_actor` `set_property` (`PlayerStartTag`), and build HUD and menu widgets with `manage_blueprint` widget authoring.
- More edits save by default; pass `save: false` to keep one in memory: `modify_scs`, every `manage_game_framework` write, every DataTable write (`create_data_table`, `create_row_struct`, both row-struct binders, row add, update, delete, import and clear), every struct edit, `manage_asset` `import`, `set_two_sided`, the Niagara graph edits (`add_niagara_module`, `connect_niagara_pins`, `remove_niagara_node`) and `create_metasound`. The GAS ability and effect edits compile and save their Blueprint on every call; one that does not compile is left unsaved with `COMPILE_FAILED`.
- `manage_sequence`: send the master sequence as `path` to `add_shot_track`, `add_subsequence` and `configure_shot_settings` (`shotSequencePath` now picks a shot instead of naming the sequence to load); `add_section` `start` and `end` are display-rate frames, not ticks; `set_playback_speed` writes the play rate into the level sequence actors that play the sequence.
- Partial results fail the call. `manage_asset` `delete` keeps an asset that something outside the delete still references and fails with `ASSET_REFERENCED`, listing it under `referencedPaths` (pass `force: true` to delete it anyway). A `control_actor` `delete` that removes only some actors answers `DELETE_PARTIAL`; an `add_component` whose properties or `meshPath` partly fail answers `PARTIAL_FAILURE`, or `PROPERTY_CONVERSION_FAILED` when none applied. The deleted actors and the added component stay, and the reply lists them.
- Parameters that were declared and never read are refused as `UNDECLARED_PARAMETER`:
  - Blueprints: `connect_pins` `linkedTo`; `probe_handle` `operations`, which never ran (batch graph edits with `edit_graph` `batch`).
  - Levels: `manage_level` `save` `levelName`, `save_as` `destinationPath` and `add_sublevel` `parentPath`; `get_level_structure_info` `levelPath`; `open_level_blueprint` `save`.
  - Animation: `targetSkeleton` on the asset creators (use `skeletonPath`), `assetPath` on the AnimGraph edits (use `blueprintPath`), `path` on `create_state_machine` and `create_blend_tree`, `meshPath` on `create_ik_rig` (use `skeletalMeshPath`), `skeletalMeshPath` on `setup_ragdoll`, `length` on `set_section_timing` and `assetPath` on `setup_ik` (use `path`).
  - Sequencer: `masterSequencePath` (use `path`), `path` and `cameraActorName` on `configure_camera_settings`, `path` and `save` on the camera rigs, `skeletalMeshPath` on the skeletal animation track, `path` on `create_media_sound_component`.
  - Effects: `path` and the debug-shape options on `particle`, `path` on `niagara` and `create_volumetric_fog`, `reset` on `reset`, `name` on `add_niagara_module`, `emitterName` on `create_niagara_emitter`.
  - Audio: the free `properties` object on `add_cue_node`, `add_source_effect`, `configure_mix_eq`, `create_reverb_effect`, `create_sound_class`, `create_sound_mix` and `set_class_properties` (send the named fields); `sourceNode`, `sourcePin`, `targetNode` and `targetPin` on `connect_metasound_nodes`, which the Changed entry above lists as read aliases (send `sourceNodeId`, `sourceOutputName`, `targetNodeId` and `targetInputName`).
  - GAS: `attributeType` on `add_attribute`.
  - Materials, textures and structs: `add_static_switch_parameter` `value` (use `defaultValue`), `add_noise` `octaves` (use `levels`), `set_streaming_priority` `streamingPriority`, `analyze_graph` `maxDepth`, `list_structs` `searchScope`.
  - Landscape, environment and lighting: `import_heightmap` `path` (use `heightmapPath`), `path` on `configure_landscape_lod`, the landscape `generate_lods`, `create_sky_sphere` and `create_fog_volume`, `set_time_of_day` `propertyValue`, `intensity` on `configure_sky_atmosphere`, `density` on `configure_volumetric_cloud` and the rain and snow particles, `speed` on `configure_water_waves`, `create_fence_spline` `spacing`, `configure_shadows` `cascadedShadows`, `build_lighting_quality` `settings`, `configure_screen_percentage` `actorName`.
  - Editor, inspect and system: `bookmarkName` and `description` on bookmarks, `metadata` on `control_editor` `start_recording`, `path` on `open_asset` and `close_asset`, `actorName` on `set_camera`, `type` on `start_profiling` and `show_stats`, `channels` on `subscribe` and `unsubscribe`, `widgetPath` on `show_widget`, `mergeActors` and `enableInstancing` on `merge_actors`, `tracePath` and `connectionType` on `capture_insights_trace`, `objectPath` on `inspect_cdo`, and the target parameters on `get_level_details`.
  - Networking and game framework: `replicationPolicy` on `configure_replication_graph`, `predictionThreshold` and `correctionThreshold` on the prediction actions, `allowSpectating` and `spectatorViewMode` on `configure_spectating`, `priority` on `create_input_mapping_context`, `serverName` on `host_lan_server`.
  - PCG: `graphPath` on `set_pcg_partition_grid_size`.
- Newly required parameters and narrowed values:
  - Blueprints and widgets: `add_component` and `add_scs_component` require `componentName`, `set_node_property` requires `propertyValue`, Blueprint `set_metadata` requires `metadata`; `create_widget_blueprint` refuses a `parentClass` that is not a UserWidget class (`INVALID_PARENT_CLASS`); widget animation `trackType` no longer accepts `material`, which never produced a track (use `opacity`, `color` or a transform type).
  - Character and animation: `set_walk_speed`, `set_jump_height`, `set_gravity_scale`, `set_ground_friction` and `set_braking_deceleration` require their value; `set_bone_parent` requires `parentBoneName`; `add_montage_slot` and `add_blend_sample` require `animationPath`; `create_montage` needs `skeletonPath` or `animationPath`; the AnimGraph edits require `blueprintPath`; `parentClass` must be a Character (`create_character_blueprint`) or an AnimInstance (`create_animation_blueprint`).
  - Levels and volumes: `streamingMethod` no longer takes `Disabled`; `create_volume` refuses a parameter its volume class ignores.
  - Sequencer: `add_shot_track` needs `shotSequencePath`, a material parameter track needs `value`, and `play` refuses `loopMode: "pingpong"`.
  - Effects: `set_emitter_properties` needs `emitterProperties` holding only `enabled`; `set_niagara_parameter` needs `parameterName` and `actorName` or `assetPath`; `particle` needs `preset` or `systemPath`.
  - GAS: `configure_asc` needs `replicationMode`, `set_ability_costs` `costEffectPath` and `set_ability_cooldown` `cooldownEffectPath`; `activationPolicy` takes LocalOnly, LocalPredicted, ServerOnly or ServerInitiated (OnInputPressed and the other old values are refused).
  - Editor and system: `set_scalability` requires `level`; `create_bookmark` and `jump_to_bookmark` require a numeric `id`; `control_editor` `start_recording` and `enable_input_mapping` need a running PIE session; `generate_collision` accepts only box, sphere, capsule, convex or convex_decomposition.
- Calls that changed nothing and reported success now fail:
  - Levels: `import_level` onto an existing destination without `overwrite` (`DESTINATION_EXISTS`), `save` or `save_as` with a `levelPath` that is not the open level (`LEVEL_NOT_LOADED`), `add_sublevel` with a `parentLevel` that is not open (`PARENT_LEVEL_NOT_LOADED`), `enable_world_partition` with `bEnableWorldPartition: false` on a partitioned level (`NOT_SUPPORTED`).
  - Animation: `add_physics_constraint` on a pair that already has a constraint (`CONSTRAINT_EXISTS`; use `set_physics_constraint` or `configure_constraint_limits`).
  - Materials and textures: a material node knob the node does not have, `delete_node` where no id matches, `create_noise_texture` with an unknown `noiseType`.
  - Landscape and environment: `sculpt` with a tool other than Raise, Lower or Flatten, `create_landscape` with a section size the engine cannot build, `remove_foliage` naming nothing, a post-process variant with no value to write, `configure_capture_source` without `captureSource`.

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
