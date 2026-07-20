/**
 * manage_combat records — part 4 of 5 (melee traces, combos, hitstop, hit
 * reactions, parry/block and weapon trails).
 *
 * Parameters mirror the field literals read by
 * McpAutomationBridge_CombatHandlersMeleeCore.cpp and MeleeDefense.cpp, plus
 * the trail branch of WeaponShellTrails.cpp. Every action targets an existing
 * melee weapon asset, so combat-handlers.ts requires `blueprintPath`. The
 * native handlers report shortened field names (traceRadius, timeDilation,
 * stunTime, trailParticlePath), which is what the outputs declare.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { C } from './combat-properties.js';

const T = 'manage_combat';
const F = 'combat';
const SWORD = '/Game/BP_Sword';

export const COMBAT_4: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_melee_trace`, action: 'create_melee_trace', family: F,
    summary: 'Create the melee sweep between two blade sockets.', whenToUse: ['Melee hit detection is needed.'], whenNotToUse: ['Use setup_hitbox_component for per-bone volumes.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, meleeTraceStartSocket: C.meleeTraceStartSocket, meleeTraceEndSocket: C.meleeTraceEndSocket, meleeTraceRadius: C.meleeTraceRadius }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'low',
    outputProps: { traceRadius: C.traceRadius }, outputRequired: [],
    exampleInput: { action: 'create_melee_trace', blueprintPath: SWORD, meleeTraceStartSocket: 'BladeBase', meleeTraceEndSocket: 'BladeTip', meleeTraceRadius: 16 }, exampleOutput: { success: true, message: 'Melee trace created', traceRadius: 16 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_combo_system`, action: 'configure_combo_system', family: F,
    summary: 'Configure the combo input window and chain length.', whenToUse: ['Chained melee attacks are needed.'], whenNotToUse: ['Use create_melee_trace for hit detection.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, comboWindowTime: C.comboWindowTime, maxComboCount: C.maxComboCount }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { maxComboCount: C.maxComboCount }, outputRequired: [],
    exampleInput: { action: 'configure_combo_system', blueprintPath: SWORD, comboWindowTime: 0.5, maxComboCount: 4 }, exampleOutput: { success: true, message: 'Combo system configured', maxComboCount: 4 } }),
  buildRecord({ parentTool: T, id: `${T}.create_hit_pause`, action: 'create_hit_pause', family: F,
    summary: 'Create the hitstop duration and time dilation.', whenToUse: ['Impact should freeze briefly.'], whenNotToUse: ['Use configure_hit_reaction for the victim animation.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, hitPauseDuration: C.hitPauseDuration, hitPauseTimeDilation: C.hitPauseTimeDilation }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { timeDilation: C.timeDilation }, outputRequired: [],
    exampleInput: { action: 'create_hit_pause', blueprintPath: SWORD, hitPauseDuration: 0.0625, hitPauseTimeDilation: 0.5 }, exampleOutput: { success: true, message: 'Hit pause created', timeDilation: 0.5 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_hit_reaction`, action: 'configure_hit_reaction', family: F,
    summary: 'Configure the hit reaction montage and stun duration.', whenToUse: ['Struck targets should react.'], whenNotToUse: ['Use create_hit_pause for attacker-side feel.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, hitReactionMontage: C.hitReactionMontage, hitReactionStunTime: C.hitReactionStunTime }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { hitReactionMontage: C.hitReactionMontage, stunTime: C.stunTime }, outputRequired: [],
    exampleInput: { action: 'configure_hit_reaction', blueprintPath: SWORD, hitReactionMontage: '/Game/Anim/HitReact', hitReactionStunTime: 0.5 }, exampleOutput: { success: true, message: 'Hit reaction configured', stunTime: 0.5 } }),
  buildRecord({ parentTool: T, id: `${T}.setup_parry_block_system`, action: 'setup_parry_block_system', family: F,
    summary: 'Set up the parry window and blocking mitigation cost.', whenToUse: ['Defensive melee options are needed.'], whenNotToUse: ['Use configure_hit_reaction for unblocked hits.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, parryWindowStart: C.parryWindowStart, parryWindowEnd: C.parryWindowEnd, parryAnimationPath: C.parryAnimationPath, blockDamageReduction: C.blockDamageReduction, blockStaminaCost: C.blockStaminaCost }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { parryAnimationPath: C.parryAnimationPath, blockDamageReduction: C.blockDamageReduction }, outputRequired: [],
    exampleInput: { action: 'setup_parry_block_system', blueprintPath: SWORD, parryWindowStart: 0, parryWindowEnd: 0.25, parryAnimationPath: '/Game/Anim/Parry', blockDamageReduction: 0.5, blockStaminaCost: 12 }, exampleOutput: { success: true, message: 'Parry/block system set up', blockDamageReduction: 0.5 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_weapon_trails`, action: 'configure_weapon_trails', family: F,
    summary: 'Configure the blade trail particle and its sockets.', whenToUse: ['Swings should leave a visible arc.'], whenNotToUse: ['Use configure_tracer for projectile trails.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, weaponTrailParticlePath: C.weaponTrailParticlePath, weaponTrailStartSocket: C.weaponTrailStartSocket, weaponTrailEndSocket: C.weaponTrailEndSocket }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { trailParticlePath: C.trailParticlePath, trailStartSocket: C.trailStartSocket }, outputRequired: [],
    exampleInput: { action: 'configure_weapon_trails', blueprintPath: SWORD, weaponTrailParticlePath: '/Game/FX/Trail', weaponTrailStartSocket: 'TrailStart', weaponTrailEndSocket: 'TrailEnd' }, exampleOutput: { success: true, message: 'Weapon trails configured', trailStartSocket: 'TrailStart' } }),
];
