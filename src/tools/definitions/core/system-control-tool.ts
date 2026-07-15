import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';
import { PERFORMANCE_ACTIONS } from '../shared/action-sets.js';
import { screenshotModeSchema } from '../shared/schema-snippets.js';

export const systemControlToolDefinition: ToolDefinition = {
    name: 'system_control',
    category: 'core',
    description: 'Control the project runtime: profiling, benchmarks, scalability/LOD/Nanite settings, CVars, console commands, Python scripts, UBT, tests, logs, and widgets.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'profile', 'show_fps', 'set_quality', 'screenshot', 'set_resolution', 'set_fullscreen', 'execute_command', 'console_command',
            'run_ubt', 'run_tests', 'subscribe', 'unsubscribe', 'spawn_category',
            'start_session', 'start_unreal_insights', 'capture_insights_trace',
            'get_trace_status', 'pause_session', 'resume_session', 'stop_session',
            'write_snapshot', 'send_snapshot', 'analyze_trace', 'lumen_update_scene',
            'play_sound', 'create_widget', 'show_widget', 'add_widget_child',
            'set_cvar', 'get_project_settings', 'validate_assets',
            'set_project_setting', 'execute_python'
          ,
            ...PERFORMANCE_ACTIONS],
          description: 'Action'
        },
        profileType: commonSchemas.stringProp,
        category: commonSchemas.stringProp,
        level: commonSchemas.numberProp,
        enabled: commonSchemas.enabled,
        resolution: commonSchemas.resolution,
        command: commonSchemas.stringProp,
        target: commonSchemas.stringProp,
        platform: commonSchemas.stringProp,
        configuration: commonSchemas.stringProp,
        arguments: commonSchemas.stringProp,
        filter: commonSchemas.stringProp,
        channels: commonSchemas.stringProp,
        connectionType: { type: 'string', enum: ['file', 'network'], description: 'Trace connection type. Network traces are restricted to loopback hosts by the bridge.' },
        host: commonSchemas.stringProp,
        port: commonSchemas.numberProp,
        traceFile: commonSchemas.outputPath,
        tracePath: commonSchemas.outputPath,
        snapshotPath: commonSchemas.outputPath,
        overwrite: commonSchemas.overwrite,
        widgetPath: commonSchemas.widgetPath,
        childClass: commonSchemas.stringProp,
        parentName: commonSchemas.stringProp,
        section: commonSchemas.stringProp,
        key: commonSchemas.stringProp,
        value: commonSchemas.stringProp,
        code: { type: 'string', description: 'Python code to execute inline', maxLength: 1048576 }, // 1MB max — prevents resource exhaustion via oversized payloads
        file: { type: 'string', description: 'Path to .py file to execute', maxLength: 4096 } // Max path length on most OS
      ,
        mode: screenshotModeSchema,
        returnBase64: { type: 'boolean', description: 'Return PNG image data as base64 when supported. Defaults to true for full_editor_window and game_viewport screenshot modes.' },
        includeMetadata: commonSchemas.booleanProp,
        metadata: commonSchemas.objectProp,
        type: { type: 'string', enum: ['CPU', 'GPU', 'Memory', 'RenderThread', 'GameThread', 'All'] },
        duration: commonSchemas.numberProp,
        outputPath: commonSchemas.outputPath,
        detailed: commonSchemas.booleanProp,
        scale: commonSchemas.numberProp,
        maxFPS: commonSchemas.numberProp,
        poolSize: commonSchemas.numberProp,
        boostPlayerLocation: commonSchemas.booleanProp,
        forceLOD: commonSchemas.numberProp,
        lodBias: commonSchemas.numberProp,
        enableInstancing: commonSchemas.booleanProp,
        enableBatching: commonSchemas.booleanProp,
        mergeActors: commonSchemas.booleanProp,
        actors: commonSchemas.arrayOfStrings,
        streamingDistance: commonSchemas.numberProp,
        cellSize: commonSchemas.numberProp,
        categoryName: commonSchemas.stringProp,
        filename: commonSchemas.stringProp,
        height: commonSchemas.numberProp,
        message: commonSchemas.stringProp,
        name: commonSchemas.name,
        packageName: commonSchemas.stringProp,
        assetPath: commonSchemas.assetPath,
        path: commonSchemas.assetPath,
        paths: commonSchemas.arrayOfStrings,
        recursive: commonSchemas.recursive,
        replaceSourceActors: commonSchemas.booleanProp,
        savePath: commonSchemas.savePath,
        text: commonSchemas.stringProp,
        volume: commonSchemas.numberProp,
        widgetId: commonSchemas.stringProp,
        width: commonSchemas.numberProp,
        windowed: commonSchemas.booleanProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        output: commonSchemas.stringProp,
        executionId: commonSchemas.stringProp,
        codeSha256: commonSchemas.stringProp,
        imageBase64: commonSchemas.stringProp,
        mimeType: commonSchemas.stringProp,
        width: commonSchemas.numberProp,
        height: commonSchemas.numberProp,
        sizeBytes: commonSchemas.numberProp,
        path: commonSchemas.stringProp,
        screenshotPath: commonSchemas.stringProp,
        mode: commonSchemas.stringProp
      }
    }
  };
