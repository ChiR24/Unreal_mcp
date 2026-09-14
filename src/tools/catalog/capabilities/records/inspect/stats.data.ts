/**
 * Statistics inspection records (3 actions): scene, performance, memory.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';

const D = 'inspect';
const NR = 'Distinct inspect verb and target; no cross-tool duplicate.';

export const STATS_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_scene_stats', dispatchAction: 'get_scene_stats', domain: D, family: 'stats',
    summary: 'Return scene statistics (actor counts, component counts, etc.).',
    whenToUse: ['Scene-level statistics must be inspected.'],
    whenNotToUse: ['Runtime performance is needed; use get_performance_stats.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_scene_stats' },
    exampleOutput: { success: true, message: 'Scene stats', actorCount: 42 },
    outputProps: { actorCount: { type: 'number', description: 'Level-actor count — the SAME set control_actor.list reports.' }, totalWorldActors: { type: 'number', description: 'Raw world actor count including editor-internal actors (explains the gap vs actorCount).' } },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_performance_stats', dispatchAction: 'get_performance_stats', domain: D, family: 'stats',
    summary: 'Return performance statistics (frame rate, frame time, draw calls).',
    whenToUse: ['Performance metrics must be inspected.'],
    whenNotToUse: ['Scene composition is needed; use get_scene_stats.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_performance_stats' },
    exampleOutput: {
      success: true, message: 'Performance stats', worldType: 'Editor',
      threadTimersAreProcessGlobal: true, deltaSeconds: 0.0166, frameTimeMs: 16.6,
      estimatedFps: 60, fps: 60, gameThreadMs: 4.2, renderThreadMs: 3.1, rhiThreadMs: 1.8,
      gpuMs: 5.5, actorCount: 42, isBenchmarking: false, useFixedTimeStep: false,
    },
    outputProps: {
      worldType: { type: 'string', description: 'World measured: "Editor", "PIE", or "None".' },
      threadTimersAreProcessGlobal: { type: 'boolean', description: 'Thread timers are process-global, not per-world.' },
      // `fps` is frame-delta derived and an idle editor throttles that delta hard.
      busiestThreadMs: { type: 'number', description: 'Slowest of game/render/GPU thread times, in milliseconds.' },
      threadTimeDerivedFps: { type: 'number', description: 'FPS implied by busiestThreadMs; unaffected by editor idle throttling.' },
      frameDeltaMayBeEditorThrottled: { type: 'boolean', description: 'True outside PIE, where an idle editor throttles the frame delta and makes fps read far lower than actual.' },
      deltaSeconds: { type: 'number', description: 'Frame delta time in seconds.' },
      frameTimeMs: { type: 'number', description: 'Frame time in milliseconds.' },
      estimatedFps: { type: 'number', description: 'Estimated frames per second from delta time.' },
      fps: { type: 'number', description: 'Reported frames per second (same as estimatedFps).' },
      gameThreadMs: { type: 'number', description: 'Game thread time in milliseconds.' },
      renderThreadMs: { type: 'number', description: 'Render thread time in milliseconds.' },
      rhiThreadMs: { type: 'number', description: 'RHI thread time in milliseconds.' },
      gpuMs: { type: 'number', description: 'GPU frame time in milliseconds.' },
      actorCount: { type: 'number', description: 'Actor count of the measured world.' },
      isBenchmarking: { type: 'boolean', description: 'Whether the engine is in benchmarking mode.' },
      useFixedTimeStep: { type: 'boolean', description: 'Whether a fixed time step is active.' },
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_memory_stats', dispatchAction: 'get_memory_stats', domain: D, family: 'stats',
    summary: 'Return memory statistics (allocated, virtual, resource counts).',
    whenToUse: ['Memory usage must be inspected.'],
    whenNotToUse: ['Performance timing is needed; use get_performance_stats.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_memory_stats' },
    exampleOutput: {
      success: true, message: 'Memory stats',
      totalPhysicalBytes: 17179869184, availablePhysicalBytes: 8589934592, usedPhysicalBytes: 8589934592,
      peakUsedPhysicalBytes: 10737418240, totalVirtualBytes: 140737488355328,
      availableVirtualBytes: 137438953472, usedVirtualBytes: 3298534883328, peakUsedVirtualBytes: 35184372088832,
      totalPhysicalMB: 16384, totalVirtualMB: 134217728, availablePhysicalMB: 8192, availableVirtualMB: 131072,
      usedPhysicalMB: 8192, usedVirtualMB: 3145728, peakUsedPhysicalMB: 10240, peakUsedVirtualMB: 33554432,
    },
    outputProps: {
      totalPhysicalBytes: { type: 'number', description: 'Total physical memory in bytes.' },
      availablePhysicalBytes: { type: 'number', description: 'Available physical memory in bytes.' },
      usedPhysicalBytes: { type: 'number', description: 'Used physical memory in bytes.' },
      peakUsedPhysicalBytes: { type: 'number', description: 'Peak used physical memory in bytes.' },
      totalVirtualBytes: { type: 'number', description: 'Total virtual memory in bytes.' },
      availableVirtualBytes: { type: 'number', description: 'Available virtual memory in bytes.' },
      usedVirtualBytes: { type: 'number', description: 'Used virtual memory in bytes.' },
      peakUsedVirtualBytes: { type: 'number', description: 'Peak used virtual memory in bytes.' },
      totalPhysicalMB: { type: 'number', description: 'Total physical memory in megabytes.' },
      totalVirtualMB: { type: 'number', description: 'Total virtual memory in megabytes.' },
      availablePhysicalMB: { type: 'number', description: 'Available physical memory in megabytes.' },
      availableVirtualMB: { type: 'number', description: 'Available virtual memory in megabytes.' },
      usedPhysicalMB: { type: 'number', description: 'Used physical memory in megabytes.' },
      usedVirtualMB: { type: 'number', description: 'Used virtual memory in megabytes.' },
      peakUsedPhysicalMB: { type: 'number', description: 'Peak used physical memory in megabytes.' },
      peakUsedVirtualMB: { type: 'number', description: 'Peak used virtual memory in megabytes.' },
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
