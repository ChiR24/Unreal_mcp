import { UnrealBridge } from '../unreal-bridge.js';

export class NiagaraTools {
  constructor(private bridge: UnrealBridge) {}

  /**
   * Create Niagara System
   */
  async createSystem(params: {
    name: string;
    savePath?: string;
    template?: 'Empty' | 'Fountain' | 'Ambient' | 'Projectile' | 'Custom';
    emitters?: Array<{
      name: string;
      spawnRate?: number;
      lifetime?: number;
      shape?: 'Point' | 'Sphere' | 'Box' | 'Cylinder' | 'Cone';
      shapeSize?: [number, number, number];
    }>;
  }) {
    try {
      const path = params.savePath || '/Game/Effects/Niagara';
      
      const commands = [
        `CreateNiagaraSystem ${params.name} ${path} ${params.template || 'Empty'}`
      ];
      
      // Add emitters
      if (params.emitters) {
        for (const emitter of params.emitters) {
          commands.push(
            `AddNiagaraEmitter ${params.name} ${emitter.name}`
          );
          
          if (emitter.spawnRate) {
            commands.push(
              `SetEmitterSpawnRate ${params.name} ${emitter.name} ${emitter.spawnRate}`
            );
          }
          
          if (emitter.lifetime) {
            commands.push(
              `SetEmitterLifetime ${params.name} ${emitter.name} ${emitter.lifetime}`
            );
          }
          
          if (emitter.shape && emitter.shapeSize) {
            const size = emitter.shapeSize;
            commands.push(
              `SetEmitterShape ${params.name} ${emitter.name} ${emitter.shape} ${size[0]} ${size[1]} ${size[2]}`
            );
          }
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Niagara system ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create Niagara system: ${err}` };
    }
  }

  /**
   * Add Emitter to System
   */
  async addEmitter(params: {
    systemName: string;
    emitterName: string;
    emitterType: 'Sprite' | 'Mesh' | 'Ribbon' | 'Beam' | 'GPU';
    properties?: {
      spawnRate?: number;
      lifetime?: number;
      velocityMin?: [number, number, number];
      velocityMax?: [number, number, number];
      size?: number;
      color?: [number, number, number, number];
      material?: string;
      mesh?: string;
    };
  }) {
    try {
      const commands = [
        `AddNiagaraEmitter ${params.systemName} ${params.emitterName} ${params.emitterType}`
      ];
      
      if (params.properties) {
        const props = params.properties;
        
        if (props.spawnRate !== undefined) {
          commands.push(
            `SetEmitterSpawnRate ${params.systemName} ${params.emitterName} ${props.spawnRate}`
          );
        }
        
        if (props.lifetime !== undefined) {
          commands.push(
            `SetEmitterLifetime ${params.systemName} ${params.emitterName} ${props.lifetime}`
          );
        }
        
        if (props.velocityMin && props.velocityMax) {
          const min = props.velocityMin;
          const max = props.velocityMax;
          commands.push(
            `SetEmitterVelocity ${params.systemName} ${params.emitterName} ${min[0]} ${min[1]} ${min[2]} ${max[0]} ${max[1]} ${max[2]}`
          );
        }
        
        if (props.size !== undefined) {
          commands.push(
            `SetEmitterSize ${params.systemName} ${params.emitterName} ${props.size}`
          );
        }
        
        if (props.color) {
          const color = props.color;
          commands.push(
            `SetEmitterColor ${params.systemName} ${params.emitterName} ${color[0]} ${color[1]} ${color[2]} ${color[3]}`
          );
        }
        
        if (props.material) {
          commands.push(
            `SetEmitterMaterial ${params.systemName} ${params.emitterName} ${props.material}`
          );
        }
        
        if (props.mesh && params.emitterType === 'Mesh') {
          commands.push(
            `SetEmitterMesh ${params.systemName} ${params.emitterName} ${props.mesh}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Emitter ${params.emitterName} added to ${params.systemName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to add emitter: ${err}` };
    }
  }

  /**
   * Set System Parameter
   */
  async setParameter(params: {
    systemName: string;
    parameterName: string;
    parameterType: 'Float' | 'Vector' | 'Color' | 'Bool' | 'Int';
    value: any;
    isUserParameter?: boolean;
  }) {
    try {
      const paramType = params.isUserParameter ? 'User' : 'System';
      let valueStr = '';
      
      switch (params.parameterType) {
        case 'Float':
        case 'Int':
        case 'Bool':
          valueStr = String(params.value);
          break;
        case 'Vector':
          const vec = params.value as number[];
          valueStr = `${vec[0]} ${vec[1]} ${vec[2]}`;
          break;
        case 'Color':
          const col = params.value as number[];
          valueStr = `${col[0]} ${col[1]} ${col[2]} ${col[3] || 1}`;
          break;
      }
      
      const command = `SetNiagara${paramType}Parameter ${params.systemName} ${params.parameterName} ${params.parameterType} ${valueStr}`;
      await this.executeCommand(command);
      
      return { 
        success: true, 
        message: `Parameter ${params.parameterName} set on ${params.systemName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to set parameter: ${err}` };
    }
  }

  /**
   * Create Preset Effect
   */
  async createEffect(params: {
    effectType: 'Fire' | 'Smoke' | 'Explosion' | 'Water' | 'Rain' | 'Snow' | 'Magic' | 'Lightning' | 'Dust' | 'Steam';
    name: string;
    location: [number, number, number];
    scale?: number;
    intensity?: number;
    customParameters?: {
      [key: string]: any;
    };
  }) {
    try {
      const locStr = `${params.location[0]} ${params.location[1]} ${params.location[2]}`;
      
      const commands = [
        `CreateNiagaraEffect ${params.effectType} ${params.name} ${locStr}`
      ];
      
      if (params.scale !== undefined) {
        commands.push(`SetEffectScale ${params.name} ${params.scale}`);
      }
      
      if (params.intensity !== undefined) {
        commands.push(`SetEffectIntensity ${params.name} ${params.intensity}`);
      }
      
      // Apply effect-specific parameters based on type
      switch (params.effectType) {
        case 'Fire':
          commands.push(`SetFireHeight ${params.name} ${params.customParameters?.flameHeight || 150}`);
          commands.push(`SetFireRadius ${params.name} ${params.customParameters?.flameRadius || 50}`);
          commands.push(`SetFireSparkCount ${params.name} ${params.customParameters?.sparkCount || 100}`);
          break;
          
        case 'Smoke':
          commands.push(`SetSmokeVelocity ${params.name} ${params.customParameters?.velocity || 100}`);
          commands.push(`SetSmokeDensity ${params.name} ${params.customParameters?.density || 0.5}`);
          commands.push(`SetSmokeColor ${params.name} ${params.customParameters?.color || '0.2 0.2 0.2 0.8'}`);
          break;
          
        case 'Explosion':
          commands.push(`SetExplosionRadius ${params.name} ${params.customParameters?.radius || 500}`);
          commands.push(`SetExplosionForce ${params.name} ${params.customParameters?.force || 1000}`);
          commands.push(`SetExplosionDebrisCount ${params.name} ${params.customParameters?.debrisCount || 50}`);
          break;
          
        case 'Water':
          commands.push(`SetWaterFlowRate ${params.name} ${params.customParameters?.flowRate || 100}`);
          commands.push(`SetWaterSplashSize ${params.name} ${params.customParameters?.splashSize || 50}`);
          commands.push(`SetWaterTransparency ${params.name} ${params.customParameters?.transparency || 0.7}`);
          break;
          
        case 'Rain':
          commands.push(`SetRainIntensity ${params.name} ${params.customParameters?.intensity || 1000}`);
          commands.push(`SetRainArea ${params.name} ${params.customParameters?.area || '1000 1000 500'}`);
          commands.push(`SetRainDropSize ${params.name} ${params.customParameters?.dropSize || 2}`);
          break;
          
        case 'Snow':
          commands.push(`SetSnowIntensity ${params.name} ${params.customParameters?.intensity || 500}`);
          commands.push(`SetSnowArea ${params.name} ${params.customParameters?.area || '2000 2000 1000'}`);
          commands.push(`SetSnowFlakeSize ${params.name} ${params.customParameters?.flakeSize || 5}`);
          commands.push(`SetSnowWindEffect ${params.name} ${params.customParameters?.windStrength || 0.3}`);
          break;
          
        case 'Magic':
          commands.push(`SetMagicColor ${params.name} ${params.customParameters?.color || '0.5 0 1 1'}`);
          commands.push(`SetMagicParticleCount ${params.name} ${params.customParameters?.particleCount || 200}`);
          commands.push(`SetMagicGlowIntensity ${params.name} ${params.customParameters?.glowIntensity || 2}`);
          break;
          
        case 'Lightning':
          commands.push(`SetLightningBranches ${params.name} ${params.customParameters?.branches || 5}`);
          commands.push(`SetLightningLength ${params.name} ${params.customParameters?.length || 1000}`);
          commands.push(`SetLightningIntensity ${params.name} ${params.customParameters?.intensity || 10}`);
          break;
          
        case 'Dust':
          commands.push(`SetDustDensity ${params.name} ${params.customParameters?.density || 0.3}`);
          commands.push(`SetDustArea ${params.name} ${params.customParameters?.area || '500 500 200'}`);
          commands.push(`SetDustColor ${params.name} ${params.customParameters?.color || '0.7 0.6 0.5 0.5'}`);
          break;
          
        case 'Steam':
          commands.push(`SetSteamVelocity ${params.name} ${params.customParameters?.velocity || 150}`);
          commands.push(`SetSteamDissipation ${params.name} ${params.customParameters?.dissipation || 2}`);
          commands.push(`SetSteamTemperature ${params.name} ${params.customParameters?.temperature || 100}`);
          break;
      }
      
      // Apply any additional custom parameters
      if (params.customParameters) {
        for (const [key, value] of Object.entries(params.customParameters)) {
          if (!['flameHeight', 'flameRadius', 'sparkCount', 'velocity', 'density', 'color', 
                'radius', 'force', 'debrisCount', 'flowRate', 'splashSize', 'transparency',
                'intensity', 'area', 'dropSize', 'flakeSize', 'windStrength', 'particleCount',
                'glowIntensity', 'branches', 'length', 'dissipation', 'temperature'].includes(key)) {
            commands.push(`SetEffectParameter ${params.name} ${key} ${value}`);
          }
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `${params.effectType} effect ${params.name} created` 
      };
    } catch (err) {
      return { success: false, error: `Failed to create effect: ${err}` };
    }
  }

  /**
   * Create GPU Simulation
   */
  async createGPUSimulation(params: {
    name: string;
    simulationType: 'Fluid' | 'Hair' | 'Cloth' | 'Debris' | 'Crowd';
    particleCount: number;
    savePath?: string;
    gpuSettings?: {
      computeShader?: string;
      textureFormat?: 'RGBA8' | 'RGBA16F' | 'RGBA32F';
      gridResolution?: [number, number, number];
      iterations?: number;
    };
  }) {
    try {
      const path = params.savePath || '/Game/Effects/GPUSimulations';
      
      const commands = [
        `CreateGPUSimulation ${params.name} ${params.simulationType} ${params.particleCount} ${path}`
      ];
      
      if (params.gpuSettings) {
        const settings = params.gpuSettings;
        
        if (settings.computeShader) {
          commands.push(
            `SetGPUComputeShader ${params.name} ${settings.computeShader}`
          );
        }
        
        if (settings.textureFormat) {
          commands.push(
            `SetGPUTextureFormat ${params.name} ${settings.textureFormat}`
          );
        }
        
        if (settings.gridResolution) {
          const res = settings.gridResolution;
          commands.push(
            `SetGPUGridResolution ${params.name} ${res[0]} ${res[1]} ${res[2]}`
          );
        }
        
        if (settings.iterations !== undefined) {
          commands.push(
            `SetGPUIterations ${params.name} ${settings.iterations}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `GPU simulation ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create GPU simulation: ${err}` };
    }
  }

  /**
   * Spawn Niagara Effect in Level
   */
  async spawnEffect(params: {
    systemPath: string;
    location: [number, number, number];
    rotation?: [number, number, number];
    scale?: [number, number, number];
    autoDestroy?: boolean;
    attachToActor?: string;
  }) {
    try {
      const locStr = `${params.location[0]} ${params.location[1]} ${params.location[2]}`;
      const rotStr = params.rotation ? 
        `${params.rotation[0]} ${params.rotation[1]} ${params.rotation[2]}` : '0 0 0';
      const scaleStr = params.scale ? 
        `${params.scale[0]} ${params.scale[1]} ${params.scale[2]}` : '1 1 1';
      
      let command = `SpawnNiagaraSystem ${params.systemPath} ${locStr} ${rotStr} ${scaleStr}`;
      
      if (params.autoDestroy !== undefined) {
        command += ` ${params.autoDestroy}`;
      }
      
      if (params.attachToActor) {
        command = `SpawnNiagaraAttached ${params.systemPath} ${params.attachToActor} ${locStr} ${rotStr} ${scaleStr}`;
      }
      
      await this.executeCommand(command);
      
      return { 
        success: true, 
        message: `Niagara effect spawned at location` 
      };
    } catch (err) {
      return { success: false, error: `Failed to spawn effect: ${err}` };
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
