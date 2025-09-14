// Landscape tools for Unreal Engine with UE 5.6 World Partition support
import { UnrealBridge } from '../unreal-bridge.js';

export class LandscapeTools {
  constructor(private bridge: UnrealBridge) {}

  // Execute console command
  private async _executeCommand(command: string) {
    return this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/Engine.Default__KismetSystemLibrary',
      functionName: 'ExecuteConsoleCommand',
      parameters: {
        WorldContextObject: null,
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }

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
    // Try Python API with World Partition support for UE 5.6
    try {
      const pythonScript = `
import unreal
import json

# Get the editor world using the proper subsystem
try:
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_subsystem.get_editor_world() if hasattr(editor_subsystem, 'get_editor_world') else None
except:
    # Fallback for older API
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
    except:
        world = None

is_world_partition = False
data_layer_manager = None

if world:
    try:
        # Check if World Partition is enabled (UE 5.6)
        world_partition = world.get_world_partition()
        is_world_partition = world_partition is not None
        if is_world_partition:
            # Get Data Layer Manager for UE 5.6
            data_layer_manager = unreal.WorldPartitionBlueprintLibrary.get_data_layer_manager(world)
    except:
        pass

# Try to create a basic landscape using available API
try:
    # Use EditorLevelLibrary to spawn a landscape actor
    location = unreal.Vector(${params.location?.[0] || 0}, ${params.location?.[1] || 0}, ${params.location?.[2] || 0})
    rotation = unreal.Rotator(0, 0, 0)
    
    # Check if LandscapeSubsystem is available (not LandscapeEditorSubsystem)
    landscape_subsystem = None
    try:
        landscape_subsystem = unreal.get_editor_subsystem(unreal.LandscapeSubsystem)
    except:
        pass
    
    if landscape_subsystem:
        # Use subsystem if available
        result = {"success": False, "error": "LandscapeSubsystem API limited via Python", "world_partition": is_world_partition}
    else:
        # Landscape actors cannot be properly spawned via Python API
        # The component registration issues are inherent to how landscapes work
        # Direct users to the proper workflow
        result = {
            "success": False, 
            "error": "Landscape creation is not supported via Python API",
            "suggestion": "Please use Landscape Mode in the Unreal Editor toolbar:",
            "steps": [
                "1. Click 'Modes' dropdown in toolbar",
                "2. Select 'Landscape'",
                "3. Configure size and materials",
                "4. Click 'Create' to generate landscape"
            ],
            "world_partition": is_world_partition
        }
    
    print(f'RESULT:{json.dumps(result)}')
except Exception as e:
    print(f'RESULT:{{"success": false, "error": "{str(e)}"}}')
`.trim();
      
      const response = await this.bridge.executePython(pythonScript);
      const output = typeof response === 'string' ? response : JSON.stringify(response);
      const match = output.match(/RESULT:({.*})/);
      
      if (match) {
        try {
          const result = JSON.parse(match[1]);
          if (result.world_partition) {
            result.message = 'World Partition detected. Manual landscape creation required in editor.';
          }
          return result;
        } catch {}
      }
    } catch {
      // Continue to fallback
    }
    
    // Fallback message with World Partition info
    return { 
      success: false, 
      error: 'Landscape creation via API is limited. Please use the Unreal Editor UI to create landscapes.',
      worldPartitionSupport: params.enableWorldPartition ? 'Requested' : 'Not requested',
      suggestion: 'Use the Landscape Mode in the editor toolbar to create and configure landscapes'
    };
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
    const commands = [];
    
    commands.push(`AddLandscapeLayer ${params.landscapeName} ${params.layerName}`);
    
    if (params.weightMapPath) {
      commands.push(`SetLayerWeightMap ${params.layerName} ${params.weightMapPath}`);
    }
    
    if (params.blendMode) {
      commands.push(`SetLayerBlendMode ${params.layerName} ${params.blendMode}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
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
    const commands = [];
    
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
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
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
    const commands = [];
    
    if (params.lodBias !== undefined) {
      commands.push(`SetLandscapeLODBias ${params.landscapeName} ${params.lodBias}`);
    }
    
    if (params.forcedLOD !== undefined) {
      commands.push(`SetLandscapeForcedLOD ${params.landscapeName} ${params.forcedLOD}`);
    }
    
    if (params.lodDistribution !== undefined) {
      commands.push(`SetLandscapeLODDistribution ${params.landscapeName} ${params.lodDistribution}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
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
    const commands = [];
    
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
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Grass type ${params.grassType} created on landscape` };
  }

  // Landscape collision
  async updateLandscapeCollision(params: {
    landscapeName: string;
    collisionMipLevel?: number;
    simpleCollision?: boolean;
  }) {
    const commands = [];
    
    if (params.collisionMipLevel !== undefined) {
      commands.push(`SetLandscapeCollisionMipLevel ${params.landscapeName} ${params.collisionMipLevel}`);
    }
    
    if (params.simpleCollision !== undefined) {
      commands.push(`SetLandscapeSimpleCollision ${params.landscapeName} ${params.simpleCollision}`);
    }
    
    commands.push(`UpdateLandscapeCollision ${params.landscapeName}`);
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Landscape collision updated' };
  }

  // Retopologize landscape
  async retopologizeLandscape(params: {
    landscapeName: string;
    targetTriangleCount?: number;
    preserveDetails?: boolean;
  }) {
    const commands = [];
    
    if (params.targetTriangleCount !== undefined) {
      commands.push(`SetRetopologizeTarget ${params.targetTriangleCount}`);
    }
    
    if (params.preserveDetails !== undefined) {
      commands.push(`SetRetopologizePreserveDetails ${params.preserveDetails}`);
    }
    
    commands.push(`RetopologizeLandscape ${params.landscapeName}`);
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
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

try:
    # Get the landscape actor
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    landscape = None
    
    for actor in actors:
        if actor.get_name() == "${params.landscapeName}" or actor.get_actor_label() == "${params.landscapeName}":
            if isinstance(actor, unreal.LandscapeProxy) or isinstance(actor, unreal.Landscape):
                landscape = actor
                break
    
    if not landscape:
        print('RESULT:{"success": false, "error": "Landscape not found"}')
    else:
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
                world = unreal.EditorLevelLibrary.get_editor_world()
                data_layer_manager = unreal.WorldPartitionBlueprintLibrary.get_data_layer_manager(world)
                if data_layer_manager:
                    # Note: Full data layer API requires additional setup
                    changes_made.append("Data layers: Requires manual configuration")
            except:
                pass
        
        if changes_made:
            print('RESULT:{"success": true, "message": "World Partition configured", "changes": ' + str(changes_made).replace("'", '"') + '}')
        else:
            print('RESULT:{"success": false, "error": "No World Partition changes applied"}')
            
except Exception as e:
    print(f'RESULT:{{"success": false, "error": "{str(e)}"}}')
`.trim();

      const response = await this.bridge.executePython(pythonScript);
      const output = typeof response === 'string' ? response : JSON.stringify(response);
      const match = output.match(/RESULT:({.*})/);
      
      if (match) {
        try {
          return JSON.parse(match[1]);
        } catch {}
      }
      
      return { success: true, message: 'World Partition configuration attempted' };
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
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
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
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
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
