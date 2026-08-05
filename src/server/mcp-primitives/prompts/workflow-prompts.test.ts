// src/server/mcp-primitives/prompts/workflow-prompts.test.ts
// Task 32 (RED first): the six workflow prompt definitions must reference ONLY
// capability ids that exist in the generated canonical registry and resource
// uris approved by Task 31, must be versioned, and must never contain autonomous
// execution / conversation-memory instructions.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { WORKFLOW_PROMPT_IDS } from './prompt-types.js';
import { WORKFLOW_PROMPTS } from './workflow-prompts.js';

const root = process.cwd();

interface CanonicalRecord {
  readonly id: string;
  readonly parent: { readonly parent: string };
  readonly legacyIds: readonly { readonly tool: string; readonly action: string }[];
}
const registry = JSON.parse(
  readFileSync(
    resolve(root, 'src/tools/catalog/capabilities/generated/canonical-registry.generated.json'),
    'utf8',
  ),
) as { readonly records: readonly CanonicalRecord[] };
const recordById = new Map(registry.records.map((r) => [r.id, r]));

// Task 31's approved resource surface (its evidence is the authoritative
// list). The evidence is a local artifact (`.omo/` is gitignored, not
// distributed); when it is absent the approval test is skipped because there
// is no approved surface to validate against.
let task31: { readonly listResources: { readonly uris: readonly string[] }; readonly listResourceTemplates: { readonly templates: readonly string[] } } | undefined;
try {
  task31 = JSON.parse(
    readFileSync(resolve(root, '.omo/evidence/task-31/qa-driver-output.json'), 'utf8'),
  ) as typeof task31;
} catch {
  task31 = undefined;
}
const approvedResourceUris =
  task31 === undefined
    ? null
    : new Set<string>([...task31.listResources.uris, ...task31.listResourceTemplates.templates]);

// Phrases that would turn user-readable guidance into an autonomy / memory claim.
const FORBIDDEN_BODY_PATTERNS =
  /\b(automatically|autonomously|without asking|without confirmation|on your behalf|remember this|store (?:this|the|it)|persist(?:ent)? memory|conversation memory|silently)\b/i;

describe('workflow-prompts definitions', () => {
  it('defines exactly the six workflow ids, uniquely and versioned', () => {
    expect(WORKFLOW_PROMPTS.map((p) => p.id)).toEqual([...WORKFLOW_PROMPT_IDS]);
    expect(new Set(WORKFLOW_PROMPTS.map((p) => p.id)).size).toBe(6);
    for (const prompt of WORKFLOW_PROMPTS) {
      expect(Number.isInteger(prompt.version)).toBe(true);
      expect(prompt.version).toBeGreaterThanOrEqual(1);
      expect(prompt.arguments.length).toBeGreaterThanOrEqual(1);
      expect(prompt.steps.length).toBeGreaterThanOrEqual(3);
    }
  });

  it('references only capability ids that exist in the generated canonical registry', () => {
    for (const prompt of WORKFLOW_PROMPTS) {
      for (const step of prompt.steps) {
        const record = recordById.get(step.capabilityId);
        expect(record, `${prompt.id}: ${step.capabilityId} must exist`).toBeDefined();
        // The rendered describe hint must match the canonical parent + legacy action.
        expect(step.parentTool).toBe(record?.parent.parent);
        expect(step.action).toBe(record?.legacyIds[0]?.action);
      }
    }
  });

  it.runIf(approvedResourceUris !== null)('references only resource uris approved by Task 31', () => {
    for (const prompt of WORKFLOW_PROMPTS) {
      for (const step of prompt.steps) {
        if (step.resourceUri !== undefined) {
          expect(approvedResourceUris?.has(step.resourceUri), `${prompt.id}: ${step.resourceUri}`).toBe(true);
        }
      }
    }
  });

  it('never declares a secret argument', () => {
    const secretName = /(token|secret|password|passwd|api[_-]?key|apikey|credential|private[_-]?key|bearer|auth)/i;
    for (const prompt of WORKFLOW_PROMPTS) {
      for (const arg of prompt.arguments) {
        expect(secretName.test(arg.name), `${prompt.id}: ${arg.name}`).toBe(false);
      }
    }
  });

  it('carries no autonomous-execution or memory instruction in any authored text', () => {
    for (const prompt of WORKFLOW_PROMPTS) {
      const texts = [
        prompt.title,
        prompt.description,
        ...prompt.arguments.map((a) => a.description),
        ...prompt.steps.map((s) => `${s.summary} ${s.safety}`),
      ];
      for (const text of texts) {
        expect(FORBIDDEN_BODY_PATTERNS.test(text), `${prompt.id}: "${text}"`).toBe(false);
      }
    }
  });
});
