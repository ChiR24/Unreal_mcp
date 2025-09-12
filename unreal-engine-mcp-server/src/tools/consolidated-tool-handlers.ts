// Consolidated tool handlers - maps 10 tools to all 36 operations
import { handleToolCall } from './tool-handlers.js';

export async function handleConsolidatedToolCall(
  name: string,
  args: any,
  tools: any
) {
  try {
    // Validate args is not null/undefined
    if (args === null || args === undefined) {
      throw new Error('Invalid arguments: null or undefined');
    }
    
    let mappedName: string;
    let mappedArgs: any = { ...args };

    switch (name) {
      // 1. ASSET MANAGER
      case 'manage_assets':
        // Validate args is not null/undefined
        if (args === null || args === undefined) {
          throw new Error('Invalid arguments: null or undefined');
        }
        
        // Validate action exists
        if (!args.action) {
          throw new Error('Missing required parameter: action');
        }
        
        switch (args.action) {
          case 'list':
            // Directory is optional, recursive is optional
            if (args.directory !== undefined && args.directory !== null) {
              if (typeof args.directory !== 'string') {
                throw new Error('Invalid directory: must be a string');
              }
            }
            if (args.recursive !== undefined && args.recursive !== null) {
              if (typeof args.recursive !== 'boolean') {
                throw new Error('Invalid recursive: must be a boolean');
              }
            }
            
            mappedName = 'list_assets';
            mappedArgs = {
              directory: args.directory,
              recursive: args.recursive
            };
            break;
            
          case 'import':
            // Validate required parameters
            if (args.sourcePath === undefined || args.sourcePath === null) {
              throw new Error('Missing required parameter: sourcePath');
            }
            if (typeof args.sourcePath !== 'string') {
              throw new Error('Invalid sourcePath: must be a string');
            }
            if (args.sourcePath.trim() === '') {
              throw new Error('Invalid sourcePath: cannot be empty');
            }
            
            if (args.destinationPath === undefined || args.destinationPath === null) {
              throw new Error('Missing required parameter: destinationPath');
            }
            if (typeof args.destinationPath !== 'string') {
              throw new Error('Invalid destinationPath: must be a string');
            }
            if (args.destinationPath.trim() === '') {
              throw new Error('Invalid destinationPath: cannot be empty');
            }
            
            mappedName = 'import_asset';
            mappedArgs = {
              sourcePath: args.sourcePath,
              destinationPath: args.destinationPath
            };
            break;
            
          case 'create_material':
            // Validate required parameters
            if (args.name === undefined || args.name === null) {
              throw new Error('Missing required parameter: name');
            }
            if (typeof args.name !== 'string') {
              throw new Error('Invalid name: must be a string');
            }
            if (args.name.trim() === '') {
              throw new Error('Invalid name: cannot be empty');
            }
            
            // Path is optional
            if (args.path !== undefined && args.path !== null) {
              if (typeof args.path !== 'string') {
                throw new Error('Invalid path: must be a string');
              }
            }
            
            mappedName = 'create_material';
            mappedArgs = {
              name: args.name,
              path: args.path
            };
            break;
            
          default:
            throw new Error(`Unknown asset action: ${args.action}`);
        }
        break;

      // 2. ACTOR CONTROL
      case 'control_actor':
        // Validate action exists
        if (!args.action) {
          throw new Error('Missing required parameter: action');
        }
        
        switch (args.action) {
          case 'spawn':
            // Validate spawn parameters
            if (!args.classPath) {
              throw new Error('Missing required parameter: classPath');
            }
            if (typeof args.classPath !== 'string' || args.classPath.trim() === '') {
              throw new Error('Invalid classPath: must be a non-empty string');
            }
            
            mappedName = 'spawn_actor';
            mappedArgs = {
              classPath: args.classPath,
              location: args.location,
              rotation: args.rotation
            };
            break;
            
          case 'delete':
            // Validate delete parameters
            if (!args.actorName) {
              throw new Error('Missing required parameter: actorName');
            }
            if (typeof args.actorName !== 'string' || args.actorName.trim() === '') {
              throw new Error('Invalid actorName: must be a non-empty string');
            }
            
            mappedName = 'delete_actor';
            mappedArgs = {
              actorName: args.actorName
            };
            break;
            
          case 'apply_force':
            // Validate apply_force parameters
            if (!args.actorName) {
              throw new Error('Missing required parameter: actorName');
            }
            if (typeof args.actorName !== 'string' || args.actorName.trim() === '') {
              throw new Error('Invalid actorName: must be a non-empty string');
            }
            if (!args.force) {
              throw new Error('Missing required parameter: force');
            }
            if (typeof args.force !== 'object' || args.force === null) {
              throw new Error('Invalid force: must be an object with x, y, z properties');
            }
            if (typeof args.force.x !== 'number' || 
                typeof args.force.y !== 'number' || 
                typeof args.force.z !== 'number') {
              throw new Error('Invalid force: x, y, z must all be numbers');
            }
            
            mappedName = 'apply_force';
            mappedArgs = {
              actorName: args.actorName,
              force: args.force
            };
            break;
            
          default:
            throw new Error(`Unknown actor action: ${args.action}`);
        }
        break;

      // 3. EDITOR CONTROL
      case 'control_editor':
        // Validate action exists
        if (!args.action) {
          throw new Error('Missing required parameter: action');
        }
        
        switch (args.action) {
          case 'play':
            mappedName = 'play_in_editor';
            mappedArgs = {};
            break;
          case 'stop':
            mappedName = 'stop_play_in_editor';
            mappedArgs = {};
            break;
          case 'pause':
            mappedName = 'pause_play_in_editor';
            mappedArgs = {};
            break;
          case 'set_game_speed':
            // Validate game speed parameter
            if (args.speed === undefined || args.speed === null) {
              throw new Error('Missing required parameter: speed');
            }
            if (typeof args.speed !== 'number') {
              throw new Error('Invalid speed: must be a number');
            }
            if (isNaN(args.speed)) {
              throw new Error('Invalid speed: cannot be NaN');
            }
            if (!isFinite(args.speed)) {
              throw new Error('Invalid speed: must be finite');
            }
            if (args.speed <= 0) {
              throw new Error('Invalid speed: must be positive');
            }
            mappedName = 'set_game_speed';
            mappedArgs = {
              speed: args.speed
            };
            break;
          case 'eject':
            mappedName = 'eject_from_pawn';
            mappedArgs = {};
            break;
          case 'possess':
            mappedName = 'possess_pawn';
            mappedArgs = {};
            break;
          case 'set_camera':
            // Allow either location or rotation or both
            // Don't require both to be present
            mappedName = 'set_camera';
            mappedArgs = {
              location: args.location,
              rotation: args.rotation
            };
            break;
          case 'set_view_mode':
            // Validate view mode parameter
            if (!args.viewMode) {
              throw new Error('Missing required parameter: viewMode');
            }
            if (typeof args.viewMode !== 'string' || args.viewMode.trim() === '') {
              throw new Error('Invalid viewMode: must be a non-empty string');
            }
            
            // Normalize view mode to match what debug.ts expects
            const validModes = ['lit', 'unlit', 'wireframe', 'detail_lighting', 'lighting_only', 
                              'light_complexity', 'shader_complexity', 'lightmap_density', 
                              'stationary_light_overlap', 'reflections', 'visualize_buffer',
                              'collision_pawn', 'collision_visibility', 'lod_coloration', 'quad_overdraw'];
            const normalizedMode = args.viewMode.toLowerCase().replace(/_/g, '');
            
            // Map to proper case for debug.ts
            let mappedMode = '';
            switch(normalizedMode) {
              case 'lit': mappedMode = 'Lit'; break;
              case 'unlit': mappedMode = 'Unlit'; break;
              case 'wireframe': mappedMode = 'Wireframe'; break;
              case 'detaillighting': mappedMode = 'DetailLighting'; break;
              case 'lightingonly': mappedMode = 'LightingOnly'; break;
              case 'lightcomplexity': mappedMode = 'LightComplexity'; break;
              case 'shadercomplexity': mappedMode = 'ShaderComplexity'; break;
              case 'lightmapdensity': mappedMode = 'LightmapDensity'; break;
              case 'stationarylightoverlap': mappedMode = 'StationaryLightOverlap'; break;
              case 'reflections': mappedMode = 'ReflectionOverride'; break;
              case 'visualizebuffer': mappedMode = 'VisualizeBuffer'; break;
              case 'collisionpawn': mappedMode = 'CollisionPawn'; break;
              case 'collisionvisibility': mappedMode = 'CollisionVisibility'; break;
              case 'lodcoloration': mappedMode = 'LODColoration'; break;
              case 'quadoverdraw': mappedMode = 'QuadOverdraw'; break;
              default:
                throw new Error(`Invalid viewMode: '${args.viewMode}'. Valid modes are: ${validModes.join(', ')}`);
            }
            
            mappedName = 'set_view_mode';
            mappedArgs = {
              mode: mappedMode
            };
            break;
          default:
            throw new Error(`Unknown editor action: ${args.action}`);
        }
        break;

      // 4. LEVEL MANAGER
      case 'manage_level':
        switch (args.action) {
          case 'load':
            mappedName = 'load_level';
            mappedArgs = {
              levelPath: args.levelPath
            };
            if (args.streaming !== undefined) {
              mappedArgs.streaming = args.streaming;
            }
            break;
          case 'save':
            mappedName = 'save_level';
            mappedArgs = {
              levelName: args.levelName
            };
            if (args.savePath !== undefined) {
              mappedArgs.savePath = args.savePath;
            }
            break;
          case 'stream':
            mappedName = 'stream_level';
            mappedArgs = {
              levelName: args.levelName,
              shouldBeLoaded: args.shouldBeLoaded,
              shouldBeVisible: args.shouldBeVisible
            };
            break;
          case 'create_light':
            // Validate light type
            if (!args.lightType) {
              throw new Error('Missing required parameter: lightType');
            }
            const validLightTypes = ['directional', 'point', 'spot', 'rect'];
            const normalizedLightType = String(args.lightType).toLowerCase();
            if (!validLightTypes.includes(normalizedLightType)) {
              throw new Error(`Invalid lightType: '${args.lightType}'. Valid types are: ${validLightTypes.join(', ')}`);
            }
            
            // Validate name
            if (!args.name) {
              throw new Error('Missing required parameter: name');
            }
            if (typeof args.name !== 'string' || args.name.trim() === '') {
              throw new Error('Invalid name: must be a non-empty string');
            }
            
            // Validate intensity if provided
            if (args.intensity !== undefined) {
              if (typeof args.intensity !== 'number' || !isFinite(args.intensity)) {
                throw new Error(`Invalid intensity: must be a finite number, got ${typeof args.intensity}`);
              }
              if (args.intensity < 0) {
                throw new Error('Invalid intensity: must be non-negative');
              }
            }
            
            // Validate location if provided
            if (args.location !== undefined && args.location !== null) {
              if (!Array.isArray(args.location) && typeof args.location !== 'object') {
                throw new Error('Invalid location: must be an array [x,y,z] or object {x,y,z}');
              }
            }
            
            mappedName = 'create_light';
            mappedArgs = {
              lightType: args.lightType,
              name: args.name,
              location: args.location,
              intensity: args.intensity
            };
            break;
          case 'build_lighting':
            mappedName = 'build_lighting';
            mappedArgs = {
              quality: args.quality
            };
            break;
          default:
            throw new Error(`Unknown level action: ${args.action}`);
        }
        break;

      // 5. ANIMATION & PHYSICS
      case 'animation_physics':
        // Validate action exists
        if (!args.action) {
          throw new Error('Missing required parameter: action');
        }
        
        switch (args.action) {
          case 'create_animation_bp':
            // Validate required parameters
            if (args.name === undefined || args.name === null) {
              throw new Error('Missing required parameter: name');
            }
            if (typeof args.name !== 'string') {
              throw new Error('Invalid name: must be a string');
            }
            if (args.name.trim() === '') {
              throw new Error('Invalid name: cannot be empty');
            }
            
            if (args.skeletonPath === undefined || args.skeletonPath === null) {
              throw new Error('Missing required parameter: skeletonPath');
            }
            if (typeof args.skeletonPath !== 'string') {
              throw new Error('Invalid skeletonPath: must be a string');
            }
            if (args.skeletonPath.trim() === '') {
              throw new Error('Invalid skeletonPath: cannot be empty');
            }
            
            // Optional savePath validation
            if (args.savePath !== undefined && args.savePath !== null) {
              if (typeof args.savePath !== 'string') {
                throw new Error('Invalid savePath: must be a string');
              }
            }
            
            mappedName = 'create_animation_blueprint';
            mappedArgs = {
              name: args.name,
              skeletonPath: args.skeletonPath,
              savePath: args.savePath
            };
            break;
            
          case 'play_montage':
            // Validate required parameters
            if (args.actorName === undefined || args.actorName === null) {
              throw new Error('Missing required parameter: actorName');
            }
            if (typeof args.actorName !== 'string') {
              throw new Error('Invalid actorName: must be a string');
            }
            if (args.actorName.trim() === '') {
              throw new Error('Invalid actorName: cannot be empty');
            }
            
            // Check for montagePath or animationPath
            const montagePath = args.montagePath || args.animationPath;
            if (montagePath === undefined || montagePath === null) {
              throw new Error('Missing required parameter: montagePath or animationPath');
            }
            if (typeof montagePath !== 'string') {
              throw new Error('Invalid montagePath: must be a string');
            }
            if (montagePath.trim() === '') {
              throw new Error('Invalid montagePath: cannot be empty');
            }
            
            // Optional playRate validation
            if (args.playRate !== undefined && args.playRate !== null) {
              if (typeof args.playRate !== 'number') {
                throw new Error('Invalid playRate: must be a number');
              }
              if (isNaN(args.playRate)) {
                throw new Error('Invalid playRate: cannot be NaN');
              }
              if (!isFinite(args.playRate)) {
                throw new Error('Invalid playRate: must be finite');
              }
            }
            
            mappedName = 'play_animation_montage';
            mappedArgs = {
              actorName: args.actorName,
              montagePath: montagePath,
              playRate: args.playRate
            };
            break;
            
          case 'setup_ragdoll':
            // Validate required parameters
            if (args.skeletonPath === undefined || args.skeletonPath === null) {
              throw new Error('Missing required parameter: skeletonPath');
            }
            if (typeof args.skeletonPath !== 'string') {
              throw new Error('Invalid skeletonPath: must be a string');
            }
            if (args.skeletonPath.trim() === '') {
              throw new Error('Invalid skeletonPath: cannot be empty');
            }
            
            if (args.physicsAssetName === undefined || args.physicsAssetName === null) {
              throw new Error('Missing required parameter: physicsAssetName');
            }
            if (typeof args.physicsAssetName !== 'string') {
              throw new Error('Invalid physicsAssetName: must be a string');
            }
            if (args.physicsAssetName.trim() === '') {
              throw new Error('Invalid physicsAssetName: cannot be empty');
            }
            
            // Optional blendWeight validation
            if (args.blendWeight !== undefined && args.blendWeight !== null) {
              if (typeof args.blendWeight !== 'number') {
                throw new Error('Invalid blendWeight: must be a number');
              }
              if (isNaN(args.blendWeight)) {
                throw new Error('Invalid blendWeight: cannot be NaN');
              }
              if (!isFinite(args.blendWeight)) {
                throw new Error('Invalid blendWeight: must be finite');
              }
            }
            
            // Optional savePath validation
            if (args.savePath !== undefined && args.savePath !== null) {
              if (typeof args.savePath !== 'string') {
                throw new Error('Invalid savePath: must be a string');
              }
            }
            
            mappedName = 'setup_ragdoll';
            mappedArgs = {
              skeletonPath: args.skeletonPath,
              physicsAssetName: args.physicsAssetName,
              blendWeight: args.blendWeight,
              savePath: args.savePath
            };
            break;
            
          default:
            throw new Error(`Unknown animation/physics action: ${args.action}`);
        }
        break;

      // 6. EFFECTS SYSTEM
      case 'create_effect':
        switch (args.action) {
          case 'particle':
            mappedName = 'create_particle_effect';
            mappedArgs = {
              effectType: args.effectType,
              name: args.name,
              location: args.location
            };
            break;
          case 'niagara':
            mappedName = 'spawn_niagara_system';
            mappedArgs = {
              systemPath: args.systemPath,
              location: args.location,
              scale: args.scale
            };
            break;
          case 'debug_shape':
            mappedName = 'draw_debug_shape';
            // Convert location object to array for position
            const pos = args.location || args.position;
            mappedArgs = {
              shape: args.shape,
              position: pos ? [pos.x || 0, pos.y || 0, pos.z || 0] : [0, 0, 0],
              size: args.size,
              color: args.color,
              duration: args.duration
            };
            break;
          default:
            throw new Error(`Unknown effect action: ${args.action}`);
        }
        break;

      // 7. BLUEPRINT MANAGER
      case 'manage_blueprint':
        switch (args.action) {
          case 'create':
            mappedName = 'create_blueprint';
            mappedArgs = {
              name: args.name,
              blueprintType: args.blueprintType,
              savePath: args.savePath
            };
            break;
          case 'add_component':
            mappedName = 'add_blueprint_component';
            mappedArgs = {
              blueprintName: args.name,
              componentType: args.componentType,
              componentName: args.componentName
            };
            break;
          default:
            throw new Error(`Unknown blueprint action: ${args.action}`);
        }
        break;

      // 8. ENVIRONMENT BUILDER
      case 'build_environment':
        switch (args.action) {
          case 'create_landscape':
            mappedName = 'create_landscape';
            mappedArgs = {
              name: args.name,
              sizeX: args.sizeX,
              sizeY: args.sizeY,
              materialPath: args.materialPath
            };
            break;
          case 'sculpt':
            mappedName = 'sculpt_landscape';
            mappedArgs = {
              landscapeName: args.name,
              tool: args.tool,
              brushSize: args.brushSize,
              strength: args.strength
            };
            break;
          case 'add_foliage':
            // Validate foliage creation parameters to avoid bad console commands / engine warnings
            if (args.name === undefined || args.name === null || typeof args.name !== 'string' || args.name.trim() === '' || String(args.name).toLowerCase() === 'undefined' || String(args.name).toLowerCase() === 'any') {
              throw new Error(`Invalid foliage name: '${args.name}'`);
            }
            if (args.meshPath === undefined || args.meshPath === null || typeof args.meshPath !== 'string' || args.meshPath.trim() === '' || String(args.meshPath).toLowerCase() === 'undefined') {
              throw new Error(`Invalid meshPath: '${args.meshPath}'`);
            }
            if (args.density !== undefined) {
              if (typeof args.density !== 'number' || !isFinite(args.density) || args.density < 0) {
                throw new Error(`Invalid density: '${args.density}' (must be non-negative finite number)`);
              }
            }
            mappedName = 'add_foliage_type';
            mappedArgs = {
              name: args.name,
              meshPath: args.meshPath,
              density: args.density
            };
            break;
          case 'paint_foliage':
            // Validate paint parameters
            if (args.foliageType === undefined || args.foliageType === null || typeof args.foliageType !== 'string' || args.foliageType.trim() === '' || String(args.foliageType).toLowerCase() === 'undefined' || String(args.foliageType).toLowerCase() === 'any') {
              throw new Error(`Invalid foliageType: '${args.foliageType}'`);
            }
            // Convert position object to array if needed
            let positionArray;
            if (args.position) {
              if (Array.isArray(args.position)) {
                positionArray = args.position;
              } else if (typeof args.position === 'object') {
                positionArray = [args.position.x || 0, args.position.y || 0, args.position.z || 0];
              } else {
                positionArray = [0, 0, 0];
              }
            } else {
              positionArray = [0, 0, 0];
            }
            // Validate numbers in position
            if (!Array.isArray(positionArray) || positionArray.length !== 3 || positionArray.some(v => typeof v !== 'number' || !isFinite(v))) {
              throw new Error(`Invalid position: '${JSON.stringify(args.position)}'`);
            }
            if (args.brushSize !== undefined) {
              if (typeof args.brushSize !== 'number' || !isFinite(args.brushSize) || args.brushSize < 0) {
                throw new Error(`Invalid brushSize: '${args.brushSize}' (must be non-negative finite number)`);
              }
            }
            mappedName = 'paint_foliage';
            mappedArgs = {
              foliageType: args.foliageType,
              position: positionArray,
              brushSize: args.brushSize
            };
            break;
          default:
            throw new Error(`Unknown environment action: ${args.action}`);
        }
        break;

      // 9. SYSTEM CONTROL
      case 'system_control':
        // Validate args is not null/undefined
        if (args === null || args === undefined) {
          throw new Error('Invalid arguments: null or undefined');
        }
        if (typeof args !== 'object') {
          throw new Error('Invalid arguments: must be an object');
        }
        // Validate action exists
        if (!args.action) {
          throw new Error('Missing required parameter: action');
        }
        
        switch (args.action) {
          case 'profile':
            // Validate profile type
            const validProfileTypes = ['CPU', 'GPU', 'Memory', 'RenderThread', 'GameThread', 'All'];
            if (!args.profileType || !validProfileTypes.includes(args.profileType)) {
              throw new Error(`Invalid profileType: '${args.profileType}'. Valid types: ${validProfileTypes.join(', ')}`);
            }
            mappedName = 'start_profiling';
            mappedArgs = {
              type: args.profileType,
              duration: args.duration
            };
            break;
            
          case 'show_fps':
            // Validate enabled is boolean
            if (args.enabled !== undefined && typeof args.enabled !== 'boolean') {
              throw new Error(`Invalid enabled: must be boolean, got ${typeof args.enabled}`);
            }
            mappedName = 'show_fps';
            mappedArgs = {
              enabled: args.enabled,
              verbose: args.verbose
            };
            break;
            
          case 'set_quality':
            // Validate category - normalize aliases and singular forms used by sg.*Quality
            const validCategories = ['ViewDistance', 'AntiAliasing', 'PostProcessing', 'PostProcess', 
                                   'Shadows', 'Shadow', 'GlobalIllumination', 'Reflections', 'Reflection', 'Textures', 'Texture', 
                                   'Effects', 'Foliage', 'Shading'];
            if (!args.category || !validCategories.includes(args.category)) {
              throw new Error(`Invalid category: '${args.category}'. Valid categories: ${validCategories.join(', ')}`);
            }
            // Validate level
            if (args.level === undefined || args.level === null) {
              throw new Error('Missing required parameter: level');
            }
            if (typeof args.level !== 'number' || !Number.isInteger(args.level) || args.level < 0 || args.level > 4) {
              throw new Error(`Invalid level: must be integer 0-4, got ${args.level}`);
            }
            // Normalize category to sg.<Base>Quality base (singular where needed)
            const map: Record<string, string> = {
              ViewDistance: 'ViewDistance',
              AntiAliasing: 'AntiAliasing',
              PostProcessing: 'PostProcess',
              PostProcess: 'PostProcess',
              Shadows: 'Shadow',
              Shadow: 'Shadow',
              GlobalIllumination: 'GlobalIllumination',
              Reflections: 'Reflection',
              Reflection: 'Reflection',
              Textures: 'Texture',
              Texture: 'Texture',
              Effects: 'Effects',
              Foliage: 'Foliage',
              Shading: 'Shading',
            };
            const categoryName = map[String(args.category)] || args.category;
            mappedName = 'set_scalability';
            mappedArgs = {
              category: categoryName,
              level: args.level
            };
            break;
            
          case 'play_sound':
            // Validate sound path
            if (!args.soundPath || typeof args.soundPath !== 'string') {
              throw new Error('Invalid soundPath: must be a non-empty string');
            }
            // Validate volume if provided
            if (args.volume !== undefined) {
              if (typeof args.volume !== 'number' || args.volume < 0 || args.volume > 1) {
                throw new Error(`Invalid volume: must be 0-1, got ${args.volume}`);
              }
            }
            mappedName = 'play_sound';
            mappedArgs = {
              soundPath: args.soundPath,
              location: args.location,
              volume: args.volume,
              is3D: args.is3D
            };
            break;
            
          case 'create_widget':
            // Validate widget name
            if (!args.widgetName || typeof args.widgetName !== 'string' || args.widgetName.trim() === '') {
              throw new Error('Invalid widgetName: must be a non-empty string');
            }
            mappedName = 'create_widget';
            mappedArgs = {
              name: args.widgetName,
              type: args.widgetType,
              savePath: args.savePath
            };
            break;
            
          case 'show_widget':
            // Validate widget name
            if (!args.widgetName || typeof args.widgetName !== 'string') {
              throw new Error('Invalid widgetName: must be a non-empty string');
            }
            // Validate visible is boolean (default to true if not provided)
            const isVisible = args.visible !== undefined ? args.visible : true;
            if (typeof isVisible !== 'boolean') {
              throw new Error(`Invalid visible: must be boolean, got ${typeof isVisible}`);
            }
            mappedName = 'show_widget';
            mappedArgs = {
              widgetName: args.widgetName,
              visible: isVisible
            };
            break;
            
          default:
            throw new Error(`Unknown system action: ${args.action}`);
        }
        break;

      // 10. CONSOLE COMMAND - pass through
      case 'console_command':
        mappedName = 'console_command';
        mappedArgs = args;
        break;

      // 11. VERIFICATION - Environment verification
      case 'verify_environment':
        mappedName = 'verify_environment';
        mappedArgs = args;
        break;

      default:
        throw new Error(`Unknown consolidated tool: ${name}`);
    }

    // Call the original handler with mapped name and args
    return await handleToolCall(mappedName, mappedArgs, tools);

  } catch (err) {
    return {
      content: [{
        type: 'text',
        text: `Failed to execute ${name}: ${err}`
      }],
      isError: true
    };
  }
}
