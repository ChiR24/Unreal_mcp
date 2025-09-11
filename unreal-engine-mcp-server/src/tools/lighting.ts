// Lighting tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class LightingTools {
  constructor(private bridge: UnrealBridge) {}

  // Helper to safely escape strings for Python
  private escapePythonString(str: string): string {
    // Escape backslashes first, then quotes
    return str.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
  }

  private ensurePythonSpawnSucceeded(label: string, result: any) {
    const logs = Array.isArray(result?.LogOutput)
      ? result.LogOutput.map((l: any) => String(l.Output || '')).join('')
      : '';

    // If Python reported a traceback or explicit failure, propagate as error
    if (/Traceback|Error:|Failed to spawn/i.test(logs)) {
      throw new Error(`Unreal reported error spawning '${label}': ${logs}`);
    }

    // If script executed (ReturnValue true) and no error patterns, treat as success
    const executed = result?.ReturnValue === true || result?.ReturnValue === 'true';
    if (executed) return;

    // Fallback: if no ReturnValue but success-like logs exist, accept
    if (/spawned/i.test(logs)) return;

    // Otherwise, uncertain
    throw new Error(`Uncertain spawn result for '${label}'. Engine logs:\n${logs}`);
  }

  // Execute console command
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

  // Create directional light
  async createDirectionalLight(params: {
    name: string;
    intensity?: number;
    color?: [number, number, number];
    rotation?: [number, number, number];
    castShadows?: boolean;
    temperature?: number;
  }) {
    // Validate name
    if (!params.name || typeof params.name !== 'string' || params.name.trim() === '') {
      throw new Error('Invalid name: must be a non-empty string');
    }
    
    // Validate numeric parameters
    if (params.intensity !== undefined) {
      if (typeof params.intensity !== 'number' || !isFinite(params.intensity)) {
        throw new Error(`Invalid intensity value: ${params.intensity}`);
      }
      if (params.intensity < 0) {
        throw new Error('Invalid intensity: must be non-negative');
      }
    }
    
    if (params.temperature !== undefined) {
      if (typeof params.temperature !== 'number' || !isFinite(params.temperature)) {
        throw new Error(`Invalid temperature value: ${params.temperature}`);
      }
    }
    
    // Validate arrays
    if (params.color !== undefined) {
      if (!Array.isArray(params.color) || params.color.length !== 3) {
        throw new Error('Invalid color: must be an array [r,g,b]');
      }
      for (const c of params.color) {
        if (typeof c !== 'number' || !isFinite(c)) {
          throw new Error('Invalid color component: must be finite numbers');
        }
      }
    }
    
    if (params.rotation !== undefined) {
      if (!Array.isArray(params.rotation) || params.rotation.length !== 3) {
        throw new Error('Invalid rotation: must be an array [pitch,yaw,roll]');
      }
      for (const r of params.rotation) {
        if (typeof r !== 'number' || !isFinite(r)) {
          throw new Error('Invalid rotation component: must be finite numbers');
        }
      }
    }
    
    const rot = params.rotation || [0, 0, 0];
    
    // Build property setters
    const propSetters: string[] = [];
    if (params.intensity !== undefined) {
      propSetters.push(`        light_component.set_intensity(${params.intensity})`);
    }
    if (params.color) {
      propSetters.push(`        light_component.set_light_color(unreal.LinearColor(${params.color[0]}, ${params.color[1]}, ${params.color[2]}, 1.0))`);
    }
    if (params.castShadows !== undefined) {
      propSetters.push(`        light_component.set_cast_shadows(${params.castShadows ? 'True' : 'False'})`);
    }
    if (params.temperature !== undefined) {
      propSetters.push(`        light_component.set_temperature(${params.temperature})`);
    }
    
    const propertiesCode = propSetters.length > 0 
      ? propSetters.join('\n') 
      : '        pass  # No additional properties';

    const pythonScript = `
import unreal

# Get editor subsystem
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Spawn the directional light
directional_light_class = unreal.DirectionalLight
spawn_location = unreal.Vector(0, 0, 500)
spawn_rotation = unreal.Rotator(${rot[0]}, ${rot[1]}, ${rot[2]})

# Spawn the actor
spawned_light = editor_actor_subsystem.spawn_actor_from_class(
    directional_light_class, 
    spawn_location, 
    spawn_rotation
)

if spawned_light:
    # Set the label/name
    spawned_light.set_actor_label("${this.escapePythonString(params.name)}")
    
    # Get the light component
    light_component = spawned_light.get_component_by_class(unreal.DirectionalLightComponent)
    
    if light_component:
${propertiesCode}
    
    print("Directional light '${this.escapePythonString(params.name)}' spawned")
else:
    print("Failed to spawn directional light '${this.escapePythonString(params.name)}'")
`;

    // Execute the Python script via Remote Control
    const result = await this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/PythonScriptPlugin.Default__PythonScriptLibrary',
      functionName: 'ExecutePythonScript',
      parameters: {
        PythonScript: pythonScript
      },
      generateTransaction: false
    });

    this.ensurePythonSpawnSucceeded(params.name, result);
    return { success: true, message: `Directional light '${params.name}' spawned` };
  }

  // Create point light
  async createPointLight(params: {
    name: string;
    location?: [number, number, number];
    intensity?: number;
    radius?: number;
    color?: [number, number, number];
    falloffExponent?: number;
    castShadows?: boolean;
  }) {
    // Validate name
    if (!params.name || typeof params.name !== 'string' || params.name.trim() === '') {
      throw new Error('Invalid name: must be a non-empty string');
    }
    
    // Validate location array
    if (params.location !== undefined) {
      if (!Array.isArray(params.location) || params.location.length !== 3) {
        throw new Error('Invalid location: must be an array [x,y,z]');
      }
      for (const l of params.location) {
        if (typeof l !== 'number' || !isFinite(l)) {
          throw new Error('Invalid location component: must be finite numbers');
        }
      }
    }
    
    // Default location if not provided
    const location = params.location || [0, 0, 0];
    
    // Validate numeric parameters
    if (params.intensity !== undefined) {
      if (typeof params.intensity !== 'number' || !isFinite(params.intensity)) {
        throw new Error(`Invalid intensity value: ${params.intensity}`);
      }
      if (params.intensity < 0) {
        throw new Error('Invalid intensity: must be non-negative');
      }
    }
    if (params.radius !== undefined) {
      if (typeof params.radius !== 'number' || !isFinite(params.radius)) {
        throw new Error(`Invalid radius value: ${params.radius}`);
      }
      if (params.radius < 0) {
        throw new Error('Invalid radius: must be non-negative');
      }
    }
    if (params.falloffExponent !== undefined) {
      if (typeof params.falloffExponent !== 'number' || !isFinite(params.falloffExponent)) {
        throw new Error(`Invalid falloffExponent value: ${params.falloffExponent}`);
      }
    }
    
    // Validate color array
    if (params.color !== undefined) {
      if (!Array.isArray(params.color) || params.color.length !== 3) {
        throw new Error('Invalid color: must be an array [r,g,b]');
      }
      for (const c of params.color) {
        if (typeof c !== 'number' || !isFinite(c)) {
          throw new Error('Invalid color component: must be finite numbers');
        }
      }
    }
    
    // Build property setters
    const propSetters: string[] = [];
    if (params.intensity !== undefined) {
      propSetters.push(`        light_component.set_intensity(${params.intensity})`);
    }
    if (params.radius !== undefined) {
      propSetters.push(`        light_component.set_attenuation_radius(${params.radius})`);
    }
    if (params.color) {
      propSetters.push(`        light_component.set_light_color(unreal.LinearColor(${params.color[0]}, ${params.color[1]}, ${params.color[2]}, 1.0))`);
    }
    if (params.castShadows !== undefined) {
      propSetters.push(`        light_component.set_cast_shadows(${params.castShadows ? 'True' : 'False'})`);
    }
    if (params.falloffExponent !== undefined) {
      propSetters.push(`        light_component.set_light_falloff_exponent(${params.falloffExponent})`);
    }
    
    const propertiesCode = propSetters.length > 0 
      ? propSetters.join('\n') 
      : '        pass  # No additional properties';

    const pythonScript = `
import unreal

# Get editor subsystem
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Spawn the point light
point_light_class = unreal.PointLight
spawn_location = unreal.Vector(${location[0]}, ${location[1]}, ${location[2]})
spawn_rotation = unreal.Rotator(0, 0, 0)

# Spawn the actor
spawned_light = editor_actor_subsystem.spawn_actor_from_class(
    point_light_class, 
    spawn_location, 
    spawn_rotation
)

if spawned_light:
    # Set the label/name
    spawned_light.set_actor_label("${this.escapePythonString(params.name)}")
    
    # Get the light component
    light_component = spawned_light.get_component_by_class(unreal.PointLightComponent)
    
    if light_component:
${propertiesCode}
    
    print("Point light '${this.escapePythonString(params.name)}' spawned at (" + str(spawn_location) + ")")
else:
    print("Failed to spawn point light '${this.escapePythonString(params.name)}'")
`;

    // Execute the Python script via Remote Control
    const result = await this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/PythonScriptPlugin.Default__PythonScriptLibrary',
      functionName: 'ExecutePythonScript',
      parameters: {
        PythonScript: pythonScript
      },
      generateTransaction: false
    });

    this.ensurePythonSpawnSucceeded(params.name, result);
    return { success: true, message: `Point light '${params.name}' spawned at ${location.join(', ')}` };
  }

  // Create spot light
  async createSpotLight(params: {
    name: string;
    location: [number, number, number];
    rotation: [number, number, number];
    intensity?: number;
    innerCone?: number;
    outerCone?: number;
    radius?: number;
    color?: [number, number, number];
    castShadows?: boolean;
  }) {
    // Validate name
    if (!params.name || typeof params.name !== 'string' || params.name.trim() === '') {
      throw new Error('Invalid name: must be a non-empty string');
    }
    
    // Validate required location and rotation arrays
    if (!params.location || !Array.isArray(params.location) || params.location.length !== 3) {
      throw new Error('Invalid location: must be an array [x,y,z]');
    }
    for (const l of params.location) {
      if (typeof l !== 'number' || !isFinite(l)) {
        throw new Error('Invalid location component: must be finite numbers');
      }
    }
    
    if (!params.rotation || !Array.isArray(params.rotation) || params.rotation.length !== 3) {
      throw new Error('Invalid rotation: must be an array [pitch,yaw,roll]');
    }
    for (const r of params.rotation) {
      if (typeof r !== 'number' || !isFinite(r)) {
        throw new Error('Invalid rotation component: must be finite numbers');
      }
    }
    
    // Validate optional numeric parameters
    if (params.intensity !== undefined) {
      if (typeof params.intensity !== 'number' || !isFinite(params.intensity)) {
        throw new Error(`Invalid intensity value: ${params.intensity}`);
      }
      if (params.intensity < 0) {
        throw new Error('Invalid intensity: must be non-negative');
      }
    }
    
    if (params.innerCone !== undefined) {
      if (typeof params.innerCone !== 'number' || !isFinite(params.innerCone)) {
        throw new Error(`Invalid innerCone value: ${params.innerCone}`);
      }
      if (params.innerCone < 0 || params.innerCone > 180) {
        throw new Error('Invalid innerCone: must be between 0 and 180 degrees');
      }
    }
    
    if (params.outerCone !== undefined) {
      if (typeof params.outerCone !== 'number' || !isFinite(params.outerCone)) {
        throw new Error(`Invalid outerCone value: ${params.outerCone}`);
      }
      if (params.outerCone < 0 || params.outerCone > 180) {
        throw new Error('Invalid outerCone: must be between 0 and 180 degrees');
      }
    }
    
    if (params.radius !== undefined) {
      if (typeof params.radius !== 'number' || !isFinite(params.radius)) {
        throw new Error(`Invalid radius value: ${params.radius}`);
      }
      if (params.radius < 0) {
        throw new Error('Invalid radius: must be non-negative');
      }
    }
    
    // Validate color array
    if (params.color !== undefined) {
      if (!Array.isArray(params.color) || params.color.length !== 3) {
        throw new Error('Invalid color: must be an array [r,g,b]');
      }
      for (const c of params.color) {
        if (typeof c !== 'number' || !isFinite(c)) {
          throw new Error('Invalid color component: must be finite numbers');
        }
      }
    }
    // Build property setters
    const propSetters: string[] = [];
    if (params.intensity !== undefined) {
      propSetters.push(`        light_component.set_intensity(${params.intensity})`);
    }
    if (params.innerCone !== undefined) {
      propSetters.push(`        light_component.set_inner_cone_angle(${params.innerCone})`);
    }
    if (params.outerCone !== undefined) {
      propSetters.push(`        light_component.set_outer_cone_angle(${params.outerCone})`);
    }
    if (params.radius !== undefined) {
      propSetters.push(`        light_component.set_attenuation_radius(${params.radius})`);
    }
    if (params.color) {
      propSetters.push(`        light_component.set_light_color(unreal.LinearColor(${params.color[0]}, ${params.color[1]}, ${params.color[2]}, 1.0))`);
    }
    if (params.castShadows !== undefined) {
      propSetters.push(`        light_component.set_cast_shadows(${params.castShadows ? 'True' : 'False'})`);
    }
    
    const propertiesCode = propSetters.length > 0 
      ? propSetters.join('\n') 
      : '        pass  # No additional properties';

    const pythonScript = `
import unreal

# Get editor subsystem
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Spawn the spot light
spot_light_class = unreal.SpotLight
spawn_location = unreal.Vector(${params.location[0]}, ${params.location[1]}, ${params.location[2]})
spawn_rotation = unreal.Rotator(${params.rotation[0]}, ${params.rotation[1]}, ${params.rotation[2]})

# Spawn the actor
spawned_light = editor_actor_subsystem.spawn_actor_from_class(
    spot_light_class, 
    spawn_location, 
    spawn_rotation
)

if spawned_light:
    # Set the label/name
    spawned_light.set_actor_label("${this.escapePythonString(params.name)}")
    
    # Get the light component
    light_component = spawned_light.get_component_by_class(unreal.SpotLightComponent)
    
    if light_component:
${propertiesCode}
    
    print("Spot light '${this.escapePythonString(params.name)}' spawned at (" + str(spawn_location) + ")")
else:
    print("Failed to spawn spot light '${this.escapePythonString(params.name)}'")
`;

    // Execute the Python script via Remote Control
    const result = await this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/PythonScriptPlugin.Default__PythonScriptLibrary',
      functionName: 'ExecutePythonScript',
      parameters: {
        PythonScript: pythonScript
      },
      generateTransaction: false
    });

    this.ensurePythonSpawnSucceeded(params.name, result);
    return { success: true, message: `Spot light '${params.name}' spawned at ${params.location.join(', ')}` };
  }

  // Create rect light
  async createRectLight(params: {
    name: string;
    location: [number, number, number];
    rotation: [number, number, number];
    width?: number;
    height?: number;
    intensity?: number;
    color?: [number, number, number];
  }) {
    // Validate name
    if (!params.name || typeof params.name !== 'string' || params.name.trim() === '') {
      throw new Error('Invalid name: must be a non-empty string');
    }
    
    // Validate required location and rotation arrays
    if (!params.location || !Array.isArray(params.location) || params.location.length !== 3) {
      throw new Error('Invalid location: must be an array [x,y,z]');
    }
    for (const l of params.location) {
      if (typeof l !== 'number' || !isFinite(l)) {
        throw new Error('Invalid location component: must be finite numbers');
      }
    }
    
    if (!params.rotation || !Array.isArray(params.rotation) || params.rotation.length !== 3) {
      throw new Error('Invalid rotation: must be an array [pitch,yaw,roll]');
    }
    for (const r of params.rotation) {
      if (typeof r !== 'number' || !isFinite(r)) {
        throw new Error('Invalid rotation component: must be finite numbers');
      }
    }
    
    // Validate optional numeric parameters
    if (params.width !== undefined) {
      if (typeof params.width !== 'number' || !isFinite(params.width)) {
        throw new Error(`Invalid width value: ${params.width}`);
      }
      if (params.width <= 0) {
        throw new Error('Invalid width: must be positive');
      }
    }
    
    if (params.height !== undefined) {
      if (typeof params.height !== 'number' || !isFinite(params.height)) {
        throw new Error(`Invalid height value: ${params.height}`);
      }
      if (params.height <= 0) {
        throw new Error('Invalid height: must be positive');
      }
    }
    
    if (params.intensity !== undefined) {
      if (typeof params.intensity !== 'number' || !isFinite(params.intensity)) {
        throw new Error(`Invalid intensity value: ${params.intensity}`);
      }
      if (params.intensity < 0) {
        throw new Error('Invalid intensity: must be non-negative');
      }
    }
    
    // Validate color array
    if (params.color !== undefined) {
      if (!Array.isArray(params.color) || params.color.length !== 3) {
        throw new Error('Invalid color: must be an array [r,g,b]');
      }
      for (const c of params.color) {
        if (typeof c !== 'number' || !isFinite(c)) {
          throw new Error('Invalid color component: must be finite numbers');
        }
      }
    }
    // Build property setters
    const propSetters: string[] = [];
    if (params.intensity !== undefined) {
      propSetters.push(`        light_component.set_intensity(${params.intensity})`);
    }
    if (params.color) {
      propSetters.push(`        light_component.set_light_color(unreal.LinearColor(${params.color[0]}, ${params.color[1]}, ${params.color[2]}, 1.0))`);
    }
    if (params.width !== undefined) {
      propSetters.push(`        light_component.set_source_width(${params.width})`);
    }
    if (params.height !== undefined) {
      propSetters.push(`        light_component.set_source_height(${params.height})`);
    }
    
    const propertiesCode = propSetters.length > 0 
      ? propSetters.join('\n') 
      : '        pass  # No additional properties';

    const pythonScript = `
import unreal

# Get editor subsystem
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Spawn the rect light
rect_light_class = unreal.RectLight
spawn_location = unreal.Vector(${params.location[0]}, ${params.location[1]}, ${params.location[2]})
spawn_rotation = unreal.Rotator(${params.rotation[0]}, ${params.rotation[1]}, ${params.rotation[2]})

# Spawn the actor
spawned_light = editor_actor_subsystem.spawn_actor_from_class(
    rect_light_class, 
    spawn_location, 
    spawn_rotation
)

if spawned_light:
    # Set the label/name
    spawned_light.set_actor_label("${this.escapePythonString(params.name)}")
    
    # Get the light component
    light_component = spawned_light.get_component_by_class(unreal.RectLightComponent)
    
    if light_component:
${propertiesCode}
    
    print("Rect light '${this.escapePythonString(params.name)}' spawned at (" + str(spawn_location) + ")")
else:
    print("Failed to spawn rect light '${this.escapePythonString(params.name)}'")
`;

    // Execute the Python script via Remote Control
    const result = await this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/PythonScriptPlugin.Default__PythonScriptLibrary',
      functionName: 'ExecutePythonScript',
      parameters: {
        PythonScript: pythonScript
      },
      generateTransaction: false
    });

    this.ensurePythonSpawnSucceeded(params.name, result);
    return { success: true, message: `Rect light '${params.name}' spawned at ${params.location.join(', ')}` };
  }

  // Create sky light
  async createSkyLight(params: {
    name: string;
    sourceType?: 'CapturedScene' | 'SpecifiedCubemap';
    cubemapPath?: string;
    intensity?: number;
    recapture?: boolean;
  }) {
    // Use summon command
    const spawnCmd = `summon SkyLight 0 0 500`;
    await this.bridge.executeConsoleCommand(spawnCmd);
    
    if (params.recapture) {
      await this.bridge.executeConsoleCommand('RecaptureSky');
    }
    
    return { success: true, message: `Sky light spawned` };
  }

  // Setup global illumination
  async setupGlobalIllumination(params: {
    method: 'Lightmass' | 'LumenGI' | 'ScreenSpace' | 'None';
    quality?: 'Low' | 'Medium' | 'High' | 'Epic';
    indirectLightingIntensity?: number;
    bounces?: number;
  }) {
    const commands = [];
    
    switch (params.method) {
      case 'Lightmass':
        commands.push('r.DynamicGlobalIlluminationMethod 0');
        break;
      case 'LumenGI':
        commands.push('r.DynamicGlobalIlluminationMethod 1');
        break;
      case 'ScreenSpace':
        commands.push('r.DynamicGlobalIlluminationMethod 2');
        break;
      case 'None':
        commands.push('r.DynamicGlobalIlluminationMethod 3');
        break;
    }
    
    if (params.quality) {
      const qualityMap = { 'Low': 0, 'Medium': 1, 'High': 2, 'Epic': 3 };
      commands.push(`r.Lumen.Quality ${qualityMap[params.quality]}`);
    }
    
    if (params.indirectLightingIntensity !== undefined) {
      commands.push(`r.IndirectLightingIntensity ${params.indirectLightingIntensity}`);
    }
    
    if (params.bounces !== undefined) {
      commands.push(`r.Lumen.MaxReflectionBounces ${params.bounces}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Global illumination configured' };
  }

  // Configure shadows
  async configureShadows(params: {
    shadowQuality?: 'Low' | 'Medium' | 'High' | 'Epic';
    cascadedShadows?: boolean;
    shadowDistance?: number;
    contactShadows?: boolean;
    rayTracedShadows?: boolean;
  }) {
    const commands = [];
    
    if (params.shadowQuality) {
      const qualityMap = { 'Low': 0, 'Medium': 1, 'High': 2, 'Epic': 3 };
      commands.push(`r.ShadowQuality ${qualityMap[params.shadowQuality]}`);
    }
    
    if (params.cascadedShadows !== undefined) {
      commands.push(`r.Shadow.CSM.MaxCascades ${params.cascadedShadows ? 4 : 1}`);
    }
    
    if (params.shadowDistance !== undefined) {
      commands.push(`r.Shadow.DistanceScale ${params.shadowDistance}`);
    }
    
    if (params.contactShadows !== undefined) {
      commands.push(`r.ContactShadows ${params.contactShadows ? 1 : 0}`);
    }
    
    if (params.rayTracedShadows !== undefined) {
      commands.push(`r.RayTracing.Shadows ${params.rayTracedShadows ? 1 : 0}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Shadow settings configured' };
  }

  // Build lighting
  async buildLighting(params: {
    quality?: 'Preview' | 'Medium' | 'High' | 'Production';
    buildOnlySelected?: boolean;
    buildReflectionCaptures?: boolean;
  }) {
    const qualityMap = {
      'Preview': 0,
      'Medium': 1,
      'High': 2,
      'Production': 3
    };
    
    const commands = [];
    
    if (params.quality) {
      commands.push(`BuildLightingQuality ${qualityMap[params.quality]}`);
    }
    
    if (params.buildOnlySelected) {
      commands.push('BuildLightingOnly Selected');
    } else {
      commands.push('BuildLightingOnly');
    }
    
    if (params.buildReflectionCaptures) {
      commands.push('BuildReflectionCaptures');
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Lighting built successfully' };
  }

  // Create lightmass importance volume
  async createLightmassVolume(params: {
    name: string;
    location: [number, number, number];
    size: [number, number, number];
  }) {
    const command = `SpawnLightmassVolume ${params.name} ${params.location.join(' ')} ${params.size.join(' ')}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Set exposure
  async setExposure(params: {
    method: 'Manual' | 'Auto';
    compensationValue?: number;
    minBrightness?: number;
    maxBrightness?: number;
  }) {
    const commands = [];
    
    commands.push(`r.EyeAdaptation.ExposureMethod ${params.method === 'Manual' ? 0 : 1}`);
    
    if (params.compensationValue !== undefined) {
      commands.push(`r.EyeAdaptation.ExposureCompensation ${params.compensationValue}`);
    }
    
    if (params.minBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MinBrightness ${params.minBrightness}`);
    }
    
    if (params.maxBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MaxBrightness ${params.maxBrightness}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Exposure settings updated' };
  }

  // Set ambient occlusion
  async setAmbientOcclusion(params: {
    enabled: boolean;
    intensity?: number;
    radius?: number;
    quality?: 'Low' | 'Medium' | 'High';
  }) {
    const commands = [];
    
    commands.push(`r.AmbientOcclusion.Enabled ${params.enabled ? 1 : 0}`);
    
    if (params.intensity !== undefined) {
      commands.push(`r.AmbientOcclusion.Intensity ${params.intensity}`);
    }
    
    if (params.radius !== undefined) {
      commands.push(`r.AmbientOcclusion.Radius ${params.radius}`);
    }
    
    if (params.quality) {
      const qualityMap = { 'Low': 0, 'Medium': 1, 'High': 2 };
      commands.push(`r.AmbientOcclusion.Quality ${qualityMap[params.quality]}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Ambient occlusion configured' };
  }

  // Setup volumetric fog
  async setupVolumetricFog(params: {
    enabled: boolean;
    density?: number;
    scatteringIntensity?: number;
    fogHeight?: number;
  }) {
    const commands = [];
    
    commands.push(`r.VolumetricFog ${params.enabled ? 1 : 0}`);
    
    if (params.density !== undefined) {
      commands.push(`r.VolumetricFog.ExtinctionScale ${params.density}`);
    }
    
    if (params.scatteringIntensity !== undefined) {
      commands.push(`r.VolumetricFog.ScatteringIntensity ${params.scatteringIntensity}`);
    }
    
    if (params.fogHeight !== undefined) {
      commands.push(`SetFogHeight ${params.fogHeight}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Volumetric fog configured' };
  }
}
