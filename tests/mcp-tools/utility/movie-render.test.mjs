#!/usr/bin/env node
/**
 * manage_sequence — Movie Render Queue (MRQ) integration + parameter-coverage
 * cases. T5 lane L2.
 *
 * Exercises the 8 MOVIE_RENDER_ACTIONS (create_render_job,
 * configure_output_settings, add_render_pass, configure_anti_aliasing,
 * configure_console_variables, configure_burn_ins, queue_render,
 * start_render) and every audited optional parameter they consume.
 *
 * These cases are captured statically by the parameter-combination audit
 * (tests/parameter-combination-audit.mjs) and also run live against a
 * connected Unreal Editor + Movie Render Pipeline plugin via `npm test`.
 * They intentionally avoid Media / RecordReplay parameters so they do not
 * mask the L3 / L4 coverage lanes.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
const ts = Date.now();

const SEQUENCE_NAME = `SEQ_MovieRender_${ts}`;
const SEQUENCE_PATH = `${TEST_FOLDER}/${SEQUENCE_NAME}`;
const JOB_NAME = `MRQ_Job_${ts}`;
const JOB_NAME_2 = `MRQ_JobB_${ts}`;
const MAP_PATH = '/Game/Maps/MCPMovieRenderMap';
const OUT_DIR = '/tmp/mcp_mrq_output';
const MAT_PATH = '/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial';
const EXECUTOR_CLASS = '/Script/MovieRenderPipelineCore.MoviePipelineInProcessExecutor';
const BURN_IN_CLASS = '/MovieRenderPipeline/Blueprints/DefaultBurnIn.DefaultBurnIn_C';

const testCases = [
  // === SETUP (realistic dependencies) ===
  { scenario: 'MRQ Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MRQ Setup: create Level Sequence for render job', toolName: 'manage_sequence', arguments: { action: 'create', name: SEQUENCE_NAME, path: TEST_FOLDER_ALIAS }, expected: 'success|already exists' },

  // === create_render_job ===
  { scenario: 'MRQ: create_render_job', toolName: 'manage_sequence', arguments: { action: 'create_render_job', sequencePath: SEQUENCE_PATH, renderJobName: JOB_NAME }, expected: 'success|already exists' },
  { scenario: 'MRQ: create_render_job optional', toolName: 'manage_sequence', arguments: { action: 'create_render_job', sequencePath: SEQUENCE_PATH, jobName: JOB_NAME_2, renderJobName: JOB_NAME_2, mapPath: MAP_PATH }, expected: 'success|already exists' },

  // === configure_output_settings ===
  { scenario: 'MRQ: configure_output_settings', toolName: 'manage_sequence', arguments: { action: 'configure_output_settings', renderJobName: JOB_NAME, outputDirectory: OUT_DIR, fileNameFormat: 'Frame_{frame}' }, expected: 'success' },
  { scenario: 'MRQ: configure_output_settings optional', toolName: 'manage_sequence', arguments: { action: 'configure_output_settings', renderJobName: JOB_NAME, width: 1920, height: 1080, settings: { handleFrameCount: 4, zeroPadFrameNumbers: 5 } }, expected: 'success' },

  // === add_render_pass ===
  { scenario: 'MRQ: add_render_pass', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', renderJobName: JOB_NAME, renderPass: 'DeferredRendering' }, expected: 'success' },
  { scenario: 'MRQ: add_render_pass optional', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', renderJobName: JOB_NAME, renderPasses: ['DeferredRendering', 'FinalImage'], materialPath: MAT_PATH, includeTranslucentObjects: true }, expected: 'success' },

  // === configure_anti_aliasing ===
  { scenario: 'MRQ: configure_anti_aliasing', toolName: 'manage_sequence', arguments: { action: 'configure_anti_aliasing', renderJobName: JOB_NAME, antiAliasingMethod: 'tsr' }, expected: 'success' },
  { scenario: 'MRQ: configure_anti_aliasing optional', toolName: 'manage_sequence', arguments: { action: 'configure_anti_aliasing', renderJobName: JOB_NAME, method: 'spatialtemporal', spatialSampleCount: 8, temporalSampleCount: 16, settings: { spatialSampleCount: 8, temporalSampleCount: 16, antiAliasingMethod: 'tsr', method: 'spatialtemporal' } }, expected: 'success' },

  // === configure_console_variables ===
  { scenario: 'MRQ: configure_console_variables', toolName: 'manage_sequence', arguments: { action: 'configure_console_variables', renderJobName: JOB_NAME, consoleVariables: { r: { MotionBlurQuality: 2 } } }, expected: 'success' },
  { scenario: 'MRQ: configure_console_variables optional', toolName: 'manage_sequence', arguments: { action: 'configure_console_variables', renderJobName: JOB_NAME, consoleVariables: { r: { AmbientOcclusionLevels: 1, Shadow: { Quality: 3 } }, sg: { PostProcessQuality: 2 } } }, expected: 'success' },

  // === configure_burn_ins ===
  { scenario: 'MRQ: configure_burn_ins', toolName: 'manage_sequence', arguments: { action: 'configure_burn_ins', renderJobName: JOB_NAME, burnIn: { enabled: true } }, expected: 'success' },
  { scenario: 'MRQ: configure_burn_ins optional', toolName: 'manage_sequence', arguments: { action: 'configure_burn_ins', renderJobName: JOB_NAME, burnIn: { enabled: true, compositeOntoFinalImage: true, classPath: BURN_IN_CLASS } }, expected: 'success' },

  // === queue_render ===
  { scenario: 'MRQ: queue_render', toolName: 'manage_sequence', arguments: { action: 'queue_render', renderJobName: JOB_NAME }, expected: 'success' },
  { scenario: 'MRQ: queue_render optional', toolName: 'manage_sequence', arguments: { action: 'queue_render', renderJobName: JOB_NAME, renderJobId: `mcp.renderJobId=${JOB_NAME}`, onlyJob: true, useCurrentLevel: true }, expected: 'success' },

  // === start_render ===
  { scenario: 'MRQ: start_render', toolName: 'manage_sequence', arguments: { action: 'start_render', renderJobName: JOB_NAME }, expected: 'success' },
  { scenario: 'MRQ: start_render optional', toolName: 'manage_sequence', arguments: { action: 'start_render', renderJobName: JOB_NAME, jobId: `mcp.renderJobId=${JOB_NAME}`, executorClass: EXECUTOR_CLASS, timeoutMs: 60000, useCurrentLevel: false, onlyJob: false }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'MRQ Cleanup: delete render sequence', toolName: 'manage_asset', arguments: { action: 'delete', path: SEQUENCE_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'MRQ Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-sequence-movie-render', testCases);
