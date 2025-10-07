import { UnrealBridge } from '../unreal-bridge.js';
import { ensureRotation, ensureVector3 } from '../utils/validation.js';
import { coerceString, coerceVector3, interpretStandardResult } from '../utils/result-helpers.js';
import { escapePythonString } from '../utils/python.js';

export class ActorTools {
  constructor(private bridge: UnrealBridge) {}

  async spawn(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; actorName?: string }) {
    if (!params.classPath || typeof params.classPath !== 'string' || params.classPath.trim().length === 0) {
      throw new Error(`Invalid classPath: ${params.classPath}`);
    }

    const className = params.classPath.trim();
    const requestedActorName = typeof params.actorName === 'string' ? params.actorName.trim() : undefined;
    if (params.actorName !== undefined && (!requestedActorName || requestedActorName.length === 0)) {
      throw new Error(`Invalid actorName: ${params.actorName}`);
    }
  const sanitizedActorName = requestedActorName?.replace(/[^A-Za-z0-9_-]/g, '_');
    const lowerName = className.toLowerCase();

    const shapeMapping: Record<string, string> = {
      cube: '/Engine/BasicShapes/Cube',
      sphere: '/Engine/BasicShapes/Sphere',
      cylinder: '/Engine/BasicShapes/Cylinder',
      cone: '/Engine/BasicShapes/Cone',
      plane: '/Engine/BasicShapes/Plane',
      torus: '/Engine/BasicShapes/Torus'
    };

    const mappedClassPath = shapeMapping[lowerName] ?? this.resolveActorClass(className);

    const [locX, locY, locZ] = ensureVector3(
      params.location ?? { x: 0, y: 0, z: 100 },
      'actor location'
    );
    const [rotPitch, rotYaw, rotRoll] = ensureRotation(
      params.rotation ?? { pitch: 0, yaw: 0, roll: 0 },
      'actor rotation'
    );

  const escapedResolvedClassPath = escapePythonString(mappedClassPath);
  const escapedRequestedPath = escapePythonString(className);
  const escapedRequestedActorName = sanitizedActorName ? escapePythonString(sanitizedActorName) : '';

    const pythonCmd = `
import unreal
import json
import time

result = {
  "success": False,
  "message": "",
  "error": "",
  "actorName": "",
  "actorPath": "",
  "requestedClass": "${escapedRequestedPath}",
  "resolvedClass": "${escapedResolvedClassPath}",
  "location": [${locX}, ${locY}, ${locZ}],
  "rotation": [${rotPitch}, ${rotYaw}, ${rotRoll}],
  "requestedActorName": "${escapedRequestedActorName}",
  "componentPaths": [],
  "warnings": [],
  "details": []
}

${this.getPythonSpawnHelper()}

abstract_classes = ['PlaneReflectionCapture', 'ReflectionCapture', 'Actor', 'Pawn', 'Character']

def finalize():
  data = dict(result)
  if data.get("success"):
    if not data.get("message"):
      data["message"] = "Actor spawned successfully"
    data.pop("error", None)
  else:
    if not data.get("error"):
      data["error"] = data.get("message") or "Failed to spawn actor"
    if not data.get("message"):
      data["message"] = data["error"]
  if not data.get("warnings"):
    data.pop("warnings", None)
  if not data.get("details"):
    data.pop("details", None)
  if not data.get("componentPaths"):
    data.pop("componentPaths", None)
  return data

try:
  les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
  if les and les.is_in_play_in_editor():
    result["message"] = "Cannot spawn actors while in Play In Editor mode. Please stop PIE first."
    result["error"] = result["message"]
    result["details"].append("Play In Editor mode detected")
    print('RESULT:' + json.dumps(finalize()))
    raise SystemExit(0)
except SystemExit:
  raise
except Exception:
  result["warnings"].append("Unable to determine Play In Editor state")

if result["requestedClass"] in abstract_classes:
  result["message"] = f"Cannot spawn {result['requestedClass']}: class is abstract and cannot be instantiated"
  result["error"] = result["message"]
else:
  try:
    class_path = result["resolvedClass"]
    requested_path = result["requestedClass"]
    location = unreal.Vector(${locX}, ${locY}, ${locZ})
    rotation = unreal.Rotator(${rotPitch}, ${rotYaw}, ${rotRoll})
    actor = None

    simple_name = requested_path.split('/')[-1] if '/' in requested_path else requested_path
    if '.' in simple_name:
      simple_name = simple_name.split('.')[-1]
    simple_name_lower = simple_name.lower()
    class_lookup_name = class_path.split('.')[-1] if '.' in class_path else simple_name

    result["details"].append(f"Attempting spawn using class path: {class_path}")

    if class_path.startswith('/Game') or class_path.startswith('/Engine'):
      try:
        asset = unreal.EditorAssetLibrary.load_asset(class_path)
      except Exception as asset_error:
        asset = None
        result["warnings"].append(f"Failed to load asset for {class_path}: {asset_error}")
      if asset:
        if isinstance(asset, unreal.Blueprint):
          try:
            actor_class = asset.generated_class()
          except Exception as blueprint_error:
            actor_class = None
            result["warnings"].append(f"Failed to resolve blueprint class: {blueprint_error}")
          if actor_class:
            actor = spawn_actor_from_class(actor_class, location, rotation)
            if actor:
              result["details"].append("Spawned using Blueprint generated class")
        elif isinstance(asset, unreal.StaticMesh):
          actor = spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
          if actor:
            mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if mesh_component:
              mesh_component.set_static_mesh(asset)
              mesh_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
              result["details"].append("Applied static mesh to spawned StaticMeshActor")

    if not actor:
      shape_map = {
        'cube': '/Engine/BasicShapes/Cube',
        'sphere': '/Engine/BasicShapes/Sphere',
        'cylinder': '/Engine/BasicShapes/Cylinder',
        'cone': '/Engine/BasicShapes/Cone',
        'plane': '/Engine/BasicShapes/Plane',
        'torus': '/Engine/BasicShapes/Torus'
      }
      mesh_path = shape_map.get(simple_name_lower)
      if not mesh_path and class_path.startswith('/Engine/BasicShapes'):
        mesh_path = class_path
      if mesh_path:
        try:
          shape_mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
        except Exception as shape_error:
          shape_mesh = None
          result["warnings"].append(f"Failed to load shape mesh {mesh_path}: {shape_error}")
        if shape_mesh:
          actor = spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
          if actor:
            mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if mesh_component:
              mesh_component.set_static_mesh(shape_mesh)
              mesh_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
              result["details"].append(f"Spawned StaticMeshActor with mesh {mesh_path}")

    if not actor:
      if class_lookup_name == "StaticMeshActor":
        actor = spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
        if actor:
          try:
            cube_mesh = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Cube')
          except Exception as cube_error:
            cube_mesh = None
            result["warnings"].append(f"Failed to load default cube mesh: {cube_error}")
          if cube_mesh:
            mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if mesh_component:
              mesh_component.set_static_mesh(cube_mesh)
              mesh_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
              result["details"].append("Applied default cube mesh to StaticMeshActor")
      elif class_lookup_name == "CameraActor":
        actor = spawn_actor_from_class(unreal.CameraActor, location, rotation)
        if actor:
          result["details"].append("Spawned CameraActor via reflected class lookup")
      else:
        actor_class = getattr(unreal, class_lookup_name, None)
        if actor_class:
          actor = spawn_actor_from_class(actor_class, location, rotation)
          if actor:
            result["details"].append(f"Spawned {class_lookup_name} via reflected class lookup")

    if actor:
      desired_name = (result.get("requestedActorName") or "").strip()
      actor_name = ""
      if desired_name:
        try:
          try:
            actor.set_actor_label(desired_name, True)
          except TypeError:
            actor.set_actor_label(desired_name)
          actor_name = actor.get_actor_label() or desired_name
        except Exception as label_error:
          result["warnings"].append(f"Failed to honor requested actor name '{desired_name}': {label_error}")
      if not actor_name:
        timestamp = int(time.time() * 1000) % 10000
        base_name = simple_name or class_lookup_name or class_path.split('/')[-1]
        fallback_name = f"{base_name}_{timestamp}"
        try:
          actor.set_actor_label(fallback_name)
        except Exception as label_error:
          result["warnings"].append(f"Failed to set actor label: {label_error}")
        actor_name = actor.get_actor_label() or fallback_name
      result["success"] = True
      result["actorName"] = actor_name
      try:
        result["actorPath"] = actor.get_path_name()
      except Exception as path_error:
        add_warning(f"Failed to resolve actor path: {path_error}")

      try:
        comps = []
        for comp in actor.get_components_by_class(unreal.ActorComponent):
          if not comp:
            continue
          comp_info = {
            'name': comp.get_name() if hasattr(comp, 'get_name') else '',
            'path': comp.get_path_name() if hasattr(comp, 'get_path_name') else '',
            'class': comp.get_class().get_path_name() if comp.get_class() else ''
          }
          comps.append(comp_info)
        if comps:
          result['componentPaths'] = comps
          add_detail(f"Captured {len(comps)} component references for follow-up operations")
      except Exception as comp_error:
        add_warning(f"Failed to capture component references: {comp_error}")
      if not result["message"]:
        result["message"] = f"Spawned {actor_name} at ({location.x}, {location.y}, {location.z})"
    else:
      result["message"] = f"Failed to spawn actor from: {class_path}. Try using /Engine/BasicShapes/Cube or StaticMeshActor"
      result["error"] = result["message"]
  except Exception as spawn_error:
    result["error"] = f"Error spawning actor: {spawn_error}"
    if not result["message"]:
      result["message"] = result["error"]

print('RESULT:' + json.dumps(finalize()))
`.trim();

    try {
      const response = await this.bridge.executePython(pythonCmd);
      const interpreted = interpretStandardResult(response, {
        successMessage: `Spawned actor ${className}`,
        failureMessage: `Failed to spawn actor ${className}`
      });

      if (!interpreted.success) {
        throw new Error(interpreted.error || interpreted.message);
      }

      const actorName = coerceString(interpreted.payload.actorName);
      const actorPath = coerceString(interpreted.payload.actorPath);
      const resolvedClass = coerceString(interpreted.payload.resolvedClass) ?? mappedClassPath;
      const requestedClass = coerceString(interpreted.payload.requestedClass) ?? className;
      const locationVector = coerceVector3(interpreted.payload.location) ?? [locX, locY, locZ];
      const rotationVector = coerceVector3(interpreted.payload.rotation) ?? [rotPitch, rotYaw, rotRoll];
      const capturedComponents = Array.isArray(interpreted.payload.componentPaths)
        ? interpreted.payload.componentPaths.map((comp: any) => ({
            name: coerceString(comp?.name) ?? undefined,
            path: coerceString(comp?.path) ?? undefined,
            class: coerceString(comp?.class) ?? undefined
          }))
        : undefined;

      const result: Record<string, unknown> = {
        success: true,
        message: interpreted.message,
        actorName: actorName ?? undefined,
        actorPath: actorPath ?? undefined,
        resolvedClass,
        requestedClass,
        location: { x: locationVector[0], y: locationVector[1], z: locationVector[2] },
        rotation: { pitch: rotationVector[0], yaw: rotationVector[1], roll: rotationVector[2] }
      };

      if (interpreted.warnings?.length) {
        result.warnings = interpreted.warnings;
      }
      if (interpreted.details?.length) {
        result.details = interpreted.details;
      }
      if (capturedComponents?.length) {
        result.componentPaths = capturedComponents;
      }

      return result;
    } catch (err) {
      throw new Error(`Failed to spawn actor via Python: ${err}`);
    }
  }
  
  async spawnViaConsole(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    try {
      const [locX, locY, locZ] = ensureVector3(params.location ?? { x: 0, y: 0, z: 100 }, 'actor location');
      // Check if editor is in play mode first
      try {
        const pieCheckPython = `
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les and les.is_in_play_in_editor():
    print("PIE_ACTIVE")
else:
    print("PIE_INACTIVE")
        `.trim();
        
        const pieCheckResult = await this.bridge.executePython(pieCheckPython);
        const outputStr = typeof pieCheckResult === 'string' ? pieCheckResult : JSON.stringify(pieCheckResult);
        
        if (outputStr.includes('PIE_ACTIVE')) {
          throw new Error('Cannot spawn actors while in Play In Editor mode. Please stop PIE first.');
        }
      } catch (pieErr: any) {
        // If the error is about PIE, throw it
        if (String(pieErr).includes('Play In Editor')) {
          throw pieErr;
        }
        // Otherwise ignore and continue
      }
      
      // List of known abstract classes that cannot be spawned
      const abstractClasses = ['PlaneReflectionCapture', 'ReflectionCapture', 'Actor'];
      
      // Check if this is an abstract class
      if (abstractClasses.includes(params.classPath)) {
        throw new Error(`Cannot spawn ${params.classPath}: class is abstract and cannot be instantiated`);
      }
      
      // Get the console-friendly class name
      const spawnClass = this.getConsoleClassName(params.classPath);
      
      // Use summon command with location if provided
  const command = `summon ${spawnClass} ${locX} ${locY} ${locZ}`;
      
      await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          WorldContextObject: null,
          Command: command,
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      
      // Console commands don't reliably report success/failure
      // We can't guarantee this actually worked, so indicate uncertainty
      return { 
        success: true, 
        message: `Actor spawn attempted via console: ${spawnClass} at ${locX},${locY},${locZ}`,
        note: 'Console spawn result uncertain - verify in editor'
      };
    } catch (err) {
      throw new Error(`Failed to spawn actor: ${err}`);
    }
  }
  private getPythonSpawnHelper(): string {
  return `
def spawn_actor_from_class(actor_class, location, rotation):
  actor = None
  try:
    actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if actor_subsys:
      actor = actor_subsys.spawn_actor_from_class(actor_class, location, rotation)
  except Exception:
    actor = None
  if not actor:
    raise RuntimeError('EditorActorSubsystem unavailable or failed to spawn actor. Enable Editor Scripting Utilities plugin and verify class path.')
  return actor
`.trim();
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
      // PlaneReflectionCapture is abstract and cannot be spawned
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

    if (classPath.startsWith('/Engine/')) {
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
    const input = classPath;

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
