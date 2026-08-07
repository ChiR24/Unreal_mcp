/**
 * Spawn and lifecycle records: spawn/spawn_actor/spawn_blueprint, duplicate,
 * delete/destroy_actor/delete_by_tag.
 *
 * Grounded in actor-basic-handlers.ts (spawn, delete, duplicate,
 * spawn_blueprint, delete_by_tag) and the native ControlActor dispatch
 * (spawn/spawn_actor -> HandleControlActorSpawn, spawn_blueprint ->
 * HandleControlActorSpawnBlueprint, delete/destroy_actor ->
 * HandleControlActorDelete, duplicate -> HandleControlActorDuplicate,
 * delete_by_tag -> HandleControlActorDeleteByTag).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { actorAlias, CANONICAL_NR, DOMAIN, P } from './properties.js';

const FAMILY_SPAWN = 'spawn';
const FAMILY_LIFECYCLE = 'lifecycle';

export const SPAWN_RECORDS: readonly CapabilityRecordSource[] = [
	buildCoreRecord({
		parentTool: 'control_actor',
		action: 'spawn',
		domain: DOMAIN,
		family: FAMILY_SPAWN,
		summary:
			'Spawn a new actor instance from a class path into the current level.',
		whenToUse: [
			'A new actor of a known Unreal class must be created in the scene.',
		],
		whenNotToUse: ['A Blueprint instance is needed (use spawn_blueprint).'],
		inputProps: {
			classPath: P.classPath,
			actorClass: P.actorClass,
			actorName: P.actorName,
			meshPath: P.meshPath,
			location: P.location,
			rotation: P.rotation,
			scale: P.scale,
		},
		required: ['classPath'],
		outputProps: { name: P.actorName },
		outputRequired: [],
		effect: 'write',
		costLatency: 'interactive',
		costResources: 'low',
		normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
		normalizationRationale: CANONICAL_NR,
		exampleInput: {
			action: 'spawn',
			classPath: '/Script/Engine.PointLight',
			actorName: 'MyLight',
			location: [0, 0, 100],
		},
		exampleOutput: {
			success: true,
			message: 'Spawned actor: MyLight',
			name: 'MyLight',
		},
	}),
	buildCoreRecord({
		parentTool: 'control_actor',
		action: 'spawn_actor',
		domain: DOMAIN,
		family: FAMILY_SPAWN,
		summary:
			'Long-form alias for spawn; normalizeActorAction maps spawn_actor to spawn.',
		whenToUse: ['Preferred when callers use the explicit spawn_actor verb.'],
		whenNotToUse: ['Use the shorter spawn form to avoid alias normalization.'],
		inputProps: {
			classPath: P.classPath,
			actorClass: P.actorClass,
			actorName: P.actorName,
			meshPath: P.meshPath,
			location: P.location,
			rotation: P.rotation,
			scale: P.scale,
		},
		required: ['classPath'],
		outputProps: { name: P.actorName },
		outputRequired: [],
		effect: 'write',
		costLatency: 'interactive',
		costResources: 'low',
		...actorAlias('spawn'),
		exampleInput: {
			action: 'spawn_actor',
			classPath: '/Script/Engine.Cube',
			actorName: 'Cube1',
		},
		exampleOutput: {
			success: true,
			message: 'Spawned actor: Cube1',
			name: 'Cube1',
		},
	}),
	buildCoreRecord({
		parentTool: 'control_actor',
		action: 'spawn_blueprint',
		domain: DOMAIN,
		family: FAMILY_SPAWN,
		summary:
			'Spawn an actor instance from a Blueprint asset path into the current level.',
		whenToUse: ['A Blueprint instance must be placed in the scene.'],
		whenNotToUse: ['A native class instance is needed (use spawn).'],
		// `scale` is read and applied by the native handler
		// (McpAutomationBridge_ControlActorBlueprintSpawn.cpp: bHasScale ->
		// SetActorScale3D) and echoed in its response, but was undeclared here.
		// With additionalProperties:false the gateway rejected it as an
		// UNDECLARED_PARAMETER, so spawning a scaled Blueprint actor needed a
		// second set_transform round-trip while `spawn` accepted scale directly.
		inputProps: {
			blueprintPath: P.blueprintPath,
			actorName: P.actorName,
			location: P.location,
			rotation: P.rotation,
			scale: P.scale,
		},
		required: ['blueprintPath'],
		outputProps: { name: P.actorName },
		outputRequired: [],
		effect: 'write',
		costLatency: 'interactive',
		costResources: 'low',
		normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
		normalizationRationale: CANONICAL_NR,
		exampleInput: {
			action: 'spawn_blueprint',
			blueprintPath: '/Game/Blueprints/BP_Lamp',
			actorName: 'Lamp1',
		},
		exampleOutput: {
			success: true,
			message: 'Spawned blueprint: Lamp1',
			name: 'Lamp1',
		},
	}),
	buildCoreRecord({
		parentTool: 'control_actor',
		action: 'duplicate',
		domain: DOMAIN,
		family: FAMILY_LIFECYCLE,
		summary:
			'Duplicate an existing actor, optionally with a new name and offset.',
		whenToUse: ['An actor must be copied within the current level.'],
		whenNotToUse: ['A distinct class instance is needed (use spawn).'],
		inputProps: {
			actorName: P.actorName,
			newName: P.newName,
			offset: P.offset,
		},
		required: ['actorName'],
		effect: 'write',
		costLatency: 'interactive',
		costResources: 'low',
		normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
		normalizationRationale: CANONICAL_NR,
		exampleInput: {
			action: 'duplicate',
			actorName: 'Cube1',
			newName: 'Cube2',
			offset: [100, 0, 0],
		},
		exampleOutput: { success: true, message: 'Duplicated Cube1 to Cube2' },
	}),
	buildCoreRecord({
		parentTool: 'control_actor',
		action: 'delete',
		domain: DOMAIN,
		family: FAMILY_LIFECYCLE,
		summary: 'Permanently delete one actor or a batch of actors by name.',
		whenToUse: ['An actor must be permanently removed from the level.'],
		whenNotToUse: ['The actor should only be hidden (use set_visibility).'],
		inputProps: { actorName: P.actorName, actorNames: P.actorNames },
		required: [],
		requiredOneOf: ['actorName', 'actorNames'],
		effect: 'destructive',
		behavior: { safeToRetry: false, supportsUndo: false },
		costLatency: 'interactive',
		costResources: 'low',
		normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
		normalizationRationale: CANONICAL_NR,
		exampleInput: { action: 'delete', actorName: 'Cube1' },
		exampleOutput: { success: true, message: 'Deleted Cube1' },
	}),
	buildCoreRecord({
		parentTool: 'control_actor',
		action: 'destroy_actor',
		domain: DOMAIN,
		family: FAMILY_LIFECYCLE,
		summary:
			'Long-form alias for delete; normalizeActorAction maps destroy_actor to delete.',
		whenToUse: ['Preferred when callers use the explicit destroy_actor verb.'],
		whenNotToUse: ['Use the shorter delete form to avoid alias normalization.'],
		inputProps: { actorName: P.actorName, actorNames: P.actorNames },
		required: [],
		requiredOneOf: ['actorName', 'actorNames'],
		effect: 'destructive',
		behavior: { safeToRetry: false, supportsUndo: false },
		costLatency: 'interactive',
		costResources: 'low',
		...actorAlias('delete'),
		exampleInput: { action: 'destroy_actor', actorName: 'Cube1' },
		exampleOutput: { success: true, message: 'Deleted Cube1' },
	}),
	buildCoreRecord({
		parentTool: 'control_actor',
		action: 'delete_by_tag',
		domain: DOMAIN,
		family: FAMILY_LIFECYCLE,
		summary: 'Permanently delete every actor matching a gameplay tag.',
		whenToUse: ['All actors sharing a tag must be removed in one operation.'],
		whenNotToUse: ['A single named actor should be removed (use delete).'],
		inputProps: { tag: P.tag },
		required: ['tag'],
		effect: 'destructive',
		behavior: { safeToRetry: false, supportsUndo: false },
		costLatency: 'interactive',
		costResources: 'low',
		normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
		normalizationRationale: CANONICAL_NR,
		exampleInput: { action: 'delete_by_tag', tag: 'Disposable' },
		exampleOutput: {
			success: true,
			message: 'Deleted actors by tag: Disposable',
		},
	}),
];
