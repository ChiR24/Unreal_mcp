/**
 * Animation/physics-specific JSON-schema property fragments.
 *
 * Private to the animation_physics record family. Every fragment here names a
 * field an animation_physics handler actually reads: either the TypeScript
 * animation/skeleton handlers (src/tools/handlers/{animation,skeleton}/) or the
 * native Animation/AnimationAuthoring/Skeleton domains. Fields the handlers
 * never read do not belong in this file.
 */
import type { JsonObject } from '../../../index.js';
import type { PropertyMap } from '../properties.js';
import { bool, num, str, vec3 } from '../../shared/schema-props.js';

const objectList = (desc: string): JsonObject => ({
  type: 'array',
  items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
  description: desc,
});

export const A: PropertyMap = {
  savePath: str('Canonical /Game directory for generated assets.'),
  outputPath: str('Canonical /Game path for the generated Physics Asset.'),
  rootBoneName: str('Root bone name for the created skeleton.'),
  attachBoneName: str('Bone the socket attaches to.'),
  relativeLocation: vec3('Socket location relative to its attach bone as [x, y, z].'),
  relativeRotation: vec3('Socket rotation relative to its attach bone as [pitch, yaw, roll].'),
  relativeScale: vec3('Socket scale relative to its attach bone as [x, y, z].'),
  scale: { description: 'Uniform scale factor, or non-uniform scale as [x, y, z].' },
  removeChildren: bool('Whether child bones are removed with the target bone.'),
  stateMachineName: str('Target state machine name inside the Animation Blueprint.'),
  conditionVariable: str('Animation Blueprint variable the transition rule tests, e.g. Speed or bFalling. Without it the rule graph is left unconnected, which reads as false forever and the state machine never leaves its entry state.'),
  conditionComparison: str('How conditionVariable is tested: greater (default), less, greater_equal, less_equal for a numeric variable; true or false for a bool.'),
  conditionValue: num('Right-hand value for a numeric conditionComparison (default 0). Ignored for true/false.'),
  crossfadeDuration: num('Transition blend duration in seconds. blendTime is accepted as an alias.'),
  priorityOrder: num('Evaluation order when several transitions out of one state can fire on the same frame; the lowest wins.'),
  automaticRule: bool('Fire the transition when the source state\'s sequence player reaches the end of its animation instead of testing a condition. Left alone when omitted.'),
  bidirectional: bool('Whether the transition also applies from toState back to fromState. Left alone when omitted.'),
  blendType: str('Blend node type (TwoWayBlend, BlendListByBool, BlendListByInt).'),
  layerSetup: objectList('Layered blend-per-bone branch filter descriptors.'),
  boneTracks: objectList(
    'Bone tracks to key: [{ boneName, frames: [{ frame, rotationDelta?: {pitch,yaw,roll}, '
    + 'rotation?: {pitch,yaw,roll}|{x,y,z,w}, location?: {x,y,z}, scale?: {x,y,z} }] }]. '
    + 'Channels left out keep the reference pose of that bone, so a rotation-only track poses '
    + 'without collapsing the skeleton. Prefer rotationDelta, which bends the bone relative '
    + 'to its rest orientation; plain rotation replaces the local rotation outright and needs '
    + 'the rest orientation to already be known.',
  ),
  weights: objectList('Per-vertex skin weight descriptors with bone influences.'),
  deltas: objectList('Per-vertex morph target position deltas.'),
  pitch: num('Aim offset pitch in degrees.'),
  yaw: num('Aim offset yaw in degrees.'),
  startFrame: num('First frame of the notify state range.'),
  endFrame: num('Last frame of the notify state range.'),
  trackIndex: num('Notify track index within the animation sequence.'),
  notifyClass: str('AnimNotify (or AnimNotifyState) class name; prefixed with AnimNotify_/AnimNotifyState_ automatically when missing.'),
  basePoseType: str('Additive base pose type (RefPose, AnimScaled, AnimFrame).'),
  basePoseFrame: num('Frame used as the additive base pose.'),
  forceRootLock: bool('Whether root motion is force-locked to the reference pose.'),
  bodyType: str('Physics body primitive type (Sphere, Box, Capsule).'),
  center: vec3('Physics body centre offset relative to its bone as [x, y, z].'),
  constraintName: str('Name of the created physics constraint.'),
  physicsAssetName: str('Name of the generated Physics Asset.'),
  assignToMesh: bool('Whether the generated Physics Asset is assigned to the mesh.'),
  axis: str('Mirror or blend axis (X, Y, Z).'),
  profileName: str('Skin weight profile name.'),
  lodIndex: num('Skeletal mesh LOD index.'),
  targetMeshPath: str('Canonical /Game skeletal mesh receiving the copied weights.'),
  morphTargetPath: str('Canonical /Game asset the morph targets are imported from.'),
  assets: {
    type: 'array',
    items: { type: 'string' },
    description: 'Canonical /Game animation asset paths to retarget.',
  },
  artifacts: {
    type: 'array',
    items: { type: 'string' },
    description: 'Canonical /Game asset paths to delete during cleanup.',
  },
  suffix: str('Suffix appended to each retargeted asset name.'),
};
