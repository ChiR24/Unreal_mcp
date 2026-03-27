#include "McpAutomationBridgeToolCatalog.h"

namespace
{
    /** @brief Contract: builds a subaction catalog row with the stable name and summary that runtime diagnostics publish. */
    FMcpAutomationBridgeToolSubActionCatalogEntry MakeSubAction(const TCHAR *Name, const TCHAR *Summary)
    {
        FMcpAutomationBridgeToolSubActionCatalogEntry Entry;
        Entry.Name = Name;
        Entry.Summary = Summary;
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
        MakeTool(TEXT("control_editor"), TEXT("Core Editor"), TEXT("Drive editor viewport, screenshots, PIE controls, and simulated input."), {MakeSubAction(TEXT("screenshot"), TEXT("Capture viewport or editor-window screenshots.")), MakeSubAction(TEXT("simulate_input"), TEXT("Send keyboard, text, mouse, wheel, and drag input into a target tab or window."))}),
        MakeTool(TEXT("manage_asset"), TEXT("Content"), TEXT("Browse, create, duplicate, move, rename, import, and delete Unreal assets.")),
        MakeTool(TEXT("manage_level"), TEXT("World"), TEXT("Load, save, validate, duplicate, and stream levels and level assets.")),
        MakeTool(TEXT("manage_sequence"), TEXT("Cinematics"), TEXT("Create and edit level sequences, bindings, tracks, and playback state.")),
        MakeTool(TEXT("manage_ui"), TEXT("UI Automation"), TEXT("Discover UI targets, visible windows, commands, menus, and editor utility widgets."), {MakeSubAction(TEXT("list_ui_targets"), TEXT("List bridge-discovered UI targets, commands, and known menus.")), MakeSubAction(TEXT("list_visible_windows"), TEXT("List current Slate windows with outer and client bounds.")), MakeSubAction(TEXT("open_ui_target"), TEXT("Open a discovered target by identifier or tab id.")), MakeSubAction(TEXT("register_editor_command"), TEXT("Register a bridge-owned command for a session.")), MakeSubAction(TEXT("add_menu_entry"), TEXT("Attach a registered command to a ToolMenus menu or toolbar."))}),
        MakeTool(TEXT("manage_input"), TEXT("UI Automation"), TEXT("Create and configure input mappings and related editor input assets.")),
        MakeTool(TEXT("manage_blueprint"), TEXT("Blueprint Authoring"), TEXT("Create and modify Blueprint assets, classes, and components.")),
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
        MakeTool(TEXT("manage_widget_authoring"), TEXT("UI Automation"), TEXT("Author widget assets and widget-related editor metadata.")),
        MakeTool(TEXT("manage_networking"), TEXT("Gameplay Systems"), TEXT("Configure networking and multiplayer-related assets and settings.")),
        MakeTool(TEXT("manage_game_framework"), TEXT("Gameplay Systems"), TEXT("Configure GameMode, GameState, PlayerController, and framework assets.")),
        MakeTool(TEXT("manage_sessions"), TEXT("Gameplay Systems"), TEXT("Configure sessions and local multiplayer/session-oriented assets.")),
        MakeTool(TEXT("manage_level_structure"), TEXT("World"), TEXT("Author level structure, streaming relationships, and organization assets.")),
        MakeTool(TEXT("manage_volumes"), TEXT("World"), TEXT("Create and configure volumes, zones, and related world helpers.")),
        MakeTool(TEXT("manage_navigation"), TEXT("World"), TEXT("Build and configure navigation assets and navigation-related state.")),
        MakeTool(TEXT("manage_splines"), TEXT("World"), TEXT("Create and update spline actors, spline components, and spline-driven content.")),
        MakeTool(TEXT("manage_pipeline"), TEXT("Diagnostics"), TEXT("Report bridge tool exposure and launch build and test-oriented editor automation."), {MakeSubAction(TEXT("run_ubt"), TEXT("Launch UnrealBuildTool with the provided target, platform, and configuration.")), MakeSubAction(TEXT("list_categories"), TEXT("Return the public bridge-owned MCP tool catalog.")), MakeSubAction(TEXT("get_status"), TEXT("Return bridge status, engine details, and catalog-derived capability counts."))}),
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