// control_actor add_tag with an 1100-character tag reached FName(*Tag), and the engine's "FName's 1023 max length
// exceeded" assert closed the editor; hundreds of handlers build an FName from a parameter. Execute now holds every
// string and key a call sends to its schema's maxLength, else to the most an FName holds, before anything is queued.
// The refusal itself is the integration case in tests/mcp-tools/core/control-actor.test.mjs; these pin the wiring and
// the parameters that carry long text.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import {
  CODE_MAX_LENGTH,
  DEFAULT_STRING_MAX_LENGTH,
  PROSE_MAX_LENGTH,
} from '../../../src/tools/catalog/capabilities/constants.js';
import { capabilityIndex } from '../support/capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { sliceBetween } from './plugin-contract-fixtures.js';

const EXECUTE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Execute');
const source = (...parts: readonly string[]): string => readFileSync(join(EXECUTE, ...parts), 'utf8');

/** The schema a pointer names: one segment per property, `[]` for an array's items. */
function schemaAt(id: string, path: readonly string[]): Record<string, unknown> | undefined {
  let node: unknown = capabilityIndex().byId.get(id)?.schemas.input;
  for (const segment of path) {
    if (!isRecord(node)) return undefined;
    node = segment === '[]' ? node.items : isRecord(node.properties) ? node.properties[segment] : undefined;
  }
  return isRecord(node) ? node : undefined;
}

describe('execute holds every input string to the FName limit unless its schema raises it', () => {
  it('sets the native default to the most an FName holds, the number the records assume', () => {
    expect(source('Request', 'McpNativeGatewayStringLimits.h')).toContain('McpDefaultStringMaxLength = NAME_SIZE - 1;');
    expect(DEFAULT_STRING_MAX_LENGTH).toBe(1023);
  });

  it('checks the params of execute and never a reply', () => {
    const validation = source('McpNativeGatewayValidation.cpp');
    const output = validation.indexOf('ValidateGatewayExecuteOutput(');
    expect(sliceBetween(validation, 'ValidateAndResolveGatewayExecute(', 'ValidateGatewayExecuteOutput('))
      .toContain('!McpCheckInputStringLengths(ToValidate, InputSchema, Violation)');
    expect(output).toBeGreaterThan(0);
    expect(validation.slice(output)).not.toContain('McpCheckInputStringLengths');
  });

  // Each one carries code or prose that its handler keeps as text, never as a name.
  it.each([
    ['blueprint.write_graph_from_code', ['code'], CODE_MAX_LENGTH],
    ['system_control.run_script', ['script'], CODE_MAX_LENGTH],
    ['system_control.save_recipe', ['script'], CODE_MAX_LENGTH],
    ['system_control.execute_python', ['code'], CODE_MAX_LENGTH],
    ['material.add_material_node', ['code'], CODE_MAX_LENGTH],
    ['material.update_custom_expression', ['code'], CODE_MAX_LENGTH],
    ['material.create_material', ['description'], PROSE_MAX_LENGTH],
    ['asset.source_control', ['description'], PROSE_MAX_LENGTH],
    ['system_control.create_plugin', ['description'], PROSE_MAX_LENGTH],
    ['struct.edit_struct', ['tooltip'], PROSE_MAX_LENGTH],
    ['manage_ai.edit_behavior_tree', ['comment'], PROSE_MAX_LENGTH],
    ['manage_audio.create_audio_asset', ['spokenText'], PROSE_MAX_LENGTH],
    ['manage_audio.set_subtitles', ['subtitles', '[]', 'text'], PROSE_MAX_LENGTH],
    ['texture.create_texture', ['text'], PROSE_MAX_LENGTH],
    ['blueprint.add_content_widget', ['text'], PROSE_MAX_LENGTH],
    ['blueprint.set_widget_layout', ['text'], PROSE_MAX_LENGTH],
    ['blueprint.create_widget_template', ['text'], PROSE_MAX_LENGTH],
    ['system_control.create_widget', ['text'], PROSE_MAX_LENGTH],
    ['system_control.create_widget', ['message'], PROSE_MAX_LENGTH],
  ] as const)('%s %j takes long text', (id, path, maxLength) => {
    expect(schemaAt(id, path)?.maxLength).toBe(maxLength);
    expect(maxLength).toBeGreaterThan(DEFAULT_STRING_MAX_LENGTH);
  });

  it('leaves a name at the default', () => {
    expect(schemaAt('control_actor.add_tag', ['tag'])).toBeDefined();
    expect(schemaAt('control_actor.add_tag', ['tag'])?.maxLength).toBeUndefined();
  });
});
