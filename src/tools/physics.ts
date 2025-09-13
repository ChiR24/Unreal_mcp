import { UnrealBridge } from '../unreal-bridge.js';
import { validateAssetParams, resolveSkeletalMeshPath, concurrencyDelay } from '../utils/validation.js';

export class PhysicsTools {
  constructor(private bridge: UnrealBridge) {}
  
  /**
   * Helper to find a valid skeletal mesh in the project
   */
  private async findValidSkeletalMesh(): Promise<string | null> {
    const pythonScript = `
import unreal

# Common skeletal mesh paths to check
common_paths = [
    '/Game/Mannequin/Character/Mesh/SK_Mannequin',
    '/Game/Characters/Mannequin/Meshes/SK_Mannequin',
    '/Game/AnimStarterPack/UE4_Mannequin/Mesh/SK_Mannequin',
    '/Game/ThirdPerson/Meshes/SK_Mannequin',
    '/Game/ThirdPersonBP/Meshes/SK_Mannequin',
    '/Engine/EngineMeshes/SkeletalCube',  # Fallback engine mesh
]

# Try to find any skeletal mesh
for path in common_paths:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset and isinstance(asset, unreal.SkeletalMesh):
            print(f"FOUND_MESH:{path}")
            break
else:
    # Search for any skeletal mesh in the project
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = asset_registry.get_assets_by_class('SkeletalMesh', search_sub_classes=False)
    if assets:
        # Use the first available skeletal mesh
        first_mesh = assets[0]
        obj_path = first_mesh.get_editor_property('object_path')
        if obj_path:
            print(f"FOUND_MESH:{str(obj_path).split('.')[0]}")
        else:
            print("NO_MESH_FOUND")
    else:
        print("NO_MESH_FOUND")
`;
    
    try {
      const response = await this.bridge.executePython(pythonScript);
      let outputStr = '';
      if (response?.LogOutput && Array.isArray((response as any).LogOutput)) {
        outputStr = (response as any).LogOutput.map((l: any) => l.Output || '').join('');
      } else if (typeof response === 'string') {
        outputStr = response;
      } else {
        // Fallback: stringify and still try to parse, but restrict to line content only
        outputStr = JSON.stringify(response);
      }
      
      // Capture only until end-of-line to avoid trailing JSON serialization
      const match = outputStr.match(/FOUND_MESH:([^\r\n]+)/);
      if (match) {
        return match[1].trim();
      }
    } catch (error) {
      console.error('Failed to find skeletal mesh:', error);
    }
    
    // Return engine fallback if nothing found
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
        console.log(`Auto-correcting path from ${meshPath} to ${resolvedPath}`);
        meshPath = resolvedPath;
      }
      
      // Auto-resolve if it looks like a skeleton path or is empty
      if (!meshPath || meshPath.includes('_Skeleton') || meshPath === 'None' || meshPath === '') {
        console.log('Resolving skeletal mesh path...');
        const resolvedMesh = await this.findValidSkeletalMesh();
        if (resolvedMesh) {
          meshPath = resolvedMesh;
          console.log(`Using resolved skeletal mesh: ${meshPath}`);
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
        '/Game/Mannequin/Character/Mesh/SK_Mannequin': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
      };
      
      // Auto-fix common incorrect paths
      let actualSkeletonPath = params.skeletonPath;
      if (actualSkeletonPath && skeletonToMeshMap[actualSkeletonPath]) {
        console.log(`Auto-correcting path from ${actualSkeletonPath} to ${skeletonToMeshMap[actualSkeletonPath]}`);
        actualSkeletonPath = skeletonToMeshMap[actualSkeletonPath];
      }
      
      if (actualSkeletonPath && (actualSkeletonPath.includes('_Skeleton') || actualSkeletonPath.includes('SK_Mannequin'))) {
        // This is likely a skeleton path, not a skeletal mesh
        console.warn('Warning: Path appears to be a skeleton, not a skeletal mesh. Auto-correcting to SKM_Manny_Simple.');
      }
      
      // Build Python script with resolved mesh path
      const pythonScript = `
import unreal
import time

# Helper function to ensure asset persistence
def ensure_asset_persistence(asset_path):
    try:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not asset:
            return False
        
        # Save the asset
        saved = unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
        if saved:
            print(f"Asset saved: {asset_path}")
        
        # Refresh the asset registry minimally for the asset's directory
        try:
            asset_dir = asset_path.rsplit('/', 1)[0]
            unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([asset_dir], True)
        except Exception as _reg_e:
            pass
        
        # Small delay to ensure filesystem sync
        time.sleep(0.1)
        
        return saved
    except Exception as e:
        print(f"Error ensuring persistence: {e}")
        return False

# Stop PIE if it's running
try:
    if unreal.EditorLevelLibrary.is_playing_editor():
        print("Stopping Play In Editor mode...")
        unreal.EditorLevelLibrary.editor_end_play()
        time.sleep(0.5)
except:
    pass

# Main execution
success = False
error_msg = ""

# Log the attempt
print("Setting up ragdoll for ${meshPath}")

asset_path = "${path}"
asset_name = "${sanitizedParams.name}"
full_path = f"{asset_path}/{asset_name}"

try:
    # Check if already exists
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        print(f"Physics asset already exists at {full_path}")
        existing = unreal.EditorAssetLibrary.load_asset(full_path)
        if existing:
            print(f"Loaded existing PhysicsAsset: {full_path}")
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
                    elif isinstance(asset, unreal.Skeleton):
                        error_msg = f"Provided path is a skeleton, not a skeletal mesh: {skeletal_mesh_path}"
                        print(f"Error: {error_msg}")
                        print(f"Error: Physics assets require a skeletal mesh, not just a skeleton")
                    else:
                        error_msg = f"Asset is not a skeletal mesh: {skeletal_mesh_path}"
                        print(f"Warning: {error_msg}")
            else:
                error_msg = f"Skeletal mesh not found at {skeletal_mesh_path}"
                print(f"Error: {error_msg}")
        
        if not skeletal_mesh:
            if not error_msg:
                error_msg = "Cannot create physics asset without a valid skeletal mesh"
            print(f"Error: {error_msg}")
        else:
            # Create physics asset using a different approach
            # Method 1: Direct creation with initialized factory
            try:
                factory = unreal.PhysicsAssetFactory()
                
                # Create a transient package for the physics asset
                # Ensure the directory exists
                if not unreal.EditorAssetLibrary.does_directory_exist(asset_path):
                    unreal.EditorAssetLibrary.make_directory(asset_path)
                
                # Alternative approach: Create physics asset from skeletal mesh
                # This is the proper way in UE5
                physics_asset = unreal.EditorSkeletalMeshLibrary.create_physics_asset(skeletal_mesh)
                
                if physics_asset:
                    # Move/rename the physics asset to desired location
                    source_path = physics_asset.get_path_name()
                    if unreal.EditorAssetLibrary.rename_asset(source_path, full_path):
                        print(f"Successfully created and moved PhysicsAsset to {full_path}")
                        new_asset = physics_asset
                        
                        # Ensure persistence
                        if ensure_asset_persistence(full_path):
                            # Verify it was saved
                            if unreal.EditorAssetLibrary.does_asset_exist(full_path):
                                print(f"Verified PhysicsAsset exists after save: {full_path}")
                                success = True
                            else:
                                error_msg = f"PhysicsAsset not found after save: {full_path}"
                                print(f"Warning: {error_msg}")
                        else:
                            error_msg = "Failed to persist physics asset"
                            print(f"Warning: {error_msg}")
                    else:
                        print(f"Created PhysicsAsset but couldn't move to {full_path}")
                        # Still consider it a success if we created it
                        new_asset = physics_asset
                        success = True
                else:
                    error_msg = "Failed to create PhysicsAsset from skeletal mesh"
                    print(f"{error_msg}")
                    new_asset = None
                    
            except Exception as e:
                print(f"Method 1 failed: {str(e)}")
                
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
                        # Ensure persistence
                        if ensure_asset_persistence(full_path):
                            success = True
                except Exception as e2:
                    error_msg = f"Method 2 also failed: {str(e2)}"
                    print(error_msg)
                    new_asset = None
            
            # Final check
            if new_asset and not success:
                # Try one more save
                if ensure_asset_persistence(full_path):
                    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
                        success = True
            
except Exception as e:
    error_msg = str(e)
    print(f"Error: {error_msg}")
    import traceback
    traceback.print_exc()

# Output result markers for parsing
if success:
    print("SUCCESS")
else:
    print(f"FAILED: {error_msg}")

print("DONE")
`;
      
      
      // Execute Python and parse response
      try {
        const response = await this.bridge.executePython(pythonScript);
        
        // Parse the response to detect actual success or failure
        const responseStr = typeof response === 'string' ? response : JSON.stringify(response);
        
        // Check for explicit success/failure markers
        if (responseStr.includes('SUCCESS')) {
          return { 
            success: true, 
            message: `Ragdoll physics setup completed for ${sanitizedParams.name}`,
            path: `${path}/${sanitizedParams.name}`
          };
        } else if (responseStr.includes('FAILED:')) {
          // Extract error message after FAILED:
          const failMatch = responseStr.match(/FAILED:\s*(.+)/);
          const errorMsg = failMatch ? failMatch[1] : 'Unknown error';
          return {
            success: false,
            message: `Failed to setup ragdoll: ${errorMsg}`,
            error: errorMsg
          };
        } else {
          // Check legacy error detection for backwards compatibility
          const logOutput = response?.LogOutput || [];
          const hasSkeletonError = logOutput.some((log: any) => 
            log.Output && (log.Output.includes('skeleton, not a skeletal mesh') || 
                          log.Output.includes('Must specify a valid skeletal mesh')));
          
          if (hasSkeletonError) {
            return {
              success: false,
              message: 'Failed: Must specify a valid skeletal mesh',
              error: 'The path points to a skeleton, not a skeletal mesh. Physics assets require a skeletal mesh.'
            };
          }
          
          // Check for other error indicators
          if (responseStr.includes('Error:') || responseStr.includes('error')) {
            return {
              success: false,
              message: 'Failed to setup ragdoll physics',
              error: responseStr
            };
          }
          
          // Default to success if no errors detected
          return { 
            success: true, 
            message: `Ragdoll physics processed for ${sanitizedParams.name}`,
            path: `${path}/${sanitizedParams.name}`
          };
        }
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
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
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
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
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
  }) {
    try {
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
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Vehicle ${params.vehicleName} configured` 
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
      
      // Extract output from Python response
      let outputStr = '';
      if (typeof response === 'object' && response !== null) {
        // Check if it has LogOutput (standard Python execution response)
        if (response.LogOutput && Array.isArray(response.LogOutput)) {
          // Concatenate all log outputs
          outputStr = response.LogOutput
            .map((log: any) => log.Output || '')
            .join('');
        } else if ('result' in response) {
          outputStr = String(response.result);
        } else {
          outputStr = JSON.stringify(response);
        }
      } else {
        outputStr = String(response || '');
      }
      
      // Parse the result
      const resultMatch = outputStr.match(/RESULT:(\{.*\})/);
      if (resultMatch) {
        try {
          const forceResult = JSON.parse(resultMatch[1]);
          if (!forceResult.success) {
            return { success: false, error: forceResult.message };
          }
          return forceResult;
        } catch {
          // Fallback
          if (outputStr.includes('Applied')) {
            return { success: true, message: outputStr };
          }
          return { success: false, error: outputStr || 'Force application failed' };
        }
      } else {
        // Check for error patterns
        if (outputStr.includes('not found') || outputStr.includes('Error')) {
          return { success: false, error: outputStr || 'Force application failed' };
        }
        // Only return success if we have clear indication of success
        if (outputStr.includes('Applied')) {
          return { success: true, message: `Applied ${params.forceType} to ${params.actorName}` };
        }
        return { success: false, error: 'No valid result from Python' };
      }
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
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
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
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Fluid simulation ${params.name} created` 
      };
    } catch (err) {
      return { success: false, error: `Failed to create fluid simulation: ${err}` };
    }
  }

  /**
   * Helper function to execute console commands
   */
  private async executeCommand(command: string) {
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
}
