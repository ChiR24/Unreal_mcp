// tests/unit/mcp-primitives/prompts-completions-native-prompts.ts
// Task 38 lane B — the native prompt-surface oracle (prompts/list + prompts/get).
//
// An INDEPENDENT, executable reimplementation of the native prompt surface after
// the Task 38 remediation: the transport now delegates to
// McpBuildPromptListEntries (full name/title/description/arguments) and
// McpRenderWorkflowPrompt (the byte-identical rendered body plus typed argument
// validation) instead of the old name+title/static-stub. The prompt data,
// render, and validation below are transcribed from the native
// McpPromptCatalog.cpp / McpPromptRender.cpp / McpPromptArgumentValidation.cpp,
// NOT imported from the TypeScript modules under test, so a parity mismatch is a
// genuine TS/native divergence.

interface NativePromptArg {
  readonly name: string;
  readonly description: string;
  readonly required: boolean;
  readonly kind: string;
  readonly allowed?: readonly string[];
}

interface NativePromptStep {
  readonly summary: string;
  readonly capabilityId: string;
  readonly parentTool: string;
  readonly action: string;
  readonly resourceUri?: string;
  readonly safety: string;
}

interface NativePromptDef {
  readonly id: string;
  readonly version: number;
  readonly title: string;
  readonly description: string;
  readonly arguments: readonly NativePromptArg[];
  readonly steps: readonly NativePromptStep[];
}

const DISCLAIMER =
  'Guidance only. Nothing here runs on its own, no conversation state is kept, and you decide '
  + 'whether to run each call. Discover exact parameters with the gateway `describe` operation, '
  + 'then run one `execute` call at a time and review each receipt yourself.';

// McpPromptCatalog.cpp — the six workflow definitions, verbatim.
export const NATIVE_PROMPTS: readonly NativePromptDef[] = [
  {
    id: 'inspect-fix',
    version: 1,
    title: 'Inspect and fix an object property',
    description: 'Walk through inspecting a UObject and correcting a single property, one reviewed call at a time.',
    arguments: [
      { name: 'objectPath', description: 'Content path of the object to inspect', required: true, kind: 'object-path' },
      { name: 'propertyName', description: 'Name of the property to review', required: false, kind: 'identifier' },
      { name: 'newValue', description: 'Value you intend to set, for your reference', required: false, kind: 'text' },
    ],
    steps: [
      { summary: 'Read the current editor selection to confirm the target.', capabilityId: 'inspect.get_selected_actors', parentTool: 'inspect', action: 'get_selected_actors', resourceUri: 'ue://selection', safety: 'Confirm the selected actor is the intended target before reading or changing anything.' },
      { summary: 'Introspect the object and its components.', capabilityId: 'inspect.inspect_object', parentTool: 'inspect', action: 'inspect_object', resourceUri: 'ue://object/{objectPath}', safety: 'Inspection is read-only; use it to understand the object before any edit.' },
      { summary: 'Read the specific property you plan to change.', capabilityId: 'inspect.get_property', parentTool: 'inspect', action: 'get_property', safety: 'Note the current value so you can revert the change by hand if needed.' },
      { summary: 'Apply the fix to a single property.', capabilityId: 'inspect.set_property', parentTool: 'inspect', action: 'set_property', safety: 'Review the new value; this edits editor state only when you run the call yourself.' },
    ],
  },
  {
    id: 'asset-import',
    version: 1,
    title: 'Import an asset into the project',
    description: 'Check the destination, import from a source you supply, then validate the result.',
    arguments: [
      { name: 'destinationPath', description: 'Content folder to import into', required: true, kind: 'content-path' },
      { name: 'sourceFormat', description: 'Source file format', required: false, kind: 'enum', allowed: ['fbx', 'obj', 'gltf', 'png', 'wav'] },
    ],
    steps: [
      { summary: 'List the destination folder contents.', capabilityId: 'asset.list', parentTool: 'manage_asset', action: 'list', resourceUri: 'ue://project', safety: 'Confirm the destination path is correct so you do not overwrite existing assets.' },
      { summary: 'Check whether the target asset already exists.', capabilityId: 'asset.exists', parentTool: 'manage_asset', action: 'exists', resourceUri: 'ue://asset/{assetPath}', safety: 'If it already exists, decide whether replacing it is intended before importing.' },
      { summary: 'Import the asset from your chosen source.', capabilityId: 'asset.import', parentTool: 'manage_asset', action: 'import', safety: 'Supply your own source path when you run this; the import applies only on execute.' },
      { summary: 'Validate the imported asset.', capabilityId: 'asset.validate', parentTool: 'manage_asset', action: 'validate', safety: 'Review the validation report before using the asset in a level.' },
    ],
  },
  {
    id: 'level-build',
    version: 1,
    title: 'Create and build a level',
    description: 'Create a working level, build its lighting, and save it, reviewing each step.',
    arguments: [{ name: 'levelPath', description: 'Content path for the level', required: true, kind: 'content-path' }],
    steps: [
      { summary: 'Read the current level.', capabilityId: 'manage_level.get_current_level', parentTool: 'manage_level', action: 'get_current_level', resourceUri: 'ue://level', safety: 'Confirm which level is active before creating or building anything.' },
      { summary: 'Create the working level.', capabilityId: 'manage_level.create_level', parentTool: 'manage_level', action: 'create_level', resourceUri: 'ue://editor', safety: 'Creating a level does not save it; the file is written only when you save.' },
      { summary: 'Build lighting for the level.', capabilityId: 'manage_level.build_lighting', parentTool: 'manage_level', action: 'build_lighting', safety: 'Lighting builds can be slow; start the build yourself when the scene is ready.' },
      { summary: 'Save the level.', capabilityId: 'manage_level.save', parentTool: 'manage_level', action: 'save', safety: 'Saving routes through the safe save wrapper; review the scene before you save.' },
    ],
  },
  {
    id: 'blueprint-edit',
    version: 1,
    title: 'Edit a Blueprint',
    description: 'Read a Blueprint, add a variable and an SCS component, then compile it.',
    arguments: [
      { name: 'blueprintPath', description: 'Content path of the Blueprint', required: true, kind: 'content-path' },
      { name: 'variableName', description: 'Name of a variable to add', required: false, kind: 'identifier' },
    ],
    steps: [
      { summary: 'Read the Blueprint definition.', capabilityId: 'blueprint.get', parentTool: 'manage_blueprint', action: 'get', resourceUri: 'ue://object/{objectPath}', safety: 'Understand the current Blueprint before editing it.' },
      { summary: 'Add a variable to the Blueprint.', capabilityId: 'blueprint.add_variable', parentTool: 'manage_blueprint', action: 'add_variable', safety: 'Pick a clear variable name; the change applies only when you run the call.' },
      { summary: 'Add a component through the SCS.', capabilityId: 'blueprint.add_scs_component', parentTool: 'manage_blueprint', action: 'add_scs_component', safety: 'Components are owned by the Simple Construction Script; review the component setup.' },
      { summary: 'Compile the Blueprint.', capabilityId: 'blueprint.compile', parentTool: 'manage_blueprint', action: 'compile', safety: 'Compile to surface errors; read the compile result yourself before using the Blueprint.' },
    ],
  },
  {
    id: 'validation',
    version: 1,
    title: 'Validate project assets and level',
    description: 'Run read-only validation across the project, a specific asset, and the current level.',
    arguments: [{ name: 'assetPath', description: 'Content path of an asset to focus on', required: false, kind: 'content-path' }],
    steps: [
      { summary: 'Read the project context.', capabilityId: 'inspect.get_project_settings', parentTool: 'inspect', action: 'get_project_settings', resourceUri: 'ue://project', safety: 'Confirm the project and engine version before running validation.' },
      { summary: "Run data validation across the project's assets.", capabilityId: 'system_control.validate_assets', parentTool: 'system_control', action: 'validate_assets', resourceUri: 'ue://capability/catalog', safety: 'Validation is read-only; review each reported issue yourself.' },
      { summary: 'Validate a specific asset.', capabilityId: 'asset.validate', parentTool: 'manage_asset', action: 'validate', resourceUri: 'ue://asset/{assetPath}', safety: 'Use this to focus on one asset flagged by the project scan.' },
      { summary: 'Validate the current level.', capabilityId: 'manage_level.validate_level', parentTool: 'manage_level', action: 'validate_level', safety: 'Level validation makes no changes; it only reports issues to review.' },
    ],
  },
  {
    id: 'sequence-render',
    version: 1,
    title: 'Render a level sequence',
    description: 'Prepare and run a Movie Render Queue job for a level sequence, reviewing each step.',
    arguments: [
      { name: 'sequencePath', description: 'Content path of the level sequence', required: true, kind: 'content-path' },
      { name: 'outputFormat', description: 'Render output image format', required: false, kind: 'enum', allowed: ['png', 'jpeg', 'exr', 'custom'] },
    ],
    steps: [
      { summary: 'Read the sequence properties.', capabilityId: 'sequence.get_properties', parentTool: 'manage_sequence', action: 'get_properties', resourceUri: 'ue://project', safety: 'Confirm the sequence, frame range, and resolution before rendering.' },
      { summary: 'Create a Movie Render Queue job.', capabilityId: 'sequence.mrq.create_render_job', parentTool: 'manage_sequence', action: 'create_render_job', safety: 'Creating a job does not render; it only prepares the queue entry.' },
      { summary: 'Configure the render output settings.', capabilityId: 'sequence.mrq.configure_output_settings', parentTool: 'manage_sequence', action: 'configure_output_settings', resourceUri: 'ue://editor', safety: 'Confirm the output directory and format yourself before rendering.' },
      { summary: 'Queue the render job.', capabilityId: 'sequence.mrq.queue_render', parentTool: 'manage_sequence', action: 'queue_render', safety: 'Queuing stages the job; nothing is written until you start the render.' },
      { summary: 'Start the render.', capabilityId: 'sequence.mrq.start_render', parentTool: 'manage_sequence', action: 'start_render', safety: 'Rendering can be long-running and writes files; you start it explicitly.' },
    ],
  },
];

const SECRET_NAME = /(token|secret|password|passwd|api[_-]?key|apikey|credential|private[_-]?key|privatekey|bearer|auth)/;
const SECRET_VALUE = /-----BEGIN|\bBearer\s+\S{8,}|\beyJ[A-Za-z0-9_-]{6,}\.[A-Za-z0-9_-]{6,}|\b[A-Fa-f0-9]{40,}\b/;
const HOST_PATH = /^[a-zA-Z]:[\\/]|\\|^~|^\/(?:home|users|etc|var|root|tmp)\b/i;
const IDENTIFIER = /^[A-Za-z_][A-Za-z0-9_]*$/;
const CONTENT_ROOTS = ['/Game', '/Engine', '/Script', '/Temp', '/Niagara'];
const MAX_ARGUMENT_LENGTH = 512;

function hasControlChar(value: string): boolean {
  for (let i = 0; i < value.length; i += 1) {
    const code = value.charCodeAt(i);
    if (code <= 0x1f || code === 0x7f) return true;
  }
  return false;
}

interface NativeArgError { readonly code: string; readonly message: string }

function validateContentPath(name: string, value: string): NativeArgError | null {
  if (value.length === 0) return { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${name}" is empty` };
  if (hasControlChar(value)) return { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${name}" has control characters` };
  if (HOST_PATH.test(value)) return { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${name}" is a host filesystem path` };
  if (value.split('/').includes('..')) return { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${name}" contains path traversal` };
  if (!CONTENT_ROOTS.some((root) => value === root || value.startsWith(`${root}/`))) {
    return { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${name}" must resolve under a UE content root` };
  }
  return null;
}

function validateArgument(spec: NativePromptArg, value: string): NativeArgError | null {
  if (value.length > MAX_ARGUMENT_LENGTH) return { code: 'PROMPT_ARGUMENT_TOO_LONG', message: `Argument "${spec.name}" is too long` };
  switch (spec.kind) {
    case 'content-path':
      return validateContentPath(spec.name, value);
    case 'object-path': {
      const parts = value.split('.');
      if (parts.length > 2 || parts.length === 0) return { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${spec.name}" is not a valid object path` };
      const pathError = validateContentPath(spec.name, parts[0]);
      if (pathError !== null) return pathError;
      if (parts.length === 2 && !IDENTIFIER.test(parts[1])) return { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${spec.name}" has an invalid object suffix` };
      return null;
    }
    case 'identifier':
      return IDENTIFIER.test(value) ? null : { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${spec.name}" is not a valid identifier` };
    case 'enum':
      return (spec.allowed ?? []).includes(value) ? null : { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${spec.name}" must be one of the allowed values` };
    case 'text':
      return hasControlChar(value) ? { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${spec.name}" has control characters` } : null;
    default:
      return { code: 'PROMPT_INVALID_ARGUMENT', message: `Argument "${spec.name}" has an unknown kind` };
  }
}

function renderBody(prompt: NativePromptDef, inputs: readonly (readonly [string, string])[]): string {
  const lines: string[] = [`# ${prompt.title}  (prompt ${prompt.id} v${String(prompt.version)})`, '', DISCLAIMER, '', 'Your inputs:'];
  if (inputs.length === 0) lines.push('- (none provided)');
  else for (const [name, value] of inputs) lines.push(`- ${name}: ${value}`);
  lines.push('', 'Steps:');
  prompt.steps.forEach((step, index) => {
    lines.push(`${String(index + 1)}. ${step.summary}`);
    lines.push(`   describe: unreal { "operation": "describe", "tool": "${step.parentTool}", "action": "${step.action}" }`);
    lines.push(`   execute:  unreal { "operation": "execute", "capability": "${step.capabilityId}", "params": { } }`);
    if (step.resourceUri !== undefined) lines.push(`   read:     ${step.resourceUri}`);
    lines.push(`   safety:   ${step.safety}`);
  });
  lines.push('', 'Finish: re-read the relevant resource and confirm the outcome before moving on.');
  lines.push('Nothing above is executed for you; run each call yourself.');
  return lines.join('\n');
}

export interface NativePromptListEntry {
  readonly name: string;
  readonly title: string;
  readonly description: string;
  readonly arguments: readonly { readonly name: string; readonly description: string; readonly required: boolean }[];
}

export function nativePromptsList(): readonly NativePromptListEntry[] {
  return NATIVE_PROMPTS.map((prompt) => ({
    name: prompt.id,
    title: prompt.title,
    description: prompt.description,
    arguments: prompt.arguments.map((arg) => ({ name: arg.name, description: arg.description, required: arg.required })),
  }));
}

export interface NativePromptGetResult {
  readonly ok: boolean;
  readonly errorCode: string | null;
  readonly errorMessage: string | null;
  readonly messages: readonly { role: 'user'; content: { type: 'text'; text: string } }[];
}

function fail(code: string, message: string): NativePromptGetResult {
  return { ok: false, errorCode: code, errorMessage: message, messages: [] };
}

export function nativePromptsGet(name: string, args: Readonly<Record<string, string>> = {}): NativePromptGetResult {
  const prompt = NATIVE_PROMPTS.find((entry) => entry.id === name);
  if (prompt === undefined) return fail('PROMPT_NOT_FOUND', `Unknown workflow prompt: ${name}`);

  for (const [argName, value] of Object.entries(args)) {
    if (SECRET_NAME.test(argName.toLowerCase())) return fail('PROMPT_SECRET_ARGUMENT', `Argument "${argName}" names a secret; prompts never accept or interpolate secrets`);
    if (SECRET_VALUE.test(value)) return fail('PROMPT_SECRET_ARGUMENT', `Argument "${argName}" holds a secret-looking value; prompts never interpolate secrets`);
  }

  const declared = new Set(prompt.arguments.map((arg) => arg.name));
  for (const argName of Object.keys(args)) {
    if (!declared.has(argName)) return fail('PROMPT_UNKNOWN_ARGUMENT', `Unknown argument: ${argName}`);
  }

  const inputs: [string, string][] = [];
  for (const spec of prompt.arguments) {
    const raw = args[spec.name];
    if (raw === undefined) {
      if (spec.required) return fail('PROMPT_MISSING_ARGUMENT', `Missing required argument: ${spec.name}`);
      continue;
    }
    const error = validateArgument(spec, raw);
    if (error !== null) return fail(error.code, error.message);
    inputs.push([spec.name, raw]);
  }

  const text = renderBody(prompt, inputs);
  return { ok: true, errorCode: null, errorMessage: null, messages: [{ role: 'user', content: { type: 'text', text } }] };
}
