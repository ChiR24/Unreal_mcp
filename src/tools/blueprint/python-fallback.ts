import { UnrealBridge } from '../../unreal-bridge.js';
import { coerceString, coerceStringArray } from '../../utils/result-helpers.js';
import * as BlueprintHelpers from './helpers.js';
import * as BlueprintProbes from './python-probes.js';

export async function addComponentViaPython(bridge: UnrealBridge, params: {
  blueprintRef: string;
  componentClass: string;
  componentName: string;
  attachTo?: string;
  transform?: Record<string, unknown>;
  properties?: Record<string, unknown>;
  compile?: boolean;
  save?: boolean;
  timeoutMs?: number;
}) {
  const { primary, candidates } = BlueprintHelpers.resolveBlueprintCandidates(params.blueprintRef);
  if (!primary || candidates.length === 0) {
    return { success: false as const, message: 'Blueprint path could not be determined', error: 'INVALID_BLUEPRINT_PATH' };
  }

  const payload = {
    blueprintCandidates: candidates,
    requestedPath: primary,
    componentClass: params.componentClass,
    componentName: params.componentName,
    attachTo: params.attachTo,
    transform: params.transform,
    properties: params.properties,
    compile: params.compile === true,
    save: params.save === true
  };

  let probeResult: any = undefined;
  let probeAttempted = false;

  const payloadJson = JSON.stringify(payload);
  const payloadBase64 = Buffer.from(payloadJson).toString('base64');

  const pythonScript = `
import unreal
import json
import traceback
import base64

# Decode payload from base64 to avoid escaping issues
payload = json.loads(base64.b64decode('${payloadBase64}').decode('utf-8'))

result = {
  'success': False,
  'message': '',
  'error': '',
  'blueprintPath': payload.get('requestedPath', ''),
  'componentName': payload.get('componentName'),
  'componentClass': payload.get('componentClass'),
  'warnings': []
}

def add_warning(message):
  try:
    text = str(message)
  except Exception:
    text = message
  warnings_list = result.setdefault('warnings', [])
  warnings_list.append(text)

def coerce_blueprint(asset):
  if not asset:
    return None
  if isinstance(asset, unreal.Blueprint):
    return asset
  try:
    blueprint = unreal.BlueprintEditorLibrary.get_blueprint_from_instance(asset)
    if blueprint:
      return blueprint
  except Exception:
    pass
  try:
    blueprint = unreal.BlueprintEditorLibrary.get_blueprint(asset)
    if blueprint:
      return blueprint
  except Exception:
    pass
  try:
    generated = getattr(asset, 'generated_class', None)
    if callable(generated):
      generated_class = generated()
      if generated_class:
        blueprint = unreal.BlueprintEditorLibrary.get_blueprint_from_class(generated_class)
        if blueprint:
          return blueprint
  except Exception:
    pass
  try:
    owning = getattr(asset, 'class_generated_by', None)
    if owning:
      sub_blueprint = coerce_blueprint(owning)
      if sub_blueprint:
        return sub_blueprint
  except Exception:
    pass
  return None

def resolve_blueprint_path(candidates):
  editor_lib = unreal.EditorAssetLibrary
  for path in candidates:
    try:
      asset = editor_lib.load_asset(path)
      if not asset:
        continue
      blueprint = coerce_blueprint(asset)
      if blueprint:
        return path, blueprint
    except Exception:
      continue
  return None, None

def resolve_simple_construction_script(blueprint):
  if not blueprint:
    return None
  scs = None
  try:
    scs = blueprint.get_editor_property('simple_construction_script')
  except Exception:
    scs = None
  if not scs:
    try:
      scs = getattr(blueprint, 'simple_construction_script', None)
    except Exception:
      scs = None
  if not scs:
    try:
      scs = blueprint.get_simple_construction_script()
    except Exception:
      scs = None
  if not scs:
    try:
      scs = unreal.BlueprintEditorLibrary.get_simple_construction_script(blueprint)
    except Exception:
      scs = None
  if not scs:
    for attr_name in ('SimpleConstructionScript', 'Simple_construction_script', 'simpleConstructionScript'):
      try:
        scs = blueprint.get_editor_property(attr_name)
        if scs:
          break
      except Exception:
        scs = None
  if not scs:
    try:
      blueprint.modify()
      scs = blueprint.get_editor_property('simple_construction_script')
    except Exception:
      scs = None
  if not scs:
    generated = None
    try:
      generated = blueprint.generated_class() if callable(getattr(blueprint, 'generated_class', None)) else getattr(blueprint, 'generated_class', None)
    except Exception:
      generated = None
    if generated:
      for attr_name in ('SimpleConstructionScript', 'simple_construction_script'):
        try:
          scs = getattr(generated, attr_name, None)
          if scs:
            break
        except Exception:
          scs = None
      if not scs:
        try:
          scs = generated.get_editor_property('SimpleConstructionScript')
        except Exception:
          scs = None
  return scs

def resolve_component_class(spec):
  if not spec:
    return None, None
  name = spec.strip()
  direct = getattr(unreal, name, None)
  if direct:
    try:
      return direct, direct.get_path_name()
    except Exception:
      return direct, name
  candidates = []
  if name.startswith('/') and '.' in name:
    candidates.append(name)
  elif name.startswith('/Script/'):
    candidates.append(name)
  elif '.' in name:
    candidates.append('/Script/' + name)
  else:
    candidates.append(f'/Script/Engine.{name}')
    candidates.append(f'/Script/UMG.{name}')
    candidates.append(f'/Script/Paper2D.{name}')
  for path in candidates:
    try:
      loaded = unreal.load_class(None, path)
      if loaded:
        return loaded, path
    except Exception:
      continue
  return None, None

def find_scs_node_by_name(scs, target_name):
  try:
    nodes = scs.get_all_nodes()
  except Exception:
    nodes = []
  target_lower = (target_name or '').lower()
  for node in nodes:
    try:
      if node and node.get_variable_name().lower() == target_lower:
        return node
    except Exception:
      continue
  return None

def assign_simple_construction_script(blueprint, scs_candidate):
  errors = []
  assigned = False
  for prop_name in ('simple_construction_script', 'SimpleConstructionScript'):
    if assigned:
      break
    try:
      blueprint.set_editor_property(prop_name, scs_candidate)
      assigned = True
    except Exception as err:
      errors.append(err)

  if not assigned:
    setter = getattr(blueprint, 'set_simple_construction_script', None)
    if callable(setter):
      try:
        setter(scs_candidate)
        assigned = True
      except Exception as setter_err:
        errors.append(setter_err)

  if not assigned:
    for attr_name in ('SimpleConstructionScript', 'simple_construction_script'):
      try:
        setattr(blueprint, attr_name, scs_candidate)
        assigned = True
        break
      except Exception as attr_err:
        errors.append(attr_err)

  return assigned, errors

def ensure_simple_construction_script(blueprint, result):
  """
  For UE 5.6+: SimpleConstructionScript does not exist in Python API.
  Instead, use SubobjectDataSubsystem or BlueprintEditorLibrary for component management.
  This function now just returns the SCS if it exists, without trying to create one.
  """
  probe = result.setdefault('scsProbe', {})
  try:
    probe['blueprintType'] = str(type(blueprint))
  except Exception:
    pass

  # Try to resolve existing SCS (read-only)
  scs = resolve_simple_construction_script(blueprint)
  probe['initialResolved'] = bool(scs)
  
  if scs:
    probe['result'] = 'existing'
    return scs
  
  # In UE 5.6+, we cannot create SCS via Python
  # Component addition should use SubobjectDataSubsystem or Blueprint Editor helpers
  probe['result'] = 'unavailable'
  probe['note'] = 'SimpleConstructionScript not available in UE 5.6+ Python API - use Subobject subsystem instead'
  return None

try:
  blueprint_path, blueprint = resolve_blueprint_path(payload.get('blueprintCandidates', []))
  if not blueprint:
    raise RuntimeError(f"Blueprint not found: {payload.get('requestedPath')}")

  result['blueprintPath'] = blueprint_path
  result['blueprintAssetType'] = str(type(blueprint))

  try:
    parent_class = getattr(blueprint, 'parent_class', None)
    if parent_class:
      result['blueprintParentClass'] = str(parent_class)
  except Exception:
    try:
      result['blueprintParentClass'] = str(blueprint.get_editor_property('parent_class'))
    except Exception:
      pass

  try:
    result['blueprintObjectPath'] = blueprint.get_path_name()
  except Exception:
    try:
      result['blueprintObjectPath'] = str(blueprint)
    except Exception:
      pass

  try:
    scs_related_attrs = [name for name in dir(blueprint) if 'construction' in name.lower()][:25]
    if scs_related_attrs:
      result['blueprintScsAttrs'] = scs_related_attrs
  except Exception:
    pass

  blueprint_editor_lib = getattr(unreal, 'BlueprintEditorLibrary', None)
  kismet_utils = getattr(unreal, 'KismetEditorUtilities', None)

  pre_compile_success = False
  pre_compile_attempted = False

  if blueprint_editor_lib and hasattr(blueprint_editor_lib, 'compile_blueprint'):
    pre_compile_attempted = True
    try:
      blueprint_editor_lib.compile_blueprint(blueprint)
      pre_compile_success = True
    except Exception as compile_before_err:
      add_warning(f"Pre-modify compile failed via BlueprintEditorLibrary: {compile_before_err}")

  if not pre_compile_success and kismet_utils and hasattr(kismet_utils, 'compile_blueprint'):
    pre_compile_attempted = True
    try:
      kismet_utils.compile_blueprint(blueprint)
      pre_compile_success = True
    except Exception as compile_before_err:
      add_warning(f"Pre-modify compile failed via KismetEditorUtilities: {compile_before_err}")

  if not pre_compile_attempted:
    add_warning('Pre-modify compile skipped: compile helpers unavailable')

  result['compiledBeforeModify'] = pre_compile_success

  scs = ensure_simple_construction_script(blueprint, result)
  if scs:
    result['scsType'] = str(type(scs))
  else:
    result['scsType'] = None

  component_class, resolved_class_path = resolve_component_class(payload.get('componentClass'))
  if not component_class:
    raise RuntimeError(f"Unable to resolve component class: {payload.get('componentClass')}")

  if not issubclass(component_class, unreal.ActorComponent):
    raise RuntimeError(f"Resolved class is not an ActorComponent: {resolved_class_path}")

  component_name = payload.get('componentName')
  if not component_name:
    raise RuntimeError('Component name is required')

  helper_errors = []
  addition_method = None
  new_node = None
  component_added = False

  # UE 5.6+ Approach: Use SubobjectDataSubsystem (preferred) or Blueprint Editor Library
  subobject_subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
  blueprint_editor_lib = getattr(unreal, 'BlueprintEditorLibrary', None)
  
  # Method 1: SubobjectDataSubsystem (UE 5.6+ preferred way)
  if subobject_subsystem and hasattr(subobject_subsystem, 'k2_gather_subobject_data_for_blueprint'):
    try:
      blueprint.modify()
      
      # Gather existing subobject data for the blueprint
      subobject_handles = subobject_subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
      
      if subobject_handles and len(subobject_handles) > 0:
        # Use the first handle as the parent (blueprint root)
        parent_handle = subobject_handles[0]
        
        # Try to add component using the subsystem
        # This is the modern UE 5.6+ way to add blueprint components
        params = unreal.AddNewSubobjectParams()
        params.parent_handle = parent_handle
        params.new_class = component_class
        params.blueprint_context = blueprint
        
        component_handle = None
        # Try single-argument call first (some engine builds expect this)
        try:
          component_handle = subobject_subsystem.add_new_subobject(params)
        except TypeError:
          # Some builds require two-arg signature; attempt second form
          try:
            component_handle = subobject_subsystem.add_new_subobject(params, parent_handle)
          except Exception as two_arg_err:
            helper_errors.append(f"SubobjectDataSubsystem (two-arg) failed: {two_arg_err}")
        except Exception as single_err:
          helper_errors.append(f"SubobjectDataSubsystem (single-arg) failed: {single_err}")
        
        if component_handle:
          # Some engine builds return non-object handles (tuples) which cannot be
          # nativized by rename_subobject_member_variable. Detect this and
          # attempt to resolve to a proper object before calling rename. If we
          # cannot, fall back to other addition methods later.
          try:
            valid_check = getattr(component_handle, 'is_valid', None)
            if callable(valid_check):
              is_valid = valid_check()
            else:
              # If no is_valid method, heuristically treat as invalid for
              # purposes of calling rename_subobject_member_variable.
              is_valid = False
          except Exception:
            is_valid = False

          if is_valid:
            # Rename the component using the subsystem method
            subobject_subsystem.rename_subobject_member_variable(blueprint, component_handle, unreal.Name(component_name))
            component_added = True
            addition_method = 'SubobjectDataSubsystem.add_new_subobject'
            result['additionMethod'] = addition_method
            result['success'] = True
            result['message'] = f"Component {component_name} added to {blueprint_path}"
            new_node = component_handle
          else:
            # Try to find the created component via BlueprintEditorLibrary if
            # the subsystem returned a non-native handle (e.g., tuple). This
            # helps on engine builds where the subsystem returns transient
            # identifiers instead of real objects.
            try:
              bel = getattr(unreal, 'BlueprintEditorLibrary', None)
              if bel and hasattr(bel, 'get_components'):
                comps = []
                try:
                  comps = bel.get_components(blueprint) or []
                except Exception:
                  comps = []
                for c in comps:
                  try:
                    # compare by variable name or by object name as fallback
                    varname = getattr(c, 'get_variable_name', lambda: None)()
                    name_ok = (str(varname or '') == component_name)
                    if not name_ok:
                      try:
                        name_ok = c.get_name() == component_name
                      except Exception:
                        name_ok = False
                    if name_ok:
                      new_node = c
                      component_added = True
                      addition_method = 'SubobjectDataSubsystem.add_new_subobject (resolved via BlueprintEditorLibrary)'
                      result['additionMethod'] = addition_method
                      result['success'] = True
                      result['message'] = f"Component {component_name} added to {blueprint_path} (resolved)"
                      break
                  except Exception:
                    continue
            except Exception as resolve_err:
              helper_errors.append(f"SubobjectDataSubsystem returned non-handle and resolution attempt failed: {resolve_err}")
          # If we've reached this point and haven't set component_added via
          # the valid handle path or the BlueprintEditorLibrary resolution,
          # don't assume success. The calling code below will use helper
          # errors to decide fallback strategies.
      else:
        helper_errors.append("SubobjectDataSubsystem: No subobject handles found for blueprint")
    except Exception as subsystem_err:
      helper_errors.append(f"SubobjectDataSubsystem: {subsystem_err}")
  
  # Method 2: BlueprintEditorLibrary (fallback)
  if not component_added and blueprint_editor_lib and hasattr(blueprint_editor_lib, 'add_component_to_blueprint'):
    try:
      blueprint.modify()
      new_node = blueprint_editor_lib.add_component_to_blueprint(blueprint, component_class, component_name)
      if new_node:
        component_added = True
        addition_method = 'BlueprintEditorLibrary.add_component_to_blueprint'
        result['additionMethod'] = addition_method
        result['success'] = True
        result['message'] = f"Component {component_name} added to {blueprint_path}"
    except Exception as helper_err:
      helper_errors.append(f"BlueprintEditorLibrary: {helper_err}")

  # Method 3: SCS direct manipulation (legacy, only if SCS exists)
  if not component_added and scs:
    try:
      blueprint.modify()
      scs.modify()
      new_node = scs.create_node(component_class, component_name)
      if new_node:
        scs.add_node(new_node)
        component_added = True
        addition_method = 'SimpleConstructionScript.create_node'
        result['additionMethod'] = addition_method
        result['success'] = True
        result['message'] = f"Component {component_name} added to {blueprint_path}"
    except Exception as create_err:
      helper_errors.append(f"SCS create_node failed: {create_err}")

  if not component_added:
    if helper_errors:
      for helper_error in helper_errors:
        add_warning(f"Component addition failed: {helper_error}")
    
    # Report failure with helpful error message
    error_msg = "Failed to add component using all available methods. "
    if not subobject_subsystem:
      error_msg += "SubobjectDataSubsystem not available. "
    if not blueprint_editor_lib:
      error_msg += "BlueprintEditorLibrary not available. "
    if not scs:
      error_msg += "SimpleConstructionScript not available. "
    
    raise RuntimeError(error_msg + f"Errors: {'; '.join(helper_errors)}")

  # Handle attachment and transforms if component was added
  if component_added and new_node:
    # Handle attachment to parent component
    attach_to = payload.get('attachTo')
    if attach_to and scs:
      parent = find_scs_node_by_name(scs, attach_to)
      if parent and new_node:
        try:
          parent.add_child_node(new_node)
          result['attachedTo'] = attach_to
        except Exception as attach_err:
          add_warning(f"Failed to attach to parent: {attach_err}")
    
    # Apply transforms if it's a scene component
    transform = payload.get('transform') or {}
    scene_template = None
    try:
      scene_template = new_node.get_editor_property('component_template')
    except Exception:
      scene_template = None

    if scene_template and isinstance(scene_template, unreal.SceneComponent):
      loc = transform.get('location') or {}
      rot = transform.get('rotation') or {}
      scale = transform.get('scale') or {}
      scene_template.set_editor_property('relative_location', unreal.Vector(loc.get('x', 0.0), loc.get('y', 0.0), loc.get('z', 0.0)))
      scene_template.set_editor_property('relative_rotation', unreal.Rotator(rot.get('pitch', 0.0), rot.get('yaw', 0.0), rot.get('roll', 0.0)))
      scene_template.set_editor_property('relative_scale3d', unreal.Vector(scale.get('x', 1.0), scale.get('y', 1.0), scale.get('z', 1.0)))
    elif transform:
      result['warnings'].append('Transform ignored for non-scene component template.')

    properties = payload.get('properties') or {}
    template = None
    try:
      template = new_node.get_editor_property('component_template')
    except Exception:
      template = None

    if template and isinstance(properties, dict):
      for key, value in properties.items():
        try:
          template.set_editor_property(key, value)
        except Exception as err:
          result['warnings'].append(f"Property {key} could not be applied: {err}")

    compile_requested = bool(payload.get('compile'))
    save_requested = bool(payload.get('save'))

    post_compile_success = False
    if compile_requested:
      post_compile_helper_used = False
      kismet_utils = getattr(unreal, 'KismetEditorUtilities', None)
      
      if kismet_utils and hasattr(kismet_utils, 'compile_blueprint'):
        post_compile_helper_used = True
        try:
          kismet_utils.compile_blueprint(blueprint)
          post_compile_success = True
        except Exception as compile_err:
          add_warning(f"Post-modify compile failed via KismetEditorUtilities: {compile_err}")

      if not post_compile_success and blueprint_editor_lib and hasattr(blueprint_editor_lib, 'compile_blueprint'):
        post_compile_helper_used = True
        try:
          blueprint_editor_lib.compile_blueprint(blueprint)
          post_compile_success = True
        except Exception as compile_err:
          add_warning(f"Post-modify compile failed via BlueprintEditorLibrary: {compile_err}")

      if not post_compile_helper_used:
        add_warning('Post-modify compile skipped: compile helpers unavailable')

    saved = False
    if save_requested:
      try:
        saved = unreal.EditorAssetLibrary.save_loaded_asset(blueprint_path)
      except Exception as save_err:
        result['warnings'].append(f"Blueprint save failed: {save_err}")

    unreal.BlueprintEditorLibrary.mark_blueprint_as_structurally_modified(blueprint)

    result['componentClass'] = resolved_class_path
    result['compiled'] = post_compile_success if compile_requested else False
    if save_requested:
      result['saved'] = saved
    if not result['warnings']:
      result.pop('warnings', None)

except Exception as err:
  result['success'] = False
  result['error'] = str(err)
  if not result['message']:
    result['message'] = str(err)

print('RESULT:' + json.dumps(result))
`.trim();

  try {
  const respTimeout = typeof params.timeoutMs === 'number' && params.timeoutMs > 0 ? params.timeoutMs : undefined;
  const response = await bridge.executePythonWithResult(pythonScript, respTimeout);
  const success = Boolean(response?.success);
    const warnings = coerceStringArray(response?.warnings) ?? undefined;
    const resultMessage = coerceString(response?.message) ?? (success ? 'Component added via Python fallback.' : 'Python fallback failed');
    const blueprintPath = coerceString(response?.blueprintPath) ?? primary;
    const resolvedClass = coerceString(response?.componentClass) ?? params.componentClass;
    const rawResponse = response && typeof response === 'object' ? (response as Record<string, unknown>) : undefined;

    if (!success) {
      const errorText = coerceString(response?.error) ?? resultMessage;
      const lowerError = (errorText || '').toLowerCase();
      if (lowerError.includes('nativize') || lowerError.includes('subobjectdatahandle')) {
        try {
          const probe = await BlueprintProbes.probeSubobjectDataHandle(bridge, params.componentClass);
          probeAttempted = true;
          probeResult = probe;
          if (Array.isArray(probe?.subobject_data_handle_fields)) {
            (warnings ?? []).push(`Probe result: ${JSON.stringify(probe).slice(0, 1200)}`);
          }
          // Retry logic omitted here for brevity; the original retry builds a coerced payload
        } catch (retryErr) {
          // attach warning and fall through to return error
          (warnings ?? []).push(`Python retry/probe failed: ${ (retryErr as any)?.message ?? String(retryErr) }`);
        }
      }

        return {
        success: false as const,
        message: `Failed to add component via Python fallback: ${errorText}`,
        error: errorText,
        blueprintPath,
        componentName: params.componentName,
        componentClass: resolvedClass,
        warnings,
        rawResponse,
        telemetry: probeAttempted ? { probeAttempted, probeResult } : undefined
      };
    }

    const payloadResult: any = {
      success: true,
      message: resultMessage,
      blueprintPath,
      componentName: params.componentName,
      componentClass: resolvedClass,
      compiled: response?.compiled === true,
      saved: response?.saved === true,
      warnings,
      transport: 'python',
      rawResponse
    };

    if (probeAttempted) payloadResult.telemetry = { probeAttempted, probeResult };
    return payloadResult;
  } catch (err: any) {
    return { success: false as const, message: 'Failed to execute Python fallback', error: err?.message || String(err), blueprintPath: primary, componentName: params.componentName, componentClass: params.componentClass };
  }
}
