// src/server/mcp-primitives/prompts/prompt-catalog.test.ts
// Task 32 (RED first): the pure list/get catalog. No transport, no execution, no
// stored state — getPrompt renders a bounded, deterministic, user-readable body
// and every failure path throws a typed PromptError.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { getPrompt, getWorkflowPrompt, listPrompts } from './prompt-catalog.js';
import { PromptError } from './prompt-errors.js';
import type { PromptReferenceValidator } from './prompt-types.js';
import { WORKFLOW_PROMPT_IDS } from './prompt-types.js';

const root = process.cwd();

// A reference validator backed by the real generated registry + Task 31 uris.
const registry = JSON.parse(
  readFileSync(
    resolve(root, 'src/tools/catalog/capabilities/generated/canonical-registry.generated.json'),
    'utf8',
  ),
) as { readonly records: readonly { readonly id: string }[] };
const capabilityIds = new Set(registry.records.map((r) => r.id));
// Task 31 evidence is a local artifact (`.omo/` is gitignored, not
// distributed); when it is absent there is no approved URI surface to
// validate against, so resource checks pass instead of failing collection.
let task31: { readonly listResources: { readonly uris: readonly string[] }; readonly listResourceTemplates: { readonly templates: readonly string[] } } | undefined;
try {
  task31 = JSON.parse(
    readFileSync(resolve(root, '.omo/evidence/task-31/qa-driver-output.json'), 'utf8'),
  ) as typeof task31;
} catch {
  task31 = undefined;
}
const resourceUris =
  task31 === undefined
    ? null
    : new Set<string>([...task31.listResources.uris, ...task31.listResourceTemplates.templates]);
const realValidator: PromptReferenceValidator = {
  capabilityExists: (id) => capabilityIds.has(id),
  resourceExists: (uri) => resourceUris === null || resourceUris.has(uri),
};
const allowAll: PromptReferenceValidator = {
  capabilityExists: () => true,
  resourceExists: () => true,
};

describe('prompt-catalog list', () => {
  it('lists the six prompts in deterministic definition order with version metadata', () => {
    const listed = listPrompts();
    expect(listed.map((p) => p.name)).toEqual([...WORKFLOW_PROMPT_IDS]);
    for (const entry of listed) {
      expect(entry.version).toBeGreaterThanOrEqual(1);
      expect(entry.title.length).toBeGreaterThan(0);
      expect(entry.description.length).toBeGreaterThan(0);
      // Arguments expose only the MCP-visible triple, never the internal kind.
      for (const arg of entry.arguments) {
        expect(Object.keys(arg).sort()).toEqual(['description', 'name', 'required']);
      }
    }
  });

  it('is a pure catalog: repeated listing is byte-identical', () => {
    expect(JSON.stringify(listPrompts())).toBe(JSON.stringify(listPrompts()));
  });
});

describe('prompt-catalog get', () => {
  it('renders the sequence-render workflow into a user-readable canonical sequence', () => {
    const out = getPrompt('sequence-render', { sequencePath: '/Game/Cinematics/Intro' }, realValidator);
    expect(out.version).toBeGreaterThanOrEqual(1);
    expect(out.messages).toHaveLength(1);
    const message = out.messages[0];
    expect(message.role).toBe('user');
    expect(message.content.type).toBe('text');
    const body = message.content.text;
    // References the exact canonical MRQ capability ids and the gateway tool.
    for (const id of [
      'sequence.get_properties',
      'sequence.mrq.create_render_job',
      'sequence.mrq.configure_output_settings',
      'sequence.mrq.queue_render',
      'sequence.mrq.start_render',
    ]) {
      expect(body).toContain(id);
    }
    expect(body).toContain('unreal');
    expect(body).toContain('"operation": "execute"');
    // Interpolates the validated argument value.
    expect(body).toContain('/Game/Cinematics/Intro');
    // States it is guidance only and nothing runs on its own.
    expect(body.toLowerCase()).toContain('guidance');
    expect(/\b(automatically|autonomously|without asking|remember this|store this)\b/i.test(body)).toBe(false);
  });

  it('produces byte-identical output for identical inputs (deterministic)', () => {
    const a = getPrompt('asset-import', { destinationPath: '/Game/Imported/Rock' }, realValidator);
    const b = getPrompt('asset-import', { destinationPath: '/Game/Imported/Rock' }, realValidator);
    expect(JSON.stringify(a)).toBe(JSON.stringify(b));
    // The reported version matches the listed version.
    const listedVersion = listPrompts().find((p) => p.name === 'asset-import')?.version;
    expect(a.version).toBe(listedVersion);
  });

  it('accepts a workflow with only optional arguments omitted', () => {
    const out = getPrompt('validation', {}, realValidator);
    expect(out.messages[0].content.text).toContain('system_control.validate_assets');
  });

  it('rejects an unknown prompt name', () => {
    expect(() => getPrompt('does-not-exist', {}, realValidator)).toThrow(PromptError);
    try {
      getWorkflowPrompt('does-not-exist' as never);
    } catch (e) {
      expect((e as PromptError).code).toBe('PROMPT_NOT_FOUND');
    }
  });

  it('rejects an undeclared (strict) argument', () => {
    try {
      getPrompt('asset-import', { destinationPath: '/Game/X', bogus: 'y' }, realValidator);
      throw new Error('expected throw');
    } catch (e) {
      expect(e).toBeInstanceOf(PromptError);
      expect((e as PromptError).code).toBe('PROMPT_UNKNOWN_ARGUMENT');
    }
  });

  it('rejects a missing required argument', () => {
    try {
      getPrompt('asset-import', {}, realValidator);
      throw new Error('expected throw');
    } catch (e) {
      expect((e as PromptError).code).toBe('PROMPT_MISSING_ARGUMENT');
    }
  });

  it('rejects a secret argument before any interpolation, even if undeclared', () => {
    const secretArgSets: Record<string, string>[] = [
      { destinationPath: '/Game/X', apiKey: 'abc123' },
      { destinationPath: '/Game/X', token: 'zzz' },
    ];
    for (const args of secretArgSets) {
      try {
        getPrompt('asset-import', args, realValidator);
        throw new Error('expected throw');
      } catch (e) {
        expect((e as PromptError).code).toBe('PROMPT_SECRET_ARGUMENT');
      }
    }
  });

  it('rejects a secret-looking value under a declared argument', () => {
    try {
      getPrompt('inspect-fix', { objectPath: '/Game/Hero', newValue: '-----BEGIN PRIVATE KEY-----' }, realValidator);
      throw new Error('expected throw');
    } catch (e) {
      expect((e as PromptError).code).toBe('PROMPT_SECRET_ARGUMENT');
    }
  });

  it('rejects an invalid typed argument value', () => {
    try {
      getPrompt('asset-import', { destinationPath: '../escape' }, realValidator);
      throw new Error('expected throw');
    } catch (e) {
      expect((e as PromptError).code).toBe('PROMPT_INVALID_ARGUMENT');
    }
  });

  it('fails closed when a referenced capability is absent (stale registry)', () => {
    const staleCapabilities: PromptReferenceValidator = {
      capabilityExists: (id) => id !== 'sequence.mrq.start_render',
      resourceExists: () => true,
    };
    try {
      getPrompt('sequence-render', { sequencePath: '/Game/Cinematics/Intro' }, staleCapabilities);
      throw new Error('expected throw');
    } catch (e) {
      expect((e as PromptError).code).toBe('PROMPT_UNKNOWN_CAPABILITY');
    }
  });

  it('fails closed when a referenced resource is absent', () => {
    const noResources: PromptReferenceValidator = {
      capabilityExists: () => true,
      resourceExists: () => false,
    };
    try {
      getPrompt('inspect-fix', { objectPath: '/Game/Hero' }, noResources);
      throw new Error('expected throw');
    } catch (e) {
      expect((e as PromptError).code).toBe('PROMPT_UNKNOWN_RESOURCE');
    }
  });

  it('keeps every rendered body within the byte budget', () => {
    for (const id of WORKFLOW_PROMPT_IDS) {
      const args = MINIMAL_ARGS[id];
      const out = getPrompt(id, args, allowAll);
      expect(Buffer.byteLength(out.messages[0].content.text, 'utf8')).toBeLessThanOrEqual(65536);
    }
  });
});

const MINIMAL_ARGS: Record<string, Record<string, string>> = {
  'inspect-fix': { objectPath: '/Game/Hero' },
  'asset-import': { destinationPath: '/Game/Imported/Rock' },
  'level-build': { levelPath: '/Game/Maps/Test' },
  'blueprint-edit': { blueprintPath: '/Game/BP/Hero' },
  validation: {},
  'sequence-render': { sequencePath: '/Game/Cinematics/Intro' },
};
