import { UnrealBridge } from '../unreal-bridge.js';

export class VerificationTools {
  constructor(private bridge: UnrealBridge) {}

  // Read back a scalability quality level via GameUserSettings (preferred) with safe category mapping
  async getQualityLevel(category: string) {
    // Normalize common aliases to correct base names used by GameUserSettings getters
    const map: Record<string, string> = {
      ViewDistance: 'ViewDistance',
      AntiAliasing: 'AntiAliasing',
      PostProcessing: 'PostProcess',
      PostProcess: 'PostProcess',
      Shadows: 'Shadow',
      Shadow: 'Shadow',
      GlobalIllumination: 'GlobalIllumination',
      Reflections: 'Reflection',
      Reflection: 'Reflection',
      Textures: 'Texture',
      Texture: 'Texture',
      Effects: 'Effects',
      Foliage: 'Foliage',
      Shading: 'Shading',
    };
    const base = map[category] || category;

    const py = `
import unreal, json
result = {'success': True, 'category': '${base}', 'actual': -1, 'method': 'GameUserSettings'}
try:
    gus = unreal.GameUserSettings.get_game_user_settings()
    if gus:
        mapping = {
            'ViewDistance': 'get_view_distance_quality',
            'AntiAliasing': 'get_anti_aliasing_quality',
            'PostProcess': 'get_post_process_quality',
            'Shadow': 'get_shadow_quality',
            'GlobalIllumination': 'get_global_illumination_quality',
            'Reflection': 'get_reflection_quality',
            'Texture': 'get_texture_quality',
            'Effects': 'get_effects_quality',
            'Foliage': 'get_foliage_quality',
            'Shading': 'get_shading_quality',
        }
        fn = mapping.get('${base}')
        if fn and hasattr(gus, fn):
            result['actual'] = int(getattr(gus, fn)())
        else:
            result['method'] = 'CVarOnly'
    else:
        result['method'] = 'NoGameUserSettings'
except Exception as e:
    result['success'] = False
    result['error'] = str(e)
print('RESULT:' + json.dumps(result))
    `.trim();

    const resp = await this.bridge.executePython(py);
    let out = '';
    if (resp?.LogOutput && Array.isArray(resp.LogOutput)) out = resp.LogOutput.map((l: any) => l.Output || '').join('');
    else if (typeof resp === 'string') out = resp; else out = JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) {
      try {
        const parsed = JSON.parse(m[1]);
        return parsed;
      } catch {}
    }
    return { success: false, error: 'No parseable result' };
  }

  // Verify if a foliage type exists in the editor (by name)
  async foliageTypeExists(name: string) {
    // Check if the foliage type asset actually exists
    const py = `
import unreal, json
result = { 'success': True, 'exists': False, 'method': 'AssetCheck', 'error': '', 'asset_path': '' }
name = "${name}"

try:
    # Check if the foliage type asset exists
    asset_path = f"/Game/Foliage/Types/{name}.{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        result['exists'] = True
        result['asset_path'] = asset_path
        result['method'] = 'AssetExists'
    else:
        result['exists'] = False
        result['method'] = 'AssetNotFound'
except Exception as e:
    result['success'] = False
    result['error'] = str(e)

print('RESULT:' + json.dumps(result))
    `.trim();

    const resp = await this.bridge.executePython(py);
    let out = '';
    if (resp?.LogOutput && Array.isArray(resp.LogOutput)) out = resp.LogOutput.map((l: any) => l.Output || '').join('');
    else if (typeof resp === 'string') out = resp; else out = JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) {
      try {
        const parsed = JSON.parse(m[1]);
        return parsed;
      } catch {}
    }
    return { success: false, error: 'No parseable result' };
  }

  // Count foliage instances near a position (best-effort)
  async countFoliageInstances(params: { position: [number, number, number]; radius: number; foliageTypeName?: string }) {
    const pos = params.position || [0,0,0];
    const radius = params.radius || 1000;
    const typeName = params.foliageTypeName || '';

    // Count actual spawned actors or HISM instances
    const py = `
import unreal, json, math

pos = unreal.Vector(${pos[0]}, ${pos[1]}, ${pos[2]})
radius = float(${radius})
name_filter = r"${typeName}"

result = { 'success': True, 'count': 0, 'method': 'ActorCount', 'details': [] }

try:
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actor_sub.get_all_level_actors() if actor_sub else []
    
    # Count spawned static mesh actors with matching labels
    for actor in all_actors:
        try:
            label = actor.get_actor_label() if hasattr(actor, 'get_actor_label') else ''
            # Check if it's a foliage instance actor
            if name_filter and f"{name_filter}_instance_" in label:
                actor_pos = actor.get_actor_location()
                dist = math.sqrt((actor_pos.x - pos.x)**2 + (actor_pos.y - pos.y)**2 + (actor_pos.z - pos.z)**2)
                if dist <= radius:
                    result['count'] += 1
        except Exception:
            pass
    
    # Also check for HISM components (if they exist)
    for actor in all_actors:
        try:
            label = actor.get_actor_label() if hasattr(actor, 'get_actor_label') else ''
            if f"FoliageContainer_{name_filter}" in label:
                # Check for HISM components
                try:
                    comps = actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
                    for comp in comps:
                        if hasattr(comp, 'get_instance_count'):
                            result['count'] += comp.get_instance_count()
                            result['details'].append({'component': comp.get_name(), 'instances': comp.get_instance_count()})
                except Exception:
                    pass
        except Exception:
            pass
    
    if result['count'] > 0:
        result['method'] = 'ActualActors'
    
except Exception as e:
    result['success'] = False
    result['error'] = str(e)

print('RESULT:' + json.dumps(result))
    `.trim();

    const resp = await this.bridge.executePython(py);
    let out = '';
    if (resp?.LogOutput && Array.isArray(resp.LogOutput)) out = resp.LogOutput.map((l: any) => l.Output || '').join('');
    else if (typeof resp === 'string') out = resp; else out = JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) {
      try {
        const parsed = JSON.parse(m[1]);
        return parsed;
      } catch {}
    }
    return { success: false, error: 'No parseable result' };
  }

  // Verify if a landscape actor with given name exists
  async landscapeExists(name: string) {
    const py = `
import unreal, json
result = { 'success': True, 'exists': False }
try:
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    for a in actors:
        try:
            cls = a.get_class().get_name()
            if 'Landscape' in cls:
                label = ''
                try:
                    label = a.get_actor_label()
                except Exception:
                    label = a.get_name()
                if label == "${name}" or a.get_name() == "${name}":
                    result['exists'] = True
                    break
        except Exception:
            pass
except Exception as e:
    result['success'] = False
    result['error'] = str(e)

print('RESULT:' + json.dumps(result))
    `.trim();

    const resp = await this.bridge.executePython(py);
    let out = '';
    if (resp?.LogOutput && Array.isArray(resp.LogOutput)) out = resp.LogOutput.map((l: any) => l.Output || '').join('');
    else if (typeof resp === 'string') out = resp; else out = JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) {
      try {
        const parsed = JSON.parse(m[1]);
        return parsed;
      } catch {}
    }
    return { success: false, error: 'No parseable result' };
  }
}
