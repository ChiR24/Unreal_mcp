// Landscape tools for Unreal Engine with UE 5.6 World Partition support
import { UnrealBridge } from '../unreal-bridge.js';
import { bestEffortInterpretedText, coerceBoolean, coerceString, interpretStandardResult } from '../utils/result-helpers.js';
import { ensureVector3 } from '../utils/validation.js';
import { escapePythonString } from '../utils/python.js';

export class LandscapeTools {
  constructor(private bridge: UnrealBridge) {}

  // Create landscape with World Partition support (UE 5.6)
  async createLandscape(params: {
    name: string;
    location?: [number, number, number];
    sizeX?: number;
    sizeY?: number;
    quadsPerSection?: number;
    sectionsPerComponent?: number;
    componentCount?: number;
    materialPath?: string;
    // World Partition specific (UE 5.6)
    enableWorldPartition?: boolean;
    runtimeGrid?: string;
    isSpatiallyLoaded?: boolean;
    dataLayers?: string[];
  }) {
    const name = params.name?.trim();
    if (!name) {
      return { success: false, error: 'Landscape name is required' };
    }
    if (typeof params.sizeX === 'number' && params.sizeX <= 0) {
      return {
        success: false,
        error: 'Landscape sizeX must be a positive number'
      };
    }
    if (typeof params.sizeY === 'number' && params.sizeY <= 0) {
      return {
        success: false,
        error: 'Landscape sizeY must be a positive number'
      };
    }

    const [locX, locY, locZ] = ensureVector3(params.location ?? [0, 0, 0], 'landscape location');
    const sectionsPerComponent = Math.max(1, Math.floor(params.sectionsPerComponent ?? 1));
    const quadsPerSection = Math.max(1, Math.floor(params.quadsPerSection ?? 63));
    const componentCount = Math.max(1, Math.floor(params.componentCount ?? 1));

    const defaultSize = 1000;
    const scaleX = params.sizeX ? Math.max(0.1, params.sizeX / defaultSize) : 1;
    const scaleY = params.sizeY ? Math.max(0.1, params.sizeY / defaultSize) : 1;

    const escapedName = escapePythonString(name);
  const escapedMaterial =
    params.materialPath && params.materialPath.trim().length > 0
    ? escapePythonString(params.materialPath.trim())
    : '';

    const runtimeGridFlag = params.runtimeGrid ? 'True' : 'False';
    const spatiallyLoadedFlag = params.isSpatiallyLoaded ? 'True' : 'False';
    const runtimeGridValue = params.runtimeGrid ? escapePythonString(params.runtimeGrid.trim()) : '';
    const dataLayerNames = Array.isArray(params.dataLayers)
      ? params.dataLayers
          .map(layer => layer?.trim())
          .filter((layer): layer is string => Boolean(layer))
          .map(layer => escapePythonString(layer))
      : [];

    const pythonScript = `
import unreal
import json

result = {
  "success": False,
  "message": "",
  "error": "",
  "warnings": [],
  "details": [],
  "landscapeName": "",
  "landscapeActor": "",
  "worldPartition": False,
  "runtimeGridRequested": ${runtimeGridFlag},
  "spatiallyLoaded": ${spatiallyLoadedFlag}
}

try:
  editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
  world = editor_subsystem.get_editor_world() if editor_subsystem and hasattr(editor_subsystem, 'get_editor_world') else None
  data_layer_manager = None
  if world:
    try:
      world_partition = world.get_world_partition()
      result["worldPartition"] = world_partition is not None
      if result["worldPartition"] and hasattr(unreal, "WorldPartitionBlueprintLibrary"):
        data_layer_manager = unreal.WorldPartitionBlueprintLibrary.get_data_layer_manager(world)
    except Exception as wp_error:
      result["warnings"].append(f"Failed to inspect world partition: {wp_error}")

  actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  if not actor_subsystem:
    result["error"] = "EditorActorSubsystem unavailable"
  else:
    existing = None
    try:
      for actor in actor_subsystem.get_all_level_actors():
        if actor and actor.get_actor_label() == "${escapedName}":
          existing = actor
          break
    except Exception as scan_error:
      result["warnings"].append(f"Actor scan failed: {scan_error}")

    if existing:
      result["success"] = True
      result["message"] = "Landscape already exists"
      result["landscapeName"] = existing.get_actor_label()
      try:
        result["landscapeActor"] = existing.get_path_name()
      except Exception:
        pass
    else:
      landscape_class = getattr(unreal, "Landscape", None)
      if not landscape_class:
        result["error"] = "Landscape class unavailable"
      else:
        location = unreal.Vector(${locX}, ${locY}, ${locZ})
        rotation = unreal.Rotator(0.0, 0.0, 0.0)
        landscape_actor = actor_subsystem.spawn_actor_from_class(landscape_class, location, rotation)
        if not landscape_actor:
          result["error"] = "Failed to spawn landscape actor"
        else:
          try:
            landscape_actor.set_actor_label("${escapedName}", True)
          except TypeError:
            landscape_actor.set_actor_label("${escapedName}")
          except Exception as label_error:
            result["warnings"].append(f"Failed to set landscape label: {label_error}")

          try:
            landscape_actor.set_actor_scale3d(unreal.Vector(${scaleX.toFixed(4)}, ${scaleY.toFixed(4)}, 1.0))
            result["details"].append(f"Actor scale set to (${scaleX.toFixed(2)}, ${scaleY.toFixed(2)}, 1.0)")
          except Exception as scale_error:
            result["warnings"].append(f"Failed to set landscape scale: {scale_error}")

          landscape_editor = None
          try:
            landscape_editor = unreal.get_editor_subsystem(unreal.LandscapeEditorSubsystem)
          except Exception as editor_error:
            result["warnings"].append(f"LandscapeEditorSubsystem unavailable: {editor_error}")

          if landscape_editor:
            try:
              landscape_editor.set_component_size(${sectionsPerComponent}, ${quadsPerSection})
              landscape_editor.set_component_count(${componentCount}, ${componentCount})
              result["details"].append(f"Component size ${sectionsPerComponent}x${quadsPerSection}, count ${componentCount}x${componentCount}")
            except Exception as config_error:
              result["warnings"].append(f"Landscape configuration limited: {config_error}")

          ${escapedMaterial ? `try:
            material = unreal.EditorAssetLibrary.load_asset("${escapedMaterial}")
            if material:
              try:
                landscape_actor.set_landscape_material(material)
              except Exception:
                landscape_actor.editor_set_landscape_material(material)
              result["details"].append("Landscape material applied")
            else:
              result["warnings"].append("Landscape material asset not found: ${escapedMaterial}")
          except Exception as material_error:
            result["warnings"].append(f"Failed to apply landscape material: {material_error}")
          ` : ''}
          ${runtimeGridValue ? `if result["worldPartition"] and hasattr(unreal, "WorldPartitionBlueprintLibrary"):
            try:
              unreal.WorldPartitionBlueprintLibrary.set_actor_runtime_grid(landscape_actor, "${runtimeGridValue}")
              result["details"].append("Runtime grid assigned: ${runtimeGridValue}")
            except Exception as grid_error:
              result["warnings"].append(f"Failed to assign runtime grid: {grid_error}")
          ` : ''}
          ${params.isSpatiallyLoaded ? `if result["worldPartition"] and hasattr(unreal, "WorldPartitionBlueprintLibrary"):
            try:
              unreal.WorldPartitionBlueprintLibrary.set_actor_spatially_loaded(landscape_actor, True)
              result["details"].append("Actor marked as spatially loaded")
            except Exception as spatial_error:
              result["warnings"].append(f"Failed to mark as spatially loaded: {spatial_error}")
          ` : ''}
          ${dataLayerNames.length ? `if result["worldPartition"] and data_layer_manager:
            for layer_name in ${JSON.stringify(dataLayerNames)}:
              try:
                data_layer = data_layer_manager.get_data_layer(layer_name)
                if data_layer:
                  unreal.WorldPartitionBlueprintLibrary.add_actor_to_data_layer(landscape_actor, data_layer)
                  result["details"].append(f"Added to data layer {layer_name}")
                else:
                  result["warnings"].append(f"Data layer not found: {layer_name}")
              except Exception as data_layer_error:
                result["warnings"].append(f"Failed to assign data layer {layer_name}: {data_layer_error}")
          ` : ''}

          try:
            result["landscapeName"] = landscape_actor.get_actor_label()
            result["landscapeActor"] = landscape_actor.get_path_name()
          except Exception:
            pass

          result["success"] = True
          result["message"] = "Landscape actor created"
except Exception as e:
  result["error"] = str(e)

if result.get("success"):
  result.pop("error", None)
else:
  if not result.get("error"):
    result["error"] = "Failed to create landscape actor"
  if not result.get("message"):
    result["message"] = result["error"]

if not result.get("warnings"):
  result.pop("warnings", None)
if not result.get("details"):
  result.pop("details", None)

print("RESULT:" + json.dumps(result))
`.trim();

    try {
      const response = await this.bridge.executePython(pythonScript);
      const interpreted = interpretStandardResult(response, {
        successMessage: 'Landscape actor created',
        failureMessage: 'Failed to create landscape actor'
      });

      if (!interpreted.success) {
        return {
          success: false,
          error: interpreted.error || interpreted.message
        };
      }

      const result: Record<string, unknown> = {
        success: true,
        message: interpreted.message,
        landscapeName: coerceString(interpreted.payload.landscapeName) ?? name,
        worldPartition: coerceBoolean(interpreted.payload.worldPartition)
      };

      const actorPath = coerceString(interpreted.payload.landscapeActor);
      if (actorPath) {
        result.landscapeActor = actorPath;
      }
      if (interpreted.warnings?.length) {
        result.warnings = interpreted.warnings;
      }
      if (interpreted.details?.length) {
        result.details = interpreted.details;
      }

      if (params.runtimeGrid) {
        result.runtimeGrid = params.runtimeGrid;
      }
      if (typeof params.isSpatiallyLoaded === 'boolean') {
        result.spatiallyLoaded = params.isSpatiallyLoaded;
      }

      return result;
    } catch (error) {
      return {
        success: false,
        error: `Failed to create landscape actor: ${error}`
      };
    }
  }

  // Sculpt landscape
  async sculptLandscape(_params: {
    landscapeName: string;
    tool: 'Sculpt' | 'Smooth' | 'Flatten' | 'Ramp' | 'Erosion' | 'Hydro' | 'Noise' | 'Retopologize';
    brushSize?: number;
    brushFalloff?: number;
    strength?: number;
    position?: [number, number, number];
  }) {
    return { success: false, error: 'sculptLandscape not implemented via Remote Control. Requires Landscape editor tools.' };
  }

  // Paint landscape
  async paintLandscape(_params: {
    landscapeName: string;
    layerName: string;
    position: [number, number, number];
    brushSize?: number;
    strength?: number;
    targetValue?: number;
  }) {
    return { success: false, error: 'paintLandscape not implemented via Remote Control. Requires Landscape editor tools.' };
  }

  // Add landscape layer
  async addLandscapeLayer(params: {
    landscapeName: string;
    layerName: string;
    weightMapPath?: string;
    blendMode?: 'Weight' | 'Alpha';
  }) {
  const commands: string[] = [];
    
    commands.push(`AddLandscapeLayer ${params.landscapeName} ${params.layerName}`);
    
    if (params.weightMapPath) {
      commands.push(`SetLayerWeightMap ${params.layerName} ${params.weightMapPath}`);
    }
    
    if (params.blendMode) {
      commands.push(`SetLayerBlendMode ${params.layerName} ${params.blendMode}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: `Layer ${params.layerName} added to landscape` };
  }

  // Create landscape spline
  async createLandscapeSpline(params: {
    landscapeName: string;
    splineName: string;
    points: Array<[number, number, number]>;
    width?: number;
    falloffWidth?: number;
    meshPath?: string;
  }) {
  const commands: string[] = [];
    
    commands.push(`CreateLandscapeSpline ${params.landscapeName} ${params.splineName}`);
    
    for (const point of params.points) {
      commands.push(`AddSplinePoint ${params.splineName} ${point.join(' ')}`);
    }
    
    if (params.width !== undefined) {
      commands.push(`SetSplineWidth ${params.splineName} ${params.width}`);
    }
    
    if (params.falloffWidth !== undefined) {
      commands.push(`SetSplineFalloffWidth ${params.splineName} ${params.falloffWidth}`);
    }
    
    if (params.meshPath) {
      commands.push(`SetSplineMesh ${params.splineName} ${params.meshPath}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: `Landscape spline ${params.splineName} created` };
  }

  // Import heightmap
  async importHeightmap(params: {
    landscapeName: string;
    heightmapPath: string;
    scale?: [number, number, number];
  }) {
    const scale = params.scale || [100, 100, 100];
    const command = `ImportLandscapeHeightmap ${params.landscapeName} ${params.heightmapPath} ${scale.join(' ')}`;
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Export heightmap
  async exportHeightmap(params: {
    landscapeName: string;
    exportPath: string;
    format?: 'PNG' | 'RAW';
  }) {
    const format = params.format || 'PNG';
    const command = `ExportLandscapeHeightmap ${params.landscapeName} ${params.exportPath} ${format}`;
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Set landscape LOD
  async setLandscapeLOD(params: {
    landscapeName: string;
    lodBias?: number;
    forcedLOD?: number;
    lodDistribution?: number;
  }) {
  const commands: string[] = [];
    
    if (params.lodBias !== undefined) {
      commands.push(`SetLandscapeLODBias ${params.landscapeName} ${params.lodBias}`);
    }
    
    if (params.forcedLOD !== undefined) {
      commands.push(`SetLandscapeForcedLOD ${params.landscapeName} ${params.forcedLOD}`);
    }
    
    if (params.lodDistribution !== undefined) {
      commands.push(`SetLandscapeLODDistribution ${params.landscapeName} ${params.lodDistribution}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: 'Landscape LOD settings updated' };
  }

  // Create landscape grass
  async createLandscapeGrass(params: {
    landscapeName: string;
    grassType: string;
    density?: number;
    minScale?: number;
    maxScale?: number;
    randomRotation?: boolean;
  }) {
  const commands: string[] = [];
    
    commands.push(`CreateLandscapeGrass ${params.landscapeName} ${params.grassType}`);
    
    if (params.density !== undefined) {
      commands.push(`SetGrassDensity ${params.grassType} ${params.density}`);
    }
    
    if (params.minScale !== undefined && params.maxScale !== undefined) {
      commands.push(`SetGrassScale ${params.grassType} ${params.minScale} ${params.maxScale}`);
    }
    
    if (params.randomRotation !== undefined) {
      commands.push(`SetGrassRandomRotation ${params.grassType} ${params.randomRotation}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: `Grass type ${params.grassType} created on landscape` };
  }

  // Landscape collision
  async updateLandscapeCollision(params: {
    landscapeName: string;
    collisionMipLevel?: number;
    simpleCollision?: boolean;
  }) {
  const commands: string[] = [];
    
    if (params.collisionMipLevel !== undefined) {
      commands.push(`SetLandscapeCollisionMipLevel ${params.landscapeName} ${params.collisionMipLevel}`);
    }
    
    if (params.simpleCollision !== undefined) {
      commands.push(`SetLandscapeSimpleCollision ${params.landscapeName} ${params.simpleCollision}`);
    }
    
    commands.push(`UpdateLandscapeCollision ${params.landscapeName}`);
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: 'Landscape collision updated' };
  }

  // Retopologize landscape
  async retopologizeLandscape(params: {
    landscapeName: string;
    targetTriangleCount?: number;
    preserveDetails?: boolean;
  }) {
  const commands: string[] = [];
    
    if (params.targetTriangleCount !== undefined) {
      commands.push(`SetRetopologizeTarget ${params.targetTriangleCount}`);
    }
    
    if (params.preserveDetails !== undefined) {
      commands.push(`SetRetopologizePreserveDetails ${params.preserveDetails}`);
    }
    
    commands.push(`RetopologizeLandscape ${params.landscapeName}`);
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: 'Landscape retopologized' };
  }

  // Create water body
  async createWaterBody(params: {
    type: 'Ocean' | 'Lake' | 'River' | 'Stream';
    name: string;
    location?: [number, number, number];
    size?: [number, number];
    depth?: number;
  }) {
    const loc = params.location || [0, 0, 0];
    const size = params.size || [1000, 1000];
    const depth = params.depth || 100;
    
    const command = `CreateWaterBody ${params.type} ${params.name} ${loc.join(' ')} ${size.join(' ')} ${depth}`;
    
    return this.bridge.executeConsoleCommand(command);
  }

  // World Partition support for landscapes (UE 5.6)
  async configureWorldPartition(params: {
    landscapeName: string;
    enableSpatialLoading?: boolean;
    runtimeGrid?: string;
    dataLayers?: string[];
    streamingDistance?: number;
  }) {
    try {
    const pythonScript = `
import unreal
import json

result = {'success': False, 'error': 'Landscape not found'}

try:
  # Get the landscape actor using modern EditorActorSubsystem
  actors = []
  try:
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if actor_subsystem and hasattr(actor_subsystem, 'get_all_level_actors'):
      actors = actor_subsystem.get_all_level_actors()
  except Exception:
    actors = []
  landscape = None

  for actor in actors:
    if actor.get_name() == "${params.landscapeName}" or actor.get_actor_label() == "${params.landscapeName}":
      if isinstance(actor, unreal.LandscapeProxy) or isinstance(actor, unreal.Landscape):
        landscape = actor
        break

  if landscape:
    changes_made = []

    # Configure spatial loading (UE 5.6)
    if ${params.enableSpatialLoading !== undefined ? 'True' : 'False'}:
      try:
        landscape.set_editor_property('is_spatially_loaded', ${params.enableSpatialLoading || false})
        changes_made.append("Spatial loading: ${params.enableSpatialLoading}")
      except:
        pass

    # Set runtime grid (UE 5.6 World Partition)
    if "${params.runtimeGrid || ''}":
      try:
        landscape.set_editor_property('runtime_grid', unreal.Name("${params.runtimeGrid}"))
        changes_made.append("Runtime grid: ${params.runtimeGrid}")
      except:
        pass

    # Configure data layers (UE 5.6)
    if ${params.dataLayers ? 'True' : 'False'}:
      try:
        # Try modern subsystem first
        try:
          world = None
          editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
          if editor_subsystem and hasattr(editor_subsystem, 'get_editor_world'):
            world = editor_subsystem.get_editor_world()
          if world is None:
            world = unreal.EditorSubsystemLibrary.get_editor_world()
        except Exception:
          world = unreal.EditorSubsystemLibrary.get_editor_world()
        data_layer_manager = unreal.WorldPartitionBlueprintLibrary.get_data_layer_manager(world)
        if data_layer_manager:
          # Note: Full data layer API requires additional setup
          changes_made.append("Data layers: Requires manual configuration")
      except:
        pass

    if changes_made:
      result = {
        'success': True,
        'message': 'World Partition configured',
        'changes': changes_made
      }
    else:
      result = {
        'success': False,
        'error': 'No World Partition changes applied'
      }

except Exception as e:
  result = {'success': False, 'error': str(e)}

print('RESULT:' + json.dumps(result))
`.trim();

    const response = await this.bridge.executePython(pythonScript);
    const interpreted = interpretStandardResult(response, {
      successMessage: 'World Partition configuration attempted',
      failureMessage: 'World Partition configuration failed'
    });

    if (interpreted.success) {
      return interpreted.payload as any;
    }

    return {
      success: false,
      error: interpreted.error ?? 'World Partition configuration failed',
      details: bestEffortInterpretedText(interpreted)
    };
    } catch (err) {
      return { success: false, error: `Failed to configure World Partition: ${err}` };
    }
  }

  // Set landscape data layers (UE 5.6)
  async setDataLayers(params: {
    landscapeName: string;
    dataLayerNames: string[];
    operation: 'add' | 'remove' | 'set';
  }) {
    try {
      const commands = [];
      
      // Use console commands for data layer management
      if (params.operation === 'set' || params.operation === 'add') {
        for (const layerName of params.dataLayerNames) {
          commands.push(`wp.Runtime.SetDataLayerRuntimeState Loaded ${layerName}`);
        }
      } else if (params.operation === 'remove') {
        for (const layerName of params.dataLayerNames) {
          commands.push(`wp.Runtime.SetDataLayerRuntimeState Unloaded ${layerName}`);
        }
      }
      
      // Execute commands
      await this.bridge.executeConsoleCommands(commands);
      
      return { 
        success: true, 
        message: `Data layers ${params.operation === 'add' ? 'added' : params.operation === 'remove' ? 'removed' : 'set'} for landscape`,
        layers: params.dataLayerNames
      };
    } catch (err) {
      return { success: false, error: `Failed to manage data layers: ${err}` };
    }
  }

  // Configure landscape streaming cells (UE 5.6 World Partition)
  async configureStreamingCells(params: {
    landscapeName: string;
    cellSize?: number;
    loadingRange?: number;
    enableHLOD?: boolean;
  }) {
    const commands = [];
    
    // World Partition runtime commands
    if (params.loadingRange !== undefined) {
      commands.push(`wp.Runtime.OverrideRuntimeSpatialHashLoadingRange -grid=0 -range=${params.loadingRange}`);
    }
    
    if (params.enableHLOD !== undefined) {
      commands.push(`wp.Runtime.HLOD ${params.enableHLOD ? '1' : '0'}`);
    }
    
    // Debug visualization commands
    commands.push('wp.Runtime.ToggleDrawRuntimeHash2D'); // Show 2D grid
    
    try {
      await this.bridge.executeConsoleCommands(commands);
      
      return { 
        success: true, 
        message: 'Streaming cells configured for World Partition',
        settings: {
          cellSize: params.cellSize,
          loadingRange: params.loadingRange,
          hlod: params.enableHLOD
        }
      };
    } catch (err) {
      return { success: false, error: `Failed to configure streaming cells: ${err}` };
    }
  }
}
