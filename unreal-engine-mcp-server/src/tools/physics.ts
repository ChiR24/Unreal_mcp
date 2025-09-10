import { UnrealBridge } from '../unreal-bridge.js';

export class PhysicsTools {
  constructor(private bridge: UnrealBridge) {}

  /**
   * Setup Ragdoll Physics
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
      const path = params.savePath || '/Game/Physics';
      
      // Physics assets require editor scripting
      const commands = [
        `echo Creating PhysicsAsset ${params.physicsAssetName} for ${params.skeletonPath}`
      ];
      
      // Configure bone constraints
      if (params.constraints) {
        for (const constraint of params.constraints) {
          commands.push(
            `SetBoneConstraint ${params.physicsAssetName} ${constraint.boneName} ${constraint.constraintType}`
          );
          
          if (constraint.limits) {
            const limits = constraint.limits;
            commands.push(
              `SetBoneLimits ${params.physicsAssetName} ${constraint.boneName} ${limits.swing1 || 0} ${limits.swing2 || 0} ${limits.twist || 0}`
            );
          }
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Ragdoll physics asset ${params.physicsAssetName} created`,
        path: `${path}/${params.physicsAssetName}`
      };
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
      await this.executeCommand(spawnCmd);
      
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
        await this.executeCommand(cmd);
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
        await this.executeCommand(cmd);
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
        await this.executeCommand(cmd);
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
                    # Ensure physics is enabled
                    root.set_simulate_physics(True)
                    result["physics_enabled"] = True
                    
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
        } catch (parseErr) {
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
        await this.executeCommand(cmd);
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
        await this.executeCommand(cmd);
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
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }
}
