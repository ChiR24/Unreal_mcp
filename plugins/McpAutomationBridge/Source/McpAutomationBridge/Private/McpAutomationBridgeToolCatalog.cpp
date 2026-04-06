#include "McpAutomationBridgeToolCatalog.h"

namespace
{
    /** @brief Contract: builds a subaction catalog row with the stable name and summary that runtime diagnostics publish. */
    FMcpAutomationBridgeToolSubActionCatalogEntry MakeSubAction(const TCHAR *Name,
                                                                const TCHAR *Summary,
                                                                bool bEditorOnly = true,
                                                                bool bRequiresLiveEditor = false,
                                                                bool bRequiresAssetEditor = false,
                                                                const TCHAR *InteractionModel = TEXT("semantic_read"),
                                                                const TCHAR *LimitationNote = TEXT(""))
    {
        FMcpAutomationBridgeToolSubActionCatalogEntry Entry;
        Entry.Name = Name;
        Entry.Summary = Summary;
        Entry.bEditorOnly = bEditorOnly;
        Entry.bRequiresLiveEditor = bRequiresLiveEditor;
        Entry.bRequiresAssetEditor = bRequiresAssetEditor;
        Entry.InteractionModel = InteractionModel;
        Entry.LimitationNote = LimitationNote;
        return Entry;
    }

    /** @brief Contract: builds one tool catalog row that every runtime metadata view reads from. */
    FMcpAutomationBridgeToolCatalogEntry MakeTool(
        const TCHAR *ToolName,
        const TCHAR *Category,
        const TCHAR *Summary,
        std::initializer_list<FMcpAutomationBridgeToolSubActionCatalogEntry> SubActions = {},
        bool bPublic = true)
    {
        FMcpAutomationBridgeToolCatalogEntry Entry;
        Entry.ToolName = ToolName;
        Entry.Category = Category;
        Entry.Summary = Summary;
        Entry.bPublic = bPublic;
        Entry.SubActions = TArray<FMcpAutomationBridgeToolSubActionCatalogEntry>(SubActions);
        return Entry;
    }
}

/** @brief Contract: returns the canonical bridge catalog that defines the public MCP surface and internal validation source. */
const TArray<FMcpAutomationBridgeToolCatalogEntry> &GetMcpAutomationBridgeToolCatalog()
{
    static const TArray<FMcpAutomationBridgeToolCatalogEntry> Catalog = {
        MakeTool(TEXT("control_actor"), TEXT("Core Editor"), TEXT("Spawn, transform, tag, and inspect actors and components inside the editor.")),
        MakeTool(TEXT("control_editor"), TEXT("Core Editor"), TEXT("Drive editor viewport, screenshots, PIE controls, semantic graph navigation, and simulated input."), {MakeSubAction(TEXT("screenshot"), TEXT("Capture viewport or editor-window screenshots."), true, true, false, TEXT("semantic_capture"), TEXT("Explicit editor targeting is safer than the viewport default when operators need asset-editor evidence.")), MakeSubAction(TEXT("simulate_input"), TEXT("Send keyboard, text, mouse, wheel, and drag input into a target tab or window."), true, true, false, TEXT("low_level_input"), TEXT("Input transport alone does not prove semantic editor-state changes such as marquee selection expansion.")), MakeSubAction(TEXT("fit_blueprint_graph"), TEXT("Frame the active Blueprint graph or its current selection in view."), true, true, true, TEXT("semantic_view"), TEXT("Requires a live Blueprint editor surface for graph navigation.")), MakeSubAction(TEXT("set_blueprint_graph_view"), TEXT("Pan or zoom the active Blueprint graph to a semantic target view."), true, true, true, TEXT("semantic_view"), TEXT("Requires a live Blueprint editor surface for graph navigation.")), MakeSubAction(TEXT("jump_to_blueprint_node"), TEXT("Jump the Blueprint editor to a node by guid, name, or title."), true, true, true, TEXT("semantic_selection"), TEXT("Requires a live Blueprint editor surface and a resolvable node selector.")), MakeSubAction(TEXT("capture_blueprint_graph_review"), TEXT("Capture a readable Blueprint or Widget Blueprint graph review from the resolved editor window."), true, true, true, TEXT("semantic_capture"), TEXT("Captures the resolved Blueprint editor window after semantic graph and optional node selection; it does not fall back to a viewport screenshot.")), MakeSubAction(TEXT("set_widget_blueprint_mode"), TEXT("Switch a Widget Blueprint editor between Designer and Graph modes."), true, true, true, TEXT("semantic_mode"), TEXT("Requires a live Widget Blueprint editor surface and may open the asset editor when the target is not already open.")), MakeSubAction(TEXT("fit_widget_designer"), TEXT("Frame the active Widget Blueprint designer canvas in view."), true, true, true, TEXT("semantic_view"), TEXT("Requires a live Widget Blueprint Designer surface.")), MakeSubAction(TEXT("set_widget_designer_view"), TEXT("Pan the active Widget Blueprint designer to a semantic target view."), true, true, true, TEXT("semantic_view"), TEXT("Requires a live Widget Blueprint Designer surface and does not prove marquee selection behavior by itself.")), MakeSubAction(TEXT("select_widget_in_designer"), TEXT("Select, append, or toggle a widget in the active Widget Blueprint designer."), true, true, true, TEXT("semantic_selection"), TEXT("Requires a live Widget Blueprint Designer surface and a resolvable widget selector; use appendOrToggle for deterministic additive or toggle selection without ctrl-click.")), MakeSubAction(TEXT("select_widgets_in_designer_rect"), TEXT("Select widgets in the active Widget Blueprint designer by a semantic rectangle."), true, true, true, TEXT("semantic_selection"), TEXT("Requires live Widget Blueprint Designer bounds and keeps the raw native marquee ceiling truthful by resolving rectangle intent semantically instead of replaying drag-box input.")), MakeSubAction(TEXT("focus_editor_surface"), TEXT("Focus a Blueprint or Widget Blueprint editor surface before keyboard or text input."), true, true, true, TEXT("semantic_focus"), TEXT("Requires a live Blueprint or Widget Blueprint editor surface and does not open missing targets implicitly."))}),
        MakeTool(TEXT("manage_asset"), TEXT("Content"), TEXT("Browse, create, duplicate, move, rename, import, and delete Unreal assets.")),
        MakeTool(TEXT("manage_level"), TEXT("World"), TEXT("Load, save, validate, duplicate, and stream levels and level assets.")),
        MakeTool(TEXT("manage_sequence"), TEXT("Cinematics"), TEXT("Create and edit level sequences, bindings, tracks, and playback state.")),
        MakeTool(TEXT("manage_ui"), TEXT("UI Automation"), TEXT("Discover UI targets, visible windows, commands, menus, and editor utility widgets."), {MakeSubAction(TEXT("list_ui_targets"), TEXT("List bridge-discovered UI targets, commands, and known menus.")), MakeSubAction(TEXT("list_visible_windows"), TEXT("List current Slate windows with outer and client bounds.")), MakeSubAction(TEXT("resolve_ui_target"), TEXT("Resolve a UI target into live target status and recovery diagnostics before acting on it."), true, true, false, TEXT("semantic_read"), TEXT("Window titles and tab ids remain live editor state and can still require reopen or re-resolution.")), MakeSubAction(TEXT("open_ui_target"), TEXT("Open a discovered target by identifier or tab id.")), MakeSubAction(TEXT("register_editor_command"), TEXT("Register a bridge-owned command for a session.")), MakeSubAction(TEXT("add_menu_entry"), TEXT("Attach a registered command to a ToolMenus menu or toolbar."))}),
        MakeTool(TEXT("manage_input"), TEXT("UI Automation"), TEXT("Create and configure input mappings and related editor input assets.")),
        MakeTool(TEXT("manage_blueprint"), TEXT("Blueprint Authoring"), TEXT("Create and modify Blueprint assets, classes, components, and public inspection metadata."), {MakeSubAction(TEXT("get_graph_details"), TEXT("Inspect the nodes, pins, and metadata for a Blueprint graph."), true, false, false, TEXT("semantic_read"), TEXT("Graph inventory remains inventory-level by default; deeper pin topology still requires follow-up inspection.")), MakeSubAction(TEXT("get_graph_review_summary"), TEXT("Return a bounded first-pass summary of a Blueprint graph for review."), true, false, false, TEXT("semantic_read"), TEXT("Use the bounded graph review summary before raw node-detail batches on large Blueprint graphs.")), MakeSubAction(TEXT("get_node_details_batch"), TEXT("Inspect a bounded batch of Blueprint node details using caller-supplied node ids."), true, false, false, TEXT("semantic_read"), TEXT("Use graph details first to inventory node ids before paging through large graphs.")), MakeSubAction(TEXT("get_pin_details"), TEXT("Inspect pins for a Blueprint node using semantic selectors such as node guid compatibility."), true, false, false, TEXT("semantic_read"), TEXT("Use graph details first to narrow large graphs before requesting pin-level follow-up."))}),
        MakeTool(TEXT("manage_blueprint_graph"), TEXT("Graph Editing"), TEXT("Edit Blueprint graphs, nodes, pins, and execution flow.")),
        MakeTool(TEXT("manage_material_authoring"), TEXT("Material Authoring"), TEXT("Create materials, material instances, parameters, and render targets.")),
        MakeTool(TEXT("manage_material_graph"), TEXT("Graph Editing"), TEXT("Edit material expression graphs, nodes, and pin connections."), {MakeSubAction(TEXT("add_node"), TEXT("Add a material expression node to a material graph.")), MakeSubAction(TEXT("connect_nodes"), TEXT("Connect two material expressions or route an expression into a material input.")), MakeSubAction(TEXT("remove_node"), TEXT("Remove a material expression from the graph.")), MakeSubAction(TEXT("get_node_details"), TEXT("Inspect nodes and current graph state."))}),
        MakeTool(TEXT("manage_texture"), TEXT("Material Authoring"), TEXT("Create, inspect, and update texture assets and texture metadata.")),
        MakeTool(TEXT("manage_geometry"), TEXT("World"), TEXT("Author geometry, dynamic meshes, and related mesh-processing workflows.")),
        MakeTool(TEXT("manage_world_partition"), TEXT("World"), TEXT("Inspect and control World Partition data and loaded cells.")),
        MakeTool(TEXT("manage_render"), TEXT("Core Editor"), TEXT("Drive render-oriented editor actions and rendering utilities.")),
        MakeTool(TEXT("manage_skeleton"), TEXT("Animation"), TEXT("Inspect and modify skeletons, sockets, bones, and physics assets.")),
        MakeTool(TEXT("animation_physics"), TEXT("Animation"), TEXT("Drive animation, ragdoll, and animation-physics workflows.")),
        MakeTool(TEXT("manage_animation_authoring"), TEXT("Animation"), TEXT("Author animation assets, tracks, and related animation workflows.")),
        MakeTool(TEXT("manage_audio"), TEXT("Audio"), TEXT("Control audio runtime/editor utilities, sound playback, and audio assets.")),
        MakeTool(TEXT("manage_audio_authoring"), TEXT("Audio"), TEXT("Author sound cues, MetaSounds, attenuation assets, dialogue assets, and sound classes."), {MakeSubAction(TEXT("create_sound_cue"), TEXT("Create a Sound Cue asset.")), MakeSubAction(TEXT("add_sound_node"), TEXT("Add a node to a Sound Cue graph.")), MakeSubAction(TEXT("create_meta_sound"), TEXT("Create a MetaSound asset when supported by the engine version.")), MakeSubAction(TEXT("create_sound_class"), TEXT("Create and configure a Sound Class or related mix asset."))}),
        MakeTool(TEXT("manage_niagara_authoring"), TEXT("Visual Effects"), TEXT("Author Niagara systems, emitters, modules, parameters, and simulation settings."), {MakeSubAction(TEXT("create_niagara_system"), TEXT("Create a Niagara system asset.")), MakeSubAction(TEXT("add_emitter_to_system"), TEXT("Attach an emitter to a Niagara system.")), MakeSubAction(TEXT("add_system_parameter"), TEXT("Create or expose a Niagara parameter.")), MakeSubAction(TEXT("configure_gpu_simulation"), TEXT("Configure GPU simulation and related stages."))}),
        MakeTool(TEXT("manage_niagara_graph"), TEXT("Graph Editing"), TEXT("Edit Niagara system and emitter graphs, modules, pins, and parameters."), {MakeSubAction(TEXT("add_module"), TEXT("Add a Niagara module node to the target graph.")), MakeSubAction(TEXT("connect_pins"), TEXT("Connect Niagara graph pins.")), MakeSubAction(TEXT("remove_node"), TEXT("Remove a Niagara graph node.")), MakeSubAction(TEXT("set_parameter"), TEXT("Set an exposed parameter value on the target graph."))}),
        MakeTool(TEXT("manage_effect"), TEXT("Visual Effects"), TEXT("Create and configure generic visual effects and debug-shape workflows.")),
        MakeTool(TEXT("manage_lighting"), TEXT("World"), TEXT("Inspect and modify lighting assets, settings, and lighting workflows.")),
        MakeTool(TEXT("build_environment"), TEXT("World"), TEXT("Build landscapes, foliage, and environment authoring assets.")),
        MakeTool(TEXT("control_environment"), TEXT("World"), TEXT("Control authored environment state and world-space environment helpers.")),
        MakeTool(TEXT("manage_gas"), TEXT("Gameplay Systems"), TEXT("Author and configure Gameplay Ability System assets and metadata.")),
        MakeTool(TEXT("manage_character"), TEXT("Gameplay Systems"), TEXT("Configure character, movement, and character-related editor assets.")),
        MakeTool(TEXT("manage_combat"), TEXT("Gameplay Systems"), TEXT("Configure combat, weapon, and combat-related gameplay assets.")),
        MakeTool(TEXT("manage_ai"), TEXT("Gameplay Systems"), TEXT("Author AI-focused assets, behavior, and supporting gameplay data.")),
        MakeTool(TEXT("manage_inventory"), TEXT("Gameplay Systems"), TEXT("Create and update inventory, item, and related gameplay assets.")),
        MakeTool(TEXT("manage_interaction"), TEXT("Gameplay Systems"), TEXT("Author interaction systems, traces, and widget-driven interaction helpers.")),
        MakeTool(TEXT("manage_widget_authoring"), TEXT("UI Automation"), TEXT("Author widget assets and widget-related editor metadata."), {MakeSubAction(TEXT("get_widget_tree"), TEXT("Inspect the recursive widget hierarchy and metadata for a Widget Blueprint."), true, false, false, TEXT("semantic_read"), TEXT("Tree inspection alone does not report current live Widget Blueprint Designer selection or surface diagnostics.")), MakeSubAction(TEXT("get_widget_designer_state"), TEXT("Return widget hierarchy plus live Widget Blueprint Designer diagnostics and current selection."), true, false, false, TEXT("semantic_read"), TEXT("Live Designer diagnostics stay empty until a Widget Blueprint editor surface is available.")), MakeSubAction(TEXT("create_property_binding"), TEXT("Create or update a Widget Blueprint property binding."), true, false, false, TEXT("semantic_write"), TEXT("Binding functions must stay signature-compatible with the target property.")), MakeSubAction(TEXT("bind_text"), TEXT("Create or reuse a text property binding for a widget."), true, false, false, TEXT("semantic_write"), TEXT("Binding functions must stay signature-compatible with the target property.")), MakeSubAction(TEXT("bind_visibility"), TEXT("Create or reuse a visibility property binding for a widget."), true, false, false, TEXT("semantic_write"), TEXT("Binding functions must stay signature-compatible with the target property.")), MakeSubAction(TEXT("bind_color"), TEXT("Create or reuse a color property binding for a widget."), true, false, false, TEXT("semantic_write"), TEXT("Binding functions must stay signature-compatible with the target property.")), MakeSubAction(TEXT("bind_enabled"), TEXT("Create or reuse an enabled-state property binding for a widget."), true, false, false, TEXT("semantic_write"), TEXT("Binding functions must stay signature-compatible with the target property.")), MakeSubAction(TEXT("bind_on_clicked"), TEXT("Create or reuse a component-bound click event binding for a widget."), true, false, false, TEXT("semantic_write"), TEXT("Event authoring stays on engine-generated handlers and explicit ensureVariable promotion.")), MakeSubAction(TEXT("bind_on_hovered"), TEXT("Create or reuse a component-bound hovered event binding for a widget."), true, false, false, TEXT("semantic_write"), TEXT("OnUnhovered follow-through remains manual even when the OnHovered event is authored.")), MakeSubAction(TEXT("bind_on_value_changed"), TEXT("Create or reuse a component-bound value-changed event binding for a widget."), true, false, false, TEXT("semantic_write"), TEXT("Requires a widget that exposes a supported value-changed delegate."))}),
        MakeTool(TEXT("manage_networking"), TEXT("Gameplay Systems"), TEXT("Configure networking and multiplayer-related assets and settings.")),
        MakeTool(TEXT("manage_game_framework"), TEXT("Gameplay Systems"), TEXT("Configure GameMode, GameState, PlayerController, and framework assets.")),
        MakeTool(TEXT("manage_sessions"), TEXT("Gameplay Systems"), TEXT("Configure sessions and local multiplayer/session-oriented assets.")),
        MakeTool(TEXT("manage_level_structure"), TEXT("World"), TEXT("Author level structure, streaming relationships, and organization assets.")),
        MakeTool(TEXT("manage_volumes"), TEXT("World"), TEXT("Create and configure volumes, zones, and related world helpers.")),
        MakeTool(TEXT("manage_navigation"), TEXT("World"), TEXT("Build and configure navigation assets and navigation-related state.")),
        MakeTool(TEXT("manage_splines"), TEXT("World"), TEXT("Create and update spline actors, spline components, and spline-driven content.")),
        MakeTool(TEXT("manage_pipeline"), TEXT("Diagnostics"), TEXT("Report bridge tool exposure and launch build and test-oriented editor automation."), {MakeSubAction(TEXT("run_ubt"), TEXT("Launch UnrealBuildTool with the provided target, platform, and configuration."), true, true, false, TEXT("editor_utility"), TEXT("Runs through the live automation bridge session instead of a static manifest.")), MakeSubAction(TEXT("list_categories"), TEXT("Return the public bridge-owned MCP tool catalog with capability-matrix metadata."), true, true, false, TEXT("catalog_discovery"), TEXT("Use the runtime catalog as the capability source of truth instead of README prose.")), MakeSubAction(TEXT("get_status"), TEXT("Return bridge status, engine details, and catalog-derived capability counts."), true, true, false, TEXT("catalog_discovery"), TEXT("Requires a live automation bridge connection to report current runtime status."))}),
        MakeTool(TEXT("manage_performance"), TEXT("Diagnostics"), TEXT("Profile performance, show FPS, and run performance-oriented editor commands.")),
        MakeTool(TEXT("manage_tests"), TEXT("Diagnostics"), TEXT("Trigger automation test runs from the editor."), {MakeSubAction(TEXT("run_tests"), TEXT("Start automation tests by filter and report initiation status."))}),
        MakeTool(TEXT("manage_logs"), TEXT("Diagnostics"), TEXT("Read recent editor logs and stream log output for the current session."), {MakeSubAction(TEXT("get_recent_logs"), TEXT("Return recent editor log output.")), MakeSubAction(TEXT("subscribe"), TEXT("Subscribe to streamed log output for the current bridge session."))}),
        MakeTool(TEXT("manage_debug"), TEXT("Diagnostics"), TEXT("Drive gameplay-debugger and related debug-visualization workflows."), {MakeSubAction(TEXT("spawn_category"), TEXT("Toggle a gameplay debugger category."))}),
        MakeTool(TEXT("manage_insights"), TEXT("Diagnostics"), TEXT("Control Unreal Insights trace capture from the editor."), {MakeSubAction(TEXT("start_session"), TEXT("Start a trace session with optional channels."))}),
        MakeTool(TEXT("system_control"), TEXT("Diagnostics"), TEXT("Run high-level system control, cvar, quality, and utility editor operations.")),
        MakeTool(TEXT("inspect"), TEXT("Diagnostics"), TEXT("Inspect actors, objects, metadata, settings, and scene/runtime state.")),
        MakeTool(TEXT("manage_behavior_tree"), TEXT("Graph Editing"), TEXT("Author and modify behavior tree graphs and nodes."))};

    return Catalog;
}

/** @brief Contract: filters the canonical catalog down to the entries published through the bridge's public MCP surface. */
TArray<FMcpAutomationBridgeToolCatalogEntry> GetPublicMcpAutomationBridgeToolCatalog()
{
    TArray<FMcpAutomationBridgeToolCatalogEntry> PublicCatalog;
    for (const FMcpAutomationBridgeToolCatalogEntry &Entry : GetMcpAutomationBridgeToolCatalog())
    {
        if (Entry.bPublic)
        {
            PublicCatalog.Add(Entry);
        }
    }
    return PublicCatalog;
}

/** @brief Contract: resolves one canonical catalog entry by exact tool name for diagnostics and dispatch validation. */
const FMcpAutomationBridgeToolCatalogEntry *FindMcpAutomationBridgeToolCatalogEntry(const FString &ToolName)
{
    for (const FMcpAutomationBridgeToolCatalogEntry &Entry : GetMcpAutomationBridgeToolCatalog())
    {
        if (Entry.ToolName.Equals(ToolName, ESearchCase::CaseSensitive))
        {
            return &Entry;
        }
    }

    return nullptr;
}

/** @brief Contract: counts the number of public tool families currently exposed by the catalog. */
int32 CountPublicMcpAutomationBridgeTools()
{
    int32 Count = 0;
    for (const FMcpAutomationBridgeToolCatalogEntry &Entry : GetMcpAutomationBridgeToolCatalog())
    {
        if (Entry.bPublic)
        {
            ++Count;
        }
    }
    return Count;
}

/** @brief Contract: counts the number of public actions represented by the catalog, including implicit single-action tools. */
int32 CountPublicMcpAutomationBridgeToolActions()
{
    int32 Count = 0;
    for (const FMcpAutomationBridgeToolCatalogEntry &Entry : GetMcpAutomationBridgeToolCatalog())
    {
        if (!Entry.bPublic)
        {
            continue;
        }

        Count += Entry.SubActions.Num() > 0 ? Entry.SubActions.Num() : 1;
    }
    return Count;
}