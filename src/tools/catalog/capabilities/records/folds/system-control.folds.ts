// Fold specs for system_control. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const SYSTEM_CONTROL_FOLDS: readonly FoldSpec[] = [
  { primary: 'console_command', summary: 'Run a validated console command.', members: ['execute_command'] },
  {
    primary: 'configure_display', selector: 'setting',
    summary: 'Configure display and rendering variables: resolution, fullscreen, quality, a console variable, FPS counter, profiling overlay.',
    topics: ['set cvar', 'console variable', 'resolution', 'fullscreen', 'quality settings', 'show fps', 'profile'],
    members: { resolution: 'set_resolution', fullscreen: 'set_fullscreen', quality: 'set_quality', cvar: 'set_cvar', fps: 'show_fps', profile: 'profile' },
  },
  {
    primary: 'configure_performance', selector: 'setting',
    summary: 'Configure performance settings: scalability, frame rate limit, resolution scale, vsync, LOD, Nanite, occlusion culling, texture streaming, World Partition streaming, GPU timing, draw-call and shader optimization, baseline profile.',
    topics: ['scalability', 'frame rate limit', 'vsync', 'resolution scale', 'nanite', 'occlusion culling', 'texture streaming', 'performance'],
    members: {
      scalability: 'set_scalability', frame_rate_limit: 'set_frame_rate_limit', resolution_scale: 'set_resolution_scale', vsync: 'set_vsync', lod: 'configure_lod',
      nanite: 'configure_nanite', occlusion_culling: 'configure_occlusion_culling', texture_streaming: 'configure_texture_streaming', world_partition: 'configure_world_partition',
      gpu_timing: 'enable_gpu_timing', draw_calls: 'optimize_draw_calls', shaders: 'optimize_shaders', baseline: 'apply_baseline_settings',
    },
  },
  {
    primary: 'profile_performance', selector: 'control',
    summary: 'Profile performance: start or stop profiling, run a benchmark, generate a memory report, show stats.',
    topics: ['profiling', 'benchmark', 'memory report', 'stat unit', 'show stats'],
    members: { start: 'start_profiling', stop: 'stop_profiling', benchmark: 'run_benchmark', memory_report: 'generate_memory_report', show_stats: 'show_stats' },
  },
  {
    primary: 'start_session', selector: 'control',
    summary: 'Unreal Insights: start, stop, pause or resume a trace session, capture a trace, send or write a snapshot, launch the Insights viewer.',
    topics: ['unreal insights', 'trace session', 'capture trace', 'trace snapshot'],
    members: { start: 'start_session', stop: 'stop_session', pause: 'pause_session', resume: 'resume_session', capture_trace: 'capture_insights_trace', send_snapshot: 'send_snapshot', write_snapshot: 'write_snapshot', launch_viewer: 'start_unreal_insights' },
  },
  {
    primary: 'get_trace_status', selector: 'info',
    summary: 'Read the trace session status, or analyze a trace file.',
    members: { status: 'get_trace_status', analyze: 'analyze_trace' },
  },
  {
    primary: 'enable_plugin', selector: 'pluginState',
    summary: 'Enable or disable a plugin.',
    members: { enable: 'enable_plugin', disable: 'disable_plugin' },
  },
  {
    primary: 'run_build', selector: 'kind',
    summary: 'Run automation tests, or run UnrealBuildTool for a target.',
    topics: ['automation tests', 'run tests', 'ubt', 'unreal build tool', 'build target'],
    members: { tests: 'run_tests', ubt: 'run_ubt' },
  },
  {
    primary: 'create_widget', selector: 'widgetOp',
    summary: 'Create a runtime widget, add a child to it, or show it.',
    members: { create: 'create_widget', add_child: 'add_widget_child', show: 'show_widget' },
  },
  {
    primary: 'subscribe', selector: 'control',
    summary: 'Log streaming: subscribe to or unsubscribe from log channels, or spawn a log category.',
    topics: ['log channels', 'subscribe logs', 'log category'],
    members: { subscribe: 'subscribe', unsubscribe: 'unsubscribe', spawn_category: 'spawn_category' },
  },
];
