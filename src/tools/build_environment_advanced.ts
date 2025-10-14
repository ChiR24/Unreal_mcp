import { UnrealBridge } from '../unreal-bridge.js';
import { interpretStandardResult, coerceBoolean, coerceNumber, coerceString, coerceStringArray } from '../utils/result-helpers.js';
import { allowPythonFallbackFromEnv } from '../utils/env.js';

/**
 * Advanced Build Environment Tools
 * Implements procedural terrain and foliage using documented Unreal Engine Python APIs
 */
export class BuildEnvironmentAdvanced {
  constructor(private bridge: UnrealBridge) {}

  /**
   * Create procedural terrain using ProceduralMeshComponent
   * This works around the landscape API limitations
   */
  async createProceduralTerrain(params: {
    name: string;
    location?: [number, number, number];
    sizeX?: number;
    sizeY?: number;
    subdivisions?: number;
    heightFunction?: string; // Python expression for height calculation
    material?: string;
  }) {
    const pythonScript = `
import unreal
import json
import math

name = ${JSON.stringify(params.name)}
location = unreal.Vector(${params.location?.[0] || 0}, ${params.location?.[1] || 0}, ${params.location?.[2] || 0})
size_x = ${params.sizeX || 2000}
size_y = ${params.sizeY || 2000}
subdivisions = ${params.subdivisions || 50}
height_function = ${JSON.stringify(params.heightFunction || 'math.sin(x/100) * 50 + math.cos(y/100) * 30')}

result = {}

try:
    # Get editor subsystem
    subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    
    # Create ProceduralMeshActor
    proc_actor = None
    proc_mesh_comp = None
    
    # Try ProceduralMeshActor first
    try:
        proc_actor = subsys.spawn_actor_from_class(
            unreal.ProceduralMeshActor,
            location,
            unreal.Rotator(0, 0, 0)
        )
        proc_actor.set_actor_label(f"{name}_ProceduralTerrain")
        proc_mesh_comp = proc_actor.get_component_by_class(unreal.ProceduralMeshComponent)
    except:
        # Fallback: Create empty actor and add ProceduralMeshComponent
        # If spawning ProceduralMeshActor failed, surface a clear error about the plugin requirement
        raise Exception("Failed to spawn ProceduralMeshActor. Ensure the 'Procedural Mesh Component' plugin is enabled and available.")
    
    if proc_mesh_comp:
        # Generate terrain mesh
        vertices = []
        triangles = []
        normals = []
        uvs = []
        vertex_colors = []
        
        step_x = size_x / subdivisions
        step_y = size_y / subdivisions
        
        # Create vertices with height variation
        for y in range(subdivisions + 1):
            for x in range(subdivisions + 1):
                # Position
                vert_x = x * step_x - size_x / 2
                vert_y = y * step_y - size_y / 2
                
                # Calculate height using the provided function
                try:
                    vert_z = eval(height_function, {"x": vert_x, "y": vert_y, "math": math})
                except:
                    vert_z = 0  # Fallback to flat if function fails
                
                vertices.append(unreal.Vector(vert_x, vert_y, vert_z))
                normals.append(unreal.Vector(0, 0, 1))  # Will be recalculated
                uvs.append(unreal.Vector2D(x / subdivisions, y / subdivisions))
                
                # Color based on height
                height_normalized = min(1.0, max(0.0, (vert_z + 100) / 200))
                vertex_colors.append(unreal.LinearColor(height_normalized, 1 - height_normalized, 0.2, 1))
        
        # Create triangles
        for y in range(subdivisions):
            for x in range(subdivisions):
                idx = y * (subdivisions + 1) + x
                
                # First triangle
                triangles.extend([idx, idx + subdivisions + 1, idx + 1])
                # Second triangle  
                triangles.extend([idx + 1, idx + subdivisions + 1, idx + subdivisions + 2])
        
        # Create mesh section
        proc_mesh_comp.create_mesh_section_linear_color(
            0,  # Section index
            vertices,
            triangles,
            normals,
            uvs,
            vertex_colors,
            [],  # Tangents
            True  # Create collision
        )
        
        # Apply material if specified
        if ${JSON.stringify(params.material || '')}:
            material = unreal.EditorAssetLibrary.load_asset(${JSON.stringify(params.material || '/Engine/MapTemplates/Materials/BasicGrid01')})
            if material:
                proc_mesh_comp.set_material(0, material)
        
        # Enable collision
        proc_mesh_comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        
        result = {
            "success": True,
            "message": f"Created procedural terrain '{name}'",
            "actor_name": proc_actor.get_actor_label(),
            "vertices": len(vertices),
            "triangles": len(triangles) // 3,
            "size": [size_x, size_y],
            "subdivisions": subdivisions
        }
    else:
        result = {"success": False, "error": "Could not create ProceduralMeshComponent"}
        
except Exception as e:
    result = {"success": False, "error": str(e)}

print(f"RESULT:{json.dumps(result)}")
`.trim();

    const allowPythonFallback = allowPythonFallbackFromEnv();
    const response = await (this.bridge as any).executeEditorPython(pythonScript, { allowPythonFallback });
        const interpreted = interpretStandardResult(response, {
            successMessage: `Created procedural terrain '${params.name}'`,
            failureMessage: `Failed to create procedural terrain '${params.name}'`
        });

        if (!interpreted.success) {
            const failure: {
                success: false;
                error: string;
                message: string;
                warnings?: string[];
                details?: string[];
                payload?: Record<string, unknown>;
            } = {
                success: false,
                error: interpreted.error ?? interpreted.message,
                message: interpreted.message
            };

            if (interpreted.warnings) {
                failure.warnings = interpreted.warnings;
            }
            if (interpreted.details) {
                failure.details = interpreted.details;
            }
            if (interpreted.payload && Object.keys(interpreted.payload).length > 0) {
                failure.payload = interpreted.payload;
            }

            return failure;
        }

        const payload = { ...interpreted.payload } as Record<string, unknown>;
        const actorName = coerceString(payload.actor_name) ?? coerceString(payload.actorName);
        const vertices = coerceNumber(payload.vertices);
        const triangles = coerceNumber(payload.triangles);
        const subdivisions = coerceNumber(payload.subdivisions);
        const sizeArray = Array.isArray(payload.size)
            ? (payload.size as unknown[]).map(entry => {
                    if (typeof entry === 'number' && Number.isFinite(entry)) {
                        return entry;
                    }
                    if (typeof entry === 'string') {
                        const parsed = Number(entry);
                        return Number.isFinite(parsed) ? parsed : undefined;
                    }
                    return undefined;
                }).filter((entry): entry is number => typeof entry === 'number')
            : undefined;

        payload.success = true;
        payload.message = interpreted.message;

        if (actorName) {
            payload.actor_name = actorName;
            payload.actorName = actorName;
        }
        if (typeof vertices === 'number') {
            payload.vertices = vertices;
        }
        if (typeof triangles === 'number') {
            payload.triangles = triangles;
        }
        if (typeof subdivisions === 'number') {
            payload.subdivisions = subdivisions;
        }
        if (sizeArray && sizeArray.length === 2) {
            payload.size = sizeArray;
        }

        if (interpreted.warnings) {
            payload.warnings = interpreted.warnings;
        }
        if (interpreted.details) {
            payload.details = interpreted.details;
        }

        return payload as any;
  }

  /**
   * Create procedural foliage using ProceduralFoliageSpawner
   * Uses the documented Unreal Engine API
   */
  async createProceduralFoliage(params: {
    name: string;
    bounds: { location: [number, number, number]; size: [number, number, number] };
    foliageTypes: Array<{
      meshPath: string;
      density: number;
      minScale?: number;
      maxScale?: number;
      alignToNormal?: boolean;
      randomYaw?: boolean;
    }>;
    seed?: number;
  }) {
    const pythonScript = `
import unreal
import json

name = ${JSON.stringify(params.name)}
bounds_location = unreal.Vector(${params.bounds.location[0]}, ${params.bounds.location[1]}, ${params.bounds.location[2]})
bounds_size = unreal.Vector(${params.bounds.size[0]}, ${params.bounds.size[1]}, ${params.bounds.size[2]})
seed = ${params.seed || 12345}

result = {}

try:
    subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    
    # Validate Procedural Foliage plugin/classes are available
    if not hasattr(unreal, 'ProceduralFoliageVolume') or not hasattr(unreal, 'ProceduralFoliageSpawner'):
        raise Exception("Procedural Foliage plugin not available. Please enable the 'Procedural Foliage' plugin and try again.")
    
    # Create ProceduralFoliageVolume
    volume_actor = subsys.spawn_actor_from_class(
        unreal.ProceduralFoliageVolume,
        bounds_location,
        unreal.Rotator(0, 0, 0)
    )
    volume_actor.set_actor_label(f"{name}_ProceduralFoliageVolume")
    volume_actor.set_actor_scale3d(unreal.Vector(bounds_size.x/100.0, bounds_size.y/100.0, bounds_size.z/100.0))  # Scale is in meters
    
    # Get the procedural component
    proc_comp = volume_actor.procedural_component
    if not proc_comp:
        proc_comp = volume_actor.get_component_by_class(unreal.ProceduralFoliageComponent)
    
    if proc_comp:
        # Create ProceduralFoliageSpawner asset
        spawner_path = f"/Game/Foliage/Spawners/{name}_Spawner"
        package_path = "/Game/Foliage/Spawners"
        
        # Ensure directory exists
        if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
            unreal.EditorAssetLibrary.make_directory(package_path)
        
        # Create spawner
        spawner = None
        if unreal.EditorAssetLibrary.does_asset_exist(spawner_path):
            spawner = unreal.EditorAssetLibrary.load_asset(spawner_path)
        else:
            # Create new spawner
            factory = unreal.ProceduralFoliageSpawnerFactory()
            spawner = asset_tools.create_asset(
                asset_name=f"{name}_Spawner",
                package_path=package_path,
                asset_class=unreal.ProceduralFoliageSpawner,
                factory=factory
            )
        
        if spawner:
            # Configure spawner (use set_editor_property for read-only attributes)
            spawner.set_editor_property('random_seed', seed)
            spawner.set_editor_property('tile_size', max(bounds_size.x, bounds_size.y))
            
            # Create foliage types
            foliage_types = []
            ft_input = json.loads(r'''${JSON.stringify(params.foliageTypes)}''')
            for ft_params in ft_input:
                # Load mesh
                mesh = unreal.EditorAssetLibrary.load_asset(ft_params['meshPath'])
                if mesh:
                    # Create FoliageTypeObject
                    ft_obj = unreal.FoliageTypeObject()
                    
                    # Try to create or load FoliageType_InstancedStaticMesh
                    ft_asset_name = f"FT_{name}_{len(foliage_types)}"
                    ft_asset_path = f"/Game/Foliage/Types/{ft_asset_name}"
                    
                    ft_asset = None
                    if unreal.EditorAssetLibrary.does_asset_exist(ft_asset_path):
                        ft_asset = unreal.EditorAssetLibrary.load_asset(ft_asset_path)
                    else:
                        # Create simple foliage type
                        ft_asset = unreal.FoliageType_InstancedStaticMesh()
                    
                    if ft_asset:
                        # Configure foliage type (use set_editor_property)
                        ft_asset.set_editor_property('mesh', mesh)
                        ft_asset.set_editor_property('density', ft_params.get('density', 1.0))
                        ft_asset.set_editor_property('random_yaw', ft_params.get('randomYaw', True))
                        ft_asset.set_editor_property('align_to_normal', ft_params.get('alignToNormal', True))
                        
                        min_scale = ft_params.get('minScale', 0.8)
                        max_scale = ft_params.get('maxScale', 1.2)
                        ft_asset.set_editor_property('scale_x', unreal.FloatInterval(min_scale, max_scale))
                        ft_asset.set_editor_property('scale_y', unreal.FloatInterval(min_scale, max_scale))
                        ft_asset.set_editor_property('scale_z', unreal.FloatInterval(min_scale, max_scale))
                        
                        ft_obj.set_editor_property('foliage_type_object', ft_asset)
                        foliage_types.append(ft_obj)
            
            # Set foliage types on spawner
            spawner.set_editor_property('foliage_types', foliage_types)
            
            # Assign spawner to component
            proc_comp.set_editor_property('foliage_spawner', spawner)
            
            # Save spawner asset
            unreal.EditorAssetLibrary.save_asset(spawner.get_path_name())
            
            # Resimulate
            try:
                unreal.ProceduralFoliageEditorLibrary.resimulate_procedural_foliage_volumes([volume_actor])
                result['resimulated'] = True
            except:
                # Manual simulation
                spawner.simulate(num_steps=-1)
                result['resimulated'] = False
                result['note'] = 'Used manual simulation'
            
            result['success'] = True
            result['message'] = f"Created procedural foliage volume '{name}'"
            result['volume_actor'] = volume_actor.get_actor_label()
            result['spawner_path'] = spawner.get_path_name()
            result['foliage_types_count'] = len(foliage_types)
        else:
            result['success'] = False
            result['error'] = 'Could not create ProceduralFoliageSpawner'
    else:
        result['success'] = False
        result['error'] = 'Could not get ProceduralFoliageComponent'
        
except Exception as e:
    result['success'] = False
    result['error'] = str(e)

print(f"RESULT:{json.dumps(result)}")
`.trim();

    const allowPythonFallback = allowPythonFallbackFromEnv();
    const response = await (this.bridge as any).executeEditorPython(pythonScript, { allowPythonFallback });
        const interpreted = interpretStandardResult(response, {
            successMessage: `Created procedural foliage volume '${params.name}'`,
            failureMessage: `Failed to create procedural foliage volume '${params.name}'`
        });

        if (!interpreted.success) {
            const failure: {
                success: false;
                error: string;
                message: string;
                warnings?: string[];
                details?: string[];
                payload?: Record<string, unknown>;
            } = {
                success: false,
                error: interpreted.error ?? interpreted.message,
                message: interpreted.message
            };

            if (interpreted.warnings) {
                failure.warnings = interpreted.warnings;
            }
            if (interpreted.details) {
                failure.details = interpreted.details;
            }
            if (interpreted.payload && Object.keys(interpreted.payload).length > 0) {
                failure.payload = interpreted.payload;
            }

            return failure;
        }

        const payload = { ...interpreted.payload } as Record<string, unknown>;
        const volumeActor = coerceString(payload.volume_actor) ?? coerceString(payload.volumeActor);
        const spawnerPath = coerceString(payload.spawner_path) ?? coerceString(payload.spawnerPath);
        const foliageCount = coerceNumber(payload.foliage_types_count) ?? coerceNumber(payload.foliageTypesCount);
        const resimulated = coerceBoolean(payload.resimulated);
        const note = coerceString(payload.note);
        const messages = coerceStringArray(payload.messages);

        payload.success = true;
        payload.message = interpreted.message;

        if (volumeActor) {
            payload.volume_actor = volumeActor;
            payload.volumeActor = volumeActor;
        }
        if (spawnerPath) {
            payload.spawner_path = spawnerPath;
            payload.spawnerPath = spawnerPath;
        }
        if (typeof foliageCount === 'number') {
            payload.foliage_types_count = foliageCount;
            payload.foliageTypesCount = foliageCount;
        }
        if (typeof resimulated === 'boolean') {
            payload.resimulated = resimulated;
        }
        if (note) {
            payload.note = note;
        }
        if (messages && messages.length > 0) {
            payload.messages = messages;
        }

        if (interpreted.warnings) {
            payload.warnings = interpreted.warnings;
        }
        if (interpreted.details) {
            payload.details = interpreted.details;
        }

        return payload as any;
  }

  /**
   * Add foliage instances using InstancedFoliageActor
   * Direct instance placement approach
   */
  async addFoliageInstances(params: {
    foliageType: string; // Path to FoliageType or mesh
    transforms: Array<{
      location: [number, number, number];
      rotation?: [number, number, number];
      scale?: [number, number, number];
    }>;
  }) {
    const pythonScript = `
import unreal
import json

foliage_type_path = ${JSON.stringify(params.foliageType)}
transforms_data = ${JSON.stringify(params.transforms)}

result = {}

try:
    # Get world context
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_subsystem.get_editor_world()
    
    # Load foliage type or mesh
    foliage_asset = unreal.EditorAssetLibrary.load_asset(foliage_type_path)
    
    if foliage_asset:
        # Prepare transforms
        transforms = []
        for t_data in transforms_data:
            location = unreal.Vector(t_data['location'][0], t_data['location'][1], t_data['location'][2])
            rotation = unreal.Rotator(
                t_data.get('rotation', [0, 0, 0])[0],
                t_data.get('rotation', [0, 0, 0])[1],
                t_data.get('rotation', [0, 0, 0])[2]
            )
            scale = unreal.Vector(
                t_data.get('scale', [1, 1, 1])[0],
                t_data.get('scale', [1, 1, 1])[1],
                t_data.get('scale', [1, 1, 1])[2]
            )
            
            transform = unreal.Transform(location, rotation, scale)
            transforms.append(transform)
        
        # Add instances using InstancedFoliageActor
        unreal.InstancedFoliageActor.add_instances(
            world,
            foliage_asset,
            transforms
        )
        
        result['success'] = True
        result['message'] = f"Added {len(transforms)} foliage instances"
        result['instances_count'] = len(transforms)
    else:
        result['success'] = False
        result['error'] = f"Could not load foliage asset: {foliage_type_path}"
        
except Exception as e:
    result['success'] = False
    result['error'] = str(e)

print(f"RESULT:{json.dumps(result)}")
`.trim();

    const allowPythonFallback = allowPythonFallbackFromEnv();
    const response = await (this.bridge as any).executeEditorPython(pythonScript, { allowPythonFallback });
        const interpreted = interpretStandardResult(response, {
            successMessage: 'Foliage instances added',
            failureMessage: 'Failed to add foliage instances'
        });

        if (!interpreted.success) {
            const failure: {
                success: false;
                error: string;
                message: string;
                warnings?: string[];
                details?: string[];
                payload?: Record<string, unknown>;
            } = {
                success: false,
                error: interpreted.error ?? interpreted.message,
                message: interpreted.message
            };

            if (interpreted.warnings) {
                failure.warnings = interpreted.warnings;
            }
            if (interpreted.details) {
                failure.details = interpreted.details;
            }
            if (interpreted.payload && Object.keys(interpreted.payload).length > 0) {
                failure.payload = interpreted.payload;
            }

            return failure;
        }

        const payload = { ...interpreted.payload } as Record<string, unknown>;
        const count = coerceNumber(payload.instances_count) ?? coerceNumber(payload.instancesCount);
        const message = coerceString(payload.message) ?? interpreted.message;

        payload.success = true;
        payload.message = message;

        if (typeof count === 'number') {
            payload.instances_count = count;
            payload.instancesCount = count;
        }

        if (interpreted.warnings) {
            payload.warnings = interpreted.warnings;
        }
        if (interpreted.details) {
            payload.details = interpreted.details;
        }

        return payload as any;
  }

  /**
   * Create landscape grass type for automatic foliage on landscape
   */
  async createLandscapeGrassType(params: {
    name: string;
    meshPath: string;
    density?: number;
    minScale?: number;
    maxScale?: number;
  }) {
    const pythonScript = `
import unreal
import json

name = ${JSON.stringify(params.name)}
mesh_path = ${JSON.stringify(params.meshPath)}
density = ${params.density || 1.0}
min_scale = ${params.minScale || 0.8}
max_scale = ${params.maxScale || 1.2}

result = {}

try:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    
    # Create directory
    package_path = "/Game/Landscape/GrassTypes"
    if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
        unreal.EditorAssetLibrary.make_directory(package_path)
    
    # Create LandscapeGrassType
    grass_type_path = f"{package_path}/{name}"
    
    if not unreal.EditorAssetLibrary.does_asset_exist(grass_type_path):
        # Create using factory
        factory = unreal.LandscapeGrassTypeFactory()
        grass_type = asset_tools.create_asset(
            asset_name=name,
            package_path=package_path,
            asset_class=unreal.LandscapeGrassType,
            factory=factory
        )
        
        if grass_type:
            # Load mesh
            mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
            if mesh:
                # Configure grass type (use set_editor_property)
                grass_variety = unreal.GrassVariety()
                grass_variety.set_editor_property('grass_mesh', mesh)
                # GrassDensity is PerPlatformFloat in UE5+; set via struct instance
                pp_density = unreal.PerPlatformFloat()
                pp_density.set_editor_property('Default', float(density * 100.0))
                grass_variety.set_editor_property('grass_density', pp_density)
                grass_variety.set_editor_property('use_grid', True)
                grass_variety.set_editor_property('placement_jitter', 1.0)
                # Set cull distances as PerPlatformInt and LOD as int (engine uses mixed types here)
                pp_start = unreal.PerPlatformInt()
                pp_start.set_editor_property('Default', 10000)
                grass_variety.set_editor_property('start_cull_distance', pp_start)
                pp_end = unreal.PerPlatformInt()
                pp_end.set_editor_property('Default', 20000)
                grass_variety.set_editor_property('end_cull_distance', pp_end)
                grass_variety.set_editor_property('min_lod', -1)
                grass_variety.set_editor_property('scaling', unreal.GrassScaling.UNIFORM)
                grass_variety.set_editor_property('scale_x', unreal.FloatInterval(min_scale, max_scale))
                grass_variety.set_editor_property('scale_y', unreal.FloatInterval(min_scale, max_scale))
                grass_variety.set_editor_property('scale_z', unreal.FloatInterval(min_scale, max_scale))
                grass_variety.set_editor_property('random_rotation', True)
                grass_variety.set_editor_property('align_to_surface', True)
                
                grass_type.set_editor_property('grass_varieties', [grass_variety])
                
                # Save asset
                unreal.EditorAssetLibrary.save_asset(grass_type.get_path_name())
                
                result['success'] = True
                result['message'] = f"Created landscape grass type '{name}'"
                result['asset_path'] = grass_type.get_path_name()
            else:
                result['success'] = False
                result['error'] = f"Could not load mesh: {mesh_path}"
        else:
            result['success'] = False
            result['error'] = "Could not create LandscapeGrassType"
    else:
        result['success'] = False
        result['error'] = f"Grass type already exists: {grass_type_path}"
        
except Exception as e:
    result['success'] = False
    result['error'] = str(e)

print(f"RESULT:{json.dumps(result)}")
`.trim();

    const allowPythonFallback = allowPythonFallbackFromEnv();
    const response = await (this.bridge as any).executeEditorPython(pythonScript, { allowPythonFallback });
        const interpreted = interpretStandardResult(response, {
            successMessage: `Created landscape grass type '${params.name}'`,
            failureMessage: `Failed to create landscape grass type '${params.name}'`
        });

        if (!interpreted.success) {
            const failure: {
                success: false;
                error: string;
                message: string;
                warnings?: string[];
                details?: string[];
                payload?: Record<string, unknown>;
            } = {
                success: false,
                error: interpreted.error ?? interpreted.message,
                message: interpreted.message
            };

            if (interpreted.warnings) {
                failure.warnings = interpreted.warnings;
            }
            if (interpreted.details) {
                failure.details = interpreted.details;
            }
            if (interpreted.payload && Object.keys(interpreted.payload).length > 0) {
                failure.payload = interpreted.payload;
            }

            return failure;
        }

        const payload = { ...interpreted.payload } as Record<string, unknown>;
        const assetPath = coerceString(payload.asset_path) ?? coerceString(payload.assetPath);
        const note = coerceString(payload.note);
        const messages = coerceStringArray(payload.messages);

        payload.success = true;
        payload.message = interpreted.message;

        if (assetPath) {
            payload.asset_path = assetPath;
            payload.assetPath = assetPath;
        }
        if (note) {
            payload.note = note;
        }
        if (messages && messages.length > 0) {
            payload.messages = messages;
        }

        if (interpreted.warnings) {
            payload.warnings = interpreted.warnings;
        }
        if (interpreted.details) {
            payload.details = interpreted.details;
        }

        return payload as any;
  }
}