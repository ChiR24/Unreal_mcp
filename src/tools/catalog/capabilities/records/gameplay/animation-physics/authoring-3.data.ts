/**
 * Animation/Physics authoring records — part 3 of 3 (ragdoll, vehicle,
 * physics simulation, blend-space ops, retarget, graph values, cleanup).
 *
 * RAGDOLL HONESTY: setup_ragdoll reaches the native runtime simulation body,
 * but the TypeScript special handler currently dispatches activate_ragdoll as
 * setup_ragdoll. Both native handlers are wrapped in
 * #if WITH_EDITOR and return NOT_IMPLEMENTED ("requires editor build") in
 * non-editor builds (Plugins/.../Animation/Physics/
 * McpAutomationBridge_AnimationHandlersSetupRagdoll.cpp:151,
 * McpAutomationBridge_AnimationHandlersActivateRagdoll.cpp:113). When an
 * editor build IS present they call SetSimulatePhysics / SetAllBodiesSimulate
 * Physics on the actor's USkeletalMeshComponent — a genuine runtime ragdoll,
 * not asset authoring. The records therefore use PIE/simulate states; the
 * misrouted activate action is retained for enum exhaustiveness but marked for
 * removal until its distinct route is reachable.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { A } from './animation-properties.js';

const T = 'animation_physics';
const F = 'physics';
const ESU = ['EditorScriptingUtilities'];
const IK = ['ControlRig', 'IKRig', 'EditorScriptingUtilities'];

export const ANIM_AUTHORED_3: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.play_montage`, action: 'play_montage', family: F,
    summary: 'Play a montage on a runtime actor (PIE).', whenToUse: ['A montage must play on a live actor.'], whenNotToUse: ['Author the montage asset.'],
    inputProps: { action: P.action, actorName: P.actorName, montagePath: P.assetPath, playRate: P.playRate }, required: ['action', 'actorName', 'montagePath'],
    effect: 'write', editorStates: ['pie', 'simulate'], behavior: { idempotency: 'idempotent', supportsUndo: false }, latency: 'interactive', resources: 'low',
    plugins: ESU,
    exampleInput: { action: 'play_montage', actorName: 'Char_1', montagePath: '/Game/MT_Slash', playRate: 1.25 }, exampleOutput: { success: true, message: 'Montage played' } }),
  buildRecord({ parentTool: T, id: `${T}.play_anim_montage`, action: 'play_anim_montage', family: F,
    summary: 'Alias of play_montage (short form).', whenToUse: ['Caller uses play_anim_montage verb.'], whenNotToUse: ['Use play_montage.'],
    inputProps: { action: P.action, actorName: P.actorName, montagePath: P.assetPath, playRate: P.playRate }, required: ['action', 'actorName', 'montagePath'],
    effect: 'write', editorStates: ['pie', 'simulate'], behavior: { idempotency: 'idempotent', supportsUndo: false }, latency: 'interactive', resources: 'low',
    plugins: ESU,
    normalizationClass: 'B_ALIAS', normalizationDisposition: 'alias', normalizationRationale: 'Alias of play_montage (animation-special-handlers.ts).',
    exampleInput: { action: 'play_anim_montage', actorName: 'Char_1', montagePath: '/Game/MT_Slash' }, exampleOutput: { success: true, message: 'Montage played' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_ragdoll`, action: 'setup_ragdoll', family: F,
    summary: 'Enable runtime ragdoll physics on a skeletal actor; requires an assigned PhysicsAsset and an editor build.',
    whenToUse: ['A skeletal actor must enter ragdoll simulation at runtime (editor build).'], whenNotToUse: ['Author the PhysicsAsset (use create_physics_asset).'],
    inputProps: { action: P.action, actorName: P.actorName, skeletalMeshPath: P.skeletalMeshPath }, required: ['action', 'actorName'],
    effect: 'write', editorStates: ['pie', 'simulate'], behavior: { idempotency: 'idempotent', supportsUndo: false, longRunning: false },
    latency: 'interactive', resources: 'low', plugins: ESU,
    normalizationClass: 'F_OBSOLETE_VERSION_SPECIFIC', normalizationRationale: 'Runtime ragdoll toggle; native handler editor-build-gated, returns ragdollActive bool (NOT_IMPLEMENTED off-editor).',
    outputProps: { ragdollActive: P.bool_, hasPhysicsAsset: P.bool_ }, outputRequired: ['ragdollActive'],
    exampleInput: { action: 'setup_ragdoll', actorName: 'Char_1' }, exampleOutput: { success: true, message: 'Ragdoll setup completed', ragdollActive: true, hasPhysicsAsset: true } }),
  buildRecord({ parentTool: T, id: `${T}.activate_ragdoll`, action: 'activate_ragdoll', family: F,
    summary: 'Switch ragdoll simulation on/off at runtime on a skeletal actor via the distinct native activate_ragdoll action (editor build).',
    whenToUse: ['Ragdoll must be switched on/off at runtime (editor build).'], whenNotToUse: ['Use setup_ragdoll for initial enable/physics asset assignment.'],
    inputProps: { action: P.action, actorName: P.actorName, activate: P.activate }, required: ['action', 'actorName'],
    effect: 'write', editorStates: ['pie', 'simulate'], behavior: { idempotency: 'idempotent', longRunning: false },
    latency: 'interactive', resources: 'low', plugins: ESU,
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationDisposition: 'canonical', normalizationRationale: 'Repaired in Task 21: animation-special-handlers.ts now routes activate_ragdoll to the distinct native activate_ragdoll body, so the toggle is reachable.',
    deprecation: { status: 'active' },
    outputProps: { ragdollActive: P.bool_, hasPhysicsAsset: P.bool_ }, outputRequired: ['ragdollActive'],
    exampleInput: { action: 'activate_ragdoll', actorName: 'Char_1', activate: true }, exampleOutput: { success: true, message: 'Ragdoll activation state changed', ragdollActive: true, hasPhysicsAsset: true } }),
  buildRecord({ parentTool: T, id: `${T}.configure_vehicle`, action: 'configure_vehicle', family: F,
    summary: 'Configure vehicle movement on a skeletal actor.', whenToUse: ['A vehicle must be set up (ChaosVehicles).'], whenNotToUse: ['Use configure_physics_simulation.'],
    inputProps: { action: P.action, actorName: P.actorName, vehicleType: P.vehicleType, mass: P.mass, dragCoefficient: P.dragCoefficient }, required: ['action', 'actorName'],
    effect: 'write', editorStates: ['edit'], latency: 'interactive', resources: 'medium', plugins: ['ChaosVehiclesPlugin', 'EditorScriptingUtilities'],
    exampleInput: { action: 'configure_vehicle', actorName: 'Car_1', vehicleType: 'WheeledVehicle4W', dragCoefficient: 0.32 }, exampleOutput: { success: true, message: 'Vehicle configured' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_physics_simulation`, action: 'setup_physics_simulation', family: F,
    summary: 'Generate and assign a Physics Asset so a skeletal mesh can simulate.', whenToUse: ['A skeletal mesh must simulate physics.'], whenNotToUse: ['Use configure_vehicle.'],
    inputProps: { action: P.action, actorName: P.actorName, skeletonPath: P.skeletonPath, skeletalMeshPath: P.skeletalMeshPath, savePath: A.savePath, physicsAssetName: A.physicsAssetName, assignToMesh: A.assignToMesh }, required: ['action'],
    effect: 'write', editorStates: ['edit', 'pie'], behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    plugins: ESU, outputProps: { existingAsset: P.bool_, meshPath: P.meshPath }, outputRequired: [],
    exampleInput: { action: 'setup_physics_simulation', skeletalMeshPath: '/Game/SKM_Char', savePath: '/Game/Physics', physicsAssetName: 'PHYS_Char', assignToMesh: false },
    exampleOutput: { success: true, message: 'Physics simulation set', existingAsset: false } }),
  buildRecord({ parentTool: T, id: `${T}.add_blend_sample`, action: 'add_blend_sample', family: F,
    summary: 'Add a blend sample to a Blend Space.', whenToUse: ['Blend sample needed.'], whenNotToUse: ['Use create_blend_space.'],
    inputProps: { action: P.action, assetPath: P.assetPath, sampleValue: P.sampleValue, animationPath: P.animationPath }, required: ['action', 'assetPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_blend_sample', assetPath: '/Game/BS_Locomotion', animationPath: '/Game/A_Run', sampleValue: 100 }, exampleOutput: { success: true, message: 'Blend sample added' } }),
  buildRecord({ parentTool: T, id: `${T}.force_rebuild_blend_space`, action: 'force_rebuild_blend_space', family: F,
    summary: 'Force a Blend Space rebuild and recompile referencers.', whenToUse: ['Axis bounds changed and dependents must recompile.'], whenNotToUse: ['Normal save suffices.'],
    inputProps: { action: P.action, assetPath: P.assetPath, compileReferencers: P.compileReferencers, rebuildBlendParameters: P.rebuildBlendParameters, save: P.save }, required: ['action', 'assetPath'],
    effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'medium', plugins: ESU,
    outputProps: { rebuiltBlendParameters: P.bool_, referencersCompiled: P.num_, compiledAnimBlueprints: P.arrayOfStrings }, outputRequired: [],
    exampleInput: { action: 'force_rebuild_blend_space', assetPath: '/Game/BS_Locomotion', compileReferencers: true, rebuildBlendParameters: true },
    exampleOutput: { success: true, message: 'Blend space rebuilt', referencersCompiled: 3 } }),
  buildRecord({ parentTool: T, id: `${T}.set_axis_settings`, action: 'set_axis_settings', family: F,
    summary: 'Set Blend Space axis settings.', whenToUse: ['Axis labels/ranges must change.'], whenNotToUse: ['Use add_blend_sample.'],
    inputProps: { action: P.action, assetPath: P.assetPath, axis: A.axis, axisName: P.axisName, minValue: P.minValue, maxValue: P.maxValue }, required: ['action', 'assetPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'set_axis_settings', assetPath: '/Game/BS_Locomotion', axis: 'X', axisName: 'Speed', minValue: 0, maxValue: 600 }, exampleOutput: { success: true, message: 'Axis settings set' } }),
  buildRecord({ parentTool: T, id: `${T}.set_interpolation_settings`, action: 'set_interpolation_settings', family: F,
    summary: 'Set Blend Space interpolation settings.', whenToUse: ['Interpolation must change.'], whenNotToUse: ['Use set_axis_settings.'],
    inputProps: { action: P.action, assetPath: P.assetPath, interpolationType: P.interpolationType }, required: ['action', 'assetPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'set_interpolation_settings', assetPath: '/Game/BS_Locomotion', interpolationType: 'Lerp' }, exampleOutput: { success: true, message: 'Interpolation set' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_retargeting`, action: 'setup_retargeting', family: F,
    summary: 'Configure retargeting between skeletons.', whenToUse: ['Cross-skeleton retarget needed.'], whenNotToUse: ['Use create_ik_retargeter.'],
    inputProps: { action: P.action, sourceSkeleton: P.skeletonPath, targetSkeleton: P.skeletonPath, assets: A.assets, savePath: A.savePath, suffix: A.suffix, overwrite: P.overwrite }, required: ['action'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: IK,
    exampleInput: { action: 'setup_retargeting', sourceSkeleton: '/Game/SK_A', targetSkeleton: '/Game/SK_B', assets: ['/Game/A_Run'], savePath: '/Game/Retargeted', suffix: '_Retargeted', overwrite: true },
    exampleOutput: { success: true, message: 'Retargeting set up' } }),
  buildRecord({ parentTool: T, id: `${T}.add_layered_blend_per_bone`, action: 'add_layered_blend_per_bone', family: F,
    summary: 'Add a per-bone layered blend node.', whenToUse: ['Partial-skeleton layering needed.'], whenNotToUse: ['Use add_blend_node.'],
    inputProps: { action: P.action, assetPath: P.assetPath, blueprintPath: P.blueprintPath, layerSetup: A.layerSetup }, required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_layered_blend_per_bone', blueprintPath: '/Game/ABP_Char', layerSetup: [{ branchFilters: [{ boneName: 'spine_01', blendDepth: 1 }] }] }, exampleOutput: { success: true, message: 'Layered blend added' } }),
  buildRecord({ parentTool: T, id: `${T}.set_anim_graph_node_value`, action: 'set_anim_graph_node_value', family: F,
    summary: 'Set a value on an AnimGraph node.', whenToUse: ['A graph node property must change.'], whenNotToUse: ['Use add_blend_node.'],
    inputProps: { action: P.action, assetPath: P.assetPath, blueprintPath: P.blueprintPath, nodeName: P.nodeName, propertyName: P.propertyName, value: P.value }, required: ['action', 'nodeName'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'set_anim_graph_node_value', blueprintPath: '/Game/ABP_Char', nodeName: 'Blend', propertyName: 'NodeComment', value: 'Updated' }, exampleOutput: { success: true, message: 'Node value set' } }),
  buildRecord({ parentTool: T, id: `${T}.set_retarget_chain_mapping`, action: 'set_retarget_chain_mapping', family: F,
    summary: 'Configure retarget chain mapping (documented no-op).', whenToUse: ['Chain mapping intended.'], whenNotToUse: ['Use setup_retargeting.'],
    inputProps: { action: P.action, assetPath: P.assetPath, sourceChain: P.sourceChain, targetChain: P.targetChain }, required: ['action', 'assetPath'],
    effect: 'read', latency: 'interactive', resources: 'low', plugins: IK,
    normalizationClass: 'F_OBSOLETE_VERSION_SPECIFIC', normalizationDisposition: 'remove', normalizationRationale: 'Raw no-op documented in route-disposition ledger (route:animation:set_retarget_chain_mapping).',
    deprecation: { status: 'deprecated', since: '5.0', guidance: 'Documented no-op: the route performs no mutation. Prefer setup_retargeting for real retarget configuration.' },
    exampleInput: { action: 'set_retarget_chain_mapping', assetPath: '/Game/IKRT_Char', sourceChain: 'Root', targetChain: 'Root' }, exampleOutput: { success: true, message: 'Retarget chain mapping no-op (no mutation performed)' } }),
  buildRecord({ parentTool: T, id: `${T}.get_animation_info`, action: 'get_animation_info', family: F,
    summary: 'Read animation asset metadata.', whenToUse: ['Inspect animation assets.'], whenNotToUse: ['Mutate assets.'],
    inputProps: { action: P.action, assetPath: P.assetPath }, required: ['action', 'assetPath'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: ESU,
    outputProps: { length: P.num_, frameRate: P.num_ }, outputRequired: [],
    exampleInput: { action: 'get_animation_info', assetPath: '/Game/A_Run' }, exampleOutput: { success: true, message: 'Animation info', length: 1.0, frameRate: 30 } }),
  buildRecord({ parentTool: T, id: `${T}.cleanup`, action: 'cleanup', family: F,
    summary: 'Delete the listed transient animation authoring assets.', whenToUse: ['Reset authoring session.'], whenNotToUse: ['Persist work.'],
    inputProps: { action: P.action, artifacts: A.artifacts }, required: ['action'],
    effect: 'destructive', behavior: { safeToRetry: true, supportsUndo: false }, latency: 'instant', resources: 'low',
    plugins: ESU, exampleInput: { action: 'cleanup', artifacts: ['/Game/MCPTest/ABP_Temp'] }, exampleOutput: { success: true, message: 'Cleanup done' } }),
];
