/**
 * Canonical parent-tool metadata lookup — the single source of truth.
 *
 * Holds the 23 canonical parent tools' shared, non-record-specific metadata:
 * the canonical `parent` tool name, its human description, and its capability
 * `category`. Record builders call `getParentToolMetadata(routing.parentTool)`
 * to stamp this onto every produced record, so the builders themselves never
 * duplicate descriptions or categories locally.
 *
 * The descriptions and categories are the canonical 23-parent contract and are
 * validated by `parent-metadata.test.ts`; the records are the sole registration
 * metadata source, so this table is the only hand-maintained parent metadata.
 */
import type { LegacyToolName } from '../index.js';
import { LegacyToolNameSchema } from '../index.js';

export type ParentToolMetadata = {
  /** Canonical parent tool name (matches `routing.parentTool`). */
  readonly parent: LegacyToolName;
  /** Canonical one-line description of the parent tool. */
  readonly description: string;
  /** Canonical capability category of the parent tool. */
  readonly category: 'core' | 'world' | 'gameplay' | 'utility';
};

const PARENT_METADATA: Readonly<
  Record<string, { readonly description: string; readonly category: ParentToolMetadata['category'] }>
> = {
  // Core
  manage_tools: {
    description:
      'Dynamic MCP tool management. List canonical tools, view category counts, and enable/disable tools or categories at runtime.',
    category: 'core',
  },
  manage_asset: {
    description:
      'Create/import/manage assets, material graphs, material instances, procedural textures, render targets, and dependency analysis.',
    category: 'core',
  },
  manage_blueprint: {
    description:
      'Create Blueprints and UMG widgets, add SCS/UI components, set defaults, and manipulate Blueprint graphs, bindings, and widget layouts.',
    category: 'core',
  },
  control_actor: {
    description:
      'Spawn actors, set transforms, enable physics, add components, manage tags, and attach actors.',
    category: 'core',
  },
  control_editor: {
    description:
      'Start/stop PIE, control viewport camera, run console commands, take screenshots, simulate input.',
    category: 'core',
  },
  manage_level: {
    description: 'Load/save levels, configure streaming, and build lighting.',
    category: 'core',
  },
  system_control: {
    description:
      'Control the project runtime: profiling, benchmarks, scalability/LOD/Nanite settings, CVars, console commands, Python scripts, UBT, tests, logs, and widgets.',
    category: 'core',
  },
  inspect: {
    description:
      'Inspect any UObject: read/write properties, list components, export snapshots, and query class info. Actions: inspect_cdo (Blueprint CDO properties + all components without spawning an actor; use blueprintPath, optional detailed/componentName/propertyNames), inspect_class (class metadata), inspect_object (world actor), get_property/set_property, get_components, get_component_details (WORLD actors: actorName+componentName; a blueprintPath is routed to inspect_cdo), list_objects, find_by_class, find_by_tag, runtime_report.',
    category: 'core',
  },
  // World
  build_environment: {
    description:
      'Build environments: landscapes, foliage, procedural terrain/biomes, lighting setups, spline roads/rivers/fences, and world decoration.',
    category: 'world',
  },
  manage_level_structure: {
    description:
      'Structure worlds: levels, sublevels, World Partition, streaming, data layers, HLOD, level instances, trigger/blocking/physics/audio/post-process volumes, and nav bounds.',
    category: 'world',
  },
  manage_geometry: {
    description:
      'Create procedural meshes using Geometry Script: booleans, deformers, UVs, collision, and LOD generation.',
    category: 'world',
  },
  manage_pcg: {
    description:
      'Create, edit, execute, and configure PCG graphs: graph assets, input/sampler/filter/spawner nodes, pin connections, node settings, and partition grid size.',
    category: 'world',
  },
  // Gameplay
  animation_physics: {
    description:
      'Author animation and physics assets: Animation Blueprints, blend spaces, montages, Control Rig/IK, skeletons, sockets, physics assets, cloth, ragdolls, and vehicles.',
    category: 'gameplay',
  },
  manage_effect: {
    description:
      'Niagara particle systems, VFX, debug shapes, and GPU simulations. Create systems, emitters, modules, and control particle effects.',
    category: 'gameplay',
  },
  manage_gas: {
    description:
      'Create Gameplay Abilities, Effects, Attribute Sets, and Gameplay Cues for ability systems.',
    category: 'gameplay',
  },
  manage_character: {
    description:
      'Create Character Blueprints with movement, locomotion, and animation state machines.',
    category: 'gameplay',
  },
  manage_combat: {
    description:
      'Create weapons with hitscan/projectile firing, configure damage types, hitboxes, reload, and melee combat (combos, parry, block).',
    category: 'gameplay',
  },
  manage_ai: {
    description:
      'Build AI systems: AI controllers, Behavior Trees, Blackboards, EQS, perception, State Trees, Smart Objects, NavMesh settings, nav modifiers, links, and pathfinding.',
    category: 'gameplay',
  },
  manage_inventory: {
    description:
      'Create item data assets, inventory components, world pickups, loot tables, and crafting recipes.',
    category: 'gameplay',
  },
  manage_interaction: {
    description:
      'Create interactive objects: doors, switches, chests, levers. Set up destructible meshes and trigger volumes.',
    category: 'gameplay',
  },
  // Utility
  manage_sequence: {
    description:
      'Edit Level Sequences, cinematic tracks, Movie Render Queue jobs, media playback assets, Take Recorder, and replay controls.',
    category: 'utility',
  },
  manage_audio: {
    description:
      'Play/stop sounds, add audio components, configure mixes, attenuation, spatial audio, and author Sound Cues/MetaSounds.',
    category: 'utility',
  },
  manage_networking: {
    description:
      'Configure multiplayer and player flow: replication, RPCs, authority/relevancy, network prediction, sessions, split-screen, LAN/voice chat, game framework classes, match rules, and input mappings.',
    category: 'utility',
  },
};

/**
 * Resolve canonical parent-tool metadata by `routing.parentTool`.
 *
 * Throws a branded `ZodError` from `LegacyToolNameSchema.parse` when the supplied
 * parent is not one of the 23 canonical tools, so callers can assert invalid
 * lookups deterministically.
 */
export function getParentToolMetadata(parentTool: string): ParentToolMetadata {
  const parent = LegacyToolNameSchema.parse(parentTool);
  const entry = PARENT_METADATA[parent];
  return {
    parent,
    description: entry.description,
    category: entry.category,
  };
}
