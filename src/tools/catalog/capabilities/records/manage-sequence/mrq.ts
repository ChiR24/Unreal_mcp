/**
 * Movie Render Queue (MRQ) records: create_render_job,
 * configure_output_settings, add_render_pass, configure_anti_aliasing,
 * configure_console_variables, configure_burn_ins, queue_render,
 * start_render.
 *
 * Gated by MCP_HAS_MOVIE_RENDER_PIPELINE (MovieRenderPipeline plugin).
 *
 * ASYNC/ARTIFACT CONTRACT:
 * - queue_render only ENABLES a job in the MRQ queue (SetIsEnabled(true));
 *   it does NOT start rendering. Enqueue != completion.
 * - start_render is the separable execution operation: it allocates an
 *   executor, starts the render, and blocks until completion/fatal/timeout.
 *   Only start_render supports advisory cancellation (notifications/cancelled
 *   -> CancelStartRender) and takes a render deadline.
 * - Timeout: renderTimeoutMs defaults to and is capped at 300000ms (5min) by the
 *   record; TRANSPORT_GRACE 35000ms (cancel wait clamped to 30000ms). It was
 *   named timeoutMs, which both gateways keep for options, so it never arrived.
 * - MRQ cancellation is ADVISORY: it requests executor stop but cannot
 *   interrupt an already-executing render frame. A second concurrent render
 *   is rejected with MRQ_ALREADY_RENDERING.
 * - Artifacts: rendered output files at the configured output directory.
 *   No plugin-side UE asset is created. Completion truth = file existence
 *   at the output directory, verified by a 0.1s ticker.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { A } from './alias-props.js';
import { buildRecord, MRQ_PLUGINS, P } from './helpers.js';

const F = 'mrq';
const D = 'movie_render';

/** ValidateOutputSettingsPayload + ApplyOutputSettings (Output*.cpp), read by create_render_job and configure_output_settings. */
const OUTPUT_SETTINGS = {
  outputDirectory: P.outputDirectory,
  fileNameFormat: P.fileNameFormat, resolution: P.mrqResolution,
  width: P.width, height: P.height, frameRate: P.frameRate,
  startFrame: P.startFrame,
  endFrame: { type: 'integer', minimum: 1, description: 'Custom playback range end frame, EXCLUSIVE: must be strictly greater than startFrame. 0..1 renders exactly one frame; 0..0 renders nothing and is refused as INVALID_FRAME_RANGE.' },
  audioOutput: { type: 'boolean', description: 'true also writes a .wav of everything the world played during the render (music, effects, gameplay sounds) beside the frames; false removes it. MRQ records audio only when the editor runs with -DeterministicAudio, and the reply warns when it does not.' },
  gameModeOverride: { type: 'string', description: 'The game mode the render plays. Unset, MRQ uses its cinematic game mode, which spawns no player pawn and no HUD: right for a cinematic, empty for gameplay. "level" plays the level\'s own game mode (its pawn, HUD and rules); a class path (/Game/X/BP_Mode or /Script/Module.Class) plays that one.' },
};

/** ResolveJob (State.cpp) selects the job by jobId or renderJobName. */
const JOB_SELECTOR = ['jobId', 'renderJobName'];

/** Output.cpp:187-189 reads only these two members of settings. */
const OUTPUT_ONLY_SETTINGS = {
  type: 'object',
  description: 'Nested output settings.',
  additionalProperties: false,
  properties: {
    handleFrameCount: P.mrqSettings.properties.handleFrameCount,
    zeroPadFrameNumbers: P.mrqSettings.properties.zeroPadFrameNumbers,
  },
};

export const MRQ_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.mrq.create_render_job', action: 'create_render_job', family: F, domain: D,
    summary: 'Create a new Movie Render Queue job for a sequence.',
    whenToUse: ['A new render job must be created for a cinematic sequence.'],
    whenNotToUse: ['A render job already exists for the sequence.'],
    // HandleCreateRenderJob (JobCreation.cpp) validates and applies the same
    // output settings configure_output_settings takes.
    inputProps: { sequencePath: P.sequencePath, mapPath: P.mapPath, renderJobName: P.renderJobName, jobName: A.jobName, ...OUTPUT_SETTINGS, settings: OUTPUT_ONLY_SETTINGS },
    required: ['sequencePath'],
    outputProps: { jobId: P.jobId },
    outputRequired: ['jobId'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: MRQ_PLUGINS,
    exampleInput: { action: 'create_render_job', sequencePath: '/Game/Cinematics/SEQ_Master', mapPath: '/Game/Maps/M_Cinematics' },
    exampleOutput: { success: true, jobId: 'render-job-1' },
  }),
  buildRecord({
    id: 'sequence.mrq.configure_output_settings', action: 'configure_output_settings', family: F, domain: D,
    summary: 'Configure output resolution, format, and directory for an MRQ job.',
    whenToUse: ['Render output settings must be specified.'],
    whenNotToUse: ['The job does not exist.'],
    inputProps: {
      jobId: P.jobId, ...OUTPUT_SETTINGS,
      settings: P.mrqSettings,
      renderJobName: P.renderJobName,
    },
    requiredOneOf: JOB_SELECTOR,
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: MRQ_PLUGINS,
    exampleInput: { action: 'configure_output_settings', jobId: 'render-job-1', outputDirectory: '/tmp/renders', resolution: '1920x1080' },
  }),
  buildRecord({
    id: 'sequence.mrq.add_render_pass', action: 'add_render_pass', family: F, domain: D,
    summary: 'Add a render pass (beauty, depth, object ID, the game UI layer, etc.) to an MRQ job.',
    whenToUse: ['An additional render pass must be added to the job.', 'A render of gameplay must show the HUD and menus (renderPass ui).'],
    whenNotToUse: ['The pass is not supported by the MRQ configuration.'],
    inputProps: {
      jobId: P.jobId, renderJobName: P.renderJobName, renderPass: P.renderPass,
      renderPasses: P.renderPasses, materialPath: P.materialPath,
      includeTranslucentObjects: P.includeTranslucentObjects,
      compositeOntoFinalImage: { type: 'boolean', description: 'For the ui pass: draw the UI over the final image (default true); false writes it as its own ViewportUI layer.' },
    },
    requiredOneOf: JOB_SELECTOR,
    effect: 'write', latency: 'instant', resources: 'low', plugins: MRQ_PLUGINS,
    exampleInput: { action: 'add_render_pass', jobId: 'render-job-1', renderPass: 'beauty' },
  }),
  buildRecord({
    id: 'sequence.mrq.configure_anti_aliasing', action: 'configure_anti_aliasing', family: F, domain: D,
    summary: 'Configure anti-aliasing method and sample counts for an MRQ job.',
    whenToUse: ['Anti-aliasing settings must be specified for render quality.'],
    whenNotToUse: ['Default anti-aliasing is acceptable.'],
    inputProps: { jobId: P.jobId, renderJobName: P.renderJobName, antiAliasingMethod: P.antiAliasingMethod, method: A.method, spatialSampleCount: P.sampleCount, temporalSampleCount: P.sampleCount, settings: P.mrqSettings },
    requiredOneOf: JOB_SELECTOR,
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: MRQ_PLUGINS,
    exampleInput: { action: 'configure_anti_aliasing', jobId: 'render-job-1', antiAliasingMethod: 'TSAA', spatialSampleCount: 4, temporalSampleCount: 4 },
  }),
  buildRecord({
    id: 'sequence.mrq.configure_console_variables', action: 'configure_console_variables', family: F, domain: D,
    summary: 'Set console variables for an MRQ job render.',
    whenToUse: ['CVars must be set for render-specific behavior.'],
    whenNotToUse: ['Default CVars are acceptable.'],
    inputProps: { jobId: P.jobId, renderJobName: P.renderJobName, consoleVariables: { type: 'object', description: 'CVar name-value pairs.', additionalProperties: true, 'x-unreal-reflection-boundary': true } },
    required: ['consoleVariables'],
    requiredOneOf: JOB_SELECTOR,
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: MRQ_PLUGINS,
    exampleInput: { action: 'configure_console_variables', jobId: 'render-job-1', consoleVariables: { 'r.AntiAliasingMethod': 2 } },
  }),
  buildRecord({
    id: 'sequence.mrq.configure_burn_ins', action: 'configure_burn_ins', family: F, domain: D,
    summary: 'Configure burn-in overlay settings for an MRQ job.',
    whenToUse: ['Burn-in overlays must be composited onto rendered frames.'],
    whenNotToUse: ['Burn-ins are not needed for the render.'],
    inputProps: { jobId: P.jobId, renderJobName: P.renderJobName, burnIn: { type: 'object', description: 'Burn-in settings.', additionalProperties: false, properties: { enabled: { type: 'boolean', description: 'Whether burn-ins are enabled.' }, compositeOntoFinalImage: { type: 'boolean', description: 'Whether to composite onto the final image.' }, classPath: P.burnInClassPath }, required: ['enabled'] } },
    requiredOneOf: JOB_SELECTOR,
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: MRQ_PLUGINS,
    exampleInput: { action: 'configure_burn_ins', jobId: 'render-job-1', burnIn: { enabled: true, compositeOntoFinalImage: true, classPath: '/Script/MovieRenderPipelineCore.MoviePipelineBurnInWidget' } },
  }),
  buildRecord({
    id: 'sequence.mrq.queue_render', action: 'queue_render', family: F, domain: D,
    summary: 'Enable a render job in the MRQ queue without starting execution. Enqueue does NOT mean render completion.',
    whenToUse: ['A job must be added to the render queue for later execution.'],
    whenNotToUse: ['The render should start immediately. Use start_render instead.'],
    inputProps: { jobId: P.jobId, renderJobId: A.renderJobId, renderJobName: P.renderJobName, onlyJob: { type: 'boolean', description: 'Whether to queue only this job.' }, useCurrentLevel: P.useCurrentLevel },
    requiredOneOf: JOB_SELECTOR,
    outputProps: { message: P.message },
    outputRequired: [],
    effect: 'write', latency: 'instant', resources: 'low', plugins: MRQ_PLUGINS,
    exampleInput: { action: 'queue_render', jobId: 'render-job-1' },
  }),
  buildRecord({
    id: 'sequence.mrq.start_render', action: 'start_render', family: F, domain: D,
    summary: 'Start MRQ render execution. Blocks until completion, fatal error, or timeout. Supports advisory cancellation. renderTimeoutMs (how long the render may run before it is cancelled) defaults to and is capped at 300000 (5 minutes); the transport waits 35000ms longer for the reply.',
    whenToUse: ['The MRQ queue must be executed to produce rendered output files.'],
    whenNotToUse: ['A render is already in progress (MRQ_ALREADY_RENDERING).'],
    // ExecuteStartRender (Execution.cpp) selects the job by jobId or
    // renderJobName, honours onlyJob, and takes a render deadline no longer
    // than the 300000ms the transport waits for (it waits 335000ms in all).
    inputProps: { jobId: P.jobId, renderJobName: P.renderJobName, onlyJob: { type: 'boolean', description: 'Render only this job; every other queued job is disabled for this render.' }, renderTimeoutMs: { type: 'integer', minimum: 1, maximum: 300000, description: 'How long the render may run before it is cancelled, in milliseconds (default and maximum 300000). options.timeoutMs is the gateway deadline for the call and never reaches the render.' }, executorClass: P.executorClass, useCurrentLevel: P.useCurrentLevel },
    requiredOneOf: JOB_SELECTOR,
    outputProps: {
      outputDirectory: P.outputDirectory,
      renderContinuesAsynchronously: { type: 'boolean', description: 'Whether render continued asynchronously after timeout.' },
      bCancellationDeadlineExpired: { type: 'boolean', description: 'Whether the cancellation deadline expired.' },
    },
    outputRequired: [],
    effect: 'write',
    behavior: { longRunning: true, safeToRetry: false },
    latency: 'long-running', resources: 'high', plugins: MRQ_PLUGINS,
    exampleInput: { action: 'start_render', jobId: 'render-job-1' },
    exampleOutput: { success: true, outputDirectory: '/tmp/renders', renderContinuesAsynchronously: false, bCancellationDeadlineExpired: false },
  }),
];
