// Fold specs for system_control. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const SYSTEM_CONTROL_FOLDS: readonly FoldSpec[] = [
  { primary: 'console_command', summary: 'Run one console command against the editor world, or through the first player while Play In Editor runs, and return what it printed (output, at most 4096 characters). A command the engine does not recognise fails (EXEC_FAILED); one holding ;, |, &&, a backtick or several lines, or a blocked name (quit, exit, py, crash and the like), is refused (COMMAND_BLOCKED). Success means the engine ran it, not that it had the effect wanted.', members: ['execute_command'] },
  {
    primary: 'configure_display', selector: 'setting',
    summary: 'Configure display and rendering variables: resolution, fullscreen, quality, a console variable, FPS counter, profiling overlay.',
    topics: ['set cvar', 'console variable', 'resolution', 'fullscreen', 'quality settings', 'show fps', 'profile', 'change quality settings'],
    members: { resolution: 'set_resolution', fullscreen: 'set_fullscreen', quality: 'set_quality', cvar: 'set_cvar', fps: 'show_fps', profile: 'profile' },
  },
  {
    primary: 'configure_performance', selector: 'setting',
    summary: 'Configure performance settings, chosen by setting: scalability levels or a baseline profile (saved, kept across restarts); session-only console settings (frame rate cap, resolution scale, vsync, LOD and mip bias, Nanite, occlusion culling, texture streaming, draw-call batching, GPU timing), each set as the console does and read back, with appliedCVars and notSet in the reply and CVAR_UNAVAILABLE when none took; World Partition streaming and grid; a shader recompile.',
    topics: ['scalability', 'frame rate limit', 'vsync', 'resolution scale', 'nanite', 'occlusion culling', 'texture streaming', 'performance', 'texture streaming pool'],
    members: {
      scalability: 'set_scalability', frame_rate_limit: 'set_frame_rate_limit', resolution_scale: 'set_resolution_scale', vsync: 'set_vsync', lod: 'configure_lod',
      nanite: 'configure_nanite', occlusion_culling: 'configure_occlusion_culling', texture_streaming: 'configure_texture_streaming', world_partition: 'configure_world_partition',
      gpu_timing: 'enable_gpu_timing', draw_calls: 'optimize_draw_calls', shaders: 'optimize_shaders', baseline: 'apply_baseline_settings',
    },
  },
  {
    primary: 'profile_performance', selector: 'control',
    summary: 'Measure performance, chosen by control. benchmark samples every frame for duration seconds (default 10) and answers averageFps, frame times and game, render and GPU thread times; start and stop record a .uestats file under Saved/Profiling for Unreal Insights (no MCP action reads it); memory_report runs memreport (detailed: memreport -full) and answers the .memreport file it wrote (reportPath, reportIsNew); show_stats turns an on-screen stat overlay on or off. A minimized or unfocused editor is throttled and measures slow. To find what makes a frame slow, inspect get_stats with stats frame names the busiest thread and what in view weighs on it.',
    topics: ['profiling', 'benchmark', 'memory report', 'stat unit', 'show stats', 'profile the game', 'find what is slow', 'check performance'],
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
    summary: 'Read the Unreal Insights trace session status (connected, destination, active channels, connection type, paused); info=analyze reads a .utrace file\'s existence, size and modified time.',
    topics: ['insights session', 'is tracing active', 'active channels', 'file size'],
    whenToUse: ['The active trace session status must be read.', 'A captured .utrace file must be checked for existence, size and modified time.'],
    members: { status: 'get_trace_status', analyze: 'analyze_trace' },
  },
  {
    primary: 'enable_plugin', selector: 'pluginState',
    summary: 'Enable or disable a plugin, or limit it to some build targets: targets ["Editor"] keeps an editor-only plugin out of the packaged game.',
    topics: ['editor only plugin', 'plugin out of packaged game', 'plugin build targets'],
    members: { enable: 'enable_plugin', disable: 'disable_plugin' },
  },
  {
    primary: 'run_build', selector: 'kind',
    summary: 'Run automation tests and answer with each one\'s result (passed or not, its errors and warnings), or run UnrealBuildTool for a target.',
    topics: ['automation tests', 'run tests', 'test results', 'ubt', 'unreal build tool', 'build target'],
    members: { tests: 'run_tests', ubt: 'run_ubt' },
  },
  {
    primary: 'create_widget', selector: 'widgetOp',
    summary: 'Make a Widget Blueprint and fill it, or raise an editor notification, chosen by widgetOp. create makes a Widget Blueprint asset (name and savePath, default /Game/UI/Widgets, or widgetPath) from UserWidget or a subclass (widgetType). add_child adds a widget of childClass (TextBlock, Button, Image and so on) under parentName (default the root panel; PARENT_NOT_FOUND for a name that is no panel), named name, with text for one that shows text. show only raises an editor toast with message for duration seconds (default 4): it puts nothing on screen in the game. To show a Widget Blueprint in the game, give a Blueprint Create Widget and Add to Viewport nodes (blueprint.edit_graph), or use blueprint.add_behaviour for a HUD piece.',
    topics: ['show widget on screen', 'add widget to viewport', 'display widget'],
    members: { create: 'create_widget', add_child: 'add_widget_child', show: 'show_widget' },
  },
  {
    primary: 'subscribe', selector: 'control',
    summary: 'Stream the editor log to this connection (subscribe, unsubscribe), or turn a Gameplay Debugger category on or off.',
    topics: ['log channels', 'subscribe logs', 'log category', 'watch game logs live', 'log stream'],
    members: { subscribe: 'subscribe', unsubscribe: 'unsubscribe', spawn_category: 'spawn_category' },
  },
];
