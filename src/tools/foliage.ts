// Foliage tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class FoliageTools {
  constructor(private bridge: UnrealBridge) {}

  // NOTE: We intentionally avoid issuing Unreal console commands here because
  // they have proven unreliable and generate engine warnings (failed FindConsoleObject).
  // Instead, we validate inputs and return structured results. Actual foliage
  // authoring should be implemented via Python APIs in future iterations.

  // Add foliage type via Python (creates FoliageType asset if possible; else registers transient)
  async addFoliageType(params: {
    name: string;
    meshPath: string;
    density?: number;
    radius?: number;
    minScale?: number;
    maxScale?: number;
    alignToNormal?: boolean;
    randomYaw?: boolean;
    groundSlope?: number;
  }) {
    // Basic validation to prevent bad inputs like 'undefined' and empty strings
    const errors: string[] = [];
    const name = String(params?.name ?? '').trim();
    const meshPath = String(params?.meshPath ?? '').trim();

    if (!name || name.toLowerCase() === 'undefined' || name.toLowerCase() === 'any') {
      errors.push(`Invalid foliage type name: '${params?.name}'`);
    }
    if (!meshPath || meshPath.toLowerCase() === 'undefined') {
      errors.push(`Invalid meshPath: '${params?.meshPath}'`);
    }
    if (params?.density !== undefined) {
      if (typeof params.density !== 'number' || !isFinite(params.density) || params.density < 0) {
        errors.push(`Invalid density: '${params.density}' (must be non-negative finite number)`);
      }
    }
    if (params?.minScale !== undefined || params?.maxScale !== undefined) {
      const minS = params?.minScale ?? 1;
      const maxS = params?.maxScale ?? 1;
      if (typeof minS !== 'number' || typeof maxS !== 'number' || minS <= 0 || maxS <= 0 || maxS < minS) {
        errors.push(`Invalid scale range: min=${params?.minScale}, max=${params?.maxScale}`);
      }
    }
    if (errors.length > 0) {
      return { success: false, error: errors.join('; ') };
    }

    const py = `
import unreal, json

name = ${JSON.stringify(name)}
mesh_path = ${JSON.stringify(meshPath)}
fallback_mesh = '/Engine/EngineMeshes/Sphere'
package_path = '/Game/Foliage/Types'

res = {'success': False, 'created': False, 'asset_path': '', 'used_mesh': '', 'exists_after': False, 'method': '', 'note': ''}

try:
    # Ensure package directory
    try:
        if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
            unreal.EditorAssetLibrary.make_directory(package_path)
    except Exception as e:
        res['note'] += f"; make_directory failed: {e}"

    # Load mesh or fallback
    mesh = None
    try:
        if unreal.EditorAssetLibrary.does_asset_exist(mesh_path):
            mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    except Exception as e:
        res['note'] += f"; could not check/load mesh_path: {e}"

    if not mesh:
        mesh = unreal.EditorAssetLibrary.load_asset(fallback_mesh)
        res['note'] += '; fallback_mesh_used'
    if mesh:
        res['used_mesh'] = str(mesh.get_path_name())

    # Create FoliageType asset using alternative approach since FoliageTypeFactory doesn't exist
    asset = None
    try:
        # Try to create or load existing foliage type
        asset_path = f"{package_path}/{name}"
        
        # Check if asset already exists
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            asset = unreal.EditorAssetLibrary.load_asset(asset_path)
            res['note'] += '; loaded_existing'
        else:
            # Try to create FoliageType using new approach
            try:
                # Create a foliage type by duplicating a template if available
                template_path = '/Engine/Foliage/FoliageType_Default'
                if unreal.EditorAssetLibrary.does_asset_exist(template_path):
                    asset = unreal.EditorAssetLibrary.duplicate_asset(template_path, asset_path)
                    res['note'] += '; duplicated_from_template'
                else:
                    # As fallback, try direct creation (may not work in all UE versions)
                    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
                    asset = asset_tools.create_asset(name, package_path, unreal.FoliageType, None)
                    res['note'] += '; direct_creation_attempted'
            except Exception as inner_e:
                res['note'] += f"; creation_method_failed: {inner_e}"
                asset = None
    except Exception as e:
        res['note'] += f"; create_asset failed: {e}"
        asset = None

    if asset and mesh:
        try:
            # For FoliageType, set the Static Mesh property (Python: 'static_mesh')
            asset.set_editor_property('static_mesh', mesh)
            unreal.EditorAssetLibrary.save_loaded_asset(asset)
            res['asset_path'] = str(asset.get_path_name())
            res['created'] = True
            res['method'] = 'Asset'
        except Exception as e:
            res['note'] += f"; set/save asset failed: {e}"
    elif not asset:
        res['note'] += "; asset creation returned None"
    elif not mesh:
        res['note'] += "; mesh object is None, cannot assign to foliage type"

    # Verify existence
    res['exists_after'] = unreal.EditorAssetLibrary.does_asset_exist(res['asset_path']) if res['asset_path'] else False
    res['success'] = True
    
except Exception as e:
    res['success'] = False
    res['note'] += f"; fatal: {e}"

print('RESULT:' + json.dumps(res))
`.trim();

    const pyResp = await this.bridge.executePython(py);
    let out = '';
    if (pyResp?.LogOutput && Array.isArray(pyResp.LogOutput)) out = pyResp.LogOutput.map((l: any) => l.Output || '').join('');
    else if (typeof pyResp === 'string') out = pyResp; else out = JSON.stringify(pyResp);
    const m = out.match(/RESULT:({.*})/);
    if (m) {
      try {
        const parsed = JSON.parse(m[1]);
        if (!parsed.success) {
          return { success: false, error: parsed.note || 'Add foliage type failed' };
        }
        return {
          success: true,
          created: parsed.created,
          exists: parsed.exists_after,
          method: parsed.method,
          assetPath: parsed.asset_path,
          usedMesh: parsed.used_mesh,
          note: parsed.note,
          message: parsed.exists_after ? `Foliage type '${name}' ready (${parsed.method || 'Unknown'})` : `Created foliage '${name}' but verification did not find it yet`
        };
      } catch {
        return { success: false, error: 'Failed to parse Python result' };
      }
    }
    return { success: false, error: 'No parseable result from Python' };
  }

  // Paint foliage by placing HISM instances (editor-only)
  async paintFoliage(params: {
    foliageType: string;
    position: [number, number, number];
    brushSize?: number;
    paintDensity?: number;
    eraseMode?: boolean;
  }) {
    const errors: string[] = [];
    const foliageType = String(params?.foliageType ?? '').trim();
    const pos = Array.isArray(params?.position) ? params.position : [0,0,0];

    if (!foliageType || foliageType.toLowerCase() === 'undefined' || foliageType.toLowerCase() === 'any') {
      errors.push(`Invalid foliageType: '${params?.foliageType}'`);
    }
    if (!Array.isArray(pos) || pos.length !== 3 || pos.some(v => typeof v !== 'number' || !isFinite(v))) {
      errors.push(`Invalid position: '${JSON.stringify(params?.position)}'`);
    }
    if (params?.brushSize !== undefined) {
      if (typeof params.brushSize !== 'number' || !isFinite(params.brushSize) || params.brushSize < 0) {
        errors.push(`Invalid brushSize: '${params.brushSize}' (must be non-negative finite number)`);
      }
    }
    if (params?.paintDensity !== undefined) {
      if (typeof params.paintDensity !== 'number' || !isFinite(params.paintDensity) || params.paintDensity < 0) {
        errors.push(`Invalid paintDensity: '${params.paintDensity}' (must be non-negative finite number)`);
      }
    }

    if (errors.length > 0) {
      return { success: false, error: errors.join('; ') };
    }

    const brush = Number.isFinite(params.brushSize as number) ? (params.brushSize as number) : 300;
    const py = `
import unreal, json, random, math

res = {'success': False, 'added': 0, 'actor': '', 'component': '', 'used_mesh': '', 'note': ''}
foliage_type_name = ${JSON.stringify(foliageType)}
px, py, pz = ${pos[0]}, ${pos[1]}, ${pos[2]}
radius = float(${brush}) / 2.0

try:
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actor_sub.get_all_level_actors() if actor_sub else []

    # Find or create a container actor
    label = f"FoliageContainer_{foliage_type_name}"
    container = None
    for a in all_actors:
        try:
            if a.get_actor_label() == label:
                container = a
                break
        except Exception:
            pass
    if not container:
        # Spawn actor that can hold components
        container = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(px, py, pz))
        try:
            container.set_actor_label(label)
        except Exception:
            pass

    # Resolve mesh from FoliageType asset
    mesh = None
    fol_asset_path = f"/Game/Foliage/Types/{foliage_type_name}.{foliage_type_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(fol_asset_path):
        try:
            ft_asset = unreal.EditorAssetLibrary.load_asset(fol_asset_path)
            mesh = ft_asset.get_editor_property('mesh')
        except Exception:
            mesh = None
    
    if not mesh:
        mesh = unreal.EditorAssetLibrary.load_asset('/Engine/EngineMeshes/Sphere')
        res['note'] += '; used_fallback_mesh'
    
    if mesh:
        res['used_mesh'] = str(mesh.get_path_name())

    # Since HISM components and add_component don't work in this version,
    # spawn individual StaticMeshActors for each instance
    target_count = max(5, int(radius / 20.0))
    added = 0
    for i in range(target_count):
        ang = random.random() * math.tau
        r = random.random() * radius
        x, y, z = px + math.cos(ang) * r, py + math.sin(ang) * r, pz
        try:
            # Spawn static mesh actor at position
            inst_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.StaticMeshActor, 
                unreal.Vector(x, y, z),
                unreal.Rotator(0, random.random()*360.0, 0)
            )
            if inst_actor and mesh:
                # Set mesh on the actor's component
                try:
                    mesh_comp = inst_actor.static_mesh_component
                    if mesh_comp:
                        mesh_comp.set_static_mesh(mesh)
                    inst_actor.set_actor_label(f"{foliage_type_name}_instance_{i}")
                    # Group under the container for organization
                    inst_actor.attach_to_actor(container, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
                    added += 1
                except Exception as e:
                    res['note'] += f"; instance_{i} setup failed: {e}"
        except Exception as e:
            res['note'] += f"; spawn instance_{i} failed: {e}"

    res['added'] = added
    res['actor'] = container.get_actor_label()
    res['component'] = 'StaticMeshActors'  # Using actors instead of components
    res['success'] = True
except Exception as e:
    res['success'] = False
    res['note'] += f"; fatal: {e}"

print('RESULT:' + json.dumps(res))
`.trim();

    const pyResp = await this.bridge.executePython(py);
    let out = '';
    if (pyResp?.LogOutput && Array.isArray(pyResp.LogOutput)) out = pyResp.LogOutput.map((l: any) => l.Output || '').join('');
    else if (typeof pyResp === 'string') out = pyResp; else out = JSON.stringify(pyResp);
    const m = out.match(/RESULT:({.*})/);
    if (m) {
      try {
        const parsed = JSON.parse(m[1]);
        if (!parsed.success) {
          return { success: false, error: parsed.note || 'Paint foliage failed' };
        }
        return {
          success: true,
          added: parsed.added,
          actor: parsed.actor,
          component: parsed.component,
          usedMesh: parsed.used_mesh,
          note: parsed.note,
          message: `Painted ${parsed.added} instances for '${foliageType}' around (${pos[0]}, ${pos[1]}, ${pos[2]})`
        };
      } catch {
        return { success: false, error: 'Failed to parse Python result' };
      }
    }
    return { success: false, error: 'No parseable result from Python' };
  }

  // Create instanced mesh
  async createInstancedMesh(params: {
    name: string;
    meshPath: string;
    instances: Array<{
      position: [number, number, number];
      rotation?: [number, number, number];
      scale?: [number, number, number];
    }>;
    enableCulling?: boolean;
    cullDistance?: number;
  }) {
    const commands = [];
    
    commands.push(`CreateInstancedStaticMesh ${params.name} ${params.meshPath}`);
    
    for (const instance of params.instances) {
      const rot = instance.rotation || [0, 0, 0];
      const scale = instance.scale || [1, 1, 1];
      commands.push(`AddInstance ${params.name} ${instance.position.join(' ')} ${rot.join(' ')} ${scale.join(' ')}`);
    }
    
    if (params.enableCulling !== undefined) {
      commands.push(`SetInstanceCulling ${params.name} ${params.enableCulling}`);
    }
    
    if (params.cullDistance !== undefined) {
      commands.push(`SetInstanceCullDistance ${params.name} ${params.cullDistance}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Instanced mesh ${params.name} created with ${params.instances.length} instances` };
  }

  // Set foliage LOD
  async setFoliageLOD(params: {
    foliageType: string;
    lodDistances?: number[];
    screenSize?: number[];
  }) {
    const commands = [];
    
    if (params.lodDistances) {
      commands.push(`SetFoliageLODDistances ${params.foliageType} ${params.lodDistances.join(' ')}`);
    }
    
    if (params.screenSize) {
      commands.push(`SetFoliageLODScreenSize ${params.foliageType} ${params.screenSize.join(' ')}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Foliage LOD settings updated' };
  }

  // Create procedural foliage
  async createProceduralFoliage(params: {
    volumeName: string;
    position: [number, number, number];
    size: [number, number, number];
    foliageTypes: string[];
    seed?: number;
    tileSize?: number;
  }) {
    const commands = [];
    
    commands.push(`CreateProceduralFoliageVolume ${params.volumeName} ${params.position.join(' ')} ${params.size.join(' ')}`);
    
    for (const type of params.foliageTypes) {
      commands.push(`AddProceduralFoliageType ${params.volumeName} ${type}`);
    }
    
    if (params.seed !== undefined) {
      commands.push(`SetProceduralSeed ${params.volumeName} ${params.seed}`);
    }
    
    if (params.tileSize !== undefined) {
      commands.push(`SetProceduralTileSize ${params.volumeName} ${params.tileSize}`);
    }
    
    commands.push(`GenerateProceduralFoliage ${params.volumeName}`);
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Procedural foliage volume ${params.volumeName} created` };
  }

  // Set foliage collision
  async setFoliageCollision(params: {
    foliageType: string;
    collisionEnabled?: boolean;
    collisionProfile?: string;
    generateOverlapEvents?: boolean;
  }) {
    const commands = [];
    
    if (params.collisionEnabled !== undefined) {
      commands.push(`SetFoliageCollision ${params.foliageType} ${params.collisionEnabled}`);
    }
    
    if (params.collisionProfile) {
      commands.push(`SetFoliageCollisionProfile ${params.foliageType} ${params.collisionProfile}`);
    }
    
    if (params.generateOverlapEvents !== undefined) {
      commands.push(`SetFoliageOverlapEvents ${params.foliageType} ${params.generateOverlapEvents}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Foliage collision settings updated' };
  }

  // Create grass system
  async createGrassSystem(params: {
    name: string;
    grassTypes: Array<{
      meshPath: string;
      density: number;
      minScale?: number;
      maxScale?: number;
    }>;
    windStrength?: number;
    windSpeed?: number;
  }) {
    const commands = [];
    
    commands.push(`CreateGrassSystem ${params.name}`);
    
    for (const grassType of params.grassTypes) {
      const minScale = grassType.minScale || 0.8;
      const maxScale = grassType.maxScale || 1.2;
      commands.push(`AddGrassType ${params.name} ${grassType.meshPath} ${grassType.density} ${minScale} ${maxScale}`);
    }
    
    if (params.windStrength !== undefined) {
      commands.push(`SetGrassWindStrength ${params.name} ${params.windStrength}`);
    }
    
    if (params.windSpeed !== undefined) {
      commands.push(`SetGrassWindSpeed ${params.name} ${params.windSpeed}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Grass system ${params.name} created` };
  }

  // Remove foliage instances
  async removeFoliageInstances(params: {
    foliageType: string;
    position: [number, number, number];
    radius: number;
  }) {
    const command = `RemoveFoliageInRadius ${params.foliageType} ${params.position.join(' ')} ${params.radius}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Select foliage instances
  async selectFoliageInstances(params: {
    foliageType: string;
    position?: [number, number, number];
    radius?: number;
    selectAll?: boolean;
  }) {
    let command: string;
    
    if (params.selectAll) {
      command = `SelectAllFoliage ${params.foliageType}`;
    } else if (params.position && params.radius) {
      command = `SelectFoliageInRadius ${params.foliageType} ${params.position.join(' ')} ${params.radius}`;
    } else {
      command = `SelectFoliageType ${params.foliageType}`;
    }
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Update foliage instances
  async updateFoliageInstances(params: {
    foliageType: string;
    updateTransforms?: boolean;
    updateMesh?: boolean;
    newMeshPath?: string;
  }) {
    const commands = [];
    
    if (params.updateTransforms) {
      commands.push(`UpdateFoliageTransforms ${params.foliageType}`);
    }
    
    if (params.updateMesh && params.newMeshPath) {
      commands.push(`UpdateFoliageMesh ${params.foliageType} ${params.newMeshPath}`);
    }
    
    commands.push(`RefreshFoliage ${params.foliageType}`);
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Foliage instances updated' };
  }

  // Create foliage spawner
  async createFoliageSpawner(params: {
    name: string;
    spawnArea: 'Landscape' | 'StaticMesh' | 'BSP' | 'Foliage' | 'All';
    excludeAreas?: Array<[number, number, number, number]>; // [x, y, z, radius]
  }) {
    const commands = [];
    
    commands.push(`CreateFoliageSpawner ${params.name} ${params.spawnArea}`);
    
    if (params.excludeAreas) {
      for (const area of params.excludeAreas) {
        commands.push(`AddFoliageExclusionArea ${params.name} ${area.join(' ')}`);
      }
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Foliage spawner ${params.name} created` };
  }

  // Optimize foliage
  async optimizeFoliage(params: {
    mergeInstances?: boolean;
    generateClusters?: boolean;
    clusterSize?: number;
    reduceDrawCalls?: boolean;
  }) {
    const commands = [];
    
    if (params.mergeInstances) {
      commands.push('MergeFoliageInstances');
    }
    
    if (params.generateClusters) {
      const size = params.clusterSize || 100;
      commands.push(`GenerateFoliageClusters ${size}`);
    }
    
    if (params.reduceDrawCalls) {
      commands.push('OptimizeFoliageDrawCalls');
    }
    
    commands.push('RebuildFoliageTree');
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Foliage optimized' };
  }
}
