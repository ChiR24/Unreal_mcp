/**
 * Per-action JSON-schema property fragments for manage_character.
 */
import type { PropertyMap } from '../properties.js';
import { str, num, bool } from '../../shared/schema-props.js';


export const CHARACTER_P: PropertyMap = {
  capsuleRadius: num('Capsule collision radius in world units.'),
  capsuleHalfHeight: num('Capsule collision half-height in world units.'),
  animBlueprintPath: str('Canonical /Game Animation Blueprint asset path.'),
  meshOffset: {
    type: 'object',
    description: 'Mesh relative location offset.',
    properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
    additionalProperties: false,
  },
  meshRotation: {
    type: 'object',
    description: 'Mesh relative rotation in degrees.',
    properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } },
    additionalProperties: false,
  },

  springArmLength: num('Spring-arm target arm length.'),
  springArmLagEnabled: bool('Whether spring-arm camera lag is enabled.'),
  springArmLagSpeed: num('Spring-arm camera lag speed.'),
  cameraUsePawnControlRotation: bool('Whether the spring arm follows the controller look rotation. The camera under it is always arm-relative.'),

  walkSpeed: num('Maximum walk speed.'),
  runSpeed: num('Run speed; ignored when walkSpeed is also supplied.'),
  crouchSpeed: num('Maximum walk speed while crouched.'),
  swimSpeed: num('Maximum swim speed.'),
  flySpeed: num('Maximum fly speed.'),
  acceleration: num('Maximum acceleration.'),
  deceleration: num('Walking deceleration.'),
  brakingDeceleration: num('Braking deceleration while walking.'),
  groundFriction: num('Ground friction.'),

  jumpHeight: num('Jump z-velocity.'),
  jumpHoldTime: num('Maximum jump hold time in seconds.'),
  maxJumpCount: num('Maximum number of jumps.'),
  airControl: num('Air control fraction (0-1).'),
  gravityScale: num('Gravity scale multiplier.'),
  fallingLateralFriction: num('Lateral friction while falling.'),

  orientToMovement: bool('Whether the character orients rotation to movement.'),
  useControllerRotationYaw: bool('Whether controller yaw drives actor rotation.'),
  useControllerRotationPitch: bool('Whether controller pitch drives actor rotation.'),
  useControllerRotationRoll: bool('Whether controller roll drives actor rotation.'),
  rotationRate: num('Rotation rate in degrees per second.'),

  navAgentRadius: num('Nav agent radius.'),
  navAgentHeight: num('Nav agent height.'),
  avoidanceEnabled: bool('Whether RVO avoidance is enabled.'),

  canCrouch: bool('Whether the character is allowed to crouch.'),
  crouchedHalfHeight: num('Capsule half-height while crouched.'),



  // --- MetaHuman Creator (UE 5.6+) -------------------------------------------
  // Names are the fields the reflective handlers under
  // plugins/.../Private/Domains/MetaHuman/ actually read; the struct-side
  // UPROPERTY names they map onto are an implementation detail of that layer.
  characterPath: str('Canonical /Game path of a MetaHuman Character asset.'),
  rigType: {
    type: 'string',
    description: 'Face rig detail to request. JointsAndBlendShapes is required for facial animation.',
    enum: ['JointsOnly', 'JointsAndBlendShapes'],
  },
  blocking: bool('Wait for auto-rigging to finish before answering. Default false: the request is queued on the Epic cloud service and metahuman_status reports canBuild once it lands. True parks the editor until the service answers.'),
  reportProgress: bool('Emit editor progress notifications during auto-rigging. Default false.'),
  pipelineType: {
    type: 'string',
    description: 'Assembly pipeline. Cinematic is the film-quality path; Optimized and UEFN trade fidelity for runtime cost.',
    enum: ['Cinematic', 'Optimized', 'UEFN'],
  },
  pipelineQuality: {
    type: 'string',
    description: 'Quality level. Only meaningful when pipelineType is Optimized or UEFN.',
    enum: ['Low', 'Medium', 'High', 'Cinematic'],
  },
  buildPath: str('Content folder for the assembled assets. Defaults to the palette setting.'),
  commonFolderPath: str('Content folder for shared MetaHuman assets.'),
  nameOverride: str('Folder name for the assembled character instead of the asset name.'),
  exportType: {
    type: 'string',
    description: 'Which artifact to export: skeletal meshes, material instances, or DNA.',
    enum: ['geometry', 'materials', 'dna'],
  },
  projectPath: str('Content folder receiving the exported assets.'),
  externalPath: str('Folder on disk for exported .dna files. DNA export only.'),
  headMesh: bool('Export the head skeletal mesh. Default true.'),
  bodyMesh: bool('Export the body skeletal mesh. Default true.'),
  fullBodyMesh: bool('Export a combined full-body skeletal mesh. Default false.'),
  dnaHead: bool('Export head DNA. Default true.'),
  dnaBody: bool('Export body DNA. Default true.'),
  applyAsOverrides: bool('Apply exported materials back onto the character as overrides. Default true.'),
  overwrite: bool('Overwrite existing assets instead of creating uniquely-named ones. Default true.'),
};
