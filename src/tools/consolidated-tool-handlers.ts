// Consolidated tool handlers - maps 13 tools to all 36 operations
import { cleanObject } from '../utils/safe-json.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('ConsolidatedToolHandler');

export async function handleConsolidatedToolCall(
  name: string,
  args: any,
  tools: any
) {
  const startTime = Date.now();
  // Use scoped logger (stderr) to avoid polluting stdout JSON
  log.debug(`Starting execution of ${name} at ${new Date().toISOString()}`);
  
  try {
    // Validate args is not null/undefined
    if (args === null || args === undefined) {
      throw new Error('Invalid arguments: null or undefined');
    }
    

    switch (name) {
      // 1. ASSET MANAGER
      case 'manage_asset':
        // Validate args is not null/undefined
        if (args === null || args === undefined) {
          throw new Error('Invalid arguments: null or undefined');
        }
        
        // Validate action exists
        if (!args.action) {
          throw new Error('Missing required parameter: action');
        }
        
switch (args.action) {
          case 'list': {
            if (args.directory !== undefined && args.directory !== null && typeof args.directory !== 'string') {
              throw new Error('Invalid directory: must be a string');
            }
            const res = await tools.assetResources.list(args.directory || '/Game', false);
            return cleanObject({ success: true, ...res });
          }
          case 'import': {
            if (typeof args.sourcePath !== 'string' || args.sourcePath.trim() === '') {
              throw new Error('Invalid sourcePath');
            }
            if (typeof args.destinationPath !== 'string' || args.destinationPath.trim() === '') {
              throw new Error('Invalid destinationPath');
            }
            const res = await tools.assetTools.importAsset(args.sourcePath, args.destinationPath);
            return cleanObject(res);
          }
          case 'create_material': {
            if (typeof args.name !== 'string' || args.name.trim() === '') {
              throw new Error('Invalid name: must be a non-empty string');
            }
            const res = await tools.materialTools.createMaterial(args.name, args.path || '/Game/Materials');
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown asset action: ${args.action}`);
        }

      // 2. ACTOR CONTROL
      case 'control_actor':
        // Validate action exists
        if (!args.action) {
          throw new Error('Missing required parameter: action');
        }
        
switch (args.action) {
          case 'spawn': {
            if (!args.classPath || typeof args.classPath !== 'string' || args.classPath.trim() === '') {
              throw new Error('Invalid classPath: must be a non-empty string');
            }
            const res = await tools.actorTools.spawn({
              classPath: args.classPath,
              location: args.location,
              rotation: args.rotation
            });
            return cleanObject(res);
          }
          case 'delete': {
            if (!args.actorName || typeof args.actorName !== 'string' || args.actorName.trim() === '') {
              throw new Error('Invalid actorName');
            }
            const res = await tools.bridge.executeEditorFunction('DELETE_ACTOR', { actor_name: args.actorName });
            return cleanObject(res);
          }
          case 'apply_force': {
            if (!args.actorName || typeof args.actorName !== 'string' || args.actorName.trim() === '') {
              throw new Error('Invalid actorName');
            }
            if (!args.force || typeof args.force.x !== 'number' || typeof args.force.y !== 'number' || typeof args.force.z !== 'number') {
              throw new Error('Invalid force: must have numeric x,y,z');
            }
            const res = await tools.physicsTools.applyForce({
              actorName: args.actorName,
              forceType: 'Force',
              vector: [args.force.x, args.force.y, args.force.z]
            });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown actor action: ${args.action}`);
        }

      // 3. EDITOR CONTROL
      case 'control_editor':
        // Validate action exists
        if (!args.action) {
          throw new Error('Missing required parameter: action');
        }
        
switch (args.action) {
          case 'play': {
            const res = await tools.editorTools.playInEditor();
            return cleanObject(res);
          }
          case 'stop': {
            const res = await tools.editorTools.stopPlayInEditor();
            return cleanObject(res);
          }
          case 'pause': {
            const res = await tools.editorTools.pausePlayInEditor();
            return cleanObject(res);
          }
          case 'set_game_speed': {
            if (typeof args.speed !== 'number' || !isFinite(args.speed) || args.speed <= 0) {
              throw new Error('Invalid speed: must be a positive number');
            }
            // Use console command via bridge
            const res = await tools.bridge.executeConsoleCommand(`slomo ${args.speed}`);
            return cleanObject(res);
          }
          case 'eject': {
            const res = await tools.bridge.executeConsoleCommand('eject');
            return cleanObject(res);
          }
          case 'possess': {
            const res = await tools.bridge.executeConsoleCommand('viewself');
            return cleanObject(res);
          }
          case 'set_camera': {
            const res = await tools.editorTools.setViewportCamera(args.location, args.rotation);
            return cleanObject(res);
          }
          case 'set_view_mode': {
            if (!args.viewMode || typeof args.viewMode !== 'string') throw new Error('Missing required parameter: viewMode');
            const res = await tools.bridge.setSafeViewMode(args.viewMode);
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown editor action: ${args.action}`);
        }

      // 4. LEVEL MANAGER
case 'manage_level':
        if (!args.action) throw new Error('Missing required parameter: action');
        switch (args.action) {
          case 'load': {
            if (!args.levelPath || typeof args.levelPath !== 'string') throw new Error('Missing required parameter: levelPath');
            const res = await tools.levelTools.loadLevel({ levelPath: args.levelPath, streaming: !!args.streaming });
            return cleanObject(res);
          }
          case 'save': {
            const res = await tools.levelTools.saveLevel({ levelName: args.levelName, savePath: args.savePath });
            return cleanObject(res);
          }
          case 'stream': {
            if (!args.levelName || typeof args.levelName !== 'string') throw new Error('Missing required parameter: levelName');
            const res = await tools.levelTools.streamLevel({ levelName: args.levelName, shouldBeLoaded: !!args.shouldBeLoaded, shouldBeVisible: !!args.shouldBeVisible });
            return cleanObject(res);
          }
          case 'create_light': {
            if (!args.lightType) throw new Error('Missing required parameter: lightType');
            if (!args.name || typeof args.name !== 'string' || args.name.trim() === '') throw new Error('Invalid name');
            const t = String(args.lightType).toLowerCase();
            if (t === 'directional') return cleanObject(await tools.lightingTools.createDirectionalLight({ name: args.name, intensity: args.intensity }));
            if (t === 'point') return cleanObject(await tools.lightingTools.createPointLight({ name: args.name, location: args.location ? [args.location.x, args.location.y, args.location.z] : [0,0,0], intensity: args.intensity }));
            if (t === 'spot') return cleanObject(await tools.lightingTools.createSpotLight({ name: args.name, location: args.location ? [args.location.x, args.location.y, args.location.z] : [0,0,0], rotation: [0,0,0], intensity: args.intensity }));
            if (t === 'rect') return cleanObject(await tools.lightingTools.createRectLight({ name: args.name, location: args.location ? [args.location.x, args.location.y, args.location.z] : [0,0,0], rotation: [0,0,0], intensity: args.intensity }));
            throw new Error(`Unknown light type: ${args.lightType}`);
          }
          case 'build_lighting': {
            const res = await tools.lightingTools.buildLighting({ quality: args.quality || 'High', buildReflectionCaptures: true });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown level action: ${args.action}`);
        }

      // 5. ANIMATION & PHYSICS
case 'animation_physics':
        // Validate action exists
        if (!args.action) {
          throw new Error('Missing required parameter: action');
        }
        
        switch (args.action) {
          case 'create_animation_bp': {
            if (typeof args.name !== 'string' || args.name.trim() === '') throw new Error('Invalid name');
            if (typeof args.skeletonPath !== 'string' || args.skeletonPath.trim() === '') throw new Error('Invalid skeletonPath');
            const res = await tools.animationTools.createAnimationBlueprint({ name: args.name, skeletonPath: args.skeletonPath, savePath: args.savePath });
            return cleanObject(res);
          }
          case 'play_montage': {
            if (typeof args.actorName !== 'string' || args.actorName.trim() === '') throw new Error('Invalid actorName');
            const montagePath = args.montagePath || args.animationPath;
            if (typeof montagePath !== 'string' || montagePath.trim() === '') throw new Error('Invalid montagePath');
            const res = await tools.animationTools.playAnimation({ actorName: args.actorName, animationType: 'Montage', animationPath: montagePath, playRate: args.playRate });
            return cleanObject(res);
          }
          case 'setup_ragdoll': {
            if (typeof args.skeletonPath !== 'string' || args.skeletonPath.trim() === '') throw new Error('Invalid skeletonPath');
            if (typeof args.physicsAssetName !== 'string' || args.physicsAssetName.trim() === '') throw new Error('Invalid physicsAssetName');
            const res = await tools.physicsTools.setupRagdoll({ skeletonPath: args.skeletonPath, physicsAssetName: args.physicsAssetName, blendWeight: args.blendWeight, savePath: args.savePath });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown animation/physics action: ${args.action}`);
        }

// 6. EFFECTS SYSTEM
      case 'create_effect':
        switch (args.action) {
          case 'particle': {
            const res = await tools.niagaraTools.createEffect({ effectType: args.effectType, name: args.name, location: args.location, scale: args.scale, customParameters: args.customParameters });
            return cleanObject(res);
          }
          case 'niagara': {
            if (typeof args.systemPath !== 'string' || args.systemPath.trim() === '') throw new Error('Invalid systemPath');
            // Create or ensure system exists (spawning in editor is not universally supported via RC)
            const name = args.name || args.systemPath.split('/').pop();
            const res = await tools.niagaraTools.createSystem({ name, savePath: args.savePath || '/Game/Effects/Niagara' });
            return cleanObject(res);
          }
          case 'debug_shape': {
            const shape = String(args.shape || 'Sphere').toLowerCase();
            const loc = args.location || { x: 0, y: 0, z: 0 };
            const size = args.size || 100;
            const color = args.color || [255, 0, 0, 255];
            const duration = args.duration || 5;
            if (shape === 'line') {
              const end = args.end || { x: loc.x + size, y: loc.y, z: loc.z };
              return cleanObject(await tools.debugTools.drawDebugLine({ start: [loc.x, loc.y, loc.z], end: [end.x, end.y, end.z], color, duration }));
            } else if (shape === 'box') {
              const extent = [size, size, size];
              return cleanObject(await tools.debugTools.drawDebugBox({ center: [loc.x, loc.y, loc.z], extent, color, duration }));
            } else if (shape === 'sphere') {
              return cleanObject(await tools.debugTools.drawDebugSphere({ center: [loc.x, loc.y, loc.z], radius: size, color, duration }));
            } else if (shape === 'capsule') {
              return cleanObject(await tools.debugTools.drawDebugCapsule({ center: [loc.x, loc.y, loc.z], halfHeight: size, radius: Math.max(10, size/3), color, duration }));
            } else if (shape === 'cone') {
              return cleanObject(await tools.debugTools.drawDebugCone({ origin: [loc.x, loc.y, loc.z], direction: [0,0,1], length: size, angleWidth: 0.5, angleHeight: 0.5, color, duration }));
            } else if (shape === 'arrow') {
              const end = args.end || { x: loc.x + size, y: loc.y, z: loc.z };
              return cleanObject(await tools.debugTools.drawDebugArrow({ start: [loc.x, loc.y, loc.z], end: [end.x, end.y, end.z], color, duration }));
            } else if (shape === 'point') {
              return cleanObject(await tools.debugTools.drawDebugPoint({ location: [loc.x, loc.y, loc.z], size, color, duration }));
            } else if (shape === 'text' || shape === 'string') {
              const text = args.text || 'Debug';
              return cleanObject(await tools.debugTools.drawDebugString({ location: [loc.x, loc.y, loc.z], text, color, duration }));
            }
            // Default fallback
            return cleanObject(await tools.debugTools.drawDebugSphere({ center: [loc.x, loc.y, loc.z], radius: size, color, duration }));
          }
          default:
            throw new Error(`Unknown effect action: ${args.action}`);
        }

// 7. BLUEPRINT MANAGER
      case 'manage_blueprint':
        switch (args.action) {
          case 'create': {
            const res = await tools.blueprintTools.createBlueprint({ name: args.name, blueprintType: args.blueprintType || 'Actor', savePath: args.savePath });
            return cleanObject(res);
          }
          case 'add_component': {
            const res = await tools.blueprintTools.addComponent({ blueprintName: args.name, componentType: args.componentType, componentName: args.componentName });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown blueprint action: ${args.action}`);
        }

// 8. ENVIRONMENT BUILDER
      case 'build_environment':
        switch (args.action) {
          case 'create_landscape': {
            const res = await tools.landscapeTools.createLandscape({ name: args.name, sizeX: args.sizeX, sizeY: args.sizeY, materialPath: args.materialPath });
            return cleanObject(res);
          }
          case 'sculpt': {
            const res = await tools.landscapeTools.sculptLandscape({ landscapeName: args.name, tool: args.tool, brushSize: args.brushSize, strength: args.strength });
            return cleanObject(res);
          }
          case 'add_foliage': {
            const res = await tools.foliageTools.addFoliageType({ name: args.name, meshPath: args.meshPath, density: args.density });
            return cleanObject(res);
          }
          case 'paint_foliage': {
            const pos = args.position ? [args.position.x || 0, args.position.y || 0, args.position.z || 0] : [0,0,0];
            const res = await tools.foliageTools.paintFoliage({ foliageType: args.foliageType, position: pos, brushSize: args.brushSize, paintDensity: args.paintDensity, eraseMode: args.eraseMode });
            return cleanObject(res);
          }
          case 'create_procedural_terrain': {
            const loc = args.location ? [args.location.x||0, args.location.y||0, args.location.z||0] : [0,0,0];
            const res = await tools.buildEnvAdvanced.createProceduralTerrain({
              name: args.name || 'ProceduralTerrain',
              location: loc as [number,number,number],
              sizeX: args.sizeX,
              sizeY: args.sizeY,
              subdivisions: args.subdivisions,
              heightFunction: args.heightFunction,
              material: args.materialPath
            });
            return cleanObject(res);
          }
          case 'create_procedural_foliage': {
            if (!args.bounds || !args.bounds.location || !args.bounds.size) throw new Error('bounds.location and bounds.size are required');
            const bounds = {
              location: [args.bounds.location.x||0, args.bounds.location.y||0, args.bounds.location.z||0] as [number,number,number],
              size: [args.bounds.size.x||1000, args.bounds.size.y||1000, args.bounds.size.z||100] as [number,number,number]
            };
            const res = await tools.buildEnvAdvanced.createProceduralFoliage({
              name: args.name || 'ProceduralFoliage',
              bounds,
              foliageTypes: args.foliageTypes || [],
              seed: args.seed
            });
            return cleanObject(res);
          }
          case 'add_foliage_instances': {
            if (!args.foliageType) throw new Error('foliageType is required');
            if (!Array.isArray(args.transforms)) throw new Error('transforms array is required');
            const transforms = (args.transforms as any[]).map(t => ({
              location: [t.location?.x||0, t.location?.y||0, t.location?.z||0] as [number,number,number],
              rotation: t.rotation ? [t.rotation.pitch||0, t.rotation.yaw||0, t.rotation.roll||0] as [number,number,number] : undefined,
              scale: t.scale ? [t.scale.x||1, t.scale.y||1, t.scale.z||1] as [number,number,number] : undefined
            }));
            const res = await tools.buildEnvAdvanced.addFoliageInstances({ foliageType: args.foliageType, transforms });
            return cleanObject(res);
          }
          case 'create_landscape_grass_type': {
            const res = await tools.buildEnvAdvanced.createLandscapeGrassType({
              name: args.name || 'GrassType',
              meshPath: args.meshPath,
              density: args.density,
              minScale: args.minScale,
              maxScale: args.maxScale
            });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown environment action: ${args.action}`);
        }

      // 9. SYSTEM CONTROL
case 'system_control':
        if (!args.action) throw new Error('Missing required parameter: action');
        switch (args.action) {
          case 'profile': {
            const res = await tools.performanceTools.startProfiling({ type: args.profileType, duration: args.duration });
            return cleanObject(res);
          }
          case 'show_fps': {
            const res = await tools.performanceTools.showFPS({ enabled: !!args.enabled, verbose: !!args.verbose });
            return cleanObject(res);
          }
          case 'set_quality': {
            const res = await tools.performanceTools.setScalability({ category: args.category, level: args.level });
            return cleanObject(res);
          }
          case 'play_sound': {
            if (args.location && typeof args.location === 'object') {
              const loc = [args.location.x || 0, args.location.y || 0, args.location.z || 0];
              const res = await tools.audioTools.playSoundAtLocation({ soundPath: args.soundPath, location: loc as [number, number, number], volume: args.volume, pitch: args.pitch, startTime: args.startTime });
              return cleanObject(res);
            }
            const res = await tools.audioTools.playSound2D({ soundPath: args.soundPath, volume: args.volume, pitch: args.pitch, startTime: args.startTime });
            return cleanObject(res);
          }
          case 'create_widget': {
            const res = await tools.uiTools.createWidget({ name: args.widgetName, type: args.widgetType, savePath: args.savePath });
            return cleanObject(res);
          }
          case 'show_widget': {
            const res = await tools.uiTools.setWidgetVisibility({ widgetName: args.widgetName, visible: args.visible !== false });
            return cleanObject(res);
          }
          case 'screenshot': {
            const res = await tools.visualTools.takeScreenshot({ resolution: args.resolution });
            return cleanObject(res);
          }
          case 'engine_start': {
            const res = await tools.engineTools.launchEditor({ editorExe: args.editorExe, projectPath: args.projectPath });
            return cleanObject(res);
          }
          case 'engine_quit': {
            const res = await tools.engineTools.quitEditor();
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown system action: ${args.action}`);
        }

      // 10. CONSOLE COMMAND - handle validation here
case 'console_command':
        if (!args.command || typeof args.command !== 'string' || args.command.trim() === '') {
          return { success: true, message: 'Empty command' } as any;
        }
        // Basic safety filter
        const cmd = String(args.command).trim();
        const blocked = [/\bquit\b/i, /\bexit\b/i, /debugcrash/i];
        if (blocked.some(r => r.test(cmd))) {
          return { success: false, error: 'Command blocked for safety' } as any;
        }
        try {
          const res = await tools.bridge.executeConsoleCommand(cmd);
          return cleanObject({ success: true, command: cmd, result: res });
        } catch (e: any) {
          return cleanObject({ success: false, command: cmd, error: e?.message || String(e) });
        }
        

      // 11. REMOTE CONTROL PRESETS - Direct implementation
      case 'manage_rc':
        if (!args.action) throw new Error('Missing required parameter: action');
        
        // Handle RC operations directly through RcTools
        let rcResult: any;
        
        switch (args.action) {
          // Support both 'create_preset' and 'create' for compatibility
          case 'create_preset':
          case 'create':
            // Support both 'name' and 'presetName' parameter names
            const presetName = args.name || args.presetName;
            if (!presetName) throw new Error('Missing required parameter: name or presetName');
            rcResult = await tools.rcTools.createPreset({ 
              name: presetName, 
              path: args.path 
            });
            // Return consistent output with presetId for tests
            if (rcResult.success) {
              rcResult.message = `Remote Control preset created: ${presetName}`;
              // Ensure presetId is set (for test compatibility)
              if (rcResult.presetPath && !rcResult.presetId) {
                rcResult.presetId = rcResult.presetPath;
              }
            }
            break;
            
          case 'list':
            // List all presets - implement via RcTools
            rcResult = await tools.rcTools.listPresets();
            break;
            
          case 'delete':
            if (!args.presetId) throw new Error('Missing required parameter: presetId');
            rcResult = await tools.rcTools.deletePreset(args.presetId);
            if (rcResult.success) {
              rcResult.message = 'Preset deleted successfully';
            }
            break;
            
          case 'expose_actor':
            if (!args.presetPath) throw new Error('Missing required parameter: presetPath');
            if (!args.actorName) throw new Error('Missing required parameter: actorName');
            
            rcResult = await tools.rcTools.exposeActor({ 
              presetPath: args.presetPath,
              actorName: args.actorName
            });
            if (rcResult.success) {
              rcResult.message = `Actor '${args.actorName}' exposed to preset`;
            }
            break;
            
          case 'expose_property':
          case 'expose':  // Support simplified name from tests
            // Support both presetPath and presetId
            const presetPathExp = args.presetPath || args.presetId;
            if (!presetPathExp) throw new Error('Missing required parameter: presetPath or presetId');
            if (!args.objectPath) throw new Error('Missing required parameter: objectPath');
            if (!args.propertyName) throw new Error('Missing required parameter: propertyName');
            
            rcResult = await tools.rcTools.exposeProperty({ 
              presetPath: presetPathExp,
              objectPath: args.objectPath, 
              propertyName: args.propertyName 
            });
            if (rcResult.success) {
              rcResult.message = `Property '${args.propertyName}' exposed to preset`;
            }
            break;
            
          case 'list_fields':
          case 'get_exposed':  // Support test naming
            const presetPathList = args.presetPath || args.presetId;
            if (!presetPathList) throw new Error('Missing required parameter: presetPath or presetId');
            
            rcResult = await tools.rcTools.listFields({ 
              presetPath: presetPathList 
            });
            // Map 'fields' to 'exposedProperties' for test compatibility
            if (rcResult.success && rcResult.fields) {
              rcResult.exposedProperties = rcResult.fields;
            }
            break;
            
          case 'set_property':
          case 'set_value':  // Support test naming
            // Support both patterns
            const objPathSet = args.objectPath || args.presetId;
            const propNameSet = args.propertyName || args.propertyLabel;
            
            if (!objPathSet) throw new Error('Missing required parameter: objectPath or presetId');
            if (!propNameSet) throw new Error('Missing required parameter: propertyName or propertyLabel');
            if (args.value === undefined) throw new Error('Missing required parameter: value');
            
            rcResult = await tools.rcTools.setProperty({ 
              objectPath: objPathSet,
              propertyName: propNameSet,
              value: args.value 
            });
            if (rcResult.success) {
              rcResult.message = `Property '${propNameSet}' value updated`;
            }
            break;
            
          case 'get_property':
          case 'get_value':  // Support test naming
            const objPathGet = args.objectPath || args.presetId;
            const propNameGet = args.propertyName || args.propertyLabel;
            
            if (!objPathGet) throw new Error('Missing required parameter: objectPath or presetId');
            if (!propNameGet) throw new Error('Missing required parameter: propertyName or propertyLabel');
            
            rcResult = await tools.rcTools.getProperty({ 
              objectPath: objPathGet,
              propertyName: propNameGet 
            });
            break;
            
          case 'call_function':
            if (!args.presetId) throw new Error('Missing required parameter: presetId');
            if (!args.functionLabel) throw new Error('Missing required parameter: functionLabel');
            
            // For now, return not implemented
            rcResult = { 
              success: false, 
              error: 'Function calls not yet implemented' 
            };
            break;
            
          default:
            throw new Error(`Unknown RC action: ${args.action}. Valid actions are: create_preset, expose_actor, expose_property, list_fields, set_property, get_property, or their simplified versions: create, list, delete, expose, get_exposed, set_value, get_value, call_function`);
        }
        
        // Return result directly - MCP formatting will be handled by response validator
        // Clean to prevent circular references
        return cleanObject(rcResult);

      // 12. SEQUENCER / CINEMATICS
      case 'manage_sequence':
        if (!args.action) throw new Error('Missing required parameter: action');
        
        // Direct handling for sequence operations
        const seqResult = await (async () => {
          const sequenceTools = tools.sequenceTools;
          if (!sequenceTools) throw new Error('Sequence tools not available');
          
          switch (args.action) {
            case 'create':
              return await sequenceTools.create({ name: args.name, path: args.path });
            case 'open':
              return await sequenceTools.open({ path: args.path });
            case 'add_camera':
              return await sequenceTools.addCamera({ spawnable: args.spawnable !== false });
            case 'add_actor':
              return await sequenceTools.addActor({ actorName: args.actorName });
            case 'add_actors':
              if (!args.actorNames) throw new Error('Missing required parameter: actorNames');
              return await sequenceTools.addActors({ actorNames: args.actorNames });
            case 'remove_actors':
              if (!args.actorNames) throw new Error('Missing required parameter: actorNames');
              return await sequenceTools.removeActors({ actorNames: args.actorNames });
            case 'get_bindings':
              return await sequenceTools.getBindings({ path: args.path });
            case 'add_spawnable_from_class':
              if (!args.className) throw new Error('Missing required parameter: className');
              return await sequenceTools.addSpawnableFromClass({ className: args.className, path: args.path });
            case 'play':
              return await sequenceTools.play({ loopMode: args.loopMode });
            case 'pause':
              return await sequenceTools.pause();
            case 'stop':
              return await sequenceTools.stop();
            case 'set_properties':
              return await sequenceTools.setSequenceProperties({
                path: args.path,
                frameRate: args.frameRate,
                lengthInFrames: args.lengthInFrames,
                playbackStart: args.playbackStart,
                playbackEnd: args.playbackEnd
              });
            case 'get_properties':
              return await sequenceTools.getSequenceProperties({ path: args.path });
            case 'set_playback_speed':
              if (args.speed === undefined) throw new Error('Missing required parameter: speed');
              return await sequenceTools.setPlaybackSpeed({ speed: args.speed });
            default:
              throw new Error(`Unknown sequence action: ${args.action}`);
          }
        })();
        
        // Return result directly - MCP formatting will be handled by response validator
        // Clean to prevent circular references
        return cleanObject(seqResult);
      // 13. INTROSPECTION
case 'inspect':
        if (!args.action) throw new Error('Missing required parameter: action');
        switch (args.action) {
          case 'inspect_object': {
            const res = await tools.introspectionTools.inspectObject({ objectPath: args.objectPath, detailed: args.detailed });
            return cleanObject(res);
          }
          case 'set_property': {
            const res = await tools.introspectionTools.setProperty({ objectPath: args.objectPath, propertyName: args.propertyName, value: args.value });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown inspect action: ${args.action}`);
        }
        

      default:
        throw new Error(`Unknown consolidated tool: ${name}`);
    }

// All cases return (or throw) above; this is a type guard for exhaustiveness.

  } catch (err: any) {
    const duration = Date.now() - startTime;
    console.log(`[ConsolidatedToolHandler] Failed execution of ${name} after ${duration}ms: ${err?.message || String(err)}`);
    
    // Return consistent error structure matching regular tool handlers
    const errorMessage = err?.message || String(err);
    const isTimeout = errorMessage.includes('timeout');
    
    return {
      content: [{
        type: 'text',
        text: isTimeout 
          ? `Tool ${name} timed out. Please check Unreal Engine connection.`
          : `Failed to execute ${name}: ${errorMessage}`
      }],
      isError: true
    };
  }
}
