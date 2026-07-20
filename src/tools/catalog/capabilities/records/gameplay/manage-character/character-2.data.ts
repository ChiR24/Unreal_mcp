/**
 * manage_character records — part 2 of 2 (advanced locomotion, footstep system,
 * crouch/sprint, info). Grounded in manage-character-tool.ts enum and native
 * Character domain. Authoring actions mutate the Character Blueprint asset.
 *
 * As in part 1, each record's optional set is exactly the fields the matching
 * native handler reads; action-specific parameters live in ./character.props.ts.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { CHARACTER_P as C } from './character.props.js';

const T = 'manage_character';
const F = 'character';

export const CHARACTER_2: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.setup_mantling`, action: 'setup_mantling', family: F,
    summary: 'Set up mantling / ledge climb.', whenToUse: ['Vault-over-ledges needed.'], whenNotToUse: ['Use setup_vaulting.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, mantleHeight: C.mantleHeight, mantleReachDistance: C.mantleReachDistance }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'setup_mantling', blueprintPath: '/Game/BP_Char', mantleHeight: 200, mantleReachDistance: 100 }, exampleOutput: { success: true, message: 'Mantling set up' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_vaulting`, action: 'setup_vaulting', family: F,
    summary: 'Set up vaulting over obstacles.', whenToUse: ['Vault needed.'], whenNotToUse: ['Use setup_mantling.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, vaultHeight: C.vaultHeight, vaultDepth: C.vaultDepth }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'setup_vaulting', blueprintPath: '/Game/BP_Char', vaultHeight: 100, vaultDepth: 100 }, exampleOutput: { success: true, message: 'Vaulting set up' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_climbing`, action: 'setup_climbing', family: F,
    summary: 'Set up ladder/wracket climbing.', whenToUse: ['Climbing needed.'], whenNotToUse: ['Use setup_wall_running.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, climbSpeed: C.climbSpeed, climbableTag: C.climbableTag }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'setup_climbing', blueprintPath: '/Game/BP_Char', climbSpeed: 300, climbableTag: 'Climbable' }, exampleOutput: { success: true, message: 'Climbing set up' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_sliding`, action: 'setup_sliding', family: F,
    summary: 'Set up sliding.', whenToUse: ['Slide needed.'], whenNotToUse: ['Use setup_climbing.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, slideSpeed: C.slideSpeed, slideDuration: C.slideDuration, slideCooldown: C.slideCooldown }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'setup_sliding', blueprintPath: '/Game/BP_Char', slideSpeed: 800, slideDuration: 1 }, exampleOutput: { success: true, message: 'Sliding set up' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_wall_running`, action: 'setup_wall_running', family: F,
    summary: 'Set up wall running.', whenToUse: ['Wall run needed.'], whenNotToUse: ['Use setup_sliding.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, wallRunSpeed: C.wallRunSpeed, wallRunDuration: C.wallRunDuration, wallRunGravityScale: C.wallRunGravityScale }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'setup_wall_running', blueprintPath: '/Game/BP_Char', wallRunSpeed: 600, wallRunDuration: 2 }, exampleOutput: { success: true, message: 'Wall running set up' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_grappling`, action: 'setup_grappling', family: F,
    summary: 'Set up grappling hook.', whenToUse: ['Grapple needed.'], whenNotToUse: ['Use setup_wall_running.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, grappleRange: C.grappleRange, grappleSpeed: C.grappleSpeed, grappleTargetTag: C.grappleTargetTag }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'setup_grappling', blueprintPath: '/Game/BP_Char', grappleRange: 2000, grappleSpeed: 1500 }, exampleOutput: { success: true, message: 'Grappling set up' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_footstep_system`, action: 'setup_footstep_system', family: F,
    summary: 'Set up the footstep system component.', whenToUse: ['Footstep FX/audio needed.'], whenNotToUse: ['Use map_surface_to_sound.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, footstepEnabled: C.footstepEnabled, footstepSocketLeft: C.footstepSocketLeft, footstepSocketRight: C.footstepSocketRight, footstepTraceDistance: C.footstepTraceDistance }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'setup_footstep_system', blueprintPath: '/Game/BP_Char', footstepEnabled: true, footstepSocketLeft: 'foot_l' }, exampleOutput: { success: true, message: 'Footstep system set up' } }),
  buildRecord({ parentTool: T, id: `${T}.map_surface_to_sound`, action: 'map_surface_to_sound', family: F,
    summary: 'Map a surface type to a footstep sound.', whenToUse: ['Per-surface audio needed.'], whenNotToUse: ['Use configure_footstep_fx.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, surfaceType: P.string_ }, required: ['action', 'blueprintPath', 'surfaceType'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'map_surface_to_sound', blueprintPath: '/Game/BP_Char', surfaceType: 'Concrete' }, exampleOutput: { success: true, message: 'Surface mapped to sound' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_footstep_fx`, action: 'configure_footstep_fx', family: F,
    summary: 'Configure footstep FX (Niagara/decals).', whenToUse: ['Footstep visuals needed.'], whenNotToUse: ['Use map_surface_to_sound.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, volumeMultiplier: C.volumeMultiplier, particleScale: C.particleScale }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_footstep_fx', blueprintPath: '/Game/BP_Char', volumeMultiplier: 1, particleScale: 1 }, exampleOutput: { success: true, message: 'Footstep FX configured' } }),
  buildRecord({ parentTool: T, id: `${T}.get_character_info`, action: 'get_character_info', family: F,
    summary: 'Read character Blueprint metadata.', whenToUse: ['Inspect a character.'], whenNotToUse: ['Mutate the character.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath }, required: ['action', 'blueprintPath'],
    effect: 'read', latency: 'instant', resources: 'low',
    outputProps: { hasMesh: P.bool_ }, outputRequired: [],
    exampleInput: { action: 'get_character_info', blueprintPath: '/Game/BP_Char' }, exampleOutput: { success: true, message: 'Character info', hasMesh: true } }),
  buildRecord({ parentTool: T, id: `${T}.configure_crouch`, action: 'configure_crouch', family: F,
    summary: 'Configure crouch height/speed.', whenToUse: ['Crouch needed.'], whenNotToUse: ['Use configure_sprint.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, canCrouch: C.canCrouch, crouchSpeed: C.crouchSpeed, crouchedHalfHeight: C.crouchedHalfHeight }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_crouch', blueprintPath: '/Game/BP_Char', canCrouch: true, crouchSpeed: 300 }, exampleOutput: { success: true, message: 'Crouch configured' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_sprint`, action: 'configure_sprint', family: F,
    summary: 'Configure sprint multiplier.', whenToUse: ['Sprint needed.'], whenNotToUse: ['Use configure_crouch.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, sprintSpeed: C.sprintSpeed }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_sprint', blueprintPath: '/Game/BP_Char', sprintSpeed: 900 }, exampleOutput: { success: true, message: 'Sprint configured' } }),
];
