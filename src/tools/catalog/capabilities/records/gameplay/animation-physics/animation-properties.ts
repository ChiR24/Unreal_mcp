/**
 * Animation/physics-specific JSON-schema property fragments.
 */
import type { JsonObject } from '../../../model.js';
import type { PropertyMap } from '../properties.js';
import { bool, num, str, vec3 } from '../../shared/schema-props.js';

const objectList = (desc: string): JsonObject => ({
  type: 'array',
  items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
  description: desc,
});

export const A: PropertyMap = {
  save: bool('Save the asset to disk after the change (default true). With false the change stays in memory only and is lost when the editor restarts.'),
  savePath: str('Canonical /Game directory for generated assets.'),
  bodyMass: num('Body mass in kg, above 0: sets the body\'s mass override (without one its mass comes from density and volume).'),
  bodySimulatePhysics: bool('true: the body simulates whenever the mesh simulates physics (PhysicsType Default, as in a ragdoll); false: it follows the animation (Kinematic).'),
  positionX: num('Graph X position of the new node (default 0).'),
  positionY: num('Graph Y position of the new node (default 0).'),
  graphBlueprintPath: str('Animation Blueprint whose AnimGraph is edited.'),
  abpParentClass: str('AnimInstance subclass the Blueprint derives from (default AnimInstance): a native class name or an Animation Blueprint path.'),
  meshSkeleton: str('Skeletal mesh whose skeleton to use when skeletonPath is omitted.'),
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
  layerSetup: objectList('One entry per blend pose, in pin order: [{ branchFilters: [{ boneName, blendDepth }] }]. A filter takes boneName and every bone under it from that blend pose over the base pose: blendDepth 0 (default) at full weight at once, N ramps the weight up over N bones down the chain (the named bone gets 1/N), and a negative depth on a bone under an earlier filter takes that branch back out (-1 removes it). Every bone must be on the Animation Blueprint\'s skeleton (BONE_NOT_FOUND).'),
  boneTracks: objectList(
    'Bone tracks to key: [{ boneName, frames: [{ frame, rotationDelta?: {pitch,yaw,roll}, '
    + 'rotation?: {pitch,yaw,roll}|{x,y,z,w}, location?: {x,y,z}, scale?: {x,y,z} }] }]. '
    + 'frame runs 0 to numFrames inclusive. Only the poses that matter need keys: frames between '
    + 'two keys of a channel are blended (rotation slerped), frames before its first key or '
    + 'after its last hold that key. Channels left out keep the reference pose of that bone, so '
    + 'a rotation-only track poses without collapsing the skeleton. Prefer rotationDelta, which '
    + 'bends the bone relative to its rest orientation; plain rotation replaces the local '
    + 'rotation outright and needs the rest orientation to already be known. Calling it again with '
    + 'the name of an existing clip re-keys that clip from empty (its additive settings are kept).',
  ),
  weights: objectList('Per-vertex skin weight descriptors with bone influences.'),
  deltas: objectList('Per-vertex morph target position deltas.'),
  pitch: num('Aim offset pitch in degrees.'),
  yaw: num('Aim offset yaw in degrees.'),
  startFrame: num('First frame of the notify state range.'),
  endFrame: num('Last frame of the notify state range.'),
  trackIndex: num('Notify track index within the animation sequence.'),
  notifyClass: str('AnimNotify (or AnimNotifyState) class name; prefixed with AnimNotify_/AnimNotifyState_ automatically when missing.'),
  basePoseFrame: num('Frame used as the additive base pose.'),
  forceRootLock: bool('Whether root motion is force-locked to the reference pose.'),
  bodyType: str('Shape added to the bone\'s body: Capsule (default), Sphere or Box; any other name is refused.'),
  center: vec3('Physics body centre offset relative to its bone as [x, y, z].'),
  constraintName: str('Name of the created physics constraint.'),
  physicsAssetName: str('Name of the generated Physics Asset.'),
  assignToMesh: bool('Whether the generated Physics Asset is assigned to the mesh.'),
  axis: str('Mirror or blend axis (X, Y, Z).'),
  profileName: str('Skin weight profile name.'),
  lodIndex: num('Skeletal mesh LOD index (default 0).'),
  dropRenderOnlyMorphs: bool('The edit rebuilds the mesh from its source data, which deletes morph targets that exist only in its render data, not its source data (left by tools that wrote render data alone). Without true, a mesh that has any is refused with RENDER_ONLY_MORPHS naming them and nothing changes (default false).'),
  limits: {
    type: 'object',
    additionalProperties: false,
    properties: {
      swing1LimitAngle: num('Swing 1 limit in degrees.'), swing2LimitAngle: num('Swing 2 limit in degrees.'), twistLimitAngle: num('Twist limit in degrees.'),
      swing1Motion: str('Free, Limited or Locked.'), swing2Motion: str('Free, Limited or Locked.'), twistMotion: str('Free, Limited or Locked.'),
    },
    description: 'Angular limits. Only the axes named change; an angle without a motion makes that axis Limited.',
  },
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
