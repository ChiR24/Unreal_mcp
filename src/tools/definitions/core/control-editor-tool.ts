import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const controlEditorToolDefinition: ToolDefinition = {
    name: 'control_editor',
    category: 'core',
    description: 'Start/stop PIE, control viewport camera, run console commands, take screenshots, simulate input.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'play', 'stop', 'stop_pie', 'pause', 'resume', 'eject', 'possess',
            'set_view_target', 'set_game_view_target',
            'set_game_speed', 'set_fixed_delta_time',
            'set_camera', 'set_camera_position', 'set_viewport_camera', 'set_camera_fov',
            'set_view_mode', 'set_viewport_resolution',
            'console_command', 'execute_command',
            'screenshot', 'take_screenshot', 'step_frame', 'single_frame_step',
            'start_recording', 'stop_recording',
            'create_bookmark', 'jump_to_bookmark',
            'set_preferences', 'set_viewport_realtime',
            'open_asset', 'close_asset', 'simulate_input',
            'browse_to', 'navigate_content_browser',
            'open_level', 'focus_actor',
            'show_stats', 'hide_stats',
            'set_editor_mode', 'set_immersive_mode', 'set_game_view',
            'undo', 'redo', 'save_all'
          ],
          description: 'Editor action'
        },
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        viewMode: commonSchemas.stringProp,
        enabled: commonSchemas.enabled,
        speed: commonSchemas.numberProp,
        filename: commonSchemas.stringProp,
        fov: commonSchemas.numberProp,
        width: commonSchemas.numberProp,
        height: commonSchemas.numberProp,
        command: commonSchemas.stringProp,
        steps: commonSchemas.integerProp,
        bookmarkName: commonSchemas.stringProp,
        // Path parameters for various actions
        assetPath: commonSchemas.assetPath,
        path: commonSchemas.directoryPath,
        levelPath: commonSchemas.levelPath,
        // Actor-related parameters
        actorName: commonSchemas.actorName,
        objectPath: commonSchemas.stringProp,
        name: commonSchemas.name,
        blendTime: commonSchemas.numberProp,
        // Action-specific parameters
        mode: { type: 'string', description: 'Editor mode for set_editor_mode, or screenshot source: editor_viewport, game_viewport, full_editor_window.' },
        returnBase64: { type: 'boolean', description: 'Return PNG image data as base64 when supported. Defaults to true for full_editor_window and game_viewport modes.' },
        includeMetadata: commonSchemas.booleanProp,
        metadata: commonSchemas.objectProp,
        deltaTime: commonSchemas.numberProp,
        resolution: commonSchemas.resolution,
        realtime: commonSchemas.booleanProp,
        stat: commonSchemas.stringProp,
        category: commonSchemas.stringProp,
        preferences: commonSchemas.objectProp,
        key: commonSchemas.stringProp,
        // simulate_input parameters
        type: commonSchemas.stringProp,
        inputType: commonSchemas.stringProp,
        inputAction: commonSchemas.stringProp,
        x: commonSchemas.numberProp,
        y: commonSchemas.numberProp,
        button: commonSchemas.stringProp,
        id: commonSchemas.stringProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
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
