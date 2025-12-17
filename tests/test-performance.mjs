#!/usr/bin/env node
/**
 * Comprehensive Performance Management Test Suite
 * Tool: manage_performance
 * Coverage: All 19 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
    // === PROFILING (start_profiling, stop_profiling, run_benchmark) ===
    {
        scenario: 'Start CPU Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'start_profiling',
            type: 'CPU',
            outputPath: './tests/reports'
        },
        expected: 'success'
    },
    {
        scenario: 'Stop CPU Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'stop_profiling',
            type: 'CPU'
        },
        expected: 'success'
    },
    {
        scenario: 'Start GPU Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'start_profiling',
            type: 'GPU'
        },
        expected: 'success'
    },
    {
        scenario: 'Stop GPU Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'stop_profiling',
            type: 'GPU'
        },
        expected: 'success'
    },
    {
        scenario: 'Start Memory Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'start_profiling',
            type: 'Memory',
            detailed: true
        },
        expected: 'success'
    },
    {
        scenario: 'Stop Memory Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'stop_profiling',
            type: 'Memory'
        },
        expected: 'success'
    },
    {
        scenario: 'Start RenderThread Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'start_profiling',
            type: 'RenderThread'
        },
        expected: 'success'
    },
    {
        scenario: 'Stop RenderThread Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'stop_profiling',
            type: 'RenderThread'
        },
        expected: 'success'
    },
    {
        scenario: 'Start GameThread Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'start_profiling',
            type: 'GameThread'
        },
        expected: 'success'
    },
    {
        scenario: 'Stop GameThread Profiling',
        toolName: 'manage_performance',
        arguments: {
            action: 'stop_profiling',
            type: 'GameThread'
        },
        expected: 'success'
    },
    {
        scenario: 'Run Benchmark',
        toolName: 'manage_performance',
        arguments: {
            action: 'run_benchmark',
            duration: 5,
            verbose: false
        },
        expected: 'success'
    },

    // === DISPLAY (show_fps, show_stats, generate_memory_report) ===
    {
        scenario: 'Show FPS',
        toolName: 'manage_performance',
        arguments: {
            action: 'show_fps',
            enabled: true
        },
        expected: 'success'
    },
    {
        scenario: 'Hide FPS',
        toolName: 'manage_performance',
        arguments: {
            action: 'show_fps',
            enabled: false
        },
        expected: 'success'
    },
    {
        scenario: 'Show Stats',
        toolName: 'manage_performance',
        arguments: {
            action: 'show_stats',
            category: 'Unit'
        },
        expected: 'success'
    },
    {
        scenario: 'Generate Memory Report',
        toolName: 'manage_performance',
        arguments: {
            action: 'generate_memory_report',
            outputPath: './tests/reports/memory_report.txt',
            detailed: true
        },
        expected: 'success'
    },

    // === SCALABILITY (set_scalability, set_resolution_scale, set_vsync, set_frame_rate_limit) ===
    {
        scenario: 'Set Scalability Low',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_scalability',
            level: 0
        },
        expected: 'success'
    },
    {
        scenario: 'Set Scalability Medium',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_scalability',
            level: 1
        },
        expected: 'success'
    },
    {
        scenario: 'Set Scalability High',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_scalability',
            level: 2
        },
        expected: 'success'
    },
    {
        scenario: 'Set Scalability Epic',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_scalability',
            level: 3
        },
        expected: 'success'
    },
    {
        scenario: 'Set Resolution Scale 100%',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_resolution_scale',
            scale: 100
        },
        expected: 'success'
    },
    {
        scenario: 'Set Resolution Scale 50%',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_resolution_scale',
            scale: 50
        },
        expected: 'success'
    },
    {
        scenario: 'Enable VSync',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_vsync',
            enabled: true
        },
        expected: 'success'
    },
    {
        scenario: 'Disable VSync',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_vsync',
            enabled: false
        },
        expected: 'success'
    },
    {
        scenario: 'Set Frame Rate Limit 60',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_frame_rate_limit',
            maxFPS: 60
        },
        expected: 'success'
    },
    {
        scenario: 'Set Frame Rate Limit 120',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_frame_rate_limit',
            maxFPS: 120
        },
        expected: 'success'
    },
    {
        scenario: 'Uncap Frame Rate',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_frame_rate_limit',
            maxFPS: 0
        },
        expected: 'success'
    },

    // === GPU TIMING (enable_gpu_timing) ===
    {
        scenario: 'Enable GPU Timing',
        toolName: 'manage_performance',
        arguments: {
            action: 'enable_gpu_timing',
            enabled: true
        },
        expected: 'success'
    },
    {
        scenario: 'Disable GPU Timing',
        toolName: 'manage_performance',
        arguments: {
            action: 'enable_gpu_timing',
            enabled: false
        },
        expected: 'success'
    },

    // === TEXTURE STREAMING (configure_texture_streaming) ===
    {
        scenario: 'Configure Texture Streaming',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_texture_streaming',
            poolSize: 1024,
            streamingDistance: 5000,
            boostPlayerLocation: true
        },
        expected: 'success'
    },
    {
        scenario: 'Disable Texture Streaming',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_texture_streaming',
            enabled: false
        },
        expected: 'success'
    },

    // === LOD (configure_lod) ===
    {
        scenario: 'Configure LOD',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_lod',
            distanceScale: 1.0,
            lodBias: 0,
            skeletalBias: 0
        },
        expected: 'success'
    },
    {
        scenario: 'Configure Aggressive LOD',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_lod',
            distanceScale: 0.5,
            lodBias: 1.0
        },
        expected: 'success'
    },
    {
        scenario: 'Force LOD Level',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_lod',
            forceLOD: 2
        },
        expected: 'success'
    },

    // === BASELINE SETTINGS (apply_baseline_settings) ===
    {
        scenario: 'Apply Baseline Settings - Mobile',
        toolName: 'manage_performance',
        arguments: {
            action: 'apply_baseline_settings',
            platform: 'Mobile'
        },
        expected: 'success'
    },
    {
        scenario: 'Apply Baseline Settings - Console',
        toolName: 'manage_performance',
        arguments: {
            action: 'apply_baseline_settings',
            platform: 'Console'
        },
        expected: 'success'
    },
    {
        scenario: 'Apply Baseline Settings - PC',
        toolName: 'manage_performance',
        arguments: {
            action: 'apply_baseline_settings',
            platform: 'PC'
        },
        expected: 'success'
    },

    // === DRAW CALLS (optimize_draw_calls) ===
    {
        scenario: 'Optimize Draw Calls',
        toolName: 'manage_performance',
        arguments: {
            action: 'optimize_draw_calls',
            enableInstancing: true,
            enableBatching: true,
            mergeActors: false
        },
        expected: 'success'
    },

    // === MERGE ACTORS ===
    {
        scenario: 'Merge Actors',
        toolName: 'manage_performance',
        arguments: {
            action: 'merge_actors',
            actors: ['StaticMeshActor_1', 'StaticMeshActor_2']
        },
        expected: 'success|not_found'
    },

    // === OCCLUSION (configure_occlusion_culling) ===
    {
        scenario: 'Configure Occlusion Culling',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_occlusion_culling',
            enabled: true,
            hzb: true,
            freezeRendering: false
        },
        expected: 'success'
    },
    {
        scenario: 'Disable Occlusion Culling',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_occlusion_culling',
            enabled: false
        },
        expected: 'success'
    },

    // === SHADERS (optimize_shaders) ===
    {
        scenario: 'Optimize Shaders',
        toolName: 'manage_performance',
        arguments: {
            action: 'optimize_shaders',
            reducePermutations: true,
            cacheShaders: true,
            compileOnDemand: false
        },
        expected: 'success'
    },

    // === NANITE (configure_nanite) ===
    {
        scenario: 'Configure Nanite',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_nanite',
            enabled: true,
            maxPixelsPerEdge: 1
        },
        expected: 'success|not_supported'
    },
    {
        scenario: 'Disable Nanite',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_nanite',
            enabled: false
        },
        expected: 'success|not_supported'
    },

    // === WORLD PARTITION (configure_world_partition) ===
    {
        scenario: 'Configure World Partition',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_world_partition',
            enabled: true,
            cellSize: 25600,
            streamingPoolSize: 512
        },
        expected: 'success|not_supported'
    },

    // === ERROR CASES ===
    {
        scenario: 'Error: Invalid Profiling Type',
        toolName: 'manage_performance',
        arguments: {
            action: 'start_profiling',
            type: 'InvalidType'
        },
        expected: 'error|unknown_type'
    },
    {
        scenario: 'Error: Invalid Scalability Level',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_scalability',
            level: 999
        },
        expected: 'error|clamped'
    },
    {
        scenario: 'Error: Invalid Platform',
        toolName: 'manage_performance',
        arguments: {
            action: 'apply_baseline_settings',
            platform: 'InvalidPlatform'
        },
        expected: 'error|unknown_platform'
    },

    // === EDGE CASES ===
    {
        scenario: 'Edge: Resolution Scale 0%',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_resolution_scale',
            scale: 0
        },
        expected: 'success|clamped'
    },
    {
        scenario: 'Edge: Resolution Scale 200%',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_resolution_scale',
            scale: 200
        },
        expected: 'success'
    },
    {
        scenario: 'Edge: Negative Frame Rate',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_frame_rate_limit',
            maxFPS: -60
        },
        expected: 'success|handled'
    },
    {
        scenario: 'Edge: Very Large Pool Size',
        toolName: 'manage_performance',
        arguments: {
            action: 'configure_texture_streaming',
            poolSize: 100000
        },
        expected: 'success|clamped'
    },

    // === RESET TO DEFAULTS ===
    {
        scenario: 'Reset - Set Resolution Scale 100%',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_resolution_scale',
            scale: 100
        },
        expected: 'success'
    },
    {
        scenario: 'Reset - Set Frame Rate Limit 0 (uncapped)',
        toolName: 'manage_performance',
        arguments: {
            action: 'set_frame_rate_limit',
            maxFPS: 0
        },
        expected: 'success'
    }
];

await runToolTests('Performance Management', testCases);
