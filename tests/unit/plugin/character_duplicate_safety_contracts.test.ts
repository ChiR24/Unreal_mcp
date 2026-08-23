// Todo 10 / BB-012 — Character setup actions must be prerequisite-safe,
// duplicate-convergent on retry, and verifiable through object-shaped
// bounded movement/setup feature evidence.
//
// These assertions read the plugin source text (what the compiler sees), so a
// behavior that exists only in a comment or in a TypeScript facade cannot pass.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const CHARACTER_DIR = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Character',
);
const BLUEPRINT_SUPPORT = resolve(CHARACTER_DIR, 'McpAutomationBridge_CharacterHandlersBlueprintSupport.cpp');
const HANDLERS_HEADER = resolve(CHARACTER_DIR, 'McpAutomationBridge_CharacterHandlers.h');
const TRAVERSAL = resolve(CHARACTER_DIR, 'McpAutomationBridge_CharacterHandlersTraversal.cpp');
const ADVANCED_MOVEMENT = resolve(CHARACTER_DIR, 'McpAutomationBridge_CharacterHandlersAdvancedMovement.cpp');
const FOOTSTEPS = resolve(CHARACTER_DIR, 'McpAutomationBridge_CharacterHandlersFootsteps.cpp');
const INFO = resolve(CHARACTER_DIR, 'McpAutomationBridge_CharacterHandlersInfo.cpp');

function read(path: string): string {
  expect(existsSync(path), `missing native file: ${path}`).toBe(true);
  return readFileSync(path, 'utf8');
}

/** Strip comments so a claim in prose cannot satisfy a code contract. */
function code(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}

function occurrences(source: string, needle: string): number {
  return (source.match(new RegExp(needle.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'g')) ?? []).length;
}

describe('BB-012 Character setup actions are duplicate-safe and prerequisite-gated', () => {
  it('AddBlueprintVariable is ensure-style: an existence check precedes AddMemberVariable and the category is still applied', () => {
    const source = code(read(BLUEPRINT_SUPPORT));
    const addIndex = source.indexOf('FBlueprintEditorUtils::AddMemberVariable');
    const existsIndex = source.indexOf('FindNewVariable');
    const categoryIndex = source.indexOf('FBlueprintEditorUtils::SetBlueprintVariableCategory');

    // A blind add is the BB-012 duplicate root cause: retry must converge
    // without re-adding the same variable.
    expect(addIndex).toBeGreaterThan(-1);
    expect(existsIndex).toBeGreaterThan(-1);
    expect(existsIndex).toBeLessThan(addIndex);
    // Category preservation: the ensure-style path still re-applies the
    // category, so a retry keeps the authored category rather than a default.
    expect(categoryIndex).toBeGreaterThan(existsIndex);
  });

  it('declares and uses a shared Character prerequisite gate before every setup mutation', () => {
    const header = code(read(HANDLERS_HEADER));
    expect(header).toContain('RequireCharacterBlueprint');

    // All three Traversal setup handlers (mantling, vaulting, climbing) and
    // all three AdvancedMovement setup handlers (sliding, wall running,
    // grappling) must go through the gate instead of a bare load.
    const traversal = code(read(TRAVERSAL));
    expect(occurrences(traversal, 'RequireCharacterBlueprint')).toBeGreaterThanOrEqual(3);
    expect(occurrences(traversal, 'LoadCharacterBlueprint(')).toBe(0);

    const advanced = code(read(ADVANCED_MOVEMENT));
    expect(occurrences(advanced, 'RequireCharacterBlueprint')).toBeGreaterThanOrEqual(3);
    expect(occurrences(advanced, 'LoadCharacterBlueprint(')).toBe(0);

    // setup_footstep_system must be gated; the two non-setup Footsteps
    // handlers are intentionally not part of the seven-action set.
    const footsteps = code(read(FOOTSTEPS));
    expect(occurrences(footsteps, 'RequireCharacterBlueprint')).toBeGreaterThanOrEqual(1);
    expect(occurrences(footsteps, 'LoadCharacterBlueprint(')).toBe(2);
  });

  it('keeps per-feature CDO writes behind the prerequisite gate', () => {
    const traversal = code(read(TRAVERSAL));
    const advanced = code(read(ADVANCED_MOVEMENT));

    // setup_climbing and setup_wall_running mutate the CDO movement speed;
    // that write must sit textually after the gate call in the same
    // translation unit so a missing GeneratedClass can never reach it.
    expect(traversal.indexOf('MaxCustomMovementSpeed')).toBeGreaterThan(traversal.indexOf('RequireCharacterBlueprint'));
    expect(advanced.indexOf('MaxCustomMovementSpeed')).toBeGreaterThan(
      advanced.indexOf('RequireCharacterBlueprint'),
    );
  });

  it('exposes bounded setup feature evidence in an object-shaped movementVariables', () => {
    const info = code(read(INFO));

    // The declared record shape is { type: 'object', additionalProperties:
    // true, 'x-unreal-reflection-boundary': true }; an array emission violates
    // the gateway output validator, so the evidence must be an object.
    expect(info).toMatch(/SetObjectField\(TEXT\("movementVariables"\)/);
    expect(info).not.toMatch(/SetArrayField\(TEXT\("movementVariables"\)/);

    // Per-feature evidence derived from actual Blueprint variables/defaults.
    expect(info).toContain('setupFeatures');
    expect(info).toContain('variableNames');
    for (const feature of ['mantle', 'vault', 'climb', 'slide', 'wallRun', 'grapple', 'footsteps']) {
      expect(info).toContain(`TEXT("${feature}")`);
    }
  });
});
