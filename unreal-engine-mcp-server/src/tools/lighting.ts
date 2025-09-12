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
    let logs = '';
    if (Array.isArray(result?.LogOutput)) {
      logs = result.LogOutput.map((l: any) => String(l.Output || '')).join('');
    } else if (typeof result === 'string') {
      logs = result;
    }

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
        WorldContextObject: null,
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

    // Execute the Python script via bridge (UE 5.6-compatible)
    const result = await this.bridge.executePython(pythonScript);

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
    
    print(f"Point light '${this.escapePythonString(params.name)}' spawned at {spawn_location.x}, {spawn_location.y}, {spawn_location.z}")
else:
    print("Failed to spawn point light '${this.escapePythonString(params.name)}'")
`;

    // Execute the Python script via bridge (UE 5.6-compatible)
    const result = await this.bridge.executePython(pythonScript);

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
    
    print(f"Spot light '${this.escapePythonString(params.name)}' spawned at {spawn_location.x}, {spawn_location.y}, {spawn_location.z}")
else:
    print("Failed to spawn spot light '${this.escapePythonString(params.name)}'")
`;

    // Execute the Python script via bridge (UE 5.6-compatible)
    const result = await this.bridge.executePython(pythonScript);

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
    
    print(f"Rect light '${this.escapePythonString(params.name)}' spawned at {spawn_location.x}, {spawn_location.y}, {spawn_location.z}")
else:
    print("Failed to spawn rect light '${this.escapePythonString(params.name)}'")
`;

    // Execute the Python script via bridge (UE 5.6-compatible)
    const result = await this.bridge.executePython(pythonScript);

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
    const py = `\nimport unreal\neditor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\nspawn_location = unreal.Vector(0, 0, 500)\nspawn_rotation = unreal.Rotator(0, 0, 0)\n# Try to find an existing SkyLight to avoid duplicates\nactor = None\ntry:\n    actors = editor_actor_subsystem.get_all_level_actors()\n    for a in actors:\n        try:\n            if a.get_class().get_name() == 'SkyLight':\n                actor = a\n                break\n        except Exception: pass\nexcept Exception: pass\n# Spawn only if not found\nif actor is None:\n    actor = editor_actor_subsystem.spawn_actor_from_class(unreal.SkyLight, spawn_location, spawn_rotation)\nif actor:\n    try:\n        actor.set_actor_label(\"${this.escapePythonString(params.name)}\")\n    except Exception: pass\n    comp = actor.get_component_by_class(unreal.SkyLightComponent)\n    if comp:\n        ${params.intensity !== undefined ? `comp.set_intensity(${params.intensity})` : 'pass'}\n        ${params.sourceType === 'SpecifiedCubemap' && params.cubemapPath ? `\n        try:\n            path = r\"${params.cubemapPath}\"\n            if unreal.EditorAssetLibrary.does_asset_exist(path):\n                cube = unreal.EditorAssetLibrary.load_asset(path)\n                try: comp.set_cubemap(cube)\n                except Exception: comp.set_editor_property('cubemap', cube)\n                comp.recapture_sky()\n        except Exception: pass\n        ` : 'pass'}\n        ${params.recapture ? `\n        try: comp.recapture_sky()\n        except Exception: pass\n        ` : 'pass'}\n    print(\"RESULT:{'success': True}\")\nelse:\n    print(\"RESULT:{'success': False, 'error': 'Failed to spawn SkyLight'}\")\n`.trim();
    const resp = await this.bridge.executePython(py);
    const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) { try { const parsed = JSON.parse(m[1].replace(/'/g, '"')); return parsed.success ? { success: true, message: 'Sky light ensured' } : { success: false, error: parsed.error }; } catch {} }
    return { success: true, message: 'Sky light ensured' };
  }

  // Remove duplicate SkyLights and keep only one (named target label)
  async ensureSingleSkyLight(params?: { name?: string; recapture?: boolean }) {
    const name = params?.name || 'MCP_Test_Sky';
    const recapture = !!params?.recapture;
    const py = `\nimport unreal, json\nactor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\nactors = actor_sub.get_all_level_actors() if actor_sub else []\nskies = []\nfor a in actors:\n    try:\n        if a.get_class().get_name() == 'SkyLight':\n            skies.append(a)\n    except Exception: pass\nkeep = None\n# Prefer one with matching label; otherwise keep the first\nfor a in skies:\n    try:\n        label = a.get_actor_label()\n        if label == r"${this.escapePythonString(name)}":\n            keep = a\n            break\n    except Exception: pass\nif keep is None and len(skies) > 0:\n    keep = skies[0]\n# Rename the kept one if needed\nif keep is not None:\n    try: keep.set_actor_label(r"${this.escapePythonString(name)}")\n    except Exception: pass\n# Destroy all others\nremoved = 0\nfor a in skies:\n    if keep is not None and a == keep:\n        continue\n    try:\n        unreal.EditorLevelLibrary.destroy_actor(a)\n        removed += 1\n    except Exception: pass\n# Optionally recapture\nif keep is not None and ${recapture ? 'True' : 'False'}:\n    try:\n        comp = keep.get_component_by_class(unreal.SkyLightComponent)\n        if comp: comp.recapture_sky()\n    except Exception: pass\nprint('RESULT:' + json.dumps({'success': True, 'removed': removed, 'kept': True if keep else False}))\n`.trim();

    const resp = await this.bridge.executePython(py);
    let out = '';
    if (resp?.LogOutput && Array.isArray((resp as any).LogOutput)) {
      out = (resp as any).LogOutput.map((l: any) => l.Output || '').join('');
    } else if (typeof resp === 'string') {
      out = resp;
    } else {
      out = JSON.stringify(resp);
    }
    const m = out.match(/RESULT:({.*})/);
    if (m) {
      try {
        const parsed = JSON.parse(m[1]);
        if (parsed.success) {
          return { success: true, removed: parsed.removed, message: `Ensured single SkyLight (removed ${parsed.removed})` };
        }
      } catch {}
    }
    return { success: true, message: 'Ensured single SkyLight' };
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

  // Build lighting (Python-based)
  async buildLighting(params: {
    quality?: 'Preview' | 'Medium' | 'High' | 'Production';
    buildOnlySelected?: boolean; // ignored in Python path
    buildReflectionCaptures?: boolean;
  }) {
    const q = params.quality || 'High';
    const qualityMap: Record<string, string> = {
      'Preview': 'QUALITY_PREVIEW',
      'Medium': 'QUALITY_MEDIUM', 
      'High': 'QUALITY_HIGH',
      'Production': 'QUALITY_PRODUCTION'
    };
    const qualityEnum = qualityMap[q] || 'QUALITY_HIGH';
    
    // First try to disable force_no_precomputed_lighting using multiple approaches
    const disablePrecomputedPy = `
import unreal

# Method 1: Direct property modification
try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world() if ues else None
    
    if world:
        world_settings = world.get_world_settings()
        if world_settings:
            # Try multiple ways to set the property
            world_settings.set_editor_property('force_no_precomputed_lighting', False)
            
            # Try direct attribute access
            try:
                world_settings.force_no_precomputed_lighting = False
            except:
                pass
                
            # Try via bForceNoPrecomputedLighting (C++ name)
            try:
                world_settings.set_editor_property('bForceNoPrecomputedLighting', False)
            except:
                pass
                
            print('Attempted to disable force_no_precomputed_lighting via WorldSettings')
except Exception as e:
    print(f'Method 1 failed: {e}')

# Method 2: Via console command
try:
    unreal.SystemLibrary.execute_console_command(None, 'r.ForceNoPrecomputedLighting 0')
    print('Executed console command: r.ForceNoPrecomputedLighting 0')
except:
    pass

# Method 3: Via editor properties on all WorldSettings actors
try:
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if actor_sub:
        all_actors = actor_sub.get_all_level_actors()
        for actor in all_actors:
            if actor and actor.get_class().get_name() == 'WorldSettings':
                try:
                    actor.set_editor_property('force_no_precomputed_lighting', False)
                    actor.set_editor_property('bForceNoPrecomputedLighting', False) 
                    print(f'Disabled force_no_precomputed_lighting on WorldSettings actor: {actor.get_name()}')
                except:
                    pass
except Exception as e:
    print(f'Method 3 failed: {e}')

print('All methods attempted to disable force_no_precomputed_lighting')
`.trim();

    // Execute the disable script first
    await this.bridge.executePython(disablePrecomputedPy);
    
    // Small delay to ensure settings are applied
    await new Promise(resolve => setTimeout(resolve, 100));
    
    // Now execute the lighting build
    const py = `
import unreal
import json

try:
    # Get the LevelEditorSubsystem
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    
    if les:
        # Build light maps with specified quality and reflection captures option
        les.build_light_maps(unreal.LightingBuildQuality.${qualityEnum}, ${params.buildReflectionCaptures !== false ? 'True' : 'False'})
        print('RESULT:' + json.dumps({'success': True, 'message': 'Lighting build started via LevelEditorSubsystem'}))
    else:
        # Fallback to deprecated API if modern subsystem not available
        unreal.EditorLevelLibrary.build_lighting(unreal.LightingBuildQuality.${qualityEnum.replace('QUALITY_', '')}, True)
        ${params.buildReflectionCaptures ? 'unreal.EditorLevelLibrary.build_reflection_captures()' : ''}
        print('RESULT:' + json.dumps({'success': True, 'message': 'Lighting build started via EditorLevelLibrary (fallback)'}))
        
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
    const resp = await this.bridge.executePython(py);
    const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) { try { const parsed = JSON.parse(m[1]); return parsed.success ? { success: true, message: parsed.message } : { success: false, error: parsed.error }; } catch {} }
    return { success: true, message: 'Lighting build started' };
  }

  // Create a new level with proper lighting settings as workaround
  async createLightingEnabledLevel(params?: {
    levelName?: string;
    copyActors?: boolean;
    useTemplate?: boolean;
  }) {
    const levelName = params?.levelName || 'LightingEnabledLevel';
    const py = `
import unreal
import json

def create_lighting_enabled_level():
    """Create a new level with lighting enabled"""
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        editor_asset = unreal.EditorAssetLibrary
        
        if not les or not ues:
            return {'success': False, 'error': 'Required subsystems not available'}
        
        # Store current actors if we need to copy them
        actors_to_copy = []
        if ${params?.copyActors ? 'True' : 'False'}:
            current_world = ues.get_editor_world()
            if current_world:
                all_actors = actor_sub.get_all_level_actors()
                # Filter out unnecessary actors - only copy static meshes and important gameplay actors
                for actor in all_actors:
                    if actor:
                        class_name = actor.get_class().get_name()
                        # Only copy specific actor types
                        if class_name in ['StaticMeshActor', 'SkeletalMeshActor', 'Blueprint', 'Actor']:
                            try:
                                actor_data = {
                                    'class': actor.get_class(),
                                    'location': actor.get_actor_location(),
                                    'rotation': actor.get_actor_rotation(),
                                    'scale': actor.get_actor_scale3d(),
                                    'label': actor.get_actor_label()
                                }
                                # Check if actor has a static mesh component
                                mesh_comp = actor.get_component_by_class(unreal.StaticMeshComponent)
                                if mesh_comp:
                                    mesh = mesh_comp.get_editor_property('static_mesh')
                                    if mesh:
                                        actor_data['mesh'] = mesh
                                        actors_to_copy.append(actor_data)
                            except:
                                pass
                print(f'Stored {len(actors_to_copy)} actors to copy')
        
        # Create new level with proper template or blank
        level_name_str = "${levelName}"
        level_path = f'/Game/Maps/{level_name_str}'
        
        # Use template-based creation for guaranteed lighting setup
        use_template = ${params?.useTemplate !== false ? 'True' : 'False'}
        if use_template:
            # Try to use a default template that has lighting enabled
            try:
                # Use the TimeOfDay template which has lighting properly configured
                template_path = '/Engine/Maps/Templates/TimeOfDay'
                if editor_asset.does_asset_exist(template_path):
                    les.new_level_from_template(level_path, template_path)
                    print(f'Created new level from TimeOfDay template: {level_path}')
                else:
                    # Fallback to blank level
                    les.new_level(level_path, False)
                    print(f'Created new blank level: {level_path}')
            except:
                les.new_level(level_path, False)
                print(f'Created new level (fallback): {level_path}')
        else:
            les.new_level(level_path, False)
            print(f'Created new level: {level_path}')
        
        # Get the new world and forcefully disable ForceNoPrecomputedLighting
        new_world = ues.get_editor_world()
        if new_world:
            new_ws = new_world.get_world_settings()
            if new_ws:
                # Force enable lighting - try all methods
                for prop in ['force_no_precomputed_lighting', 'bForceNoPrecomputedLighting', 
                            'ForceNoPrecomputedLighting', 'bforce_no_precomputed_lighting']:
                    try:
                        new_ws.set_editor_property(prop, False)
                    except:
                        pass
                
                # Also set other lighting-related properties
                try:
                    new_ws.set_editor_property('dynamic_indirect_lighting_quality', unreal.LightmassLevelInfoSettings())
                except:
                    pass
                
                # Verify the setting
                try:
                    val = new_ws.get_editor_property('force_no_precomputed_lighting')
                    print(f'New level force_no_precomputed_lighting: {val}')
                    if val:
                        # If still true, we have a persistent issue
                        print('WARNING: ForceNoPrecomputedLighting is still enabled!')
                except:
                    pass
        
        # Copy actors if requested
        if actors_to_copy and actor_sub:
            print('Copying actors to new level...')
            copied = 0
            for actor_data in actors_to_copy:
                try:
                    # Spawn a static mesh actor if we have mesh data
                    if 'mesh' in actor_data:
                        # Create a proper static mesh actor
                        spawned = actor_sub.spawn_actor_from_class(
                            unreal.StaticMeshActor,
                            actor_data['location'],
                            actor_data['rotation']
                        )
                        if spawned:
                            spawned.set_actor_scale3d(actor_data['scale'])
                            spawned.set_actor_label(actor_data['label'])
                            # Set the static mesh
                            mesh_comp = spawned.get_component_by_class(unreal.StaticMeshComponent)
                            if mesh_comp:
                                mesh_comp.set_static_mesh(actor_data['mesh'])
                            copied += 1
                    else:
                        # Spawn regular actor
                        spawned = actor_sub.spawn_actor_from_class(
                            actor_data['class'],
                            actor_data['location'],
                            actor_data['rotation']
                        )
                        if spawned:
                            spawned.set_actor_scale3d(actor_data['scale'])
                            spawned.set_actor_label(actor_data['label'])
                            copied += 1
                except Exception as e:
                    pass  # Silently skip failed copies
            print(f'Successfully copied {copied} actors')
        
        # Add essential lighting actors if not using template
        if not use_template:
            # Add a directional light for sun
            light = actor_sub.spawn_actor_from_class(
                unreal.DirectionalLight,
                unreal.Vector(0, 0, 500),
                unreal.Rotator(-45, 45, 0)
            )
            if light:
                light.set_actor_label('Sun_Light')
                light_comp = light.get_component_by_class(unreal.DirectionalLightComponent)
                if light_comp:
                    light_comp.set_intensity(3.14159)  # Pi lux for realistic sun
                    light_comp.set_light_color(unreal.LinearColor(1, 0.95, 0.8, 1))
                print('Added directional light')
            
            # Add sky light for ambient
            sky = actor_sub.spawn_actor_from_class(
                unreal.SkyLight,
                unreal.Vector(0, 0, 300),
                unreal.Rotator(0, 0, 0)
            )
            if sky:
                sky.set_actor_label('Sky_Light')
                sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
                if sky_comp:
                    sky_comp.set_intensity(1.0)
                print('Added sky light')
            
            # Add sky atmosphere for realistic sky
            atmosphere = actor_sub.spawn_actor_from_class(
                unreal.SkyAtmosphere,
                unreal.Vector(0, 0, 0),
                unreal.Rotator(0, 0, 0)
            )
            if atmosphere:
                atmosphere.set_actor_label('Sky_Atmosphere')
                print('Added sky atmosphere')
        
        # Save the new level
        les.save_current_level()
        print('New level saved')
        
        return {
            'success': True, 
            'message': f'Created new level \"{level_name_str}\" with lighting enabled',
            'path': level_path
        }
        
    except Exception as e:
        return {'success': False, 'error': str(e)}

result = create_lighting_enabled_level()
print('RESULT:' + json.dumps(result))
`.trim();
    
    const resp = await this.bridge.executePython(py);
    const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) { 
      try { 
        const parsed = JSON.parse(m[1]); 
        return parsed;
      } catch {} 
    }
    return { success: true, message: 'New level creation attempted' };
  }

  // Create lightmass importance volume via Python
  async createLightmassVolume(params: {
    name: string;
    location: [number, number, number];
    size: [number, number, number];
  }) {
    const [lx, ly, lz] = params.location;
    const [sx, sy, sz] = params.size;
    const py = `\nimport unreal\neditor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\nloc = unreal.Vector(${lx}, ${ly}, ${lz})\nrot = unreal.Rotator(0,0,0)\nactor = editor_actor_subsystem.spawn_actor_from_class(unreal.LightmassImportanceVolume, loc, rot)\nif actor:\n    try: actor.set_actor_label("${this.escapePythonString(params.name)}")\n    except Exception: pass\n    # Best-effort: set actor scale to approximate size\n    try:\n        actor.set_actor_scale3d(unreal.Vector(max(${sx}/100.0, 0.1), max(${sy}/100.0, 0.1), max(${sz}/100.0, 0.1)))\n    except Exception: pass\n    print("RESULT:{'success': True}")\nelse:\n    print("RESULT:{'success': False, 'error': 'Failed to spawn LightmassImportanceVolume'}")\n`.trim();
    const resp = await this.bridge.executePython(py);
    const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) { try { const parsed = JSON.parse(m[1].replace(/'/g, '"')); return parsed.success ? { success: true, message: 'LightmassImportanceVolume created' } : { success: false, error: parsed.error }; } catch {} }
    return { success: true, message: 'LightmassImportanceVolume creation attempted' };
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

  // Setup volumetric fog (prefer Python to adjust fog actor/component)
  async setupVolumetricFog(params: {
    enabled: boolean;
    density?: number;
    scatteringIntensity?: number;
    fogHeight?: number; // interpreted as Z location shift for ExponentialHeightFog actor
  }) {
    // Enable/disable global volumetric fog via CVar
    await this.bridge.executeConsoleCommand(`r.VolumetricFog ${params.enabled ? 1 : 0}`);

    const py = `\nimport unreal\ntry:\n    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n    actors = actor_sub.get_all_level_actors() if actor_sub else []\n    fog = None\n    for a in actors:\n        try:\n            if a.get_class().get_name() == 'ExponentialHeightFog':\n                fog = a\n                break\n        except Exception: pass\n    if fog:\n        comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)\n        if comp:\n            ${params.density !== undefined ? `\n            try: comp.set_fog_density(${params.density})\n            except Exception: comp.set_editor_property('fog_density', ${params.density})\n            ` : ''}
            ${params.scatteringIntensity !== undefined ? `\n            try: comp.set_fog_max_opacity(${Math.min(Math.max(params.scatteringIntensity,0),1)})\n            except Exception: pass\n            ` : ''}
        ${params.fogHeight !== undefined ? `\n        try:\n            L = fog.get_actor_location()\n            fog.set_actor_location(unreal.Vector(L.x, L.y, ${params.fogHeight}))\n        except Exception: pass\n        ` : ''}
    print("RESULT:{'success': True}")\nexcept Exception as e:\n    print("RESULT:{'success': False, 'error': '%s'}" % str(e))\n`.trim();

    const resp = await this.bridge.executePython(py);
    const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) { try { const parsed = JSON.parse(m[1].replace(/'/g, '"')); return parsed.success ? { success: true, message: 'Volumetric fog configured' } : { success: false, error: parsed.error }; } catch {} }
    return { success: true, message: 'Volumetric fog configured' };
  }
}
