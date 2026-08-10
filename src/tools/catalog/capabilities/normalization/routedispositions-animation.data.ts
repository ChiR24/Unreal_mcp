/**
 * Animation (C21 / C16) and skeleton (C22 / O25) route dispositions.
 *
 * Skeleton: 15 hidden/promote (real functional routes absent from TS +
 * native metadata) + 1 raw/remove (preview_physics no-op). All 16 are
 * native Dispatched routes in McpAutomationBridge_SkeletonHandlers.cpp
 * absent from the TS SKELETON_ACTIONS enum and McpTool_ManageSkeleton.cpp.
 */
import { type RawRouteDisposition, ROUTE_EVIDENCE_PATHS } from './routedispositions-paths.js';

const { ANIM_HANDLERS, SKELETON_HANDLERS, ANIM_AUTHORING_IK_RETARGETING, TS_ANIM_AUTHORING_EVENTS } = ROUTE_EVIDENCE_PATHS;

/** 15 real functional skeleton routes: hidden, promote (v2 source-derived). */
const SKELETON_HIDDEN: ReadonlyArray<{
  readonly route: string;
  readonly symbol: string;
  readonly target: string;
  readonly rationale: string;
}> = [
  { route: 'add_socket', symbol: '{TEXT("add_socket"), &UMcpAutomationBridgeSubsystem::HandleCreateSocket}', target: 'cap:animation_physics:add_socket', rationale: 'C14/C22/O25: alias of create_socket, real mutation, absent from TS + native metadata; hidden, promote.' },
  { route: 'modify_socket', symbol: '{TEXT("modify_socket"), &UMcpAutomationBridgeSubsystem::HandleConfigureSocket}', target: 'cap:animation_physics:modify_socket', rationale: 'C14/C22/O25: alias of configure_socket; real mutation; hidden, promote.' },
  { route: 'modify_physics_body', symbol: '{TEXT("modify_physics_body"), &UMcpAutomationBridgeSubsystem::HandleConfigurePhysicsBody}', target: 'cap:animation_physics:modify_physics_body', rationale: 'C14/C22/O25: alias of configure_physics_body; real mutation; hidden, promote.' },
  { route: 'set_physics_asset', symbol: '{TEXT("set_physics_asset"), &UMcpAutomationBridgeSubsystem::HandleSetPhysicsAsset}', target: 'cap:animation_physics:set_physics_asset', rationale: 'C14/C22/O25: persistent mutation; hidden, promote.' },
  { route: 'remove_physics_body', symbol: '{TEXT("remove_physics_body"), &UMcpAutomationBridgeSubsystem::HandleRemovePhysicsBody}', target: 'cap:animation_physics:remove_physics_body', rationale: 'C14/C22/O25: persistent mutation; hidden, promote.' },
  { route: 'get_physics_asset_info', symbol: '{TEXT("get_physics_asset_info"), &UMcpAutomationBridgeSubsystem::HandleGetPhysicsAssetInfo}', target: 'cap:animation_physics:get_physics_asset_info', rationale: 'C14/C22/O25: read-only query; hidden, promote.' },
  { route: 'list_morph_targets', symbol: '{TEXT("list_morph_targets"), &UMcpAutomationBridgeSubsystem::HandleListMorphTargets}', target: 'cap:animation_physics:list_morph_targets', rationale: 'C14/C22/O25: read-only query; hidden, promote.' },
  { route: 'delete_morph_target', symbol: '{TEXT("delete_morph_target"), &UMcpAutomationBridgeSubsystem::HandleDeleteMorphTarget}', target: 'cap:animation_physics:delete_morph_target', rationale: 'C14/C22/O25: persistent mutation; hidden, promote.' },
  { route: 'delete_socket', symbol: '{TEXT("delete_socket"), &UMcpAutomationBridgeSubsystem::HandleDeleteSocket}', target: 'cap:animation_physics:delete_socket', rationale: 'C14/C22/O25: persistent mutation; hidden, promote.' },
  { route: 'remove_socket', symbol: '{TEXT("remove_socket"), &UMcpAutomationBridgeSubsystem::HandleDeleteSocket}', target: 'cap:animation_physics:remove_socket', rationale: 'C14/C22/O25: alias of delete_socket; persistent mutation; hidden, promote.' },
  { route: 'get_bone_transform', symbol: '{TEXT("get_bone_transform"), &UMcpAutomationBridgeSubsystem::HandleGetBoneTransform}', target: 'cap:animation_physics:get_bone_transform', rationale: 'C14/C22/O25: read-only query; hidden, promote.' },
  { route: 'list_virtual_bones', symbol: '{TEXT("list_virtual_bones"), &UMcpAutomationBridgeSubsystem::HandleListVirtualBones}', target: 'cap:animation_physics:list_virtual_bones', rationale: 'C14/C22/O25: read-only query; hidden, promote.' },
  { route: 'delete_virtual_bone', symbol: '{TEXT("delete_virtual_bone"), &UMcpAutomationBridgeSubsystem::HandleDeleteVirtualBone}', target: 'cap:animation_physics:delete_virtual_bone', rationale: 'C14/C22/O25: persistent mutation; hidden, promote.' },
  { route: 'set_physics_constraint', symbol: '{TEXT("set_physics_constraint"), &UMcpAutomationBridgeSubsystem::HandleAddPhysicsConstraint}', target: 'cap:animation_physics:set_physics_constraint', rationale: 'C14/C22/O25: mis-named mutation (NewObject constraint template + ConstraintSetup.Add); hidden, promote.' },
  { route: 'set_morph_target_value', symbol: '{TEXT("set_morph_target_value"), &UMcpAutomationBridgeSubsystem::HandleSetMorphTargetValue}', target: 'cap:animation_physics:set_morph_target_value', rationale: 'C14/C22/O25: runtime-only real behavior (writes live component weight, not persisted); real functional route absent from discovery; hidden, promote.' },
];

export const ANIMATION_SKELETON_ROUTE_DISPOSITIONS: readonly RawRouteDisposition[] = [
  // Animation (4 routes)
  {
    key: 'route:animation:create_pose_library',
    route: 'create_pose_library',
    domain: 'animation',
    status: 'dead',
    owner: 'Animation',
    evidenceSource: ANIM_HANDLERS,
    evidenceSymbol: '{TEXT("create_pose_library"), McpAnimationHandlers::HandleAnimationCreatePoseLibraryAction}',
    evidenceTool: 'animation_physics',
    disposition: 'remove',
    removalGuidance:
      'C21: one canonical live runtime route plus one unreachable authoring branch; remove the dead authoring branch.',
    rationale: 'C21: dead create_pose_library authoring branch proven unreachable.',
  },
  {
    key: 'route:animation:add_notify',
    route: 'add_notify',
    domain: 'animation',
    status: 'raw',
    owner: 'AnimationAuthoring',
    evidenceSource: TS_ANIM_AUTHORING_EVENTS,
    evidenceSymbol: "case 'add_notify':",
    evidenceTool: 'manage_animation_authoring',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_animation_authoring:add_notify',
    rationale:
      'C21/O24: raw frame-based authoring add_notify path reachable via manage_animation_authoring; map to that canonical id.',
  },
  {
    key: 'route:animation:set_retarget_chain_mapping',
    route: 'set_retarget_chain_mapping',
    domain: 'animation',
    status: 'raw',
    owner: 'AnimationAuthoring',
    evidenceSource: ANIM_AUTHORING_IK_RETARGETING,
    evidenceSymbol: 'SubAction == TEXT("set_retarget_chain_mapping")',
    evidenceTool: 'animation_physics',
    disposition: 'remove',
    removalGuidance:
      'C16: reports success without performing any mutation; no-op. Remove or implement real retarget-chain mapping.',
    rationale: 'C16: advertised behavior absent from leaf body (no mutation).',
  },
  {
    key: 'route:animation:assign_cloth_asset_to_mesh',
    route: 'assign_cloth_asset_to_mesh',
    domain: 'animation',
    status: 'raw',
    owner: 'Skeleton',
    evidenceSource: SKELETON_HANDLERS,
    evidenceSymbol: '{TEXT("assign_cloth_asset_to_mesh"), &UMcpAutomationBridgeSubsystem::HandleAssignClothAssetToMesh}',
    evidenceTool: 'animation_physics',
    disposition: 'remove',
    removalGuidance:
      'C16: explicitly performs no assignment (cloth assignment requires manual intervention); no-op. Remove or implement real assignment.',
    rationale: 'C16: no assignment performed despite success report.',
  },
  // Skeleton: 15 hidden/promote (real functional routes absent from discovery)
  ...SKELETON_HIDDEN.map((s) => ({
    key: `route:skeleton:${s.route}`,
    route: s.route,
    domain: 'skeleton',
    status: 'hidden' as const,
    owner: 'Skeleton',
    evidenceSource: SKELETON_HANDLERS,
    evidenceSymbol: s.symbol,
    evidenceTool: 'animation_physics',
    disposition: 'promote' as const,
    targetCanonicalId: s.target,
    rationale: s.rationale,
  })),
  // Skeleton: 1 raw/remove (deceptive no-op)
  {
    key: 'route:skeleton:preview_physics',
    route: 'preview_physics',
    domain: 'skeleton',
    status: 'raw',
    owner: 'Skeleton',
    evidenceSource: SKELETON_HANDLERS,
    evidenceSymbol: '{TEXT("preview_physics"), &HandlePreviewPhysicsAction}',
    evidenceTool: 'animation_physics',
    disposition: 'remove',
    removalGuidance:
      'C22/O25: success-reporting no-op; physics preview not actually applied. Remove or implement real preview.',
    rationale: 'C22/O25: deceptive no-op; returns success without mutating physics.',
  },
];
