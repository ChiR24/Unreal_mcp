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
  ['create blueprint', 'blueprint.create'],
  ['create a new blueprint actor', 'blueprint.create'],
  ['make blueprint', 'blueprint.create'],
  ['add variable to blueprint', 'blueprint.edit_variable'],
  ['add a float variable to my blueprint', 'blueprint.edit_variable'],
  ['add component to blueprint', 'blueprint.edit_scs'],
  ['add static mesh component to blueprint', 'blueprint.edit_scs'],
  ['compile blueprint', 'blueprint.compile'],
  ['add function to blueprint', 'blueprint.add_function'],
  ['add event to blueprint', 'blueprint.add_function'],
  ['create node in blueprint graph', 'blueprint.edit_graph'],
  ['connect pins in blueprint', 'blueprint.edit_graph'],
  ['set blueprint default value', 'blueprint.edit_variable'],
  ['get blueprint info', 'blueprint.get_blueprint'],
  ['list blueprint variables', 'blueprint.get_blueprint'],
  ['create widget blueprint', 'blueprint.edit_widget_blueprint'],
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
  ['find actor by name', 'control_actor.find'],
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
  ['find all material assets', 'asset.query_asset'],
  ['find assets', 'asset.query_asset'],
  ['does asset exist', 'asset.query_asset'],
  ['save all assets', 'control_editor.save_all'],
  ['create level', ['manage_level.create_level', 'manage_level_structure.create_level_structure']],
  ['load level', 'manage_level.load'],
  ['open map', 'control_editor.open_level'],
  ['save level', 'manage_level.save'],
  ['save current level', 'manage_level.save'],
  ['build lighting', ['build_environment.build_lighting', 'manage_level.build_lighting']],
  ['what is the current level', 'manage_level.get_summary'],
  ['start play in editor', 'control_editor.play'],
  ['play in editor', 'control_editor.play'],
  ['start pie', 'control_editor.play'],
  ['stop PIE', 'control_editor.play'],
  ['take screenshot', 'control_editor.screenshot'],
  ['screenshot of viewport', 'control_editor.screenshot'],
  ['run console command', ['control_editor.console_command', 'system_control.console_command']],
  ['set viewport camera', 'control_editor.set_camera'],
  ['click widget button', 'control_editor.simulate_input'],
  ['press UI button in game', 'control_editor.simulate_input'],
  ['maps to cook when packaging', 'system_control.package_project'],
  ['cook maps', 'system_control.package_project'],
  ['move camera to actor', 'control_editor.focus_actor'],
  ['get project settings', ['inspect.get_editor_state', 'system_control.get_project_settings']],
  ['inspect actor properties', 'inspect.inspect_object'],
  ['set property on actor', 'inspect.set_property'],
  ['get property of object', 'inspect.get_property'],
  ['list components of actor', 'inspect.get_components'],
  ['what is selected', 'inspect.get_editor_state'],
  ['create landscape', 'build_environment.create_landscape'],
  ['add foliage', 'build_environment.add_foliage'],
  ['create niagara system', 'manage_effect.create_effect'],
  ['spawn particle effect', ['manage_effect.spawn_niagara', 'manage_effect.create_effect']],
  ['play sound', 'manage_audio.play_sound'],
  ['create sound cue', 'manage_audio.create_audio_asset'],
  ['create animation blueprint', 'animation_physics.create_animation_blueprint'],
  ['create montage', 'animation_physics.create_animation_asset'],
  ['create behavior tree', 'manage_ai.create_behavior_tree'],
  ['create ai controller', 'manage_ai.create_ai_controller'],
  ['create gameplay ability', 'manage_gas.create_gas_asset'],
  ['create character', 'manage_character.create_character_blueprint'],
  ['create weapon', 'manage_combat.create_combat_asset'],
  ['create level sequence', 'sequence.create'],
  ['create cinematic', 'sequence.create'],
  ['create pcg graph', 'manage_pcg.edit_pcg_graph'],
  ['enable replication on variable', 'manage_networking.configure_replication'],
  ['replicate variable', 'manage_networking.configure_replication'],
  ['create input mapping', 'manage_networking.configure_input'],
  ['create input action', 'manage_networking.configure_input'],
  ['create door', 'manage_interaction.create_interactable'],
  ['create inventory', 'manage_inventory.configure_inventory'],
  ['list all tools', 'manage_tools.list_tools'],
  ['undo last change', 'control_editor.undo'],
  ['set cvar', 'system_control.configure_display'],
  ['run python script', 'system_control.execute_python'],
  ['run automation tests', 'system_control.run_build'],
  ['create trigger volume', 'manage_level_structure.create_volume'],
  ['create sublevel', 'manage_level_structure.create_level_structure'],
  ['create box mesh', 'manage_geometry.create_primitive']
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
    ['blueprint.add_component_to_blueprint', 'blueprint.edit_scs'],
    ['manage_networking.replicate_variable', 'manage_networking.configure_replication']
  ])('search { capability: %s } answers %s', (alias, canonical) => {
    const result = searchGatewayCapabilities({ operation: 'search', capability: alias }) as {
      results?: Array<{ capability?: string }>;
    };
    expect(result.results?.[0]?.capability).toBe(canonical);
  });
});
