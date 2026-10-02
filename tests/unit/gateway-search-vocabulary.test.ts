import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../src/server/gateway/gateway-capability-index.js';
import { searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';
import { RETRIEVAL_TOKENIZATION } from '../../src/tools/catalog/capabilities/retrieval/constants.js';
import { queryCapabilityTokens, tokenizeCapabilityText } from '../../src/tools/catalog/capabilities/retrieval/tokenize.js';
import { readAllNativeShardRecords } from './capability-records/native-shard-records.js';

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
  // remove/destroy/erase fold to delete in queries and catalog text alike (2026-09-26).
  ['remove actor', 'control_actor.delete'],
  ['destroy actor', 'control_actor.delete'],
  ['remove node from blueprint graph', 'blueprint.delete_node'],
  ['delete blueprint graph node', 'blueprint.delete_node'],
  ['remove tag from actor', 'control_actor.add_tag'],
  ['move actor', 'control_actor.set_transform'],
  ['set actor location', 'control_actor.set_transform'],
  ['set actor transform', 'control_actor.set_transform'],
  ['rotate actor', 'control_actor.set_transform'],
  ['list actors in level', 'control_actor.list'],
  ['list actors', 'control_actor.list'],
  ['how many actors in the level', 'control_actor.list'],
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
  ['start the game', 'control_editor.play'],
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
  ['create inventory', 'manage_inventory.create_inventory_asset'],
  ['list all tools', 'manage_tools.list_tools'],
  ['undo last change', 'control_editor.undo'],
  ['set cvar', 'system_control.configure_display'],
  ['run python script', 'system_control.execute_python'],
  ['run automation tests', 'system_control.run_build'],
  ['create trigger volume', 'manage_level_structure.create_volume'],
  ['create sublevel', 'manage_level_structure.create_level_structure'],
  ['create box mesh', 'manage_geometry.create_primitive'],
  // Subdivision-surface modelling: a polygon cage, smooth subdivision, fillets, masks and per-face materials.
  ['catmull clark subdivision', 'manage_geometry.optimize_mesh'],
  ['subdivide a cage', 'manage_geometry.optimize_mesh'],
  ['loop subdivision', 'manage_geometry.optimize_mesh'],
  ['fillet the seams', 'manage_geometry.optimize_mesh'],
  ['make a polygon cage', 'manage_geometry.edit_dynamic_mesh'],
  ['extrude the top faces', 'manage_geometry.model_mesh'],
  // The mesh ASSET's slots; a placed actor's component slot is control_actor.set_material.
  ['set mesh materials', 'asset.process_asset'],
  ['assign materials to a static mesh', 'asset.process_asset'],
  ['mesh material slots', 'asset.process_asset'],
  ['which material slot is which part of a mesh', 'inspect.inspect_object']
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

// Phrasings a small model types for capabilities whose name says none of the words ("where is the
// player" for get_transform, "press play" for play). A topic is the one place a record can carry them
// (weight 8 in the TypeScript ranker, 12 in the native word rules). Both doors rank from the same
// generated registry, so pinning the record pins both. Not every phrase is top-1 on both doors yet:
// an action name that says the word outranks a topic that does, so this checks what was declared.
const DECLARED_TOPICS: ReadonlyArray<readonly [string, readonly string[]]> = [
  ['control_actor.get_transform', ['where is the player', 'player location', 'actor position']],
  ['control_actor.list', ['how many actors', 'count actors', 'actors in the level']],
  ['asset.list', ['what is in this folder', 'folder contents', 'assets in folder']],
  ['blueprint.set_widget_layout', ['make text bold', 'change button text', 'hide widget', 'show widget', 'button label']],
  ['blueprint.compile', ['compile widget blueprint', 'compile widget']],
  ['control_editor.play', ['press play', 'start the game', 'run the game']],
  ['blueprint.bind_widget', ['button click event', 'on clicked', 'click event']],
  ['blueprint.edit_graph', ['connect blueprint nodes', 'connect nodes', 'wire pins', 'print string', 'print to screen']],
  ['blueprint.get_widget_info', ['read widget layout', 'widget layout', 'slot layout']],
  ['material.get_material_info', ['get material parameters', 'material parameters']],
  ['asset.process_asset', ['set mesh materials', 'mesh material slots', 'assign materials to a static mesh']],
  ['inspect.inspect_object', ['material slot bounds', 'which slot is which part']],
  ['manage_geometry.optimize_mesh', ['catmull clark', 'loop subdivision', 'smooth subdivision', 'fillet']],
  ['manage_geometry.edit_dynamic_mesh', ['polygon cage', 'material ids']],
  ['manage_geometry.model_mesh', ['select faces by region']]
];

describe('phrasings a name cannot carry are declared as topics', () => {
  it.each(DECLARED_TOPICS.map(([id, phrases]) => [id, phrases] as const))('%s declares its plain phrasings', (id, phrases) => {
    const topics = capabilityIndex().byId.get(id)?.discovery.topics ?? [];
    for (const phrase of phrases) expect(topics, `${id} is missing the topic "${phrase}"`).toContain(phrase);
  });

  it('the native door carries the same phrasings in its generated shards', () => {
    const shards = readAllNativeShardRecords();
    for (const [id, phrases] of DECLARED_TOPICS) {
      const discovery = shards.get(id)?.discovery as { topics?: readonly string[] } | undefined;
      for (const phrase of phrases) expect(discovery?.topics, `native ${id} is missing the topic "${phrase}"`).toContain(phrase);
    }
  });

  it('get_widget_info says slot layout in its summary too', () => {
    expect(capabilityIndex().byId.get('blueprint.get_widget_info')?.discovery.summary).toContain('slot layout');
  });

  // Only a query is capped at maxTokens; record text is read whole, as the native door reads it. The cap
  // once cut every summary past 48 tokens, so the tail of set_widget_layout's never reached search.
  it('reads a long summary to its end and caps only the query', () => {
    const summary = capabilityIndex().byId.get('blueprint.set_widget_layout')?.discovery.summary ?? '';
    expect(tokenizeCapabilityText(summary).length).toBeGreaterThan(RETRIEVAL_TOKENIZATION.maxTokens);
    expect(tokenizeCapabilityText(summary).join(' ')).toContain('press sound');
    const longQuery = Array.from({ length: 60 }, (_, index) => `word${index}`).join(' ');
    expect(queryCapabilityTokens(longQuery)).toHaveLength(RETRIEVAL_TOKENIZATION.maxTokens);
  });
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
