import { UnrealBridge } from '../unreal-bridge.js';
import { validateAssetParams, resolveSkeletalMeshPath, concurrencyDelay } from '../utils/validation.js';
import { bestEffortInterpretedText, coerceString, coerceStringArray, interpretStandardResult } from '../utils/result-helpers.js';

export class PhysicsTools {
  constructor(private bridge: UnrealBridge) {}
  
  /**
   * Helper to find a valid skeletal mesh in the project
   */
  private async findValidSkeletalMesh(): Promise<string | null> {
    const pythonScript = `
import unreal
import json

result = {
    'success': False,
    'meshPath': None,
    'source': None
}

common_paths = [
  '/Game/Characters/Mannequins/Meshes/SKM_Manny',
  '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
  '/Game/Characters/Mannequins/Meshes/SKM_Manny_Complex',
  '/Game/Characters/Mannequins/Meshes/SKM_Quinn',
  '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple',
  '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Complex'
]

for candidate in common_paths:
    if unreal.EditorAssetLibrary.does_asset_exist(candidate):
        mesh = unreal.EditorAssetLibrary.load_asset(candidate)
        if mesh and isinstance(mesh, unreal.SkeletalMesh):
            result['success'] = True
            result['meshPath'] = candidate
            result['source'] = 'common'
            break

if not result['success']:
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = asset_registry.get_assets_by_class('SkeletalMesh', search_sub_classes=False)
    if assets:
        first_mesh = assets[0]
        obj_path = first_mesh.get_editor_property('object_path') if hasattr(first_mesh, 'get_editor_property') else None
        if not obj_path and hasattr(first_mesh, 'object_path'):
            obj_path = first_mesh.object_path
        if obj_path:
            result['success'] = True
            result['meshPath'] = str(obj_path).split('.')[0]
            result['source'] = 'registry'
            if hasattr(first_mesh, 'asset_name'):
                result['assetName'] = str(first_mesh.asset_name)

if not result['success']:
    result['fallback'] = '/Engine/EngineMeshes/SkeletalCube'

print('RESULT:' + json.dumps(result))
`;

    try {
      const response = await this.bridge.executePython(pythonScript);
      const interpreted = interpretStandardResult(response, {
        successMessage: 'Skeletal mesh discovery complete',
        failureMessage: 'Failed to discover skeletal mesh'
      });

      if (interpreted.success) {
        const meshPath = coerceString(interpreted.payload.meshPath);
        if (meshPath) {
          return meshPath;
        }
      }

      const fallback = coerceString(interpreted.payload.fallback);
      if (fallback) {
        return fallback;
      }

      const detail = bestEffortInterpretedText(interpreted);
      if (detail) {
        console.error('Failed to parse skeletal mesh discovery:', detail);
      }
    } catch (error) {
      console.error('Failed to find skeletal mesh:', error);
    }
    
    return '/Engine/EngineMeshes/SkeletalCube';
  }

  /**
   * Setup Ragdoll Physics
   * NOTE: Requires a valid skeletal mesh to create physics asset
   * @param skeletonPath - Path to an existing skeletal mesh asset (required)
   * @param physicsAssetName - Name for the new physics asset
   * @param savePath - Directory to save the asset (default: /Game/Physics)
   */
  async setupRagdoll(params: {
    skeletonPath: string;
    physicsAssetName: string;
    savePath?: string;
    blendWeight?: number;
    constraints?: Array<{
      boneName: string;
      constraintType: 'Fixed' | 'Limited' | 'Free';
      limits?: {
        swing1?: number;
        swing2?: number;
        twist?: number;
      };
    }>;
  }) {
    try {
      // Strong validation for physics asset name
      if (!params.physicsAssetName || params.physicsAssetName.trim() === '') {
        return {
          success: false,
          message: 'Failed to setup ragdoll: Name cannot be empty',
          error: 'Name cannot be empty'
        };
      }
      
      // Check for invalid characters in name
      if (params.physicsAssetName.includes('@') || params.physicsAssetName.includes('#') || 
          params.physicsAssetName.includes('$') || params.physicsAssetName.includes('%')) {
        return {
          success: false,
          message: 'Failed to setup ragdoll: Name contains invalid characters',
          error: 'Name contains invalid characters'
        };
      }
      
      // Check if skeleton path is provided instead of skeletal mesh
      if (params.skeletonPath && (params.skeletonPath.includes('_Skeleton') || 
          params.skeletonPath.includes('SK_Mannequin') && !params.skeletonPath.includes('SKM_'))) {
        return {
          success: false,
          message: 'Failed to setup ragdoll: Must specify a valid skeletal mesh',
          error: 'Must specify a valid skeletal mesh, not a skeleton'
        };
      }
      
      // Validate and sanitize parameters
      const validation = validateAssetParams({
        name: params.physicsAssetName,
        savePath: params.savePath || '/Game/Physics'
      });
      
      if (!validation.valid) {
        return {
          success: false,
          message: `Failed to setup ragdoll: ${validation.error}`,
          error: validation.error
        };
      }
      
      const sanitizedParams = validation.sanitized;
      const path = sanitizedParams.savePath || '/Game/Physics';
      
      // Resolve skeletal mesh path
      let meshPath = params.skeletonPath;
      
      // Try to resolve skeleton to mesh mapping
      const resolvedPath = resolveSkeletalMeshPath(meshPath);
      if (resolvedPath && resolvedPath !== meshPath) {
        console.error(`Auto-correcting path from ${meshPath} to ${resolvedPath}`);
        meshPath = resolvedPath;
      }
      
      // Auto-resolve if it looks like a skeleton path or is empty
      if (!meshPath || meshPath.includes('_Skeleton') || meshPath === 'None' || meshPath === '') {
        console.error('Resolving skeletal mesh path...');
        const resolvedMesh = await this.findValidSkeletalMesh();
        if (resolvedMesh) {
          meshPath = resolvedMesh;
          console.error(`Using resolved skeletal mesh: ${meshPath}`);
        }
      }
      
      // Add concurrency delay to prevent race conditions
      await concurrencyDelay();
      
      // IMPORTANT: Physics assets require a SKELETAL MESH, not a skeleton
      // UE5 uses: /Game/Characters/Mannequins/Meshes/SKM_Manny_Simple or SKM_Quinn_Simple
      // UE4 used: /Game/Mannequin/Character/Mesh/SK_Mannequin (which no longer exists)
      // Fallback: /Engine/EngineMeshes/SkeletalCube
      
      // Common skeleton paths that should be replaced with actual skeletal mesh paths
      const skeletonToMeshMap: { [key: string]: string } = {
        '/Game/Mannequin/Character/Mesh/UE4_Mannequin_Skeleton': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
        '/Game/Characters/Mannequins/Meshes/SK_Mannequin': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
        '/Game/Mannequin/Character/Mesh/SK_Mannequin': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
        '/Game/Characters/Mannequins/Skeletons/UE5_Mannequin_Skeleton': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
        '/Game/Characters/Mannequins/Skeletons/UE5_Female_Mannequin_Skeleton': '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple'
      };
      
      // Auto-fix common incorrect paths
      let actualSkeletonPath = params.skeletonPath;
      if (actualSkeletonPath && skeletonToMeshMap[actualSkeletonPath]) {
        console.error(`Auto-correcting path from ${actualSkeletonPath} to ${skeletonToMeshMap[actualSkeletonPath]}`);
        actualSkeletonPath = skeletonToMeshMap[actualSkeletonPath];
      }
      
      if (actualSkeletonPath && (actualSkeletonPath.includes('_Skeleton') || actualSkeletonPath.includes('SK_Mannequin'))) {
        // This is likely a skeleton path, not a skeletal mesh
        console.error('Warning: Path appears to be a skeleton, not a skeletal mesh. Auto-correcting to SKM_Manny_Simple.');
      }
      
      // Build Python script with resolved mesh path
    const pythonScript = `
import unreal
import time
import json

result = {
  "success": False,
  "path": None,
  "message": "",
  "error": None,
  "warnings": [],
  "details": [],
  "existingAsset": False,
  "meshPath": "${meshPath}"
}

def record_detail(message):
  result["details"].append(message)

def record_warning(message):
  result["warnings"].append(message)

def record_error(message):
  result["error"] = message

# Helper function to ensure asset persistence
def ensure_asset_persistence(asset_path):
  try:
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
      record_warning(f"Asset persistence check failed: {asset_path} not loaded")
      return False
        
    # Save the asset
    saved = unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
    if saved:
      print(f"Asset saved: {asset_path}")
      record_detail(f"Asset saved: {asset_path}")
        
    # Refresh the asset registry minimally for the asset's directory
    try:
      asset_dir = asset_path.rsplit('/', 1)[0]
      unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([asset_dir], True)
    except Exception as _reg_e:
      record_warning(f"Asset registry refresh warning: {_reg_e}")
        
    # Small delay to ensure filesystem sync
    time.sleep(0.1)
        
    return saved
  except Exception as e:
    print(f"Error ensuring persistence: {e}")
    record_error(f"Error ensuring persistence: {e}")
    return False

# Stop PIE if running using modern subsystems
try:
  level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
  play_subsystem = None
  try:
    play_subsystem = unreal.get_editor_subsystem(unreal.EditorPlayWorldSubsystem)
  except Exception:
    play_subsystem = None

  is_playing = False
  if level_subsystem and hasattr(level_subsystem, 'is_in_play_in_editor'):
    is_playing = level_subsystem.is_in_play_in_editor()
  elif play_subsystem and hasattr(play_subsystem, 'is_playing_in_editor'):  # type: ignore[attr-defined]
    is_playing = play_subsystem.is_playing_in_editor()  # type: ignore[attr-defined]

  if is_playing:
    print("Stopping Play In Editor mode...")
    record_detail("Stopping Play In Editor mode")
    if level_subsystem and hasattr(level_subsystem, 'editor_request_end_play'):
      level_subsystem.editor_request_end_play()
    elif play_subsystem and hasattr(play_subsystem, 'stop_playing_session'):  # type: ignore[attr-defined]
      play_subsystem.stop_playing_session()  # type: ignore[attr-defined]
    elif play_subsystem and hasattr(play_subsystem, 'end_play'):  # type: ignore[attr-defined]
      play_subsystem.end_play()  # type: ignore[attr-defined]
    else:
      record_warning('Unable to stop Play In Editor via modern subsystems; please stop PIE manually.')
    time.sleep(0.5)
except Exception as pie_error:
  record_warning(f"PIE stop check failed: {pie_error}")

# Main execution
success = False
error_msg = ""
new_asset = None

# Log the attempt
print("Setting up ragdoll for ${meshPath}")
record_detail("Setting up ragdoll for ${meshPath}")

asset_path = "${path}"
asset_name = "${sanitizedParams.name}"
full_path = f"{asset_path}/{asset_name}"

try:
  # Check if already exists
  if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    print(f"Physics asset already exists at {full_path}")
    record_detail(f"Physics asset already exists at {full_path}")
    existing = unreal.EditorAssetLibrary.load_asset(full_path)
    if existing:
      print(f"Loaded existing PhysicsAsset: {full_path}")
      record_detail(f"Loaded existing PhysicsAsset: {full_path}")
      success = True
      result["existingAsset"] = True
      result["message"] = f"Physics asset already exists at {full_path}"
  else:
    # Try to load skeletal mesh first - it's required
    skeletal_mesh_path = "${meshPath}"
    skeletal_mesh = None
        
    if skeletal_mesh_path and skeletal_mesh_path != "None":
      if unreal.EditorAssetLibrary.does_asset_exist(skeletal_mesh_path):
        asset = unreal.EditorAssetLibrary.load_asset(skeletal_mesh_path)
        if asset:
          if isinstance(asset, unreal.SkeletalMesh):
            skeletal_mesh = asset
            print(f"Loaded skeletal mesh: {skeletal_mesh_path}")
            record_detail(f"Loaded skeletal mesh: {skeletal_mesh_path}")
          elif isinstance(asset, unreal.Skeleton):
            error_msg = f"Provided path is a skeleton, not a skeletal mesh: {skeletal_mesh_path}"
            print(f"Error: {error_msg}")
            record_error(error_msg)
            result["message"] = error_msg
            print("Error: Physics assets require a skeletal mesh, not just a skeleton")
            record_warning("Physics assets require a skeletal mesh, not just a skeleton")
          else:
            error_msg = f"Asset is not a skeletal mesh: {skeletal_mesh_path}"
            print(f"Warning: {error_msg}")
            record_warning(error_msg)
      else:
        error_msg = f"Skeletal mesh not found at {skeletal_mesh_path}"
        print(f"Error: {error_msg}")
        record_error(error_msg)
        result["message"] = error_msg
        
    if not skeletal_mesh:
      if not error_msg:
        error_msg = "Cannot create physics asset without a valid skeletal mesh"
      print(f"Error: {error_msg}")
      record_error(error_msg)
      if not result["message"]:
        result["message"] = error_msg
    else:
      # Create physics asset using a different approach
      # Method 1: Direct creation with initialized factory
      try:
        factory = unreal.PhysicsAssetFactory()
                
        # Ensure the directory exists
        if not unreal.EditorAssetLibrary.does_directory_exist(asset_path):
          unreal.EditorAssetLibrary.make_directory(asset_path)
                
        # Alternative approach: Create physics asset from skeletal mesh
        # This is the proper way in UE5
        try:
          # Try modern physics asset creation methods first
          try:
            # Method 1: Try using SkeletalMesh editor utilities if available
            if hasattr(unreal, 'SkeletalMeshEditorSubsystem'):
              skel_subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
              if hasattr(skel_subsystem, 'create_physics_asset'):
                physics_asset = skel_subsystem.create_physics_asset(skeletal_mesh)
              else:
                # Fallback to deprecated EditorSkeletalMeshLibrary
                physics_asset = unreal.EditorSkeletalMeshLibrary.create_physics_asset(skeletal_mesh)
            else:
              physics_asset = unreal.EditorSkeletalMeshLibrary.create_physics_asset(skeletal_mesh)
          except Exception as method1_modern_error:
            record_warning(f"Modern creation path fallback: {method1_modern_error}")
            # Final fallback to deprecated API
            physics_asset = unreal.EditorSkeletalMeshLibrary.create_physics_asset(skeletal_mesh)
        except Exception as e:
          print(f"Physics asset creation failed: {str(e)}")
          record_error(f"Physics asset creation failed: {str(e)}")
          physics_asset = None
                
        if physics_asset:
          # Move/rename the physics asset to desired location
          source_path = physics_asset.get_path_name()
          if unreal.EditorAssetLibrary.rename_asset(source_path, full_path):
            print(f"Successfully created and moved PhysicsAsset to {full_path}")
            record_detail(f"Successfully created and moved PhysicsAsset to {full_path}")
            new_asset = physics_asset
                        
            # Ensure persistence
            if ensure_asset_persistence(full_path):
              # Verify it was saved
              if unreal.EditorAssetLibrary.does_asset_exist(full_path):
                print(f"Verified PhysicsAsset exists after save: {full_path}")
                record_detail(f"Verified PhysicsAsset exists after save: {full_path}")
                success = True
                result["message"] = f"Ragdoll physics setup completed for {asset_name}"
              else:
                error_msg = f"PhysicsAsset not found after save: {full_path}"
                print(f"Warning: {error_msg}")
                record_warning(error_msg)
            else:
              error_msg = "Failed to persist physics asset"
              print(f"Warning: {error_msg}")
              record_warning(error_msg)
          else:
            print(f"Created PhysicsAsset but couldn't move to {full_path}")
            record_warning(f"Created PhysicsAsset but couldn't move to {full_path}")
            # Still consider it a success if we created it
            new_asset = physics_asset
            success = True
            result["message"] = f"Physics asset created but not moved to {full_path}"
        else:
          error_msg = "Failed to create PhysicsAsset from skeletal mesh"
          print(error_msg)
          record_error(error_msg)
          new_asset = None
                    
              successMessage: \`Skeletal mesh discovery complete\`,
              failureMessage: \`Failed to discover skeletal mesh\`
        record_warning(f"Method 1 failed: {str(e)}")
                
        # Method 2: Try older approach
        try:
          asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
          factory = unreal.PhysicsAssetFactory()
                    
          # Try to initialize factory with the skeletal mesh
          factory.create_physics_asset_from_skeletal_mesh = skeletal_mesh
                    
          new_asset = asset_tools.create_asset(
            asset_name=asset_name,
            package_path=asset_path,
            asset_class=unreal.PhysicsAsset,
            factory=factory
          )
                    
          if new_asset:
            print(f"Successfully created PhysicsAsset at {full_path} (Method 2)")
            record_detail(f"Successfully created PhysicsAsset at {full_path} (Method 2)")
            # Ensure persistence
            if ensure_asset_persistence(full_path):
              success = True
              result["message"] = f"Ragdoll physics setup completed for {asset_name}"
            else:
              record_warning("Persistence check failed after Method 2 creation")
        except Exception as e2:
          error_msg = f"Method 2 also failed: {str(e2)}"
          print(error_msg)
          record_error(error_msg)
          new_asset = None
            
      # Final check
      if new_asset and not success:
        # Try one more save
        if ensure_asset_persistence(full_path):
          if unreal.EditorAssetLibrary.does_asset_exist(full_path):
            success = True
            result["message"] = f"Ragdoll physics setup completed for {asset_name}"
          else:
            record_warning(f"Final existence check failed for {full_path}")

except Exception as e:
  error_msg = str(e)
  print(f"Error: {error_msg}")
  record_error(error_msg)
  import traceback
  traceback.print_exc()

# Finalize result
result["success"] = bool(success)
result["path"] = full_path if success else None

if not result["message"]:
  if success:
    result["message"] = f"Ragdoll physics setup completed for {asset_name}"
  elif error_msg:
    result["message"] = error_msg
  else:
    result["message"] = "Failed to setup ragdoll"

if not success:
  if not result["error"]:
    result["error"] = error_msg or "Unknown error"

print('RESULT:' + json.dumps(result))
`;
      
      
      // Execute Python and interpret response
      try {
        const response = await this.bridge.executePython(pythonScript);
        const interpreted = interpretStandardResult(response, {
          successMessage: `Ragdoll physics setup completed for ${sanitizedParams.name}`,
          failureMessage: `Failed to setup ragdoll for ${sanitizedParams.name}`
        });

        const warnings = interpreted.warnings ?? [];
        const details = interpreted.details ?? [];

        if (interpreted.success) {
          const successPayload: {
            success: true;
            message: string;
            path: string;
            existingAsset?: boolean;
            warnings?: string[];
            details?: string[];
          } = {
            success: true,
            message: interpreted.message,
            path: coerceString(interpreted.payload.path) ?? `${path}/${sanitizedParams.name}`
          };

          if (interpreted.payload.existingAsset === true) {
            successPayload.existingAsset = true;
          }

          if (warnings.length > 0) {
            successPayload.warnings = warnings;
          }
          if (details.length > 0) {
            successPayload.details = details;
          }

          return successPayload;
        }

        const errorMessage = interpreted.error ?? `Failed to setup ragdoll for ${sanitizedParams.name}`;

        return {
          success: false as const,
          message: errorMessage,
          error: errorMessage,
          warnings: warnings.length > 0 ? warnings : undefined,
          details: details.length > 0 ? details : undefined
        };
      } catch (error) {
        return {
          success: false,
          message: 'Failed to setup ragdoll physics',
          error: String(error)
        };
      }
    } catch (err) {
      return { success: false, error: `Failed to setup ragdoll: ${err}` };
    }
  }

  /**
   * Create Physics Constraint
   */
  async createConstraint(params: {
    name: string;
    actor1: string;
    actor2: string;
    constraintType: 'Fixed' | 'Hinge' | 'Prismatic' | 'Ball' | 'Cone';
    location: [number, number, number];
    breakThreshold?: number;
    limits?: {
      swing1?: number;
      swing2?: number;
      twist?: number;
      linear?: number;
    };
  }) {
    try {
      // Spawn constraint actor
      const spawnCmd = `spawnactor /Script/Engine.PhysicsConstraintActor ${params.location[0]} ${params.location[1]} ${params.location[2]}`;
      await this.bridge.executeConsoleCommand(spawnCmd);
      
      // Configure constraint
      const commands = [
        `SetConstraintActors ${params.name} ${params.actor1} ${params.actor2}`,
        `SetConstraintType ${params.name} ${params.constraintType}`
      ];
      
      if (params.breakThreshold) {
        commands.push(`SetConstraintBreakThreshold ${params.name} ${params.breakThreshold}`);
      }
      
      if (params.limits) {
        const limits = params.limits;
        if (limits.swing1 !== undefined) {
          commands.push(`SetConstraintSwing1 ${params.name} ${limits.swing1}`);
        }
        if (limits.swing2 !== undefined) {
          commands.push(`SetConstraintSwing2 ${params.name} ${limits.swing2}`);
        }
        if (limits.twist !== undefined) {
          commands.push(`SetConstraintTwist ${params.name} ${limits.twist}`);
        }
        if (limits.linear !== undefined) {
          commands.push(`SetConstraintLinear ${params.name} ${limits.linear}`);
        }
      }
      
      await this.bridge.executeConsoleCommands(commands);
      
      return { 
        success: true, 
        message: `Physics constraint ${params.name} created between ${params.actor1} and ${params.actor2}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to create constraint: ${err}` };
    }
  }

  /**
   * Setup Chaos Destruction
   */
  async setupDestruction(params: {
    meshPath: string;
    destructionName: string;
    savePath?: string;
    fractureSettings?: {
      cellCount: number;
      minimumVolumeSize: number;
      seed: number;
    };
    damageThreshold?: number;
    debrisLifetime?: number;
  }) {
    try {
      const path = params.savePath || '/Game/Destruction';
      
      const commands = [
        `CreateGeometryCollection ${params.destructionName} ${params.meshPath} ${path}`
      ];
      
      // Configure fracture
      if (params.fractureSettings) {
        const settings = params.fractureSettings;
        commands.push(
          `FractureGeometry ${params.destructionName} ${settings.cellCount} ${settings.minimumVolumeSize} ${settings.seed}`
        );
      }
      
      // Set damage threshold
      if (params.damageThreshold) {
        commands.push(`SetDamageThreshold ${params.destructionName} ${params.damageThreshold}`);
      }
      
      // Set debris lifetime
      if (params.debrisLifetime) {
        commands.push(`SetDebrisLifetime ${params.destructionName} ${params.debrisLifetime}`);
      }
      
      await this.bridge.executeConsoleCommands(commands);
      
      return { 
        success: true, 
        message: `Chaos destruction ${params.destructionName} created`,
        path: `${path}/${params.destructionName}`
      };
    } catch (err) {
      return { success: false, error: `Failed to setup destruction: ${err}` };
    }
  }

  /**
   * Configure Vehicle Physics
   */
  async configureVehicle(params: {
    vehicleName: string;
    vehicleType: 'Car' | 'Bike' | 'Tank' | 'Aircraft';
    wheels?: Array<{
      name: string;
      radius: number;
      width: number;
      mass: number;
      isSteering: boolean;
      isDriving: boolean;
    }>;
    engine?: {
      maxRPM: number;
      torqueCurve: Array<[number, number]>;
    };
    transmission?: {
      gears: number[];
      finalDriveRatio: number;
    };
    pluginDependencies?: string[];
  }) {
    try {
      const dependencies = Array.isArray(params.pluginDependencies)
        ? params.pluginDependencies
            .map(dep => (typeof dep === 'string' ? dep.trim() : ''))
            .filter(dep => dep.length > 0)
        : [];

      if (dependencies.length > 0) {
        const missingPlugins = await this.bridge.ensurePluginsEnabled(dependencies, 'physics.configureVehicle');
        if (missingPlugins.length > 0) {
          const missingList = missingPlugins.join(', ');
          return {
            success: false,
            error: `Required Unreal plugins not enabled: ${missingList}`,
            warnings: [
              `Enable ${missingList} in the editor (Edit > Plugins) and restart the session before running physics.configureVehicle.`
            ]
          };
        }
      }

      const commands = [
        `CreateVehicle ${params.vehicleName} ${params.vehicleType}`
      ];
      
      // Configure wheels
      if (params.wheels) {
        for (const wheel of params.wheels) {
          commands.push(
            `AddVehicleWheel ${params.vehicleName} ${wheel.name} ${wheel.radius} ${wheel.width} ${wheel.mass}`
          );
          
          if (wheel.isSteering) {
            commands.push(`SetWheelSteering ${params.vehicleName} ${wheel.name} true`);
          }
          if (wheel.isDriving) {
            commands.push(`SetWheelDriving ${params.vehicleName} ${wheel.name} true`);
          }
        }
      }
      
      // Configure engine
      if (params.engine) {
        commands.push(`SetEngineMaxRPM ${params.vehicleName} ${params.engine.maxRPM}`);
        
        for (const [rpm, torque] of params.engine.torqueCurve) {
          commands.push(`AddTorqueCurvePoint ${params.vehicleName} ${rpm} ${torque}`);
        }
      }
      
      // Configure transmission
      if (params.transmission) {
        for (let i = 0; i < params.transmission.gears.length; i++) {
          commands.push(
            `SetGearRatio ${params.vehicleName} ${i} ${params.transmission.gears[i]}`
          );
        }
        commands.push(
          `SetFinalDriveRatio ${params.vehicleName} ${params.transmission.finalDriveRatio}`
        );
      }
      
      await this.bridge.executeConsoleCommands(commands);

      const warnings: string[] = [];
      warnings.push('Verify wheel class assignments and offsets in the vehicle movement component to ensure they match your project defaults.');

      return {
        success: true,
        message: `Vehicle ${params.vehicleName} configured`,
        warnings: warnings.length > 0 ? warnings : undefined
      };
    } catch (err) {
      return { success: false, error: `Failed to configure vehicle: ${err}` };
    }
  }

  /**
   * Apply Force or Impulse to Actor
   */
  async applyForce(params: {
    actorName: string;
    forceType: 'Force' | 'Impulse' | 'Velocity' | 'Torque';
    vector: [number, number, number];
    boneName?: string;
    isLocal?: boolean;
  }) {
    try {
      // Use Python to apply physics forces since console commands don't exist for this
      const pythonCode = `
import unreal
import json

result = {"success": False, "message": "", "actor_found": False, "physics_enabled": False}

# Check if editor is in play mode first
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les and les.is_in_play_in_editor():
        result["message"] = "Cannot apply physics while in Play In Editor mode. Please stop PIE first."
        print(f"RESULT:{json.dumps(result)}")
        # Exit early from this script
        raise SystemExit(0)
except SystemExit:
    # Re-raise the SystemExit to exit properly
    raise
except:
    pass  # Continue if we can't check PIE state

try:
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    search_name = "${params.actorName}"
    
    for actor in actors:
        if actor:
            # Check both actor name and label with case-insensitive partial matching
            actor_name = actor.get_name()
            actor_label = actor.get_actor_label()
            
            if (search_name.lower() in actor_label.lower() or
                actor_label.lower().startswith(search_name.lower() + "_") or
                actor_label.lower() == search_name.lower() or
                actor_name.lower() == search_name.lower()):
                
                result["actor_found"] = True
                # Get the primitive component if it exists
                root = actor.get_editor_property('root_component')
                
                if root and isinstance(root, unreal.PrimitiveComponent):
                    # Check if the component is static or movable
                    mobility = root.get_editor_property('mobility')
                    if mobility == unreal.ComponentMobility.STATIC:
                        # Try to set to movable first
                        try:
                            root.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
                        except:
                            result["message"] = f"Actor {actor_label} has static mobility and cannot simulate physics"
                            break
                    
                    # Ensure physics is enabled
                    try:
                        root.set_simulate_physics(True)
                        result["physics_enabled"] = True
                    except Exception as physics_err:
                        # If we can't enable physics, try applying force anyway (some actors respond without physics sim)
                        result["physics_enabled"] = False
                    
                    force = unreal.Vector(${params.vector[0]}, ${params.vector[1]}, ${params.vector[2]})
                    
                    if "${params.forceType}" == "Force":
                        root.add_force(force, 'None', False)
                        result["success"] = True
                        result["message"] = f"Applied Force to {actor_label}: {force}"
                    elif "${params.forceType}" == "Impulse":
                        root.add_impulse(force, 'None', False)
                        result["success"] = True
                        result["message"] = f"Applied Impulse to {actor_label}: {force}"
                    elif "${params.forceType}" == "Velocity":
                        root.set_physics_linear_velocity(force)
                        result["success"] = True
                        result["message"] = f"Set Velocity on {actor_label}: {force}"
                    elif "${params.forceType}" == "Torque":
                        root.add_torque_in_radians(force, 'None', False)
                        result["success"] = True
                        result["message"] = f"Applied Torque to {actor_label}: {force}"
                else:
                    result["message"] = f"Actor {actor_label} doesn't have a physics-enabled component"
                break
                
    if not result["actor_found"]:
        result["message"] = f"Actor not found: {search_name}"
        # List actors with physics enabled for debugging
        physics_actors = []
        for actor in actors[:20]:
            if actor:
                label = actor.get_actor_label()
                if "mesh" in label.lower() or "cube" in label.lower() or "static" in label.lower():
                    physics_actors.append(label)
        if physics_actors:
            result["available_actors"] = physics_actors
            
except Exception as e:
    result["message"] = f"Error applying force: {e}"
    
print(f"RESULT:{json.dumps(result)}")
      `.trim();
      
      const response = await this.bridge.executePython(pythonCode);
      const interpreted = interpretStandardResult(response, {
        successMessage: `Applied ${params.forceType} to ${params.actorName}`,
        failureMessage: 'Force application failed'
      });

      const availableActors = coerceStringArray(interpreted.payload.available_actors);

      if (interpreted.success) {
        return {
          success: true,
          message: interpreted.message,
          availableActors,
          details: interpreted.details
        };
      }

  const fallbackText = bestEffortInterpretedText(interpreted) ?? '';
      if (/Applied/i.test(fallbackText)) {
        return {
          success: true,
          message: fallbackText || interpreted.message,
          availableActors,
          details: interpreted.details
        };
      }

      if (/not found/i.test(fallbackText) || /error/i.test(fallbackText)) {
        return {
          success: false,
          error: interpreted.error ?? (fallbackText || 'Force application failed'),
          availableActors,
          details: interpreted.details ?? (fallbackText ? [fallbackText] : undefined)
        };
      }

      return {
        success: false,
        error: interpreted.error ?? 'No valid result from Python',
        availableActors,
        details: interpreted.details ?? (fallbackText ? [fallbackText] : undefined)
      };
    } catch (err) {
      return { success: false, error: `Failed to apply force: ${err}` };
    }
  }

  /**
   * Configure Cloth Simulation
   */
  async setupCloth(params: {
    meshName: string;
    clothPreset: 'Silk' | 'Leather' | 'Denim' | 'Rubber' | 'Custom';
    customSettings?: {
      stiffness?: number;
      damping?: number;
      friction?: number;
      density?: number;
      gravity?: number;
      windVelocity?: [number, number, number];
    };
  }) {
    try {
      const commands = [
        `EnableClothSimulation ${params.meshName}`,
        `SetClothPreset ${params.meshName} ${params.clothPreset}`
      ];
      
      if (params.clothPreset === 'Custom' && params.customSettings) {
        const settings = params.customSettings;
        
        if (settings.stiffness !== undefined) {
          commands.push(`SetClothStiffness ${params.meshName} ${settings.stiffness}`);
        }
        if (settings.damping !== undefined) {
          commands.push(`SetClothDamping ${params.meshName} ${settings.damping}`);
        }
        if (settings.friction !== undefined) {
          commands.push(`SetClothFriction ${params.meshName} ${settings.friction}`);
        }
        if (settings.density !== undefined) {
          commands.push(`SetClothDensity ${params.meshName} ${settings.density}`);
        }
        if (settings.gravity !== undefined) {
          commands.push(`SetClothGravity ${params.meshName} ${settings.gravity}`);
        }
        if (settings.windVelocity) {
          const wind = settings.windVelocity;
          commands.push(`SetClothWind ${params.meshName} ${wind[0]} ${wind[1]} ${wind[2]}`);
        }
      }
      
      await this.bridge.executeConsoleCommands(commands);
      
      return { 
        success: true, 
        message: `Cloth simulation enabled for ${params.meshName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to setup cloth: ${err}` };
    }
  }

  /**
   * Create Fluid Simulation (Niagara-based)
   */
  async createFluidSimulation(params: {
    name: string;
    fluidType: 'Water' | 'Smoke' | 'Fire' | 'Lava' | 'Custom';
    location: [number, number, number];
    volume: [number, number, number];
    customSettings?: {
      viscosity?: number;
      density?: number;
      temperature?: number;
      turbulence?: number;
      color?: [number, number, number, number];
    };
  }) {
    try {
      const locStr = `${params.location[0]} ${params.location[1]} ${params.location[2]}`;
      const volStr = `${params.volume[0]} ${params.volume[1]} ${params.volume[2]}`;
      
      const commands = [
        `CreateFluidSimulation ${params.name} ${params.fluidType} ${locStr} ${volStr}`
      ];
      
      if (params.customSettings) {
        const settings = params.customSettings;
        
        if (settings.viscosity !== undefined) {
          commands.push(`SetFluidViscosity ${params.name} ${settings.viscosity}`);
        }
        if (settings.density !== undefined) {
          commands.push(`SetFluidDensity ${params.name} ${settings.density}`);
        }
        if (settings.temperature !== undefined) {
          commands.push(`SetFluidTemperature ${params.name} ${settings.temperature}`);
        }
        if (settings.turbulence !== undefined) {
          commands.push(`SetFluidTurbulence ${params.name} ${settings.turbulence}`);
        }
        if (settings.color) {
          const color = settings.color;
          commands.push(
            `SetFluidColor ${params.name} ${color[0]} ${color[1]} ${color[2]} ${color[3]}`
          );
        }
      }
      
      await this.bridge.executeConsoleCommands(commands);
      
      return { 
        success: true, 
        message: `Fluid simulation ${params.name} created` 
      };
    } catch (err) {
      return { success: false, error: `Failed to create fluid simulation: ${err}` };
    }
  }

}
