/**
 * manage_combat records — part 3 of 5 (reload, ammo, attachments, switching and
 * the muzzle/tracer/impact/shell effect actions).
 *
 * Parameters mirror the field literals read by
 * McpAutomationBridge_CombatHandlersWeaponAmmo.cpp, WeaponEquipment.cpp,
 * WeaponEffects.cpp and WeaponShellTrails.cpp. Every action here targets an
 * existing weapon asset, so combat-handlers.ts requires `blueprintPath`. The
 * effect actions report back the generic particlePath/soundPath/decalPath
 * fields the native handlers write, not the per-effect input names.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { C } from './combat-properties.js';

const T = 'manage_combat';
const F = 'combat';
const WEAPON = '/Game/BP_Rifle';

export const COMBAT_3: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.setup_reload_system`, action: 'setup_reload_system', family: F,
    summary: 'Set up magazine size, reload timing and reload montage.', whenToUse: ['The weapon reloads.'], whenNotToUse: ['Use setup_ammo_system for carried ammo.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, magazineSize: C.magazineSize, reloadTime: C.reloadTime, reloadAnimationPath: C.reloadAnimationPath }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { magazineSize: C.magazineSize, reloadAnimationPath: C.reloadAnimationPath }, outputRequired: [],
    exampleInput: { action: 'setup_reload_system', blueprintPath: WEAPON, magazineSize: 24, reloadTime: 2, reloadAnimationPath: '/Game/Anim/Reload' }, exampleOutput: { success: true, message: 'Reload system set up', magazineSize: 24 } }),
  buildRecord({ parentTool: T, id: `${T}.setup_ammo_system`, action: 'setup_ammo_system', family: F,
    summary: 'Set up ammo type, reserves and per-shot consumption.', whenToUse: ['Carried ammo must be tracked.'], whenNotToUse: ['Use setup_reload_system for magazine reloads.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, ammoType: C.ammoType, maxAmmo: C.maxAmmo, startingAmmo: C.startingAmmo, ammoPerShot: C.ammoPerShot, infiniteAmmo: C.infiniteAmmo }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { maxAmmo: C.maxAmmo, ammoPerShot: C.ammoPerShot, infiniteAmmo: C.infiniteAmmo }, outputRequired: [],
    exampleInput: { action: 'setup_ammo_system', blueprintPath: WEAPON, ammoType: 'Rifle', maxAmmo: 120, startingAmmo: 48, ammoPerShot: 1, infiniteAmmo: false }, exampleOutput: { success: true, message: 'Ammo system set up', maxAmmo: 120 } }),
  buildRecord({ parentTool: T, id: `${T}.setup_attachment_system`, action: 'setup_attachment_system', family: F,
    summary: 'Create attachment slots and their scene components.', whenToUse: ['Modular optics or grips are needed.'], whenNotToUse: ['Use setup_weapon_switching to swap whole weapons.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, attachmentSlots: C.attachmentSlots }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { attachmentSlots: C.attachmentSlots }, outputRequired: [],
    exampleInput: { action: 'setup_attachment_system', blueprintPath: WEAPON, attachmentSlots: [{ slotName: 'Optic', socketName: 'OpticSocket', allowedTypes: ['Scope'] }] }, exampleOutput: { success: true, message: 'Attachment system set up' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_weapon_switching`, action: 'setup_weapon_switching', family: F,
    summary: 'Set up equip/unequip timing and montages.', whenToUse: ['The character carries multiple weapons.'], whenNotToUse: ['Use setup_attachment_system for per-weapon parts.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, switchInTime: C.switchInTime, switchOutTime: C.switchOutTime, equipAnimationPath: C.equipAnimationPath, unequipAnimationPath: C.unequipAnimationPath }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { switchInTime: C.switchInTime, equipAnimationPath: C.equipAnimationPath, unequipAnimationPath: C.unequipAnimationPath }, outputRequired: [],
    exampleInput: { action: 'setup_weapon_switching', blueprintPath: WEAPON, switchInTime: 0.5, switchOutTime: 0.25, equipAnimationPath: '/Game/Anim/Equip', unequipAnimationPath: '/Game/Anim/Unequip' }, exampleOutput: { success: true, message: 'Weapon switching set up', switchInTime: 0.5 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_muzzle_flash`, action: 'configure_muzzle_flash', family: F,
    summary: 'Configure muzzle flash particle, scale and firing sound.', whenToUse: ['Muzzle feedback is needed.'], whenNotToUse: ['Use configure_tracer for the round in flight.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, muzzleFlashParticlePath: C.muzzleFlashParticlePath, muzzleFlashScale: C.muzzleFlashScale, muzzleSoundPath: C.muzzleSoundPath }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { particlePath: C.particlePath, soundPath: C.soundPath }, outputRequired: [],
    exampleInput: { action: 'configure_muzzle_flash', blueprintPath: WEAPON, muzzleFlashParticlePath: '/Game/FX/Muzzle', muzzleFlashScale: 1.5, muzzleSoundPath: '/Game/Audio/Fire' }, exampleOutput: { success: true, message: 'Muzzle flash configured', particlePath: '/Game/FX/Muzzle' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_tracer`, action: 'configure_tracer', family: F,
    summary: 'Configure tracer particle and travel speed.', whenToUse: ['Rounds should leave a visible trail.'], whenNotToUse: ['Use configure_muzzle_flash for the barrel flash.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, tracerParticlePath: C.tracerParticlePath, tracerSpeed: C.tracerSpeed }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { tracerPath: C.tracerPath, tracerSpeed: C.tracerSpeed }, outputRequired: [],
    exampleInput: { action: 'configure_tracer', blueprintPath: WEAPON, tracerParticlePath: '/Game/FX/Tracer', tracerSpeed: 15000 }, exampleOutput: { success: true, message: 'Tracer configured', tracerSpeed: 15000 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_impact_effects`, action: 'configure_impact_effects', family: F,
    summary: 'Configure impact particle, sound and decal.', whenToUse: ['Surface impact feedback is needed.'], whenNotToUse: ['Use configure_shell_ejection for casings.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, impactParticlePath: C.impactParticlePath, impactSoundPath: C.impactSoundPath, impactDecalPath: C.impactDecalPath }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { particlePath: C.particlePath, soundPath: C.soundPath, decalPath: C.decalPath }, outputRequired: [],
    exampleInput: { action: 'configure_impact_effects', blueprintPath: WEAPON, impactParticlePath: '/Game/FX/Impact', impactSoundPath: '/Game/Audio/Impact', impactDecalPath: '/Game/FX/Decal' }, exampleOutput: { success: true, message: 'Impact effects configured', decalPath: '/Game/FX/Decal' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_shell_ejection`, action: 'configure_shell_ejection', family: F,
    summary: 'Configure ejected shell mesh, impulse and lifetime.', whenToUse: ['Spent casings should be ejected.'], whenNotToUse: ['Use configure_impact_effects for surface hits.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, shellMeshPath: C.shellMeshPath, shellEjectionForce: C.shellEjectionForce, shellLifespan: C.shellLifespan }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { shellMeshPath: C.shellMeshPath, ejectionForce: C.ejectionForce }, outputRequired: [],
    exampleInput: { action: 'configure_shell_ejection', blueprintPath: WEAPON, shellMeshPath: '/Engine/BasicShapes/Cube.Cube', shellEjectionForce: 400, shellLifespan: 3 }, exampleOutput: { success: true, message: 'Shell ejection configured', ejectionForce: 400 } }),
];
