/**
 * Focused tests for the control_editor capability-record catalog.
 *
 * Proves: exact 42-action set equality with the canonical tool definition,
 * 1:1 legacy-id mapping, unique canonical IDs, schema closure, routing
 * (tool/action/local modes), cross-parent/fallback misroute metadata,
 * effect/idempotency semantics, availability, and hash parity.
 *
 * Grounded in:
 * - src/tools/definitions/core/control-editor-tool.ts (canonical action enum)
 * - src/tools/handlers/editor/* (TS handler routing/normalization)
 * - plugins/.../MCP/Tools/Core/McpTool_ControlEditor.cpp (native action set)
 * - plugins/.../Domains/ControlEditor/McpAutomationBridge_ControlEditorDispatch.cpp
 * - src/tools/catalog/capabilities/normalization-inventory.json (class A/C, keep)
 */
import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../../../catalog/consolidated-tool-definitions.js';
import { isRecord } from '../../../../../utils/validation/type-guards.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../../index.js';
import {
  CONTROL_EDITOR_RECORD_COUNT,
  CONTROL_EDITOR_RECORDS,
  CONTROL_EDITOR_SOURCES,
} from './index.js';

const ALL_42_ACTIONS = [
  'play', 'stop', 'stop_pie', 'pause', 'resume', 'eject', 'possess',
  'set_game_speed', 'set_fixed_delta_time', 'step_frame', 'single_frame_step',
  'start_recording', 'stop_recording',
  'set_view_target', 'set_game_view_target',
  'set_camera', 'set_camera_position', 'set_viewport_camera', 'set_camera_fov',
  'set_view_mode', 'set_viewport_resolution', 'set_viewport_realtime',
  'set_editor_mode', 'set_immersive_mode', 'set_game_view',
  'show_stats', 'hide_stats',
  'console_command', 'execute_command', 'set_preferences',
  'screenshot', 'take_screenshot',
  'create_bookmark', 'jump_to_bookmark',
  'open_asset', 'close_asset', 'open_level', 'focus_actor', 'save_all',
  'simulate_input',
  'undo', 'redo',
] as const;

function canonicalActionEnum(): readonly string[] {
  const definition = consolidatedToolDefinitions.find((tool) => tool.name === 'control_editor');
  if (definition === undefined) throw new TypeError('control_editor tool definition is unavailable');
  const properties: unknown = definition.inputSchema.properties;
  const action: unknown = isRecord(properties) ? properties.action : undefined;
  const values: unknown = isRecord(action) ? action.enum : undefined;
  if (!Array.isArray(values)) throw new TypeError('control_editor action enum is unavailable');
  return values.filter((value): value is string => typeof value === 'string');
}

function findByAction(action: string) {
  const record = CONTROL_EDITOR_RECORDS.find((r) => r.legacyIds[0].action === action);
  if (!record) throw new Error(`Record not found for action: ${action}`);
  return record;
}

describe('control_editor exact-set: 42 records mapped 1:1 to tool actions', () => {
  it('produces exactly 42 capability records', () => {
    expect(CONTROL_EDITOR_RECORD_COUNT).toBe(42);
    expect(CONTROL_EDITOR_SOURCES).toHaveLength(42);
    expect(CONTROL_EDITOR_RECORDS).toHaveLength(42);
  });

  it('maps every control_editor tool action to exactly one record legacy ID', () => {
    const legacyKeys = new Set(
      CONTROL_EDITOR_RECORDS.flatMap((r) => r.legacyIds.map((li) => `${li.tool}::${li.action}`)),
    );
    for (const action of ALL_42_ACTIONS) {
      expect(legacyKeys.has(`control_editor::${action}`)).toBe(true);
    }
    expect(legacyKeys.size).toBe(42);
  });

  it('the tool definition action enum matches the union of actions exactly', () => {
    const controlEditorToolDefinition = consolidatedToolDefinitions.find((t) => t.name === 'control_editor') as NonNullable<typeof consolidatedToolDefinitions[number]>;
    const props = controlEditorToolDefinition.inputSchema.properties as Record<string, { enum?: readonly string[] }>;
    const actionProp = props.action;
    if (!actionProp?.enum) throw new TypeError('control_editor action enum is unavailable');
    const enumSet = new Set(actionProp.enum);
    for (const action of ALL_42_ACTIONS) {
      expect(enumSet.has(action)).toBe(true);
    }
    expect(enumSet.size).toBe(ALL_42_ACTIONS.length);
  });

  it('has no duplicate canonical IDs, aliases, or legacy IDs across all 42 records', () => {
    const catalog = parseCapabilityCatalog([...CONTROL_EDITOR_RECORDS]);
    expect(catalog).toHaveLength(42);
  });

  it('preserves canonical action order identical to the tool definition enum', () => {
    const recordActions = CONTROL_EDITOR_RECORDS.map((r) => r.legacyIds[0].action);
    expect(recordActions).toEqual([...canonicalActionEnum()]);
  });
});

describe('control_editor routing and cross-parent dispatch', () => {
  it('routes all non-cross-parent actions through control_editor tool dispatch', () => {
    const crossParent = new Set(['console_command', 'execute_command', 'set_viewport_resolution']);
    for (const record of CONTROL_EDITOR_RECORDS) {
      const action = record.legacyIds[0].action;
      if (crossParent.has(action)) continue;
      expect(record.routing.parentTool).toBe('control_editor');
      expect(record.routing.dispatchMode).toBe('tool');
    }
  });

  it('console_command, execute_command, and set_viewport_resolution use cross-parent action dispatch', () => {
    for (const action of ['console_command', 'execute_command', 'set_viewport_resolution']) {
      const record = findByAction(action);
      expect(record.routing.dispatchMode).toBe('action');
      expect(record.routing.dispatchAction).toBe('console_command');
      expect(record.routing.parentTool).toBe('control_editor');
    }
  });

  it('alias actions dispatch to their normalized bridge action name', () => {
    const aliases: Record<string, string> = {
      set_game_view_target: 'set_view_target',
      set_camera_position: 'set_camera',
      set_viewport_camera: 'set_camera',
      take_screenshot: 'screenshot',
      single_frame_step: 'step_frame',
    };
    for (const [action, dispatchAction] of Object.entries(aliases)) {
      const record = findByAction(action);
      expect(record.routing.dispatchAction).toBe(dispatchAction);
      expect(record.routing.dispatchMode).toBe('tool');
    }
  });

  it('documents the screenshot conditional cross-parent misroute to system_control', () => {
    const screenshot = findByAction('screenshot');
    expect(screenshot.routing.dispatchMode).toBe('tool');
    expect(screenshot.normalization.rationale.toLowerCase()).toContain('game_viewport');
  });

  it('documents the start_recording console_command DemoRec fallback misroute', () => {
    const startRec = findByAction('start_recording');
    expect(startRec.routing.dispatchMode).toBe('tool');
    expect(startRec.normalization.rationale.toLowerCase()).toContain('fallback');
  });
});

describe('control_editor effect, idempotency, and behavior semantics', () => {
  it('classifies viewport/camera/display actions as read (idempotent, safe to retry)', () => {
    const readActions = [
      'screenshot', 'take_screenshot', 'focus_actor', 'jump_to_bookmark',
      'show_stats', 'hide_stats', 'set_view_mode', 'set_viewport_resolution',
      'set_viewport_realtime', 'set_editor_mode', 'set_immersive_mode', 'set_game_view',
      'set_view_target', 'set_game_view_target',
      'set_camera', 'set_camera_position', 'set_viewport_camera', 'set_camera_fov',
      'open_asset',
    ];
    for (const action of readActions) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('read');
      expect(record.behavior.idempotency).toBe('idempotent');
      expect(record.behavior.safeToRetry).toBe(true);
      expect(record.behavior.supportsUndo).toBe(false);
    }
  });

  it('classifies PIE/timing/state-mutating actions as write', () => {
    const writeActions = [
      'play', 'eject', 'possess', 'step_frame', 'single_frame_step',
      'start_recording', 'stop_recording', 'create_bookmark', 'simulate_input',
      'undo', 'redo', 'save_all', 'close_asset', 'open_level',
      'console_command', 'execute_command', 'set_preferences',
    ];
    for (const action of writeActions) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('write');
      expect(record.behavior.supportsUndo).toBe(true);
    }
  });

  it('marks stop/stop_pie/pause/resume/set_game_speed/set_fixed_delta_time as write with idempotent override', () => {
    const idempotentWrite = [
      'stop', 'stop_pie', 'pause', 'resume', 'set_game_speed', 'set_fixed_delta_time',
    ];
    for (const action of idempotentWrite) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('write');
      expect(record.behavior.idempotency).toBe('idempotent');
    }
  });
});

describe('control_editor availability and normalization', () => {
  it('all records target UE 5.0-5.8 Preview with no required plugins and edit state', () => {
    for (const record of CONTROL_EDITOR_RECORDS) {
      expect(record.availability.unreal.min).toEqual({ major: 5, minor: 0, patch: 0, channel: 'stable' });
      expect(record.availability.unreal.max).toEqual({ major: 5, minor: 8, patch: 0, channel: 'preview', preview: 1 });
      expect(record.availability.requiredPlugins).toEqual([]);
      expect(record.availability.editorStates).toEqual(['edit']);
    }
  });

  it('all records use retain disposition and active deprecation with correct normalization class', () => {
    const trueDuplicateActions = new Set(['console_command', 'execute_command', 'screenshot', 'show_stats']);
    for (const record of CONTROL_EDITOR_RECORDS) {
      const action = record.legacyIds[0].action;
      if (trueDuplicateActions.has(action)) {
        expect(record.normalization.class).toBe('A_TRUE_DUPLICATE');
      } else {
        expect(record.normalization.class).toBe('C_SAME_VERB_DIFFERENT_TARGET');
      }
      expect(record.normalization.disposition).toBe('retain');
      expect(record.deprecation.status).toBe('active');
    }
  });

  it('console_command and execute_command rationales name system_control, not a nonexistent console_command parent', () => {
    for (const action of ['console_command', 'execute_command']) {
      const record = findByAction(action);
      const rationale = record.normalization.rationale.toLowerCase();
      expect(rationale).toContain('system_control');
      expect(rationale).not.toContain('console_command parent');
    }
  });

  it('all input schemas close with additionalProperties false and require action', () => {
    for (const record of CONTROL_EDITOR_RECORDS) {
      expect(record.schemas.input.additionalProperties).toBe(false);
      expect(record.schemas.input.required).toContain('action');
      expect(record.schemas.input.properties).toHaveProperty('action');
    }
  });

  it('every record has exactly one example with matching input and output', () => {
    for (const record of CONTROL_EDITOR_RECORDS) {
      expect(record.examples).toHaveLength(1);
      expect(record.examples[0].input).toHaveProperty('action');
      expect(record.examples[0].output).toHaveProperty('success');
    }
  });
});

describe('control_editor hash parity: TS source, JSON round-trip, and recompute', () => {
  it('every record hash matches a fresh recompute from its source', () => {
    for (let i = 0; i < CONTROL_EDITOR_SOURCES.length; i++) {
      const recomputed = createCapabilityRecord(CONTROL_EDITOR_SOURCES[i]);
      expect(recomputed.hashes.schema).toBe(CONTROL_EDITOR_RECORDS[i].hashes.schema);
      expect(recomputed.hashes.content).toBe(CONTROL_EDITOR_RECORDS[i].hashes.content);
    }
  });

  it('JSON round-trip preserves all 42 records with identical hashes', () => {
    const json = JSON.stringify(CONTROL_EDITOR_RECORDS);
    const restored = JSON.parse(json) as typeof CONTROL_EDITOR_RECORDS;
    const catalog = parseCapabilityCatalog([...restored]);
    expect(catalog).toHaveLength(42);
    for (let i = 0; i < 42; i++) {
      expect(catalog[i].hashes).toEqual(CONTROL_EDITOR_RECORDS[i].hashes);
    }
  });

  it('deterministic: two createCapabilityRecord calls on the same source produce identical hashes', () => {
    for (const source of CONTROL_EDITOR_SOURCES) {
      const a = createCapabilityRecord(source);
      const b = createCapabilityRecord(source);
      expect(a.hashes).toEqual(b.hashes);
    }
  });
});
