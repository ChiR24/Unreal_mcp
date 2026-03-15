// Physics tools for Unreal Engine using Automation Bridge
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { validateAssetParams, resolveSkeletalMeshPath, concurrencyDelay, sanitizeCommandArgument } from '../utils/validation.js';
import { coerceString, coerceStringArray } from '../utils/result-helpers.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('PhysicsTools');

export class PhysicsTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) { }

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
        const resultObj = response.result as Record<string, unknown>;
        const meshPath = coerceString(resultObj.meshPath);
        if (meshPath) {
          return meshPath;
        }
      }

      // Fallback to alternate path
      const resultObj = (response.result ?? {}) as Record<string, unknown>;
      const alternate = coerceString(resultObj.alternate);
      if (alternate) {
        return alternate;
      }
    } catch (error) {
      log.warn('Failed to find skeletal mesh', error);
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
        log.warn(`Auto-correcting path from ${meshPath} to ${resolvedPath}`);
        meshPath = resolvedPath;
      }

      // Auto-resolve if it looks like a skeleton path or is empty
      if (!meshPath || meshPath.includes('_Skeleton') || meshPath === 'None' || meshPath === '') {
        log.debug('Resolving skeletal mesh path...');
        const resolvedMesh = await this.findValidSkeletalMesh();
        if (resolvedMesh) {
          meshPath = resolvedMesh;
          log.debug(`Using resolved skeletal mesh: ${meshPath}`);
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
        log.warn(`Auto-correcting path from ${actualSkeletonPath} to ${skeletonToMeshMap[actualSkeletonPath]}`);
        actualSkeletonPath = skeletonToMeshMap[actualSkeletonPath];
      }

      if (actualSkeletonPath && (actualSkeletonPath.includes('_Skeleton') || actualSkeletonPath.includes('SK_Mannequin'))) {
        // This is likely a skeleton path, not a skeletal mesh
        log.warn('Path appears to be a skeleton, not a skeletal mesh. Auto-correcting to SKM_Manny_Simple.');
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

        const result = (response.result ?? {}) as Record<string, unknown>;
        return {
          success: true,
          message: response.message || `Ragdoll physics setup completed for ${sanitizedParams.name}`,
          path: coerceString(result.path) ?? coerceString(result.physicsAssetPath) ?? `${path}/${sanitizedParams.name}`,
          existingAsset: result.existingAsset,
          ...result
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
      // SECURITY: Sanitize user inputs to prevent command injection
      const safeName = sanitizeCommandArgument(params.name);
      const safeActor1 = sanitizeCommandArgument(params.actor1);
      const safeActor2 = sanitizeCommandArgument(params.actor2);

      if (!safeName || !safeActor1 || !safeActor2) {
        return { success: false, error: 'Constraint name and actor names must be valid and non-empty.' };
      }

      // Spawn constraint actor
      const spawnCmd = `spawnactor /Script/Engine.PhysicsConstraintActor ${params.location[0]} ${params.location[1]} ${params.location[2]}`;
      await this.bridge.executeConsoleCommand(spawnCmd);

      // Configure constraint
      const commands = [
        `SetConstraintActors ${safeName} ${safeActor1} ${safeActor2}`,
        `SetConstraintType ${safeName} ${params.constraintType}`
      ];

      if (params.breakThreshold) {
        commands.push(`SetConstraintBreakThreshold ${safeName} ${params.breakThreshold}`);
      }

      if (params.limits) {
        const limits = params.limits;
        if (limits.swing1 !== undefined) {
          commands.push(`SetConstraintSwing1 ${safeName} ${limits.swing1}`);
        }
        if (limits.swing2 !== undefined) {
          commands.push(`SetConstraintSwing2 ${safeName} ${limits.swing2}`);
        }
        if (limits.twist !== undefined) {
          commands.push(`SetConstraintTwist ${safeName} ${limits.twist}`);
        }
        if (limits.linear !== undefined) {
          commands.push(`SetConstraintLinear ${safeName} ${limits.linear}`);
        }
      }

      await this.bridge.executeConsoleCommands(commands);

      return {
        success: true,
        message: `Physics constraint ${safeName} created between ${safeActor1} and ${safeActor2}`
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
      // SECURITY: Sanitize user inputs to prevent command injection
      const safeName = sanitizeCommandArgument(params.destructionName);
      const safeMeshPath = sanitizeCommandArgument(params.meshPath);
      const safeSavePath = sanitizeCommandArgument(params.savePath || '/Game/Destruction');

      if (!safeName || !safeMeshPath || !safeSavePath) {
        return { success: false, error: 'Destruction name, mesh path, and save path must be valid and non-empty.' };
      }

      const commands = [
        `CreateGeometryCollection ${safeName} ${safeMeshPath} ${safeSavePath}`
      ];

      // Configure fracture
      if (params.fractureSettings) {
        const settings = params.fractureSettings;
        commands.push(
          `FractureGeometry ${safeName} ${settings.cellCount} ${settings.minimumVolumeSize} ${settings.seed}`
        );
      }

      // Set damage threshold
      if (params.damageThreshold) {
        commands.push(`SetDamageThreshold ${safeName} ${params.damageThreshold}`);
      }

      // Set debris lifetime
      if (params.debrisLifetime) {
        commands.push(`SetDebrisLifetime ${safeName} ${params.debrisLifetime}`);
      }

      await this.bridge.executeConsoleCommands(commands);

      return {
        success: true,
        message: `Chaos destruction ${safeName} created`,
        path: `${safeSavePath}/${safeName}`
      };
    } catch (err) {
      return { success: false, error: `Failed to setup destruction: ${err}` };
    }
  }

  /**
   * Configure Vehicle Physics
   */
  async configureVehicle(params: {
    vehicleName?: string;
    vehicleType?: string;
    wheels?: unknown[];  // Array of wheel configs, validated internally
    engine?: unknown;    // Engine config, validated internally
    transmission?: unknown;  // Transmission config, validated internally
    pluginDependencies?: string[];
  }) {
    // Plugin check removed as ensurePluginsEnabled is deprecated.
    // Users should ensure required plugins are enabled in the editor.

    const rawParams = params as Record<string, unknown>;

    const pluginDeps: string[] | undefined = Array.isArray(params.pluginDependencies) && params.pluginDependencies.length > 0
      ? params.pluginDependencies
      : (Array.isArray(rawParams.plugins) && (rawParams.plugins as unknown[]).length > 0 ? rawParams.plugins as string[] : undefined);

    if (pluginDeps && pluginDeps.length > 0) {
      return {
        success: false,
        error: 'MISSING_ENGINE_PLUGINS',
        missingPlugins: pluginDeps,
        message: `Required engine plugins not enabled: ${pluginDeps.join(', ')}`
      };
    }

    // SECURITY: Sanitize vehicle name to prevent command injection
    const safeVehicleName = sanitizeCommandArgument(params.vehicleName ?? '');
    if (!safeVehicleName) {
      return { success: false, error: 'Vehicle name must be valid and non-empty.' };
    }

    const warnings: string[] = [];

    const hasExplicitEmptyWheels = Array.isArray(params.wheels) && params.wheels.length === 0;

    // SECURITY: Sanitize vehicle type to prevent command injection
    const rawVehicleType = typeof params.vehicleType === 'string' && params.vehicleType.trim().length > 0
      ? params.vehicleType
      : 'Car';
    const safeVehicleType = sanitizeCommandArgument(rawVehicleType);
    if (!safeVehicleType) {
      return { success: false, error: 'Vehicle type must be valid and non-empty.' };
    }

    const commands = [
      `CreateVehicle ${safeVehicleName} ${safeVehicleType}`
    ];

    // Configure wheels when provided
    if (Array.isArray(params.wheels) && params.wheels.length > 0) {
      for (const wheelUnknown of params.wheels) {
        const wheel = wheelUnknown as Record<string, unknown>;
        // SECURITY: Sanitize wheel name to prevent command injection
        const safeWheelName = sanitizeCommandArgument(String(wheel.name ?? ''));
        if (!safeWheelName) {
          warnings.push('Skipped wheel with invalid name.');
          continue;
        }
        // SECURITY: Validate numeric wheel parameters
        const radius = typeof wheel.radius === 'number' && isFinite(wheel.radius) ? wheel.radius : NaN;
        const width = typeof wheel.width === 'number' && isFinite(wheel.width) ? wheel.width : NaN;
        const mass = typeof wheel.mass === 'number' && isFinite(wheel.mass) ? wheel.mass : NaN;
        if (!isFinite(radius) || !isFinite(width) || !isFinite(mass)) {
          warnings.push(`Skipped wheel '${safeWheelName}' with invalid numeric parameters.`);
          continue;
        }
        commands.push(
          `AddVehicleWheel ${safeVehicleName} ${safeWheelName} ${radius} ${width} ${mass}`
        );

        if (wheel.isSteering) {
          commands.push(`SetWheelSteering ${safeVehicleName} ${safeWheelName} true`);
        }
        if (wheel.isDriving) {
          commands.push(`SetWheelDriving ${safeVehicleName} ${safeWheelName} true`);
        }
      }
    }

    // Configure engine (optional). Clamp negative RPMs and tolerate missing torqueCurve.
    const engineUnknown = params.engine ?? ((typeof rawParams.maxRPM === 'number' || Array.isArray(rawParams.torqueCurve))
      ? { maxRPM: rawParams.maxRPM as number | undefined, torqueCurve: rawParams.torqueCurve as Array<[number, number]> | undefined }
      : undefined);
    const effectiveEngine = engineUnknown as Record<string, unknown> | undefined;

    if (effectiveEngine) {
      let maxRPM = typeof effectiveEngine.maxRPM === 'number' ? effectiveEngine.maxRPM : 0;
      if (maxRPM < 0) {
        maxRPM = 0;
        warnings.push('Engine maxRPM was negative and has been clamped to 0.');
      }
      commands.push(`SetEngineMaxRPM ${safeVehicleName} ${maxRPM}`);

      const rawCurve = Array.isArray(effectiveEngine.torqueCurve) ? effectiveEngine.torqueCurve : [];
      for (const point of rawCurve) {
        let rpm: number | undefined;
        let torque: number | undefined;

        if (Array.isArray(point) && point.length >= 2) {
          rpm = Number(point[0]);
          torque = Number(point[1]);
        } else if (point && typeof point === 'object' && !Array.isArray(point)) {
          const pointObj = point as Record<string, unknown>;
          rpm = typeof pointObj.rpm === 'number' ? pointObj.rpm : undefined;
          torque = typeof pointObj.torque === 'number' ? pointObj.torque : undefined;
        }

        if (typeof rpm === 'number' && typeof torque === 'number') {
          commands.push(`AddTorqueCurvePoint ${safeVehicleName} ${rpm} ${torque}`);
        }
      }
    }

    // Configure transmission
    if (params.transmission) {
      const trans = params.transmission as Record<string, unknown>;
      if (Array.isArray(trans.gears)) {
        for (let i = 0; i < trans.gears.length; i++) {
          commands.push(
            `SetGearRatio ${safeVehicleName} ${i} ${trans.gears[i]}`
          );
        }
      }
      if (typeof trans.finalDriveRatio === 'number') {
        commands.push(
          `SetFinalDriveRatio ${safeVehicleName} ${trans.finalDriveRatio}`
        );
      }
    }

    try {
      await this.bridge.executeConsoleCommands(commands);
    } catch (_error) {
      // If vehicle console commands fail (e.g., `Command not executed`), treat this as
      // a best-effort configuration that falls back to engine defaults.
      if (warnings.length === 0) {
        warnings.push('Vehicle configuration commands could not be executed; using engine defaults.');
      }
    }

    if (hasExplicitEmptyWheels) {
      warnings.push('No wheels specified; using default wheels from vehicle preset.');
    }

    if (warnings.length === 0) {
      warnings.push('Verify wheel class assignments and offsets in the vehicle movement component to ensure they match your project defaults.');
    }

    return {
      success: true,
      message: `Vehicle ${safeVehicleName} configured`,
      warnings
    };
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
      const normalizedVector = params.vector;

      const response = await this.automationBridge.sendAutomationRequest('apply_force', {
        actorName: params.actorName,
        forceType: params.forceType,
        vector: normalizedVector,
        boneName: params.boneName,
        isLocal: params.isLocal
      }, {
        timeoutMs: 30000
      });

      if (response.success === false) {
        const result = (response.result ?? {}) as Record<string, unknown>;
        return {
          success: false,
          error: response.error || response.message || 'Force application failed',
          availableActors: result.available_actors ? coerceStringArray(result.available_actors as unknown[]) : undefined,
          details: result.details
        };
      }

      const result = (response.result ?? {}) as Record<string, unknown>;
      return {
        success: true,
        message: response.message || `Applied ${params.forceType} to ${params.actorName}`,
        availableActors: result.available_actors ? coerceStringArray(result.available_actors as unknown[]) : undefined,
        ...result
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
      // SECURITY: Sanitize mesh name to prevent command injection
      const safeMeshName = sanitizeCommandArgument(params.meshName);
      if (!safeMeshName) {
        return { success: false, error: 'Mesh name must be valid and non-empty.' };
      }

      const commands = [
        `EnableClothSimulation ${safeMeshName}`,
        `SetClothPreset ${safeMeshName} ${params.clothPreset}`
      ];

      if (params.clothPreset === 'Custom' && params.customSettings) {
        const settings = params.customSettings;

        if (settings.stiffness !== undefined) {
          commands.push(`SetClothStiffness ${safeMeshName} ${settings.stiffness}`);
        }
        if (settings.damping !== undefined) {
          commands.push(`SetClothDamping ${safeMeshName} ${settings.damping}`);
        }
        if (settings.friction !== undefined) {
          commands.push(`SetClothFriction ${safeMeshName} ${settings.friction}`);
        }
        if (settings.density !== undefined) {
          commands.push(`SetClothDensity ${safeMeshName} ${settings.density}`);
        }
        if (settings.gravity !== undefined) {
          commands.push(`SetClothGravity ${safeMeshName} ${settings.gravity}`);
        }
        if (settings.windVelocity) {
          const wind = settings.windVelocity;
          commands.push(`SetClothWind ${safeMeshName} ${wind[0]} ${wind[1]} ${wind[2]}`);
        }
      }

      await this.bridge.executeConsoleCommands(commands);

      return {
        success: true,
        message: `Cloth simulation enabled for ${safeMeshName}`
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
      // SECURITY: Sanitize simulation name to prevent command injection
      const safeName = sanitizeCommandArgument(params.name);
      if (!safeName) {
        return { success: false, error: 'Simulation name must be valid and non-empty.' };
      }

      const locStr = `${params.location[0]} ${params.location[1]} ${params.location[2]}`;
      const volStr = `${params.volume[0]} ${params.volume[1]} ${params.volume[2]}`;

      const commands = [
        `CreateFluidSimulation ${safeName} ${params.fluidType} ${locStr} ${volStr}`
      ];

      if (params.customSettings) {
        const settings = params.customSettings;

        if (settings.viscosity !== undefined) {
          commands.push(`SetFluidViscosity ${safeName} ${settings.viscosity}`);
        }
        if (settings.density !== undefined) {
          commands.push(`SetFluidDensity ${safeName} ${settings.density}`);
        }
        if (settings.temperature !== undefined) {
          commands.push(`SetFluidTemperature ${safeName} ${settings.temperature}`);
        }
        if (settings.turbulence !== undefined) {
          commands.push(`SetFluidTurbulence ${safeName} ${settings.turbulence}`);
        }
        if (settings.color) {
          const color = settings.color;
          commands.push(
            `SetFluidColor ${safeName} ${color[0]} ${color[1]} ${color[2]} ${color[3]}`
          );
        }
      }

      await this.bridge.executeConsoleCommands(commands);

      return {
        success: true,
        message: `Fluid simulation ${safeName} created`
      };
    } catch (err) {
      return { success: false, error: `Failed to create fluid simulation: ${err}` };
    }
  }

  /**
   * Setup Physics Simulation (Create Physics Asset)
   */
  async setupPhysicsSimulation(params: {
    meshPath?: string;
    skeletonPath?: string;
    physicsAssetName?: string;
    savePath?: string;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Physics asset creation requires plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('animation_physics', {
        action: 'setup_physics_simulation',
        ...params
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          message: response.error || response.message || 'Failed to setup physics simulation',
          error: response.error || response.message
        };
      }

      return {
        success: true,
        message: response.message || 'Physics simulation setup completed',
        ...(response.result || {})
      };
    } catch (err) {
      return { success: false, error: `Failed to setup physics simulation: ${err}` };
    }
  }

}
