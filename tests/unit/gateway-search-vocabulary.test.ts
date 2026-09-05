import { describe, expect, it } from 'vitest';

import { searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';

// Task phrasings a model actually types, not catalog vocabulary. Every record
// used to carry only its action name as `topics`, so "move actor" could not
// reach control_actor.set_transform and "add component to blueprint" landed on
// the actor tool. Three things now make these rank first, and this file pins
// all three through the real gateway operation:
//   - `discovery.topics` declares the words a caller uses;
//   - a declared alias (`control_actor.move_actor`) is scored as one of the
//     record's own names, so a verb synonym reaches the action it names;
//   - question words are function words, so "how do i spawn an actor" ranks
//     like "spawn actor".
// An array lists documented duplicates that run the same action; either row is
// a correct pick and a model reading the summaries cannot go wrong.

function top(query: string): string {
  const result = searchGatewayCapabilities({ operation: 'search', query, limit: 5 }) as {
    results?: Array<{ capability?: string }>;
  };
  return (result.results ?? []).map((row) => String(row.capability)).join(', ');
}

const CASES: ReadonlyArray<readonly [string, string | readonly string[]]> = [
  ['create blueprint', ['blueprint.create', 'blueprint.create_blueprint']],
  ['create a new blueprint actor', 'blueprint.create'],
  ['make blueprint', 'blueprint.create'],
  ['add variable to blueprint', 'blueprint.add_variable'],
  ['add a float variable to my blueprint', 'blueprint.add_variable'],
  ['add component to blueprint', 'blueprint.add_scs_component'],
  ['add static mesh component to blueprint', 'blueprint.add_scs_component'],
  ['compile blueprint', 'blueprint.compile'],
  ['add function to blueprint', 'blueprint.add_function'],
  ['add event to blueprint', 'blueprint.add_event'],
  ['create node in blueprint graph', 'blueprint.create_node'],
  ['connect pins in blueprint', 'blueprint.connect_pins'],
  ['set blueprint default value', 'blueprint.set_default'],
  ['get blueprint info', 'blueprint.get_blueprint'],
  ['list blueprint variables', 'blueprint.get_blueprint'],
  ['create widget blueprint', 'blueprint.create_widget_blueprint'],
  ['spawn actor', 'control_actor.spawn'],
  ['spawn a cube in the level', 'control_actor.spawn'],
  ['how do i spawn an actor', 'control_actor.spawn'],
  ['delete actor', 'control_actor.delete'],
  ['move actor', 'control_actor.set_transform'],
  ['set actor location', 'control_actor.set_transform'],
  ['set actor transform', 'control_actor.set_transform'],
  ['rotate actor', 'control_actor.set_transform'],
  ['list actors in level', 'control_actor.list'],
  ['list actors', 'control_actor.list'],
  ['find actor by name', 'control_actor.find_by_name'],
  ['attach actor to another actor', 'control_actor.attach'],
  ['add tag to actor', 'control_actor.add_tag'],
  ['hide actor', 'control_actor.set_visibility'],
  ['show actor', 'control_actor.set_visibility'],
  ['create material', 'material.create_material'],
  ['create material instance', 'material.create_material_instance'],
  ['set material parameter', 'material.set_material_parameter'],
  ['import fbx', 'asset.import'],
  ['import asset', 'asset.import'],
  ['list assets in folder', 'asset.list'],
  ['delete asset', 'asset.delete'],
  ['rename asset', 'asset.rename'],
  ['find all material assets', 'asset.search_assets'],
  ['find assets', 'asset.search_assets'],
  ['does asset exist', 'asset.exists'],
  ['save all assets', 'control_editor.save_all'],
  ['create level', 'manage_level.create_level'],
  ['load level', ['manage_level.load', 'manage_level.load_level']],
  ['open map', 'control_editor.open_level'],
  ['save level', 'manage_level.save'],
  ['save current level', 'manage_level.save'],
  ['build lighting', ['build_environment.build_lighting', 'manage_level.build_lighting']],
  ['what is the current level', 'manage_level.get_current_level'],
  ['start play in editor', 'control_editor.play'],
  ['play in editor', 'control_editor.play'],
  ['start pie', 'control_editor.play'],
  ['stop PIE', ['control_editor.stop_pie', 'control_editor.stop']],
  ['take screenshot', 'control_editor.take_screenshot'],
  ['screenshot of viewport', 'control_editor.take_screenshot'],
  ['run console command', 'control_editor.console_command'],
  ['set viewport camera', 'control_editor.set_viewport_camera'],
  ['move camera to actor', 'control_editor.focus_actor'],
  ['get project settings', ['inspect.get_project_settings', 'system_control.get_project_settings']],
  ['inspect actor properties', 'inspect.inspect_object'],
  ['set property on actor', 'inspect.set_property'],
  ['get property of object', 'inspect.get_property'],
  ['list components of actor', 'inspect.get_components'],
  ['what is selected', 'inspect.get_selected_actors'],
  ['create landscape', 'build_environment.create_landscape'],
  ['add foliage', 'build_environment.add_foliage'],
  ['create niagara system', 'manage_effect.create_niagara_system'],
  ['spawn particle effect', 'manage_effect.spawn_niagara'],
  ['play sound', 'manage_audio.play_sound_2d'],
  ['create sound cue', 'manage_audio.create_sound_cue'],
  ['create animation blueprint', 'animation_physics.create_animation_blueprint'],
  ['create montage', 'animation_physics.create_montage'],
  ['create behavior tree', 'manage_ai.create_behavior_tree'],
  ['create ai controller', 'manage_ai.create_ai_controller'],
  ['create gameplay ability', 'manage_gas.create_gameplay_ability'],
  ['create character', 'manage_character.create_character_blueprint'],
  ['create weapon', 'manage_combat.create_weapon_blueprint'],
  ['create level sequence', 'sequence.create'],
  ['create cinematic', 'sequence.create'],
  ['create pcg graph', 'manage_pcg.create_pcg_graph'],
  ['enable replication on variable', 'manage_networking.set_property_replicated'],
  ['replicate variable', 'manage_networking.set_property_replicated'],
  ['create input mapping', 'manage_networking.create_input_mapping_context'],
  ['create input action', 'manage_networking.create_input_action'],
  ['create door', 'manage_interaction.create_door_actor'],
  ['create inventory', 'manage_inventory.create_inventory_component'],
  ['list all tools', 'manage_tools.list_tools'],
  ['undo last change', 'control_editor.undo'],
  ['set cvar', 'system_control.set_cvar'],
  ['run python script', 'system_control.execute_python'],
  ['run automation tests', 'system_control.run_tests'],
  ['create trigger volume', 'manage_level_structure.create_trigger_volume'],
  ['create sublevel', 'manage_level_structure.create_sublevel'],
  ['create box mesh', 'manage_geometry.create_box']
];

describe('plain-language task phrasings rank the intended capability first', () => {
  it.each(CASES.map(([query, expected]) => [query, typeof expected === 'string' ? expected : expected.join(' or ')] as const))(
    '"%s" -> %s',
    (query, label) => {
      const accepted = label.split(' or ');
      const page = top(query);
      const first = page.split(', ')[0];
      expect(accepted, `top-1 for "${query}" was ${first || 'nothing'}; page: ${page}`).toContain(first);
    }
  );
});

describe('a declared alias resolves like the capability it names', () => {
  it.each([
    ['control_actor.move_actor', 'control_actor.set_transform'],
    ['control_editor.start_pie', 'control_editor.play'],
    ['blueprint.add_component_to_blueprint', 'blueprint.add_scs_component'],
    ['manage_networking.replicate_variable', 'manage_networking.set_property_replicated']
  ])('search { capability: %s } answers %s', (alias, canonical) => {
    const result = searchGatewayCapabilities({ operation: 'search', capability: alias }) as {
      results?: Array<{ capability?: string }>;
    };
    expect(result.results?.[0]?.capability).toBe(canonical);
  });
});
