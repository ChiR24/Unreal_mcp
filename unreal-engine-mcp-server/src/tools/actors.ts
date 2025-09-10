import { UnrealBridge } from '../unreal-bridge.js';

export class ActorTools {
  constructor(private bridge: UnrealBridge) {}

  async spawn(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    // Try Python API first for better control and naming
    try {
      return await this.spawnViaPython(params);
    } catch (pythonErr) {
      // Fallback to console if Python fails
      console.log('Python spawn failed, falling back to console:', pythonErr);
      return this.spawnViaConsole(params);
    }
  }
  
  async spawnViaPython(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    try {
      const loc = params.location || { x: 0, y: 0, z: 100 };
      const rot = params.rotation || { pitch: 0, yaw: 0, roll: 0 };
      
      // Resolve the class path
      const fullClassPath = this.resolveActorClass(params.classPath);
      let className = params.classPath;
      
      // Extract simple class name for naming the actor
      if (fullClassPath.includes('.')) {
        className = fullClassPath.split('.').pop() || params.classPath;
      }
      
      const pythonCmd = `
import unreal

# Get the world
world = unreal.EditorLevelLibrary.get_editor_world()

# Try to spawn the actor based on class type
if "${params.classPath}" == "StaticMeshActor":
    # For StaticMeshActor, use a basic approach
    location = unreal.Vector(${loc.x}, ${loc.y}, ${loc.z})
    rotation = unreal.Rotator(${rot.pitch}, ${rot.yaw}, ${rot.roll})
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, 
        location, 
        rotation
    )
    if actor:
        # Find existing actors to determine counter
        import time
        timestamp = int(time.time() * 1000) % 10000
        actor.set_actor_label(f"StaticMeshActor_{timestamp}")
        print(f"Spawned StaticMeshActor_{timestamp} at {location}")
    else:
        print("Failed to spawn StaticMeshActor")
elif "${params.classPath}" == "CameraActor":
    # For CameraActor
    location = unreal.Vector(${loc.x}, ${loc.y}, ${loc.z})
    rotation = unreal.Rotator(${rot.pitch}, ${rot.yaw}, ${rot.roll})
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.CameraActor, 
        location, 
        rotation
    )
    if actor:
        import time
        timestamp = int(time.time() * 1000) % 10000
        actor.set_actor_label(f"CameraActor_{timestamp}")
        print(f"Spawned CameraActor_{timestamp} at {location}")
    else:
        print("Failed to spawn CameraActor")
else:
    # Generic spawn for other actor types
    try:
        actor_class = None
        class_name = "${params.classPath}"
        
        # Try to get the class
        if hasattr(unreal, class_name):
            actor_class = getattr(unreal, class_name)
        
        if actor_class:
            location = unreal.Vector(${loc.x}, ${loc.y}, ${loc.z})
            rotation = unreal.Rotator(${rot.pitch}, ${rot.yaw}, ${rot.roll})
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                actor_class, 
                location, 
                rotation
            )
            if actor:
                actor.set_actor_label(f"{class_name}")
                print(f"Spawned {class_name} at {location}")
            else:
                print(f"Failed to spawn {class_name}")
        else:
            print(f"Class not found: {class_name}")
    except Exception as e:
        print(f"Error spawning actor: {e}")
`.trim();
      
      await this.bridge.executePython(pythonCmd);
      return { success: true, message: `Actor spawned: ${className} at ${loc.x},${loc.y},${loc.z}` };
    } catch (err) {
      throw new Error(`Failed to spawn actor via Python: ${err}`);
    }
  }
  
  async spawnViaConsole(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    try {
      // Get the console-friendly class name
      let spawnClass = this.getConsoleClassName(params.classPath);
      
      // Use summon command with location if provided
      const loc = params.location || { x: 0, y: 0, z: 100 };
      const command = `summon ${spawnClass} ${loc.x} ${loc.y} ${loc.z}`;
      
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: command,
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: `Actor spawned: ${spawnClass} at ${loc.x},${loc.y},${loc.z}` };
    } catch (err) {
      throw new Error(`Failed to spawn actor: ${err}`);
    }
  }
  
  private resolveActorClass(classPath: string): string {
    // Map common names to full Unreal class paths
    const classMap: { [key: string]: string } = {
      'PointLight': '/Script/Engine.PointLight',
      'DirectionalLight': '/Script/Engine.DirectionalLight',
      'SpotLight': '/Script/Engine.SpotLight',
      'RectLight': '/Script/Engine.RectLight',
      'SkyLight': '/Script/Engine.SkyLight',
      'StaticMeshActor': '/Script/Engine.StaticMeshActor',
      'PlayerStart': '/Script/Engine.PlayerStart',
      'Camera': '/Script/Engine.CameraActor',
      'CameraActor': '/Script/Engine.CameraActor',
      'Pawn': '/Script/Engine.DefaultPawn',
      'Character': '/Script/Engine.Character',
      'TriggerBox': '/Script/Engine.TriggerBox',
      'TriggerSphere': '/Script/Engine.TriggerSphere',
      'BlockingVolume': '/Script/Engine.BlockingVolume',
      'PostProcessVolume': '/Script/Engine.PostProcessVolume',
      'LightmassImportanceVolume': '/Script/Engine.LightmassImportanceVolume',
      'NavMeshBoundsVolume': '/Script/Engine.NavMeshBoundsVolume',
      'ExponentialHeightFog': '/Script/Engine.ExponentialHeightFog',
      'AtmosphericFog': '/Script/Engine.AtmosphericFog',
      'SphereReflectionCapture': '/Script/Engine.SphereReflectionCapture',
      'BoxReflectionCapture': '/Script/Engine.BoxReflectionCapture',
      'PlaneReflectionCapture': '/Script/Engine.PlaneReflectionCapture',
      'DecalActor': '/Script/Engine.DecalActor'
    };
    
    // Check if it's a simple name that needs mapping
    if (classMap[classPath]) {
      return classMap[classPath];
    }
    
    // Check if it already looks like a full path
    if (classPath.startsWith('/Script/') || classPath.startsWith('/Game/')) {
      return classPath;
    }
    
    // Check for Blueprint paths
    if (classPath.includes('Blueprint') || classPath.includes('BP_')) {
      // Ensure it has the proper prefix
      if (!classPath.startsWith('/Game/')) {
        return '/Game/' + classPath;
      }
      return classPath;
    }
    
    // Default: assume it's an engine class
    return '/Script/Engine.' + classPath;
  }
  
  private getConsoleClassName(classPath: string): string {
    // Normalize class path for console 'summon'
    let input = classPath;

    // Engine classes: reduce '/Script/Engine.ClassName' to 'ClassName'
    if (input.startsWith('/Script/Engine.')) {
      return input.replace('/Script/Engine.', '');
    }

    // If it's already a simple class name (no path) and not a /Game asset, strip optional _C and return
    if (!input.startsWith('/Game/') && !input.includes('/')) {
      if (input.endsWith('_C')) return input.slice(0, -2);
      return input;
    }

    // Blueprint assets under /Game: ensure '/Game/Path/Asset.Asset_C'
    if (input.startsWith('/Game/')) {
      // Remove any existing ".Something" suffix to rebuild normalized class ref
      const pathWithoutSuffix = input.split('.')[0];
      const parts = pathWithoutSuffix.split('/');
      const assetName = parts[parts.length - 1].replace(/_C$/, '');
      const normalized = `${pathWithoutSuffix}.${assetName}_C`;
      return normalized;
    }

    // Fallback: return input unchanged
    return input;
  }
}
