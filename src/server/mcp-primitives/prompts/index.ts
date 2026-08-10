// src/server/mcp-primitives/prompts/index.ts
// Task 32: public surface of the user-selected workflow prompt primitive. This
// barrel is the single import point for Task 37 protocol wiring; it exposes the
// pure list/get catalog, the definitions, the primitive types, and the typed
// errors. It adds NO behavior and performs NO side effects on import.

export {
  getPrompt,
  getWorkflowPrompt,
  listPrompts,
} from './prompt-catalog.js';
export { WORKFLOW_PROMPTS } from './workflow-prompts.js';
export {
  MAX_ARGUMENT_LENGTH,
  MAX_PROMPT_BYTES,
  PROMPT_CONTENT_ROOTS,
  PROMPT_ERROR_CODES,
  PromptError,
  type PromptErrorCode,
} from './prompt-errors.js';
export {
  INITIAL_PROMPT_VERSION,
  PROMPT_ARGUMENT_KINDS,
  WORKFLOW_PROMPT_IDS,
  asPromptVersion,
  isPromptArgumentKind,
  isWorkflowPromptId,
  type GetPromptOutput,
  type ListPromptEntry,
  type PromptArgumentKind,
  type PromptArgumentSpec,
  type PromptReferenceValidator,
  type PromptStep,
  type PromptTextMessage,
  type PromptVersion,
  type WorkflowPrompt,
  type WorkflowPromptId,
} from './prompt-types.js';
