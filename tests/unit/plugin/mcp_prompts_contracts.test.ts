import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const root = process.cwd();
const nativeRoot = resolve(
  root,
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP',
);

const promptHeader = readFileSync(resolve(nativeRoot, 'Primitives/McpPromptCatalog.h'), 'utf8');
const promptCpp = readFileSync(resolve(nativeRoot, 'Primitives/McpPromptCatalog.cpp'), 'utf8');
const nativeText = `${promptHeader}\n${promptCpp}`;

const tsTypes = readFileSync(resolve(root, 'src/server/mcp-primitives/prompts/prompt-types.ts'), 'utf8');
const tsErrors = readFileSync(resolve(root, 'src/server/mcp-primitives/prompts/prompt-errors.ts'), 'utf8');
const tsWorkflows = readFileSync(resolve(root, 'src/server/mcp-primitives/prompts/workflow-prompts.ts'), 'utf8');

const WORKFLOW_IDS = ['inspect-fix', 'asset-import', 'level-build', 'blueprint-edit', 'validation', 'sequence-render'];

const CAPABILITY_IDS = [
  'inspect.get_selected_actors', 'inspect.inspect_object', 'inspect.get_property', 'inspect.set_property', 'inspect.get_project_settings',
  'asset.list', 'asset.exists', 'asset.import', 'asset.validate',
  'manage_level.get_current_level', 'manage_level.create_level', 'manage_level.build_lighting', 'manage_level.save', 'manage_level.validate_level',
  'blueprint.get', 'blueprint.add_variable', 'blueprint.add_scs_component', 'blueprint.compile',
  'system_control.validate_assets',
  'sequence.get_properties', 'sequence.mrq.create_render_job', 'sequence.mrq.configure_output_settings', 'sequence.mrq.queue_render', 'sequence.mrq.start_render',
];

const RESOURCE_URIS = [
  'ue://selection', 'ue://object/{objectPath}', 'ue://project',
  'ue://asset/{assetPath}', 'ue://level', 'ue://editor', 'ue://capability/catalog',
];

const ERROR_CODES = [
  'PROMPT_NOT_FOUND', 'PROMPT_UNKNOWN_ARGUMENT', 'PROMPT_MISSING_ARGUMENT', 'PROMPT_INVALID_ARGUMENT',
  'PROMPT_SECRET_ARGUMENT', 'PROMPT_ARGUMENT_TOO_LONG', 'PROMPT_TOO_LARGE',
  'PROMPT_UNKNOWN_CAPABILITY', 'PROMPT_UNKNOWN_RESOURCE',
];

const FORBIDDEN_BODY_PATTERNS =
  /\b(automatically|autonomously|without asking|on your behalf|remember this|conversation memory|silently)\b/i;

const countPureLines = (source: string): number =>
  source
    .split(/\r?\n/u)
    .filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:#|\/\/)/u.test(line))
    .length;

describe('mcp prompts native source contracts', () => {
  it('mirrors the six workflow ids on both surfaces', () => {
    for (const id of WORKFLOW_IDS) {
      expect(nativeText).toContain(id);
      expect(tsTypes).toContain(id);
      expect(tsWorkflows).toContain(id);
    }
    expect(nativeText).toContain('McpIsWorkflowPromptId');
    expect(nativeText).toContain('FMcpWorkflowPrompt');
  });

  it('mirrors every referenced canonical capability id on both surfaces', () => {
    for (const capabilityId of CAPABILITY_IDS) {
      expect(nativeText, `native must reference ${capabilityId}`).toContain(capabilityId);
      expect(tsWorkflows, `ts must reference ${capabilityId}`).toContain(capabilityId);
    }
  });

  it('mirrors every referenced Task 31 resource uri on both surfaces', () => {
    for (const uri of RESOURCE_URIS) {
      expect(nativeText, `native must reference ${uri}`).toContain(uri);
      expect(tsWorkflows, `ts must reference ${uri}`).toContain(uri);
    }
  });

  it('mirrors the bounded budgets on both surfaces', () => {
    expect(promptHeader).toContain('65536');
    expect(promptHeader).toContain('512');
    expect(tsErrors).toContain('65536');
    expect(tsErrors).toContain('512');
  });

  it('mirrors the typed error codes on both surfaces', () => {
    for (const code of ERROR_CODES) {
      expect(promptHeader).toContain(code);
      expect(tsErrors).toContain(code);
    }
  });

  it('declares the strict argument kinds and secret guard on both surfaces', () => {
    for (const kind of ['content-path', 'object-path', 'identifier', 'enum', 'engine-version', 'text']) {
      expect(tsTypes).toContain(kind);
    }
    for (const kind of ['content-path', 'object-path', 'enum']) {
      expect(nativeText).toContain(kind);
    }
    for (const fragment of ['token', 'secret', 'password', 'bearer']) {
      expect(promptCpp).toContain(fragment);
      expect(tsErrors).toContain(fragment);
    }
  });

  it('never emits a host path or reads the project-path env in native metadata', () => {
    for (const forbidden of ['C:\\', '/home/', '/Users/', '.uproject', 'UE_PROJECT_PATH']) {
      expect(nativeText).not.toContain(forbidden);
    }
  });

  it('carries no autonomous-execution or memory instruction in native prompt data', () => {
    expect(FORBIDDEN_BODY_PATTERNS.test(nativeText)).toBe(false);
  });

  it('keeps the native prompt files within the 250 pure-line ceiling', () => {
    expect(countPureLines(promptHeader)).toBeLessThanOrEqual(250);
    expect(countPureLines(promptCpp)).toBeLessThanOrEqual(250);
  });
});
