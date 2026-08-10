// src/server/mcp-primitives/prompts/prompt-catalog.ts
// Task 32: the pure list/get prompt catalog. It renders a bounded, deterministic,
// user-readable workflow body from a definition plus validated arguments. It has
// NO transport, executes NOTHING, stores NO state, and never interpolates a
// secret. Every failure path throws a typed PromptError. Task 37 wires this into
// the `prompts/list` and `prompts/get` protocol methods.

import {
  PROMPT_ERROR_CODES,
  PromptError,
  assertNotSecret,
  enforcePromptByteBudget,
  validateArgumentValue,
} from './prompt-errors.js';
import {
  type GetPromptOutput,
  type ListPromptEntry,
  type PromptReferenceValidator,
  type WorkflowPrompt,
  type WorkflowPromptId,
  isWorkflowPromptId,
} from './prompt-types.js';
import { WORKFLOW_PROMPTS } from './workflow-prompts.js';

const PROMPT_BY_ID: ReadonlyMap<WorkflowPromptId, WorkflowPrompt> = new Map(
  WORKFLOW_PROMPTS.map((prompt) => [prompt.id, prompt]),
);

function toListEntry(prompt: WorkflowPrompt): ListPromptEntry {
  return {
    name: prompt.id,
    version: prompt.version,
    title: prompt.title,
    description: prompt.description,
    arguments: prompt.arguments.map((arg) => ({
      name: arg.name,
      description: arg.description,
      required: arg.required,
    })),
  };
}

/** The `prompts/list` payload: every workflow in stable definition order. */
export function listPrompts(): readonly ListPromptEntry[] {
  return WORKFLOW_PROMPTS.map(toListEntry);
}

/** Resolve one workflow definition, or throw a typed not-found error. */
export function getWorkflowPrompt(id: WorkflowPromptId): WorkflowPrompt {
  const prompt = PROMPT_BY_ID.get(id);
  if (prompt === undefined) {
    throw new PromptError(PROMPT_ERROR_CODES.NOT_FOUND, id, `Unknown workflow prompt: ${id}`);
  }
  return prompt;
}

const DISCLAIMER =
  'Guidance only. Nothing here runs on its own, no conversation state is kept, and you decide '
  + 'whether to run each call. Discover exact parameters with the gateway `describe` operation, '
  + 'then run one `execute` call at a time and review each receipt yourself.';

function renderPromptBody(prompt: WorkflowPrompt, inputs: Readonly<Record<string, string>>): string {
  const lines: string[] = [];
  lines.push(`# ${prompt.title}  (prompt ${prompt.id} v${String(prompt.version)})`, '', DISCLAIMER, '');

  const provided = Object.entries(inputs);
  lines.push('Your inputs:');
  if (provided.length === 0) {
    lines.push('- (none provided)');
  } else {
    for (const [name, value] of provided) {
      lines.push(`- ${name}: ${value}`);
    }
  }
  lines.push('', 'Steps:');

  prompt.steps.forEach((step, index) => {
    lines.push(`${String(index + 1)}. ${step.summary}`);
    lines.push(`   describe: unreal { "operation": "describe", "tool": "${step.parentTool}", "action": "${step.action}" }`);
    lines.push(`   execute:  unreal { "operation": "execute", "capability": "${step.capabilityId}", "params": { } }`);
    if (step.resourceUri !== undefined) {
      lines.push(`   read:     ${step.resourceUri}`);
    }
    lines.push(`   safety:   ${step.safety}`);
  });

  lines.push('', 'Finish: re-read the relevant resource and confirm the outcome before moving on.');
  lines.push('Nothing above is executed for you; run each call yourself.');
  return lines.join('\n');
}

/**
 * Render one workflow prompt into a bounded, deterministic `prompts/get` result.
 * Validation order is deliberate: secrets are refused first (even undeclared
 * ones), then strict/declared checks, then reference existence (fail closed on a
 * stale registry), then the byte budget. Nothing is executed or stored.
 */
export function getPrompt(
  name: string,
  args: Readonly<Record<string, string>>,
  validator: PromptReferenceValidator,
): GetPromptOutput {
  if (!isWorkflowPromptId(name)) {
    throw new PromptError(PROMPT_ERROR_CODES.NOT_FOUND, name, `Unknown workflow prompt: ${name}`);
  }
  const prompt = getWorkflowPrompt(name);

  for (const [argName, value] of Object.entries(args)) {
    assertNotSecret(name, argName, value);
  }

  const declared = new Set(prompt.arguments.map((arg) => arg.name));
  for (const argName of Object.keys(args)) {
    if (!declared.has(argName)) {
      throw new PromptError(PROMPT_ERROR_CODES.UNKNOWN_ARGUMENT, name, `Unknown argument: ${argName}`);
    }
  }

  const inputs: Record<string, string> = {};
  for (const spec of prompt.arguments) {
    const raw = args[spec.name];
    if (raw === undefined) {
      if (spec.required) {
        throw new PromptError(PROMPT_ERROR_CODES.MISSING_ARGUMENT, name, `Missing required argument: ${spec.name}`);
      }
      continue;
    }
    validateArgumentValue(name, spec, raw);
    inputs[spec.name] = raw;
  }

  for (const step of prompt.steps) {
    if (!validator.capabilityExists(step.capabilityId)) {
      throw new PromptError(PROMPT_ERROR_CODES.UNKNOWN_CAPABILITY, name, `Referenced capability does not exist: ${step.capabilityId}`);
    }
    if (step.resourceUri !== undefined && !validator.resourceExists(step.resourceUri)) {
      throw new PromptError(PROMPT_ERROR_CODES.UNKNOWN_RESOURCE, name, `Referenced resource does not exist: ${step.resourceUri}`);
    }
  }

  const text = renderPromptBody(prompt, inputs);
  enforcePromptByteBudget(name, text);
  return {
    description: prompt.description,
    version: prompt.version,
    messages: [{ role: 'user', content: { type: 'text', text } }],
  };
}
