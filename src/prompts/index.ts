/**
 * MCP Prompts for Unreal Engine MCP Server
 * Phase E1: Enable MCP Prompts feature with 5 starter prompts
 * 
 * Prompts provide templated guidance for common Unreal Engine workflows.
 * They do NOT execute actions directly - they return instructional content.
 */

import { Logger } from '../utils/logger.js';

const log = new Logger('Prompts');

/**
 * Prompt definition matching MCP SDK schema
 */
export interface Prompt {
  name: string;
  description: string;
  arguments?: Array<{
    name: string;
    description: string;
    required?: boolean;
  }>;
}

/**
 * Prompt message for GetPrompt responses
 */
export interface PromptMessage {
  role: 'user' | 'assistant';
  content: { type: 'text'; text: string };
}

/**
 * All available prompt definitions
 */
export const promptDefinitions: Prompt[] = [
  {
    name: 'create-actor',
    description: 'Guide for spawning actors in Unreal Engine with proper transforms and components',
    arguments: [
      { name: 'actorType', description: 'Type of actor (StaticMeshActor, PointLight, etc.)', required: true },
      { name: 'location', description: 'Spawn location as "x,y,z"', required: false }
    ]
  },
  {
    name: 'debug-blueprint',
    description: 'Troubleshooting guide for Blueprint compilation and runtime issues',
    arguments: [
      { name: 'blueprintPath', description: 'Path to the problematic Blueprint', required: true },
      { name: 'errorType', description: 'Type of error (compile, runtime, node)', required: false }
    ]
  },
  {
    name: 'optimize-level',
    description: 'Performance optimization checklist for Unreal Engine levels',
    arguments: [
      { name: 'levelPath', description: 'Path to the level to optimize', required: false },
      { name: 'targetPlatform', description: 'Target platform (PC, Console, Mobile)', required: false }
    ]
  },
  {
    name: 'setup-lighting',
    description: 'Step-by-step guide for setting up realistic lighting in a scene',
    arguments: [
      { name: 'lightingType', description: 'Lighting type (indoor, outdoor, stylized)', required: false },
      { name: 'timeOfDay', description: 'Time of day (dawn, noon, dusk, night)', required: false }
    ]
  },
  {
    name: 'create-sequence',
    description: 'Guide for creating cinematics with Level Sequencer',
    arguments: [
      { name: 'sequenceType', description: 'Type (cutscene, gameplay, camera)', required: false },
      { name: 'duration', description: 'Target duration in seconds', required: false }
    ]
  }
];

/**
 * Get prompt messages for a given prompt name and arguments
 * @param name Prompt name
 * @param args Prompt arguments
 * @returns Array of prompt messages
 */
export function getPromptMessages(name: string, args: Record<string, string>): PromptMessage[] {
  switch (name) {
    case 'create-actor':
      return [{
        role: 'user',
        content: {
          type: 'text',
          text: `Guide me through spawning a ${args.actorType || 'StaticMeshActor'} actor${args.location ? ` at location ${args.location}` : ''}.

Key steps:
1. Use control_actor with action "spawn" or "spawn_blueprint"
2. Specify classPath for native actors or blueprintPath for Blueprints
3. Set location, rotation, and scale transforms
4. Optionally add components and tags after spawning

Common actor types: StaticMeshActor, PointLight, SpotLight, DirectionalLight, CameraActor, PlayerStart

Example tool call:
{
  "tool": "control_actor",
  "action": "spawn",
  "classPath": "/Script/Engine.${args.actorType || 'StaticMeshActor'}",
  "location": { "x": 0, "y": 0, "z": 0 },
  "rotation": { "pitch": 0, "yaw": 0, "roll": 0 }
}`
        }
      }];

    case 'debug-blueprint':
      return [{
        role: 'user',
        content: {
          type: 'text',
          text: `Help me debug the Blueprint at ${args.blueprintPath || '/Game/Blueprints/MyBP'}${args.errorType ? ` with ${args.errorType} errors` : ''}.

Debugging workflow:
1. Use manage_asset action "bp_compile" to check for compilation errors
2. Use manage_asset action "bp_get" to inspect Blueprint structure
3. Use manage_asset action "bp_get_graph_details" to examine nodes
4. Check for disconnected pins, missing variables, or circular references
5. Verify parent class compatibility and interface implementations

Common issues:
- Compilation Error: Check for disconnected pins or type mismatches
- Runtime Error: Look for null references or invalid array access
- Node Error: Verify function signatures and input/output types

Diagnostic tool calls:
{
  "tool": "manage_asset",
  "action": "bp_compile",
  "assetPath": "${args.blueprintPath || '/Game/Blueprints/MyBP'}"
}

{
  "tool": "manage_asset", 
  "action": "bp_get_graph_details",
  "assetPath": "${args.blueprintPath || '/Game/Blueprints/MyBP'}",
  "graphName": "EventGraph"
}`
        }
      }];

    case 'optimize-level':
      return [{
        role: 'user',
        content: {
          type: 'text',
          text: `Optimize the level${args.levelPath ? ` at ${args.levelPath}` : ''}${args.targetPlatform ? ` for ${args.targetPlatform}` : ''}.

Performance Optimization Checklist:

1. PROFILING - Identify bottlenecks first
   manage_performance action "profile" with profileType "GPU" or "CPU"

2. NANITE - Enable for high-poly static meshes
   manage_asset action "enable_nanite_mesh" for meshes > 50k triangles

3. LODs - Configure for non-Nanite meshes
   manage_asset action "generate_lods" with appropriate reduction settings

4. LIGHTING - Optimize shadow and GI settings
   manage_lighting action "configure_light" to reduce shadow quality for distant lights
   build_environment action "build_lighting" with quality "Preview" for testing

5. WORLD PARTITION - For large open worlds
   manage_level action "configure_world_partition" with appropriate grid sizes
   
6. HLOD - For distant geometry
   manage_level action "configure_hlod_layer" for LOD groups

7. CULLING - Set up visibility volumes
   manage_volumes action "create_volume" with volumeType "CullDistance"

${args.targetPlatform === 'Mobile' ? `
Mobile-specific optimizations:
- Reduce texture sizes with manage_material_authoring action "configure_texture"
- Disable volumetric fog and complex post-processing
- Use forward shading instead of deferred
` : ''}`
        }
      }];

    case 'setup-lighting':
      return [{
        role: 'user',
        content: {
          type: 'text',
          text: `Set up ${args.lightingType || 'outdoor'} lighting${args.timeOfDay ? ` for ${args.timeOfDay}` : ''}.

${args.lightingType === 'indoor' ? `
INDOOR LIGHTING SETUP:

1. Sky Light (ambient fill)
   manage_lighting action "create_light" with lightType "SkyLight"
   Set intensity low (0.5-1.0) for subtle ambient

2. Main Light Sources
   manage_level action "create_light" with lightType "Point" or "Spot"
   Place at practical light locations (lamps, windows)

3. Bounce Cards
   Use rect lights for fill: lightType "Rect"
   Low intensity (0.3-0.5), large area

4. Post Process Volume
   manage_lighting action "configure_post_process"
   Enable eye adaptation, bloom, ambient occlusion
` : `
OUTDOOR LIGHTING SETUP:

1. Sky Atmosphere (realistic sky rendering)
   build_environment action "create_sky_atmosphere"

2. Directional Light (sun)
   manage_level action "create_light" with lightType "Directional"
   ${args.timeOfDay === 'dawn' ? 'Rotation: pitch -10, warm color temperature' : 
     args.timeOfDay === 'noon' ? 'Rotation: pitch -80, neutral color' :
     args.timeOfDay === 'dusk' ? 'Rotation: pitch -15, orange/red tint' :
     args.timeOfDay === 'night' ? 'Very low intensity, blue tint for moonlight' :
     'Adjust pitch for desired sun angle'}

3. Sky Light (ambient)
   manage_lighting action "create_light" with lightType "SkyLight"
   Enable "Real Time Capture" for dynamic skies

4. Exponential Height Fog
   build_environment action "create_exponential_height_fog"
   Enable volumetric fog for god rays

5. Build Lighting
   build_environment action "build_lighting" with quality "Production"
`}`
        }
      }];

    case 'create-sequence':
      return [{
        role: 'user',
        content: {
          type: 'text',
          text: `Create a ${args.sequenceType || 'cutscene'} Level Sequence${args.duration ? ` with ${args.duration}s duration` : ''}.

SEQUENCER WORKFLOW:

1. CREATE SEQUENCE
   manage_sequence action "create_sequence"
   Set name and save path

2. ADD CAMERA (for cutscenes/cinematics)
   control_actor action "spawn" with classPath "CameraActor"
   manage_sequence action "bind_actor" to add camera to sequence

3. ADD CAMERA CUT TRACK
   manage_sequence action "add_track" with trackType "CameraCut"
   This controls which camera is active

4. BIND ACTORS
   manage_sequence action "bind_actor" for each actor to animate
   Creates tracks for transform, visibility, etc.

5. ADD KEYFRAMES
   manage_sequence action "add_keyframe"
   Specify time, property, and value

6. CONFIGURE PLAYBACK
   manage_sequence action "set_playback_settings"
   Set start/end times, loop mode

${args.sequenceType === 'gameplay' ? `
GAMEPLAY SEQUENCE TIPS:
- Use "Event Trigger" tracks for gameplay events
- Bind player character for scripted movement
- Use "Level Visibility" track to show/hide actors
` : ''}

${args.duration ? `
DURATION: ${args.duration}s
- Frame rate: 30fps = ${parseInt(args.duration) * 30} frames
- Frame rate: 60fps = ${parseInt(args.duration) * 60} frames
` : ''}

7. RENDER (Optional - MRQ)
   manage_sequence action "configure_mrq_render"
   manage_sequence action "start_mrq_render"`
        }
      }];

    default:
      log.warn(`Unknown prompt requested: ${name}`);
      return [{
        role: 'user',
        content: { 
          type: 'text', 
          text: `Unknown prompt: ${name}

Available prompts:
- create-actor: Guide for spawning actors with transforms and components
- debug-blueprint: Troubleshooting Blueprint issues
- optimize-level: Performance optimization checklist
- setup-lighting: Lighting setup guide
- create-sequence: Sequencer cinematics workflow

Use one of these prompt names to get detailed guidance.` 
        }
      }];
  }
}
