import { UnrealBridge } from '../unreal-bridge.js';

export class PythonHelper {
  constructor(private bridge: UnrealBridge) {}

  /**
   * Resolve a UObject path to a simple info payload via Python.
   */
  async resolveObject(path: string) {
    const script = `
import unreal, json
obj = unreal.load_object(None, r"${path}")
if not obj:
    print('RESULT:' + json.dumps({'success': False, 'error': f'Object not found: {path}'}))
else:
    outer = obj.get_outer() if hasattr(obj, 'get_outer') else None
    data = {
        'success': True,
        'path': obj.get_path_name(),
        'class': obj.get_class().get_path_name() if obj.get_class() else None,
        'outer': outer.get_path_name() if outer else None
    }
    if hasattr(obj, 'get_name'):
        data['name'] = obj.get_name()
    if hasattr(obj, 'get_actor_label'):
        try:
            data['label'] = obj.get_actor_label()
        except Exception:
            pass
    print('RESULT:' + json.dumps(data))
`.trim();
    return this.bridge.executePythonWithResult(script);
  }

  /**
   * List immediate components for an actor.
   */
  async listActorComponents(actorPath: string) {
    const script = `
import unreal, json
actor = unreal.load_object(None, r"${actorPath}")
if not actor:
    print('RESULT:' + json.dumps({'success': False, 'error': f'Actor not found: {actorPath}'}))
else:
    components = []
    for comp in actor.get_components_by_class(unreal.ActorComponent):
        info = {
            'name': comp.get_name(),
            'path': comp.get_path_name(),
            'class': comp.get_class().get_path_name()
        }
        if hasattr(comp, 'get_outer'):
            outer = comp.get_outer()
            info['outer'] = outer.get_path_name() if outer else None
        components.append(info)
    print('RESULT:' + json.dumps({'success': True, 'components': components}))
`.trim();
    return this.bridge.executePythonWithResult(script);
  }

  /**
   * Get Blueprint CDO path and class info.
   */
  async getBlueprintCdo(blueprintPath: string) {
    const script = `
import unreal, json
candidate = r"${blueprintPath}"

def candidate_paths(raw_name):
    name = (raw_name or '').strip()
    if not name:
        return []
    if name.startswith('/'):
        base = name[:-7] if name.endswith('.uasset') else name
        return [base]
    bases = [
        f"/Game/{name}",
        f"/Game/Blueprints/{name}",
        f"/Game/Blueprints/Types/{name}",
        f"/Game/Blueprints/DirectAPI/{name}",
        f"/Game/Blueprints/ComponentTests/{name}",
        f"/Game/Blueprints/LiveTests/{name}",
    ]
    results = []
    for entry in bases:
        if entry.endswith('.uasset'):
            results.append(entry[:-7])
        results.append(entry)
    return results

bp = None
resolved_path = None
for path in candidate_paths(candidate):
    try:
        bp = unreal.load_object(None, path)
    except Exception:
        bp = None
    if bp:
        resolved_path = path
        break

if not bp:
    print('RESULT:' + json.dumps({'success': False, 'error': f'Blueprint not found: {blueprintPath}'}))
else:
    try:
        generated = bp.generated_class()
    except Exception:
        generated = None
    if not generated:
        print('RESULT:' + json.dumps({'success': False, 'error': 'Generated class unavailable'}))
    else:
        cdo = generated.get_default_object()
        path = cdo.get_path_name() if cdo else None
        print('RESULT:' + json.dumps({
            'success': True,
            'requested': candidate,
            'blueprint': resolved_path or bp.get_path_name(),
            'generatedClass': generated.get_path_name() if generated else None,
            'cdoPath': path
        }))
`.trim();
    return this.bridge.executePythonWithResult(script);
  }

  async setBlueprintDefault(params: {
    blueprintCandidates: string[];
    requestedPath: string;
    propertyName: string;
    value: unknown;
    save?: boolean;
  }) {
    const payload = JSON.stringify({
      blueprintCandidates: params.blueprintCandidates,
      requestedPath: params.requestedPath,
      propertyName: params.propertyName,
      value: params.value ?? null,
      save: params.save === true
    });

    const script = `
import unreal, json

payload = json.loads(r'''${payload}''')

result = {
    'success': False,
    'message': '',
    'error': '',
    'warnings': [],
    'blueprintPath': payload.get('requestedPath'),
    'propertyName': payload.get('propertyName'),
    'value': payload.get('value')
}

def add_warning(message):
    try:
        text = str(message)
    except Exception:
        text = message
    warnings = result.setdefault('warnings', [])
    warnings.append(text)

def coerce_value(raw_value):
    if isinstance(raw_value, str) and raw_value.startswith('/'):
        asset = unreal.load_object(None, raw_value)
        if asset:
            return asset
    if isinstance(raw_value, dict):
        lowered = {str(k).lower(): v for k, v in raw_value.items()}
        if {'x', 'y', 'z'}.issubset(lowered.keys()):
            try:
                return unreal.Vector(
                    float(lowered.get('x', 0.0)),
                    float(lowered.get('y', 0.0)),
                    float(lowered.get('z', 0.0))
                )
            except Exception as err:
                add_warning(f"Vector coercion failed: {err}")
        if {'pitch', 'yaw', 'roll'}.issubset(lowered.keys()):
            try:
                return unreal.Rotator(
                    float(lowered.get('pitch', 0.0)),
                    float(lowered.get('yaw', 0.0)),
                    float(lowered.get('roll', 0.0))
                )
            except Exception as err:
                add_warning(f"Rotator coercion failed: {err}")
    return raw_value

def resolve_blueprint():
    candidates = payload.get('blueprintCandidates') or []
    editor_lib = unreal.EditorAssetLibrary
    for path in candidates:
        try:
            asset = editor_lib.load_asset(path)
        except Exception:
            asset = None
        if asset:
            return path, asset
    return None, None

def resolve_generated_class(blueprint):
    generated = None
    for attribute in ('generated_class', 'GeneratedClass'):
        try:
            generated = getattr(blueprint, attribute)
            if callable(generated):
                generated = generated()
        except Exception:
            generated = None
        if generated:
            break
    if generated:
        return generated
    try:
        unreal.KismetEditorUtilities.compile_blueprint(blueprint)
    except Exception as compile_err:
        add_warning(f"Compile attempt failed: {compile_err}")
    for attribute in ('generated_class', 'GeneratedClass'):
        try:
            generated = getattr(blueprint, attribute)
            if callable(generated):
                generated = generated()
        except Exception:
            generated = None
        if generated:
            break
    return generated

def apply_property(target, property_path, value):
    if not isinstance(property_path, str) or not property_path:
        raise ValueError('Invalid property name')
    segments = property_path.split('.')
    focus = target
    for segment in segments[:-1]:
        focus = focus.get_editor_property(segment)
        if focus is None:
            raise AttributeError(segment)
    prop_name = segments[-1]
    focus.set_editor_property(prop_name, value)
    try:
        focus.modify()
    except Exception:
        pass
    return focus, prop_name

def main():
    resolved_path, blueprint = resolve_blueprint()
    if not blueprint:
        result['error'] = f"Blueprint not found: {payload.get('requestedPath')}"
        return result
    result['blueprintPath'] = resolved_path or blueprint.get_path_name()
    generated = resolve_generated_class(blueprint)
    if not generated:
        result['error'] = 'Generated class unavailable'
        return result
    try:
        cdo = generated.get_default_object()
    except Exception as cdo_err:
        add_warning(f"get_default_object failed: {cdo_err}")
        cdo = None
    if not cdo:
        result['error'] = 'Unable to resolve class default object'
        return result
    coerced_value = coerce_value(payload.get('value'))
    try:
        focus, prop_name = apply_property(cdo, payload.get('propertyName'), coerced_value)
    except Exception as set_err:
        result['error'] = str(set_err)
        return result
    try:
        blueprint.modify()
    except Exception:
        pass
    try:
        focus.post_edit_change()
    except Exception:
        pass
    try:
        cdo.post_edit_change()
    except Exception:
        pass
    try:
        blueprint.mark_package_dirty()
    except Exception:
        pass
    if payload.get('save'):
        try:
            unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
            result['saved'] = True
        except Exception as save_err:
            add_warning(f"Save failed: {save_err}")
    result.update({
        'success': True,
        'message': f"Updated {prop_name} on {focus.get_path_name() if hasattr(focus, 'get_path_name') else 'target'}",
        'cdoPath': cdo.get_path_name(),
        'propertyName': prop_name
    })
    return result

try:
    outcome = main()
except Exception as err:
    add_warning('Unhandled exception while setting Blueprint default.')
    outcome = result
    outcome['error'] = str(err)

print('RESULT:' + json.dumps(outcome))
`.trim();

    return this.bridge.executePythonWithResult(script);
  }

  /**
   * Set a property on an arbitrary UObject using Python to handle typed values.
   */
  async setProperty(params: {
    objectPath: string;
    propertyName: string;
    value: unknown;
    valueType?: 'auto' | 'string' | 'number' | 'bool' | 'vector' | 'rotator' | 'path';
  }) {
    const payload = JSON.stringify(params.value ?? null);
    const script = `
import unreal, json
obj = unreal.load_object(None, r"${params.objectPath}")
if not obj:
    print('RESULT:' + json.dumps({'success': False, 'error': f'Object not found: ${params.objectPath}'}))
else:
    data = json.loads(r'''${payload}''')
    prop = '${params.propertyName}'
    try:
        value = data
        if isinstance(value, str) and value.startswith('/'):  # assume asset path
            asset = unreal.load_object(None, value)
            if asset:
                value = asset
        obj.set_editor_property(prop, value)
        print('RESULT:' + json.dumps({'success': True, 'object': obj.get_path_name(), 'property': prop}))
    except Exception as err:
        print('RESULT:' + json.dumps({'success': False, 'error': str(err), 'property': prop}))
`.trim();
    return this.bridge.executePythonWithResult(script);
  }
}