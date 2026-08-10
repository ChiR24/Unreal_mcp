// src/server/mcp-primitives/prompts/workflow-prompts.ts
// Task 32: the six versioned, user-selected workflow prompt definitions. Each is
// a bounded, deterministic sequence of canonical capability ids (verified
// against the generated registry) and Task 31 resource uris, with a human safety
// note per step. These are DATA ONLY: no execution, no stored state, no autonomy
// or memory instruction. The native mirror is McpPromptCatalog.{h,cpp}.

import { INITIAL_PROMPT_VERSION, type WorkflowPrompt } from './prompt-types.js';

/**
 * All workflow prompts, in stable definition order (this is the `prompts/list`
 * order). Every `capabilityId` exists in the canonical registry and every
 * `resourceUri` is a Task 31 approved uri; the tests assert both.
 */
export const WORKFLOW_PROMPTS: readonly WorkflowPrompt[] = [
  {
    id: 'inspect-fix',
    version: INITIAL_PROMPT_VERSION,
    title: 'Inspect and fix an object property',
    description: 'Walk through inspecting a UObject and correcting a single property, one reviewed call at a time.',
    arguments: [
      { name: 'objectPath', description: 'Content path of the object to inspect', required: true, kind: 'object-path', example: '/Game/Heroes/BP_Hero' },
      { name: 'propertyName', description: 'Name of the property to review', required: false, kind: 'identifier', example: 'RelativeLocation' },
      { name: 'newValue', description: 'Value you intend to set, for your reference', required: false, kind: 'text', example: 'Z=120' },
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
    version: INITIAL_PROMPT_VERSION,
    title: 'Import an asset into the project',
    description: 'Check the destination, import from a source you supply, then validate the result.',
    arguments: [
      { name: 'destinationPath', description: 'Content folder to import into', required: true, kind: 'content-path', example: '/Game/Imported/Rock' },
      { name: 'sourceFormat', description: 'Source file format', required: false, kind: 'enum', allowed: ['fbx', 'obj', 'gltf', 'png', 'wav'], example: 'fbx' },
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
    version: INITIAL_PROMPT_VERSION,
    title: 'Create and build a level',
    description: 'Create a working level, build its lighting, and save it, reviewing each step.',
    arguments: [
      { name: 'levelPath', description: 'Content path for the level', required: true, kind: 'content-path', example: '/Game/Maps/Arena' },
    ],
    steps: [
      { summary: 'Read the current level.', capabilityId: 'manage_level.get_current_level', parentTool: 'manage_level', action: 'get_current_level', resourceUri: 'ue://level', safety: 'Confirm which level is active before creating or building anything.' },
      { summary: 'Create the working level.', capabilityId: 'manage_level.create_level', parentTool: 'manage_level', action: 'create_level', resourceUri: 'ue://editor', safety: 'Creating a level does not save it; the file is written only when you save.' },
      { summary: 'Build lighting for the level.', capabilityId: 'manage_level.build_lighting', parentTool: 'manage_level', action: 'build_lighting', safety: 'Lighting builds can be slow; start the build yourself when the scene is ready.' },
      { summary: 'Save the level.', capabilityId: 'manage_level.save', parentTool: 'manage_level', action: 'save', safety: 'Saving routes through the safe save wrapper; review the scene before you save.' },
    ],
  },
  {
    id: 'blueprint-edit',
    version: INITIAL_PROMPT_VERSION,
    title: 'Edit a Blueprint',
    description: 'Read a Blueprint, add a variable and an SCS component, then compile it.',
    arguments: [
      { name: 'blueprintPath', description: 'Content path of the Blueprint', required: true, kind: 'content-path', example: '/Game/Blueprints/BP_Door' },
      { name: 'variableName', description: 'Name of a variable to add', required: false, kind: 'identifier', example: 'OpenSpeed' },
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
    version: INITIAL_PROMPT_VERSION,
    title: 'Validate project assets and level',
    description: 'Run read-only validation across the project, a specific asset, and the current level.',
    arguments: [
      { name: 'assetPath', description: 'Content path of an asset to focus on', required: false, kind: 'content-path', example: '/Game/Imported/Rock' },
    ],
    steps: [
      { summary: 'Read the project context.', capabilityId: 'inspect.get_project_settings', parentTool: 'inspect', action: 'get_project_settings', resourceUri: 'ue://project', safety: 'Confirm the project and engine version before running validation.' },
      { summary: "Run data validation across the project's assets.", capabilityId: 'system_control.validate_assets', parentTool: 'system_control', action: 'validate_assets', resourceUri: 'ue://capability/catalog', safety: 'Validation is read-only; review each reported issue yourself.' },
      { summary: 'Validate a specific asset.', capabilityId: 'asset.validate', parentTool: 'manage_asset', action: 'validate', resourceUri: 'ue://asset/{assetPath}', safety: 'Use this to focus on one asset flagged by the project scan.' },
      { summary: 'Validate the current level.', capabilityId: 'manage_level.validate_level', parentTool: 'manage_level', action: 'validate_level', safety: 'Level validation makes no changes; it only reports issues to review.' },
    ],
  },
  {
    id: 'sequence-render',
    version: INITIAL_PROMPT_VERSION,
    title: 'Render a level sequence',
    description: 'Prepare and run a Movie Render Queue job for a level sequence, reviewing each step.',
    arguments: [
      { name: 'sequencePath', description: 'Content path of the level sequence', required: true, kind: 'content-path', example: '/Game/Cinematics/Intro' },
      { name: 'outputFormat', description: 'Render output image format', required: false, kind: 'enum', allowed: ['png', 'jpeg', 'exr', 'custom'], example: 'png' },
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
