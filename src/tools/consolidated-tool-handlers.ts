// Consolidated tool handlers - maps 13 tools to all 36 operations
import { cleanObject } from '../utils/safe-json.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('ConsolidatedToolHandler');

const ACTION_REQUIRED_ERROR = 'Missing required parameter: action';

function ensureArgsPresent(args: any) {
  if (args === null || args === undefined) {
    throw new Error('Invalid arguments: null or undefined');
  }
}

function requireAction(args: any): string {
  ensureArgsPresent(args);
  const action = args.action;
  if (typeof action !== 'string' || action.trim() === '') {
    throw new Error(ACTION_REQUIRED_ERROR);
  }
  return action;
}

function requireNonEmptyString(value: any, field: string, message?: string): string {
  if (typeof value !== 'string' || value.trim() === '') {
    throw new Error(message ?? `Invalid ${field}: must be a non-empty string`);
  }
  return value;
}

function requirePositiveNumber(value: any, field: string, message?: string): number {
  if (typeof value !== 'number' || !isFinite(value) || value <= 0) {
    throw new Error(message ?? `Invalid ${field}: must be a positive number`);
  }
  return value;
}

function requireVector3Components(
  vector: any,
  message: string
): [number, number, number] {
  if (
    !vector ||
    typeof vector.x !== 'number' ||
    typeof vector.y !== 'number' ||
    typeof vector.z !== 'number'
  ) {
    throw new Error(message);
  }
  return [vector.x, vector.y, vector.z];
}

function getElicitationTimeoutMs(tools: any): number | undefined {
  if (!tools) return undefined;
  const direct = tools.elicitationTimeoutMs;
  if (typeof direct === 'number' && Number.isFinite(direct)) {
    return direct;
  }
  if (typeof tools.getElicitationTimeoutMs === 'function') {
    const value = tools.getElicitationTimeoutMs();
    if (typeof value === 'number' && Number.isFinite(value)) {
      return value;
    }
  }
  return undefined;
}

async function elicitMissingPrimitiveArgs(
  tools: any,
  args: any,
  prompt: string,
  fieldSchemas: Record<string, { type: 'string' | 'number' | 'integer' | 'boolean'; title?: string; description?: string; enum?: string[]; enumNames?: string[]; minimum?: number; maximum?: number; minLength?: number; maxLength?: number; pattern?: string; format?: string; default?: unknown }>
) {
  if (
    !tools ||
    typeof tools.supportsElicitation !== 'function' ||
    !tools.supportsElicitation() ||
    typeof tools.elicit !== 'function'
  ) {
    return;
  }

  const properties: Record<string, any> = {};
  const required: string[] = [];

  for (const [key, schema] of Object.entries(fieldSchemas)) {
    const value = args?.[key];
    const missing =
      value === undefined ||
      value === null ||
      (typeof value === 'string' && value.trim() === '');
    if (missing) {
      properties[key] = schema;
      required.push(key);
    }
  }

  if (required.length === 0) return;

  const timeoutMs = getElicitationTimeoutMs(tools);
  const options: any = {
    fallback: async () => ({ ok: false, error: 'missing-params' })
  };
  if (typeof timeoutMs === 'number') {
    options.timeoutMs = timeoutMs;
  }

  try {
    const elicited = await tools.elicit(
      prompt,
      { type: 'object', properties, required },
      options
    );

    if (elicited?.ok && elicited.value) {
      for (const key of required) {
        const value = elicited.value[key];
        if (value === undefined || value === null) continue;
        args[key] = typeof value === 'string' ? value.trim() : value;
      }
    }
  } catch (err) {
    log.debug('Special elicitation fallback skipped', {
      prompt,
      err: (err as any)?.message || String(err)
    });
  }
}

export async function handleConsolidatedToolCall(
  name: string,
  args: any,
  tools: any
) {
  const startTime = Date.now();
  // Use scoped logger (stderr) to avoid polluting stdout JSON
  log.debug(`Starting execution of ${name} at ${new Date().toISOString()}`);
  
  try {
    ensureArgsPresent(args);

    switch (name) {
      // 1. ASSET MANAGER
      case 'manage_asset':
        switch (requireAction(args)) {
          case 'list': {
            if (args.directory !== undefined && args.directory !== null && typeof args.directory !== 'string') {
              throw new Error('Invalid directory: must be a string');
            }
            const res = await tools.assetResources.list(args.directory || '/Game', false);
            return cleanObject({ success: true, ...res });
          }
          case 'import': {
            let sourcePath = typeof args.sourcePath === 'string' ? args.sourcePath.trim() : '';
            let destinationPath = typeof args.destinationPath === 'string' ? args.destinationPath.trim() : '';

            if ((!sourcePath || !destinationPath) && typeof tools.supportsElicitation === 'function' && tools.supportsElicitation() && typeof tools.elicit === 'function') {
              const schemaProps: Record<string, any> = {};
              const required: string[] = [];

              if (!sourcePath) {
                schemaProps.sourcePath = {
                  type: 'string',
                  title: 'Source File Path',
                  description: 'Full path to the asset file on disk to import'
                };
                required.push('sourcePath');
              }

              if (!destinationPath) {
                schemaProps.destinationPath = {
                  type: 'string',
                  title: 'Destination Path',
                  description: 'Unreal content path where the asset should be imported (e.g., /Game/MCP/Assets)'
                };
                required.push('destinationPath');
              }

              if (required.length > 0) {
                const timeoutMs = getElicitationTimeoutMs(tools);
                const options: any = { fallback: async () => ({ ok: false, error: 'missing-import-params' }) };
                if (typeof timeoutMs === 'number') {
                  options.timeoutMs = timeoutMs;
                }
                const elicited = await tools.elicit(
                  'Provide the missing import parameters for manage_asset.import',
                  { type: 'object', properties: schemaProps, required },
                  options
                );

                if (elicited?.ok && elicited.value) {
                  if (typeof elicited.value.sourcePath === 'string') {
                    sourcePath = elicited.value.sourcePath.trim();
                  }
                  if (typeof elicited.value.destinationPath === 'string') {
                    destinationPath = elicited.value.destinationPath.trim();
                  }
                }
              }
            }

            const sourcePathValidated = requireNonEmptyString(sourcePath || args.sourcePath, 'sourcePath', 'Invalid sourcePath');
            const destinationPathValidated = requireNonEmptyString(destinationPath || args.destinationPath, 'destinationPath', 'Invalid destinationPath');
            const res = await tools.assetTools.importAsset(sourcePathValidated, destinationPathValidated);
            return cleanObject(res);
          }
          case 'create_material': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the material details for manage_asset.create_material',
              {
                name: {
                  type: 'string',
                  title: 'Material Name',
                  description: 'Name for the new material asset'
                },
                path: {
                  type: 'string',
                  title: 'Save Path',
                  description: 'Optional Unreal content path where the material should be saved'
                }
              }
            );
            const sanitizedName = typeof args.name === 'string' ? args.name.trim() : args.name;
            const sanitizedPath = typeof args.path === 'string' ? args.path.trim() : args.path;
            const name = requireNonEmptyString(sanitizedName, 'name', 'Invalid name: must be a non-empty string');
            const res = await tools.materialTools.createMaterial(name, sanitizedPath || '/Game/Materials');
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown asset action: ${args.action}`);
        }

      // 2. ACTOR CONTROL
      case 'control_actor':
        switch (requireAction(args)) {
          case 'spawn': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the spawn parameters for control_actor.spawn',
              {
                classPath: {
                  type: 'string',
                  title: 'Actor Class or Asset Path',
                  description: 'Class name (e.g., StaticMeshActor) or asset path (e.g., /Engine/BasicShapes/Cube) to spawn'
                }
              }
            );
            const classPathInput = typeof args.classPath === 'string' ? args.classPath.trim() : args.classPath;
            const classPath = requireNonEmptyString(classPathInput, 'classPath', 'Invalid classPath: must be a non-empty string');
            const actorNameInput = typeof args.actorName === 'string' && args.actorName.trim() !== ''
              ? args.actorName
              : (typeof args.name === 'string' ? args.name : undefined);
            const res = await tools.actorTools.spawn({
              classPath,
              location: args.location,
              rotation: args.rotation,
              actorName: actorNameInput
            });
            return cleanObject(res);
          }
          case 'delete': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Which actor should control_actor.delete remove?',
              {
                actorName: {
                  type: 'string',
                  title: 'Actor Name',
                  description: 'Exact label of the actor to delete'
                }
              }
            );
            const actorNameArg = typeof args.actorName === 'string' && args.actorName.trim() !== ''
              ? args.actorName
              : (typeof args.name === 'string' ? args.name : undefined);
            const actorName = requireNonEmptyString(actorNameArg, 'actorName', 'Invalid actorName');
            const res = await tools.bridge.executeEditorFunction('DELETE_ACTOR', { actor_name: actorName });
            return cleanObject(res);
          }
          case 'apply_force': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the target actor for control_actor.apply_force',
              {
                actorName: {
                  type: 'string',
                  title: 'Actor Name',
                  description: 'Physics-enabled actor that should receive the force'
                }
              }
            );
            const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Invalid actorName');
            const vector = requireVector3Components(args.force, 'Invalid force: must have numeric x,y,z');
            const res = await tools.physicsTools.applyForce({
              actorName,
              forceType: 'Force',
              vector
            });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown actor action: ${args.action}`);
        }

      // 3. EDITOR CONTROL
      case 'control_editor':
        switch (requireAction(args)) {
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
            const speed = requirePositiveNumber(args.speed, 'speed', 'Invalid speed: must be a positive number');
            // Use console command via bridge
            const res = await tools.bridge.executeConsoleCommand(`slomo ${speed}`);
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
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the view mode for control_editor.set_view_mode',
              {
                viewMode: {
                  type: 'string',
                  title: 'View Mode',
                  description: 'Viewport view mode (e.g., Lit, Unlit, Wireframe)'
                }
              }
            );
            const viewMode = requireNonEmptyString(args.viewMode, 'viewMode', 'Missing required parameter: viewMode');
            const res = await tools.bridge.setSafeViewMode(viewMode);
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown editor action: ${args.action}`);
        }

      // 4. LEVEL MANAGER
case 'manage_level':
        switch (requireAction(args)) {
          case 'load': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Select the level to load for manage_level.load',
              {
                levelPath: {
                  type: 'string',
                  title: 'Level Path',
                  description: 'Content path of the level asset to load (e.g., /Game/Maps/MyLevel)'
                }
              }
            );
            const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');
            const res = await tools.levelTools.loadLevel({ levelPath, streaming: !!args.streaming });
            return cleanObject(res);
          }
          case 'save': {
            const res = await tools.levelTools.saveLevel({ levelName: args.levelName, savePath: args.savePath });
            return cleanObject(res);
          }
          case 'stream': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the streaming level name for manage_level.stream',
              {
                levelName: {
                  type: 'string',
                  title: 'Level Name',
                  description: 'Streaming level name to toggle'
                }
              }
            );
            const levelName = requireNonEmptyString(args.levelName, 'levelName', 'Missing required parameter: levelName');
            const res = await tools.levelTools.streamLevel({ levelName, shouldBeLoaded: !!args.shouldBeLoaded, shouldBeVisible: !!args.shouldBeVisible });
            return cleanObject(res);
          }
          case 'create_light': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the light details for manage_level.create_light',
              {
                lightType: {
                  type: 'string',
                  title: 'Light Type',
                  description: 'Directional, Point, Spot, Rect, or Sky'
                },
                name: {
                  type: 'string',
                  title: 'Light Name',
                  description: 'Name for the new light actor'
                }
              }
            );
            const lightType = requireNonEmptyString(args.lightType, 'lightType', 'Missing required parameter: lightType');
            const name = requireNonEmptyString(args.name, 'name', 'Invalid name');
            const typeKey = lightType.toLowerCase();
            const toVector = (value: any, fallback: [number, number, number]): [number, number, number] => {
              if (Array.isArray(value) && value.length === 3) {
                return [Number(value[0]) || 0, Number(value[1]) || 0, Number(value[2]) || 0];
              }
              if (value && typeof value === 'object') {
                return [Number(value.x) || 0, Number(value.y) || 0, Number(value.z) || 0];
              }
              return fallback;
            };
            const toRotator = (value: any, fallback: [number, number, number]): [number, number, number] => {
              if (Array.isArray(value) && value.length === 3) {
                return [Number(value[0]) || 0, Number(value[1]) || 0, Number(value[2]) || 0];
              }
              if (value && typeof value === 'object') {
                return [Number(value.pitch) || 0, Number(value.yaw) || 0, Number(value.roll) || 0];
              }
              return fallback;
            };
            const toColor = (value: any): [number, number, number] | undefined => {
              if (Array.isArray(value) && value.length === 3) {
                return [Number(value[0]) || 0, Number(value[1]) || 0, Number(value[2]) || 0];
              }
              if (value && typeof value === 'object') {
                return [Number(value.r) || 0, Number(value.g) || 0, Number(value.b) || 0];
              }
              return undefined;
            };

            const location = toVector(args.location, [0, 0, typeKey === 'directional' ? 500 : 0]);
            const rotation = toRotator(args.rotation, [0, 0, 0]);
            const color = toColor(args.color);
            const castShadows = typeof args.castShadows === 'boolean' ? args.castShadows : undefined;

            if (typeKey === 'directional') {
              return cleanObject(await tools.lightingTools.createDirectionalLight({
                name,
                intensity: args.intensity,
                color,
                rotation,
                castShadows,
                temperature: args.temperature
              }));
            }
            if (typeKey === 'point') {
              return cleanObject(await tools.lightingTools.createPointLight({
                name,
                location,
                intensity: args.intensity,
                radius: args.radius,
                color,
                falloffExponent: args.falloffExponent,
                castShadows
              }));
            }
            if (typeKey === 'spot') {
              const innerCone = typeof args.innerCone === 'number' ? args.innerCone : undefined;
              const outerCone = typeof args.outerCone === 'number' ? args.outerCone : undefined;
              if (innerCone !== undefined && outerCone !== undefined && innerCone >= outerCone) {
                throw new Error('innerCone must be less than outerCone');
              }
              return cleanObject(await tools.lightingTools.createSpotLight({
                name,
                location,
                rotation,
                intensity: args.intensity,
                innerCone: args.innerCone,
                outerCone: args.outerCone,
                radius: args.radius,
                color,
                castShadows
              }));
            }
            if (typeKey === 'rect') {
              return cleanObject(await tools.lightingTools.createRectLight({
                name,
                location,
                rotation,
                intensity: args.intensity,
                width: args.width,
                height: args.height,
                color
              }));
            }
            if (typeKey === 'sky' || typeKey === 'skylight') {
              return cleanObject(await tools.lightingTools.createSkyLight({
                name,
                sourceType: args.sourceType,
                cubemapPath: args.cubemapPath,
                intensity: args.intensity,
                recapture: args.recapture
              }));
            }
            throw new Error(`Unknown light type: ${lightType}`);
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
        switch (requireAction(args)) {
          case 'create_animation_bp': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for animation_physics.create_animation_bp',
              {
                name: {
                  type: 'string',
                  title: 'Blueprint Name',
                  description: 'Name of the Animation Blueprint to create'
                },
                skeletonPath: {
                  type: 'string',
                  title: 'Skeleton Path',
                  description: 'Content path of the skeleton asset to bind'
                }
              }
            );
            const name = requireNonEmptyString(args.name, 'name', 'Invalid name');
            const skeletonPath = requireNonEmptyString(args.skeletonPath, 'skeletonPath', 'Invalid skeletonPath');
            const res = await tools.animationTools.createAnimationBlueprint({ name, skeletonPath, savePath: args.savePath });
            return cleanObject(res);
          }
          case 'play_montage': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide playback details for animation_physics.play_montage',
              {
                actorName: {
                  type: 'string',
                  title: 'Actor Name',
                  description: 'Actor that should play the montage'
                },
                montagePath: {
                  type: 'string',
                  title: 'Montage Path',
                  description: 'Montage or animation asset path to play'
                }
              }
            );
            const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Invalid actorName');
            const montagePath = args.montagePath || args.animationPath;
            const validatedMontage = requireNonEmptyString(montagePath, 'montagePath', 'Invalid montagePath');
            const res = await tools.animationTools.playAnimation({ actorName, animationType: 'Montage', animationPath: validatedMontage, playRate: args.playRate });
            return cleanObject(res);
          }
          case 'setup_ragdoll': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide setup details for animation_physics.setup_ragdoll',
              {
                skeletonPath: {
                  type: 'string',
                  title: 'Skeleton Path',
                  description: 'Content path for the skeleton asset'
                },
                physicsAssetName: {
                  type: 'string',
                  title: 'Physics Asset Name',
                  description: 'Name of the physics asset to apply'
                }
              }
            );
            const skeletonPath = requireNonEmptyString(args.skeletonPath, 'skeletonPath', 'Invalid skeletonPath');
            const physicsAssetName = requireNonEmptyString(args.physicsAssetName, 'physicsAssetName', 'Invalid physicsAssetName');
            const res = await tools.physicsTools.setupRagdoll({ skeletonPath, physicsAssetName, blendWeight: args.blendWeight, savePath: args.savePath });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown animation/physics action: ${args.action}`);
        }

// 6. EFFECTS SYSTEM
      case 'create_effect':
        switch (requireAction(args)) {
          case 'particle': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the particle effect details for create_effect.particle',
              {
                effectType: {
                  type: 'string',
                  title: 'Effect Type',
                  description: 'Preset effect type to spawn (e.g., Fire, Smoke)'
                }
              }
            );
            const res = await tools.niagaraTools.createEffect({ effectType: args.effectType, name: args.name, location: args.location, scale: args.scale, customParameters: args.customParameters });
            return cleanObject(res);
          }
          case 'niagara': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the Niagara system path for create_effect.niagara',
              {
                systemPath: {
                  type: 'string',
                  title: 'Niagara System Path',
                  description: 'Asset path of the Niagara system to spawn'
                }
              }
            );
            const systemPath = requireNonEmptyString(args.systemPath, 'systemPath', 'Invalid systemPath');
            const verifyResult = await tools.bridge.executePythonWithResult(`
import unreal, json
path = r"${systemPath}"
exists = unreal.EditorAssetLibrary.does_asset_exist(path)
print('RESULT:' + json.dumps({'success': exists, 'exists': exists, 'path': path}))
`.trim());
            if (!verifyResult?.exists) {
              return cleanObject({ success: false, error: `Niagara system not found at ${systemPath}` });
            }
            const loc = Array.isArray(args.location)
              ? { x: args.location[0], y: args.location[1], z: args.location[2] }
              : args.location || { x: 0, y: 0, z: 0 };
            const res = await tools.niagaraTools.spawnEffect({
              systemPath,
              location: [loc.x ?? 0, loc.y ?? 0, loc.z ?? 0],
              rotation: Array.isArray(args.rotation) ? args.rotation : undefined,
              scale: args.scale
            });
            return cleanObject(res);
          }
          case 'debug_shape': {
            const shapeInput = args.shape ?? 'Sphere';
            const shape = String(shapeInput).trim().toLowerCase();
            const originalShapeLabel = String(shapeInput).trim() || 'shape';
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
            return cleanObject({ success: false, error: `Unsupported debug shape: ${originalShapeLabel}` });
          }
          default:
            throw new Error(`Unknown effect action: ${args.action}`);
        }

// 7. BLUEPRINT MANAGER
      case 'manage_blueprint':
        switch (requireAction(args)) {
          case 'create': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for manage_blueprint.create',
              {
                name: {
                  type: 'string',
                  title: 'Blueprint Name',
                  description: 'Name for the new Blueprint asset'
                },
                blueprintType: {
                  type: 'string',
                  title: 'Blueprint Type',
                  description: 'Base type such as Actor, Pawn, Character, etc.'
                }
              }
            );
            const res = await tools.blueprintTools.createBlueprint({
              name: args.name,
              blueprintType: args.blueprintType || 'Actor',
              savePath: args.savePath,
              parentClass: args.parentClass
            });
            return cleanObject(res);
          }
          case 'add_component': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for manage_blueprint.add_component',
              {
                name: {
                  type: 'string',
                  title: 'Blueprint Name',
                  description: 'Blueprint asset to modify'
                },
                componentType: {
                  type: 'string',
                  title: 'Component Type',
                  description: 'Component class to add (e.g., StaticMeshComponent)'
                },
                componentName: {
                  type: 'string',
                  title: 'Component Name',
                  description: 'Name for the new component'
                }
              }
            );
            const res = await tools.blueprintTools.addComponent({ blueprintName: args.name, componentType: args.componentType, componentName: args.componentName });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown blueprint action: ${args.action}`);
        }

// 8. ENVIRONMENT BUILDER
      case 'build_environment':
        switch (requireAction(args)) {
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
        switch (requireAction(args)) {
          case 'read_log': {
            const filterCategoryRaw = args.filter_category;
            const filterCategory = Array.isArray(filterCategoryRaw)
              ? filterCategoryRaw
              : typeof filterCategoryRaw === 'string' && filterCategoryRaw.trim() !== ''
                ? filterCategoryRaw.split(',').map((s: string) => s.trim()).filter(Boolean)
                : undefined;
            const res = await tools.logTools.readOutputLog({
              filterCategory,
              filterLevel: args.filter_level,
              lines: typeof args.lines === 'number' ? args.lines : undefined,
              logPath: typeof args.log_path === 'string' ? args.log_path : undefined,
              includePrefixes: Array.isArray(args.include_prefixes) ? args.include_prefixes : undefined,
              excludeCategories: Array.isArray(args.exclude_categories) ? args.exclude_categories : undefined
            });
            return cleanObject(res);
          }
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
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the audio asset for system_control.play_sound',
              {
                soundPath: {
                  type: 'string',
                  title: 'Sound Asset Path',
                  description: 'Asset path of the sound to play'
                }
              }
            );
            const soundPath = requireNonEmptyString(args.soundPath, 'soundPath', 'Missing required parameter: soundPath');
            if (args.location && typeof args.location === 'object') {
              const loc = [args.location.x || 0, args.location.y || 0, args.location.z || 0];
              const res = await tools.audioTools.playSoundAtLocation({ soundPath, location: loc as [number, number, number], volume: args.volume, pitch: args.pitch, startTime: args.startTime });
              return cleanObject(res);
            }
            const res = await tools.audioTools.playSound2D({ soundPath, volume: args.volume, pitch: args.pitch, startTime: args.startTime });
            return cleanObject(res);
          }
          case 'create_widget': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for system_control.create_widget',
              {
                widgetName: {
                  type: 'string',
                  title: 'Widget Name',
                  description: 'Name for the new UI widget asset'
                },
                widgetType: {
                  type: 'string',
                  title: 'Widget Type',
                  description: 'Widget type such as HUD, Menu, Overlay, etc.'
                }
              }
            );
            const widgetName = requireNonEmptyString(args.widgetName ?? args.name, 'widgetName', 'Missing required parameter: widgetName');
            const widgetType = requireNonEmptyString(args.widgetType, 'widgetType', 'Missing required parameter: widgetType');
            const res = await tools.uiTools.createWidget({ name: widgetName, type: widgetType as any, savePath: args.savePath });
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
          const raw = await tools.bridge.executeConsoleCommand(cmd);
          const summary = tools.bridge.summarizeConsoleCommand(cmd, raw);
          const output = summary.output || '';
          const looksInvalid = /unknown|invalid/i.test(output);
          return cleanObject({
            success: summary.returnValue !== false && !looksInvalid,
            command: summary.command,
            output: output || undefined,
            logLines: summary.logLines?.length ? summary.logLines : undefined,
            returnValue: summary.returnValue,
            message: !looksInvalid
              ? (output || 'Command executed')
              : undefined,
            error: looksInvalid ? output : undefined,
            raw: summary.raw
          });
        } catch (e: any) {
          return cleanObject({ success: false, command: cmd, error: e?.message || String(e) });
        }
        

      // 11. REMOTE CONTROL PRESETS - Direct implementation
      case 'manage_rc':
        // Handle RC operations directly through RcTools
        let rcResult: any;
        const rcAction = requireAction(args);
        switch (rcAction) {
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
          case 'delete_preset':
            const presetIdentifier = args.presetId || args.presetPath;
            if (!presetIdentifier) throw new Error('Missing required parameter: presetId');
            rcResult = await tools.rcTools.deletePreset(presetIdentifier);
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
            throw new Error(`Unknown RC action: ${rcAction}. Valid actions are: create_preset, expose_actor, expose_property, list_fields, set_property, get_property, or their simplified versions: create, list, delete, expose, get_exposed, set_value, get_value, call_function`);
        }
        
        // Return result directly - MCP formatting will be handled by response validator
        // Clean to prevent circular references
        return cleanObject(rcResult);

      // 12. SEQUENCER / CINEMATICS
      case 'manage_sequence':
        // Direct handling for sequence operations
        const seqResult = await (async () => {
          const sequenceTools = tools.sequenceTools;
          if (!sequenceTools) throw new Error('Sequence tools not available');
          const action = requireAction(args);
          
          switch (action) {
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
              throw new Error(`Unknown sequence action: ${action}`);
          }
        })();
        
        // Return result directly - MCP formatting will be handled by response validator
        // Clean to prevent circular references
        return cleanObject(seqResult);
      // 13. INTROSPECTION
case 'inspect':
  const inspectAction = requireAction(args);
  switch (inspectAction) {
          case 'inspect_object': {
            const res = await tools.introspectionTools.inspectObject({ objectPath: args.objectPath, detailed: args.detailed });
            return cleanObject(res);
          }
          case 'set_property': {
            const res = await tools.introspectionTools.setProperty({ objectPath: args.objectPath, propertyName: args.propertyName, value: args.value });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown inspect action: ${inspectAction}`);
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
