/**
 * manage_blueprint pilot capability records: exact-set and hidden-disposition
 * and SCS ownership tests.
 *
 * Verifies:
 * - Exactly 121 canonical records (39 core + 82 widget)
 * - All 121 TS enum actions are represented with matching legacy IDs
 * - All 21 hidden operations have explicit promote/map/remove dispositions
 * - apply_style_to_widget and set_animation_speed no-op routes are NOT
 *   active canonical records (route disposition: remove)
 * - graph get_nodes is NOT an active canonical record (route disposition: remove)
 * - create_widget is an alias of create_widget_blueprint (route disposition: map)
 * - SCS component templates require SCS-owned construction (add_scs_component
 *   has distinct contract from add_component)
 * - All records pass CapabilityCatalogSchema validation (no duplicate IDs/aliases)
 *
 * Widget/graph handles, schema/hash parity, and availability/plugin gates live
 * in manage-blueprint-pilot-records-schema.test.ts.
 */
import { describe, expect, it } from 'vitest';
import { CapabilityCatalogSchema } from '../../src/tools/catalog/capabilities/index.js';
import { GRAPH_ROUTE_DISPOSITIONS } from '../../src/tools/catalog/capabilities/normalization/routedispositions-graph.data.js';
import { WIDGET_UNOWNED_PROMOTE } from '../../src/tools/catalog/capabilities/normalization/routedispositions-paths.js';
import { WIDGET_ROUTE_DISPOSITIONS } from '../../src/tools/catalog/capabilities/normalization/routedispositions-widget.data.js';
import {
  MANAGE_BLUEPRINT_RECORD_COUNT,
  MANAGE_BLUEPRINT_RECORD_IDS,
  MANAGE_BLUEPRINT_RECORDS,
} from '../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';

// The 121 actions from the TS enum (39 core + 82 widget)
const CORE_ACTIONS = [
  'create', 'create_blueprint', 'get_blueprint', 'get', 'compile',
  'add_component', 'set_default', 'modify_scs', 'get_scs', 'add_scs_component',
  'remove_scs_component', 'reparent_scs_component', 'set_scs_transform', 'set_scs_property',
  'ensure_exists', 'probe_handle', 'add_variable', 'remove_variable', 'rename_variable',
  'add_function', 'remove_function', 'add_event', 'remove_event', 'add_construction_script',
  'set_variable_metadata', 'set_metadata', 'create_node', 'add_node', 'delete_node',
  'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node',
  'get_node_details', 'get_graph_details', 'get_pin_details', 'list_node_types',
  'set_pin_default_value', 'create_struct_make_break_nodes',
] as const;

const WIDGET_ACTIONS = [
  'create_widget_blueprint', 'set_widget_parent_class', 'add_canvas_panel', 'add_horizontal_box',
  'add_vertical_box', 'add_overlay', 'add_grid_panel', 'add_uniform_grid', 'add_wrap_box',
  'add_scroll_box', 'add_size_box', 'add_scale_box', 'add_border', 'add_text_block',
  'add_rich_text_block', 'add_image', 'add_button', 'add_check_box', 'add_slider', 'add_progress_bar',
  'add_text_input', 'add_combo_box', 'add_spin_box', 'add_list_view', 'add_tree_view',
  'set_anchor', 'set_alignment', 'set_position', 'set_size', 'set_padding', 'set_z_order',
  'set_render_transform', 'set_visibility', 'set_style', 'set_clipping', 'create_property_binding',
  'bind_text', 'bind_visibility', 'bind_color', 'bind_enabled', 'bind_on_clicked', 'bind_on_hovered',
  'bind_on_value_changed', 'create_widget_animation', 'add_animation_track', 'add_animation_keyframe',
  'set_animation_loop', 'create_main_menu', 'create_pause_menu', 'create_settings_menu',
  'create_loading_screen', 'create_hud_widget', 'add_health_bar', 'add_ammo_counter', 'add_minimap',
  'add_crosshair', 'add_compass', 'add_interaction_prompt', 'add_objective_tracker',
  'add_damage_indicator', 'create_inventory_ui', 'create_dialog_widget', 'create_radial_menu',
  'get_widget_info', 'preview_widget',
  'add_quest_tracker', 'add_safe_zone', 'add_spacer', 'add_widget_component',
  'add_widget_switcher', 'bind_localized_text', 'create_credits_screen', 'create_shop_ui',
  'delete_animation', 'get_widget_slot_info', 'remove_widget',
  'rename_widget', 'reparent_widget', 'set_font', 'set_localization_key',
  'set_margin', 'set_widget_binding',
] as const;

const ALL_TS_ENUM_ACTIONS = [...CORE_ACTIONS, ...WIDGET_ACTIONS];

describe('manage_blueprint pilot: exact record set', () => {
  it('has exactly 121 canonical records (39 core + 82 widget)', () => {
    expect(MANAGE_BLUEPRINT_RECORD_COUNT).toBe(121);
    expect(CORE_ACTIONS.length).toBe(39);
    expect(WIDGET_ACTIONS.length).toBe(82);
    expect(ALL_TS_ENUM_ACTIONS.length).toBe(121);
  });

  it('every TS enum action has a matching legacy ID in the records', () => {
    const legacyActions = new Set(
      MANAGE_BLUEPRINT_RECORDS.flatMap((r) => r.legacyIds.map((l) => l.action)),
    );
    for (const action of ALL_TS_ENUM_ACTIONS) {
      expect(legacyActions.has(action)).toBe(true);
    }
  });

  it('every record has a unique canonical ID', () => {
    const ids = MANAGE_BLUEPRINT_RECORD_IDS;
    expect(new Set(ids).size).toBe(ids.length);
  });

  it('all records route through manage_blueprint parent tool', () => {
    for (const record of MANAGE_BLUEPRINT_RECORDS) {
      expect(record.routing.parentTool).toBe('manage_blueprint');
    }
  });

  it('all records have valid hashes (schema + content)', () => {
    for (const record of MANAGE_BLUEPRINT_RECORDS) {
      expect(record.hashes.algorithm).toBe('sha256');
      expect(record.hashes.schema).toMatch(/^[0-9a-f]{64}$/);
      expect(record.hashes.content).toMatch(/^[0-9a-f]{64}$/);
    }
  });

  it('passes CapabilityCatalogSchema validation (no duplicate IDs/aliases/legacy IDs)', () => {
    const result = CapabilityCatalogSchema.safeParse(MANAGE_BLUEPRINT_RECORDS);
    expect(result.success).toBe(true);
  });
});

describe('manage_blueprint pilot: hidden operation dispositions', () => {
  // The 21 hidden operations: 18 widget promote + 2 widget remove + 1 graph remove
  const widgetPromoteRoutes = WIDGET_UNOWNED_PROMOTE;
  const widgetRemoveRoutes = ['apply_style_to_widget', 'set_animation_speed'];
  const graphRemoveRoutes = ['get_nodes'];
  // create_widget maps to create_widget_blueprint (alias, not a separate record)
  const widgetMapRoute = 'create_widget';

  it('has 18 widget promote routes with promote disposition', () => {
    expect(widgetPromoteRoutes.length).toBe(18);
    const promoteDispositions = WIDGET_ROUTE_DISPOSITIONS.filter((d) => d.disposition === 'promote');
    expect(promoteDispositions.length).toBe(18);
    for (const route of widgetPromoteRoutes) {
      const found = WIDGET_ROUTE_DISPOSITIONS.find((d) => d.route === route);
      expect(found).toBeDefined();
      expect(found?.disposition).toBe('promote');
    }
  });

  it('has 2 widget remove routes (no-op) with remove disposition', () => {
    expect(widgetRemoveRoutes.length).toBe(2);
    for (const route of widgetRemoveRoutes) {
      const found = WIDGET_ROUTE_DISPOSITIONS.find((d) => d.route === route);
      expect(found).toBeDefined();
      expect(found?.disposition).toBe('remove');
    }
  });

  it('has 1 graph remove route (dead) with remove disposition', () => {
    expect(graphRemoveRoutes.length).toBe(1);
    const found = GRAPH_ROUTE_DISPOSITIONS.find((d) => d.route === 'get_nodes');
    expect(found).toBeDefined();
    expect(found?.disposition).toBe('remove');
  });

  it('has 1 widget map route (create_widget) mapping to create_widget_blueprint', () => {
    const found = WIDGET_ROUTE_DISPOSITIONS.find((d) => d.route === widgetMapRoute);
    expect(found).toBeDefined();
    expect(found?.disposition).toBe('map');
    expect(found?.targetCanonicalId).toBe('cap:manage_blueprint:create_widget_blueprint');
  });

  it('total hidden operations = 21 (18 promote + 2 widget remove + 1 graph remove)', () => {
    const total = widgetPromoteRoutes.length + widgetRemoveRoutes.length + graphRemoveRoutes.length;
    expect(total).toBe(21);
  });

  it('apply_style_to_widget and set_animation_speed are NOT active canonical records (no-op)', () => {
    const canonicalIds = new Set(MANAGE_BLUEPRINT_RECORD_IDS);
    expect(canonicalIds.has('blueprint.apply_style_to_widget')).toBe(false);
    expect(canonicalIds.has('blueprint.set_animation_speed')).toBe(false);
  });

  it('graph get_nodes is NOT an active canonical record (dead)', () => {
    const canonicalIds = new Set(MANAGE_BLUEPRINT_RECORD_IDS);
    expect(canonicalIds.has('blueprint.get_nodes')).toBe(false);
  });

  it('create_widget is an alias of create_widget_blueprint (map disposition)', () => {
    const cwb = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.create_widget_blueprint');
    expect(cwb).toBeDefined();
    expect(cwb?.aliases).toContain('blueprint.create_widget');
  });
});

describe('manage_blueprint pilot: SCS ownership and distinct contracts', () => {
  it('add_scs_component and add_component have distinct contracts (SCS-owned vs instance)', () => {
    const scsRecord = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.add_scs_component');
    const compRecord = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.add_component');
    expect(scsRecord).toBeDefined();
    expect(compRecord).toBeDefined();
    // add_scs_component returns SCS-specific verification fields
    expect(scsRecord?.schemas.output.properties).toHaveProperty('scsVerification');
    // add_component does NOT return SCS verification
    expect(compRecord?.schemas.output.properties).not.toHaveProperty('scsVerification');
  });

  it('set_default targets CDO and returns the re-read `value`, distinct from set_scs_property', () => {
    const setDefault = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.set_default');
    const setScsProp = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.set_scs_property');
    expect(setDefault).toBeDefined();
    expect(setScsProp).toBeDefined();
    // set_default emits the CDO read-back as `value` (never verifiedValue); set_scs_property emits verifiedValue
    expect(setDefault?.schemas.output.properties).toHaveProperty('value');
    expect(setDefault?.schemas.output.properties).not.toHaveProperty('verifiedValue');
    expect(setScsProp?.schemas.output.properties).toHaveProperty('verifiedValue');
    // set_default discovery summary mentions CDO
    expect(setDefault?.discovery.summary).toContain('CDO');
  });

  it('set_scs_property declares verifiedValue as an optional (conditional) post-write verification', () => {
    const setScsProp = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.set_scs_property');
    expect(setScsProp?.schemas.output.properties).toHaveProperty('verifiedValue');
    // Native emits verifiedValue only when the re-read exports to JSON, so it is declared but not required
    expect(setScsProp?.schemas.output.required).not.toContain('verifiedValue');
  });

  it('add_scs_component output includes SCS verification fields (compiled, saved, scsVerification)', () => {
    const addScs = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.add_scs_component');
    expect(addScs?.schemas.output.properties).toHaveProperty('compiled');
    expect(addScs?.schemas.output.properties).toHaveProperty('saved');
    expect(addScs?.schemas.output.properties).toHaveProperty('scsVerification');
  });
});
