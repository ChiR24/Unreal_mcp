// Physics tools for Unreal Engine
//
// Physics operations now use the Automation Bridge for complex physics asset operations.
// Python fallbacks have been removed in favor of direct plugin communication.
//
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { validateAssetParams, resolveSkeletalMeshPath, concurrencyDelay } from '../utils/validation.js';
import { coerceString, coerceStringArray } from '../utils/result-helpers.js';

export class PhysicsTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }
  
  /**
   * Helper to find a valid skeletal mesh in the project
   */
  private async findValidSkeletalMesh(): Promise<string | null> {
    if (!this.automationBridge) {
      // Return common fallback paths without plugin
      return '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple';
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('find_skeletal_mesh', {
        commonPaths: [
          '/Game/Characters/Mannequins/Meshes/SKM_Manny',
          '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
          '/Game/Characters/Mannequins/Meshes/SKM_Manny_Complex',
          '/Game/Characters/Mannequins/Meshes/SKM_Quinn',
          '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple',
          '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Complex'
        ],
        fallback: '/Engine/EngineMeshes/SkeletalCube'
      }, {
        timeoutMs: 30000
      });

      if (response.success !== false && response.result) {
        const meshPath = coerceString((response.result as any).meshPath);
        if (meshPath) {
          return meshPath;
        }
      }

      // Fallback to alternate path
      const alternate = coerceString((response.result as any)?.alternate);
      if (alternate) {
        return alternate;
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
  // Alternate path: /Engine/EngineMeshes/SkeletalCube
      
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
      
      // Use Automation Bridge for physics asset creation
      if (!this.automationBridge) {
        throw new Error('Automation Bridge not available. Physics asset creation requires plugin support.');
      }

      try {
        const response = await this.automationBridge.sendAutomationRequest('setup_ragdoll', {
          meshPath,
          physicsAssetName: sanitizedParams.name,
          savePath: path,
          blendWeight: params.blendWeight,
          constraints: params.constraints
        }, {
          timeoutMs: 120000 // 2 minutes for complex physics asset creation
        });

        if (response.success === false) {
          return {
            success: false,
            message: response.error || response.message || `Failed to setup ragdoll for ${sanitizedParams.name}`,
            error: response.error || response.message || 'Failed to setup ragdoll'
          };
        }

        const result = response.result as any;
        return {
          success: true,
          message: response.message || `Ragdoll physics setup completed for ${sanitizedParams.name}`,
          path: coerceString(result?.path) ?? `${path}/${sanitizedParams.name}`,
          existingAsset: result?.existingAsset,
          warnings: result?.warnings,
          details: result?.details
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
          // Surface explicit error when required engine plugins are missing.
          return {
            success: false,
            error: 'MISSING_ENGINE_PLUGINS',
            message: `Required Unreal plugins not enabled: ${missingList}`,
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
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Physics force application requires plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('apply_force', {
        actorName: params.actorName,
        forceType: params.forceType,
        vector: params.vector,
        boneName: params.boneName,
        isLocal: params.isLocal
      }, {
        timeoutMs: 30000
      });

      if (response.success === false) {
        const result = response.result as any;
        return {
          success: false,
          error: response.error || response.message || 'Force application failed',
          availableActors: result?.available_actors ? coerceStringArray(result.available_actors) : undefined,
          details: result?.details
        };
      }

      const result = response.result as any;
      return {
        success: true,
        message: response.message || `Applied ${params.forceType} to ${params.actorName}`,
        availableActors: result?.available_actors ? coerceStringArray(result.available_actors) : undefined,
        details: result?.details
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
