/**
 * Niagara/effect-specific JSON-schema property fragments.
 *
 * Private to the manage_effect record family. Every fragment here names a field
 * that a manage_effect handler actually reads: either the TypeScript effect
 * handlers (src/tools/handlers/{effect,niagara}/) or the native Effect/Niagara
 * domains (Private/Domains/{Effect,Niagara*}/). Fields the handlers never read
 * do not belong in this file.
 */
import type { JsonObject } from '../../../index.js';
import type { PropertyMap } from '../properties.js';

const str = (desc: string): JsonObject => ({ type: 'string', description: desc });
const num = (desc: string): JsonObject => ({ type: 'number', description: desc });
const bool = (desc: string): JsonObject => ({ type: 'boolean', description: desc });
const vector = (desc: string): JsonObject => ({
  type: 'object',
  properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
  additionalProperties: false,
  description: desc,
});

export const E: PropertyMap = {
  systemPath: str('Canonical /Game Niagara System asset path.'),
  systemName: str('Runtime Niagara system/component name on the target actor.'),
  system: str('Niagara system path; alias normalized to systemPath.'),
  emitterName: str('Emitter name within the target Niagara system.'),
  emitter: str('Emitter name; alias normalized to emitterName.'),
  emitterProperties: {
    type: 'object',
    additionalProperties: true,
    description: 'Emitter property key-value pairs applied by reflection.',
    'x-unreal-reflection-boundary': true,
  },
  savePath: str('Canonical /Game directory for the created asset.'),
  scriptType: str('Niagara script usage (System, Emitter, Particle).'),
  modulePath: str('Canonical /Niagara module script path to insert.'),
  nodeId: str('Niagara graph node identifier.'),
  targetNodeId: str('Niagara graph node the dynamic input is attached to.'),
  autoConnect: bool('Whether inserted pins are auto-connected.'),
  dynamicInputScriptPath: str('Canonical /Niagara dynamic input script path.'),
  inputName: str('Module input name receiving the dynamic input.'),
  replaceExisting: bool('Whether an existing dynamic input is replaced.'),
  parameterName: str('Niagara parameter name.'),
  parameterType: str('Niagara parameter type (Float, Vector, Color, ...).'),
  parameterValue: { description: 'Niagara parameter value (any type).' },
  sourceBinding: str('Data source the parameter binds to.'),
  spawnRate: num('Particles spawned per second.'),
  burstCount: num('Particle count emitted per burst.'),
  burstTime: num('Normalized emitter time at which the burst fires.'),
  spawnPerUnit: num('Particles spawned per unit of movement.'),
  lifetime: num('Particle lifetime in seconds.'),
  forceType: str('Force module type (Gravity, Drag, Wind, Curl, Vortex, PointAttraction).'),
  forceStrength: num('Force magnitude applied to particles.'),
  acceleration: vector('Constant acceleration vector applied to particles.'),
  velocityMode: str('Velocity module mode (Linear, Cone, FromPoint).'),
  sizeMode: str('Sprite size mode.'),
  uniformSize: num('Uniform sprite size.'),
  colorMode: str('Color module mode.'),
  color: { description: 'Color as an {r,g,b,a} object or an [r, g, b, a] array.' },
  cameraOffset: num('Camera-relative offset distance.'),
  collisionMode: str('Particle collision mode.'),
  dieOnCollision: bool('Whether particles are destroyed on collision.'),
  friction: num('Collision friction coefficient.'),
  restitution: num('Collision restitution (bounciness).'),
  killCondition: str('Expression deciding when particles are killed.'),
  materialPath: str('Canonical /Game material asset path for the renderer.'),
  lightRadius: num('Per-particle light radius.'),
  eventName: str('Niagara event name.'),
  eventType: str('Niagara event generator type (Location, Death, Collision).'),
  eventPayload: {
    type: 'array',
    items: {
      type: 'object',
      properties: { name: { type: 'string' }, type: { type: 'string' } },
      additionalProperties: false,
    },
    description: 'Event payload attribute descriptors as [{ name, type }].',
  },
  eventSpawnCount: num('Particles spawned per received event.'),
  spawnOnEvent: bool('Whether the receiver spawns particles on each event.'),
  stageName: str('Simulation stage name.'),
  stageIterationSource: str('Simulation stage iteration source.'),
  deterministicEnabled: bool('Whether deterministic GPU simulation is enabled.'),
  fixedBoundsEnabled: bool('Whether fixed system bounds are enabled.'),
  attachToActor: str('Name of the actor the spawned system attaches to.'),
  reset: bool('Whether the effect restarts from its initial state.'),
  deltaTime: num('Simulation delta time per step, in seconds.'),
  steps: num('Number of simulation steps to advance.'),
  filter: str('Actor name prefix selecting which transient effects are cleaned up.'),
  preset: str('Particle preset name or canonical /Game particle asset path.'),
  shapeType: str('Debug shape type (Sphere, Box, Line, Capsule).'),
  intensity: num('Light intensity.'),
  lightType: str('Dynamic light type (Point, Spot, Directional, Rect).'),
  density: num('Volumetric fog density.'),
  scattering: num('Volumetric fog scattering distribution.'),
  extinction: num('Volumetric fog extinction scale.'),
  timeoutMs: num('Per-request timeout in milliseconds applied by the bridge dispatcher.'),
};
