import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { PythonHelper } from '../utils/python-helpers.js';
import { validateAssetParams, concurrencyDelay } from '../utils/validation.js';
import { extractTaggedLine } from '../utils/python-output.js';
import { interpretStandardResult, coerceBoolean, coerceString, coerceStringArray, bestEffortInterpretedText } from '../utils/result-helpers.js';
import { escapePythonString } from '../utils/python.js';

type VectorInput = [number, number, number] | { [key: string]: unknown };

type TransformInput = {
  location?: VectorInput;
  rotation?: VectorInput;
  scale?: VectorInput;
};

type BlueprintScsOperationInput = {
  type: string;
  componentName?: string;
  componentClass?: string;
  attachTo?: string;
  transform?: TransformInput;
  properties?: Record<string, unknown>;
};
export class BlueprintTools {
  private python: PythonHelper;

  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {
    this.python = new PythonHelper(bridge);
  }

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  private normalizePartialVector(value: unknown, alternateKeys: string[] = ['x', 'y', 'z']): Record<string, number> | undefined {
    if (value === undefined || value === null) {
      return undefined;
    }

    const result: Record<string, number> = {};

    const assignIfPresent = (component: 'x' | 'y' | 'z', raw: unknown) => {
      const numberValue = this.toFiniteNumber(raw);
      if (numberValue !== undefined) {
        result[component] = numberValue;
      }
    };

    if (Array.isArray(value)) {
      if (value.length > 0) {
        assignIfPresent('x', value[0]);
      }
      if (value.length > 1) {
        assignIfPresent('y', value[1]);
      }
      if (value.length > 2) {
        assignIfPresent('z', value[2]);
      }
    } else if (typeof value === 'object') {
      const obj = value as Record<string, unknown>;
      assignIfPresent('x', obj.x ?? obj[alternateKeys[0]]);
      assignIfPresent('y', obj.y ?? obj[alternateKeys[1]]);
      assignIfPresent('z', obj.z ?? obj[alternateKeys[2]]);
    } else {
      assignIfPresent('x', value);
    }

    return Object.keys(result).length > 0 ? result : undefined;
  }

  private toFiniteNumber(raw: unknown): number | undefined {
    if (typeof raw === 'number' && Number.isFinite(raw)) return raw;
    if (typeof raw === 'string') {
      const trimmed = raw.trim();
      if (trimmed.length === 0) return undefined;
      const parsed = Number(trimmed);
      if (Number.isFinite(parsed)) return parsed;
    }
    return undefined;
  }

  private normalizeTransformInput(transform: TransformInput | undefined): Record<string, unknown> | undefined {
    if (!transform || typeof transform !== 'object') {
      return undefined;
    }

    const result: Record<string, unknown> = {};
    const location = this.normalizePartialVector(transform.location);
    if (location) {
      result.location = location;
    }
    const rotation = this.normalizePartialVector(transform.rotation, ['pitch', 'yaw', 'roll']);
    if (rotation) {
      result.rotation = rotation;
    }
    const scale = this.normalizePartialVector(transform.scale);
    if (scale) {
      result.scale = scale;
    }

    return Object.keys(result).length > 0 ? result : undefined;
  }

  private sanitizeScsOperation(rawOperation: BlueprintScsOperationInput, index: number): { ok: true; operation: Record<string, unknown> } | { ok: false; error: string } {
    if (!rawOperation || typeof rawOperation !== 'object') {
      return { ok: false, error: `Operation at index ${index} must be an object.` };
    }

    const type = coerceString(rawOperation.type)?.toLowerCase();
    if (!type) {
      return { ok: false, error: `Operation at index ${index} missing type.` };
    }

    const operation: Record<string, unknown> = { type };

    const componentName = coerceString(rawOperation.componentName ?? (rawOperation as any).name);
    const componentClass = coerceString(rawOperation.componentClass ?? (rawOperation as any).componentType ?? (rawOperation as any).class);
    const attachTo = coerceString(rawOperation.attachTo ?? (rawOperation as any).parent ?? (rawOperation as any).attach);
    const transform = this.normalizeTransformInput(rawOperation.transform);
    const properties = rawOperation.properties && typeof rawOperation.properties === 'object' ? rawOperation.properties : undefined;

    switch (type) {
      case 'add_component': {
        if (!componentName) {
          return { ok: false, error: `add_component operation at index ${index} requires componentName.` };
        }
        if (!componentClass) {
          return { ok: false, error: `add_component operation for ${componentName} missing componentClass.` };
        }
        operation.componentName = componentName;
        operation.componentClass = componentClass;
        if (attachTo) {
          operation.attachTo = attachTo;
        }
        if (transform) {
          operation.transform = transform;
        }
        if (properties) {
          operation.properties = properties;
        }
        break;
      }
      case 'remove_component': {
        if (!componentName) {
          return { ok: false, error: `remove_component operation at index ${index} requires componentName.` };
        }
        operation.componentName = componentName;
        break;
      }
      case 'set_component_properties':
      case 'modify_component': {
        if (!componentName) {
          return { ok: false, error: `set_component_properties operation at index ${index} requires componentName.` };
        }
        operation.componentName = componentName;
        if (transform) {
          operation.transform = transform;
        }
        if (properties) {
          operation.properties = properties;
        }
        break;
      }
      case 'attach_component': {
        // Attach a component to a parent component by name
        const parent = coerceString((rawOperation as any).parentComponent ?? (rawOperation as any).parent);
        if (!componentName) {
          return { ok: false, error: `attach_component operation at index ${index} requires componentName.` };
        }
        if (!parent) {
          return { ok: false, error: `attach_component operation at index ${index} requires parentComponent.` };
        }
        operation.componentName = componentName;
        operation.attachTo = parent;
        break;
      }
      default:
        return { ok: false, error: `Unknown SCS operation type: ${type}` };
    }

    return { ok: true, operation };
  }

  private resolveBlueprintCandidates(rawName: string | undefined): { primary: string | undefined; candidates: string[] } {
    const trimmed = coerceString(rawName)?.trim();
    if (!trimmed) {
      return { primary: undefined, candidates: [] };
    }

    const normalizedInput = trimmed.replace(/\\/g, '/').replace(/\/+/g, '/');
    const withoutLeading = normalizedInput.replace(/^\/+/, '');
    const seen = new Set<string>();
    const primaryCandidates: string[] = [];
    const secondaryCandidates: string[] = [];

    const addCandidate = (value: string | undefined, primary = false) => {
      if (!value) {
        return;
      }
      const normalized = value.replace(/\\/g, '/').replace(/\/+/g, '/');
      const rooted = normalized.startsWith('/') ? normalized : `/${normalized}`;
      if (seen.has(rooted)) {
        return;
      }
      seen.add(rooted);
      if (primary) {
        primaryCandidates.push(rooted);
      } else {
        secondaryCandidates.push(rooted);
      }

      if (!rooted.includes('.')) {
        const segments = rooted.split('/');
        const leaf = segments[segments.length - 1]?.trim();
        if (leaf) {
          const objectPath = `${rooted}.${leaf}`;
          if (!seen.has(objectPath)) {
            seen.add(objectPath);
            if (primary) {
              primaryCandidates.push(objectPath);
            } else {
              secondaryCandidates.push(objectPath);
            }
          }

          const generatedCandidate = rooted.endsWith('_C') ? rooted : `${rooted}_C`;
          if (!seen.has(generatedCandidate)) {
            seen.add(generatedCandidate);
            if (primary) {
              primaryCandidates.push(generatedCandidate);
            } else {
              secondaryCandidates.push(generatedCandidate);
            }
          }
        }
      }
    };

    if (normalizedInput.startsWith('/Game/Blueprints/')) {
      addCandidate(normalizedInput, true);
    } else if (!normalizedInput.startsWith('/')) {
      addCandidate(`/Game/Blueprints/${withoutLeading}`, true);
      addCandidate(`/Game/${withoutLeading}`);
      addCandidate(`/${withoutLeading}`);
    } else if (normalizedInput.startsWith('/Game/')) {
      const remainder = withoutLeading.slice('Game/'.length);
      if (remainder) {
        addCandidate(`/Game/Blueprints/${remainder}`, true);
      }
      addCandidate(normalizedInput);
    } else {
      addCandidate(normalizedInput, true);
      addCandidate(`/Game/${withoutLeading}`);
      addCandidate(`/Game/Blueprints/${withoutLeading}`);
    }

    addCandidate(normalizedInput.startsWith('/') ? normalizedInput : `/${withoutLeading}`);

    const ordered = [...primaryCandidates, ...secondaryCandidates];
    return { primary: ordered[0], candidates: ordered };
  }

  private shouldAttemptPythonFallback(errorCode?: string, message?: string): boolean {
    const normalizedCode = (errorCode ?? '').toUpperCase();
    const normalizedMessage = (message ?? '').toLowerCase();

    if (!normalizedCode && !normalizedMessage) {
      return false;
    }

    if (normalizedCode.includes('AUTOMATION_BRIDGE')) {
      return true;
    }

    if (normalizedMessage.includes('automation bridge')) {
      return true;
    }

    // Fall back to Python for SCS/unavailable construction script scenarios and
    // for blueprint/property related errors where the C++ automation path may
    // not be able to perform the requested operation (UE API differences).
    if (normalizedCode.includes('SCS_UNAVAILABLE') || normalizedMessage.includes('simpleconstructionscript') || normalizedMessage.includes('scs_unavailable')) {
      return true;
    }

    // Allow fallback to Python when the automation bridge couldn't resolve a component class
    if (normalizedCode.includes('COMPONENT_CLASS_NOT_FOUND') || normalizedMessage.includes('unable to load component class')) {
      return true;
    }

    // If the automation bridge returned a property-not-found style error, allow
    // python fallback so higher-level tooling can attempt to add variables
    // or use editor helper APIs to resolve the condition.
    if (normalizedCode.includes('PROPERTY_NOT_FOUND') || normalizedMessage.includes('property') && normalizedMessage.includes('not found')) {
      return true;
    }

    return false;
  }

  private async addComponentViaPython(params: {
    blueprintRef: string;
    componentClass: string;
    componentName: string;
    attachTo?: string;
    transform?: Record<string, unknown>;
    properties?: Record<string, unknown>;
    compile?: boolean;
    save?: boolean;
  }) {
    const { primary, candidates } = this.resolveBlueprintCandidates(params.blueprintRef);
    if (!primary || candidates.length === 0) {
      return {
        success: false as const,
        message: 'Blueprint path could not be determined',
        error: 'INVALID_BLUEPRINT_PATH'
      };
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

    // Use base64 encoding to avoid any escaping issues with JSON in Python strings
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
        
        if component_handle and getattr(component_handle, 'is_valid', lambda: True)():
          # Rename the component using the subsystem method
          subobject_subsystem.rename_subobject_member_variable(blueprint, component_handle, unreal.Name(component_name))
          component_added = True
          addition_method = 'SubobjectDataSubsystem.add_new_subobject'
          result['additionMethod'] = addition_method
          result['success'] = True
          result['message'] = f"Component {component_name} added to {blueprint_path}"
          
          # Store handle for later use
          new_node = component_handle
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
    const response = await this.bridge.executePythonWithResult(pythonScript);
    const success = Boolean(response?.success);
      const simulated = response?.simulated === true;
      const warnings = coerceStringArray(response?.warnings) ?? undefined;
      const resultMessage = coerceString(response?.message) ?? (success ? 'Component added via Python fallback.' : 'Python fallback failed');
      const blueprintPath = coerceString(response?.blueprintPath) ?? primary;
      const resolvedClass = coerceString(response?.componentClass) ?? params.componentClass;
    const rawResponse = response && typeof response === 'object' ? (response as Record<string, unknown>) : undefined;

      if (!success) {
        const errorText = coerceString(response?.error) ?? resultMessage;
        return {
          success: false as const,
          message: `Failed to add component via Python fallback: ${errorText}`,
          error: errorText,
          blueprintPath,
          componentName: params.componentName,
          componentClass: resolvedClass,
          warnings,
          rawResponse,
          simulated: simulated ? true : undefined
        };
      }

      const payloadResult: {
        success: true;
        message: string;
        blueprintPath: string;
        componentName: string;
        componentClass: string;
        component?: string;
        compiled?: boolean;
        saved?: boolean;
        warnings?: string[];
        simulated?: boolean;
        transport: 'python';
        rawResponse?: Record<string, unknown>;
      } = {
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

      if (simulated) {
        payloadResult.simulated = true;
      }

      payloadResult.component = params.componentName;

      if (!payloadResult.compiled) {
        delete payloadResult.compiled;
      }
      if (!payloadResult.saved) {
        delete payloadResult.saved;
      }
      if (!payloadResult.warnings || payloadResult.warnings.length === 0) {
        delete payloadResult.warnings;
      }
      if (!payloadResult.rawResponse) {
        delete payloadResult.rawResponse;
      }
      if (!payloadResult.simulated) {
        delete payloadResult.simulated;
      }

      return payloadResult;
    } catch (err: any) {
      return {
        success: false as const,
        message: 'Failed to execute Python fallback',
        error: err?.message || String(err),
        blueprintPath: primary,
        componentName: params.componentName,
        componentClass: params.componentClass
      };
    }
  }

  private async validateParentClassReference(parentClass: string, blueprintType: string): Promise<{ ok: boolean; resolved?: string; error?: string }> {
    const trimmed = parentClass?.trim();
    if (!trimmed) {
      return { ok: true };
    }

    const escapedParent = escapePythonString(trimmed);
    const python = `
import unreal
import json

result = {
  'success': False,
  'resolved': '',
  'error': ''
}

def resolve_parent(spec, bp_type):
  name = (spec or '').strip()
  editor_lib = unreal.EditorAssetLibrary
  if not name:
    return None
  try:
    if name.startswith('/Script/'):
      return unreal.load_class(None, name)
  except Exception:
    pass
  try:
    if name.startswith('/Game/'):
      asset = editor_lib.load_asset(name)
      if asset:
        if hasattr(asset, 'generated_class'):
          try:
            generated = asset.generated_class()
            if generated:
              return generated
          except Exception:
            pass
        return asset
  except Exception:
    pass
  try:
    candidate = getattr(unreal, name, None)
    if candidate:
      return candidate
  except Exception:
    pass
  return None

try:
  parent_spec = r"${escapedParent}"
  resolved = resolve_parent(parent_spec, "${blueprintType}")
  resolved_path = ''

  if resolved:
    try:
      resolved_path = resolved.get_path_name()
    except Exception:
      try:
        resolved_path = str(resolved.get_outer().get_path_name())
      except Exception:
        resolved_path = str(resolved)

    normalized_resolved = resolved_path.replace('Class ', '').replace('class ', '').strip().lower()
    normalized_spec = parent_spec.strip().lower()

    if normalized_spec.startswith('/script/'):
      if not normalized_resolved.endswith(normalized_spec):
        resolved = None
    elif normalized_spec.startswith('/game/'):
      try:
        if not unreal.EditorAssetLibrary.does_asset_exist(parent_spec):
          resolved = None
      except Exception:
        resolved = None

  if resolved:
    result['success'] = True
    try:
      result['resolved'] = resolved_path or str(resolved)
    except Exception:
      result['resolved'] = str(resolved)
  else:
    result['error'] = 'Parent class not found: ' + parent_spec
except Exception as e:
  result['error'] = str(e)

print('RESULT:' + json.dumps(result))
`.trim();

    try {
      const response = await this.bridge.executePython(python);
      const interpreted = interpretStandardResult(response, {
        successMessage: 'Parent class resolved',
        failureMessage: 'Parent class validation failed'
      });

      if (interpreted.success) {
        return { ok: true, resolved: (interpreted.payload as any)?.resolved ?? interpreted.message };
      }

      const error = interpreted.error || (interpreted.payload as any)?.error || `Parent class not found: ${trimmed}`;
      return { ok: false, error };
    } catch (err: any) {
      return { ok: false, error: err?.message || String(err) };
    }
  }

  /**
   * Create Blueprint
   */
  async createBlueprint(params: {
    name: string;
    blueprintType: 'Actor' | 'Pawn' | 'Character' | 'GameMode' | 'PlayerController' | 'HUD' | 'ActorComponent';
    savePath?: string;
    parentClass?: string;
  }) {
    try {
      // Validate and sanitize parameters
      const validation = validateAssetParams({
        name: params.name,
        savePath: params.savePath || '/Game/Blueprints'
      });
      
      if (!validation.valid) {
        return {
          success: false,
          message: `Failed to create blueprint: ${validation.error}`,
          error: validation.error
        };
      }
      const sanitizedParams = validation.sanitized;
      const path = sanitizedParams.savePath || '/Game/Blueprints';

      if (path.startsWith('/Engine')) {
        const message = `Failed to create blueprint: destination path ${path} is read-only`;
        return { success: false, message, error: message };
      }
      if (params.parentClass && params.parentClass.trim()) {
        const parentValidation = await this.validateParentClassReference(params.parentClass, params.blueprintType);
        if (!parentValidation.ok) {
          const error = parentValidation.error || `Parent class not found: ${params.parentClass}`;
          const message = `Failed to create blueprint: ${error}`;
          return { success: false, message, error };
        }
      }
  const escapedName = escapePythonString(sanitizedParams.name);
  const escapedPath = escapePythonString(path);
  const escapedParent = escapePythonString(params.parentClass ?? '');

      await concurrencyDelay();

      const pythonScript = `
import unreal
import time
import json
import traceback

def ensure_asset_persistence(asset_path):
  try:
    asset_subsystem = None
    try:
      asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    except Exception:
      asset_subsystem = None

    editor_lib = unreal.EditorAssetLibrary

    asset = None
    if asset_subsystem and hasattr(asset_subsystem, 'load_asset'):
      try:
        asset = asset_subsystem.load_asset(asset_path)
      except Exception:
        asset = None
    if not asset:
      try:
        asset = editor_lib.load_asset(asset_path)
      except Exception:
        asset = None
    if not asset:
      return False

    saved = False
    if asset_subsystem and hasattr(asset_subsystem, 'save_loaded_asset'):
      try:
        saved = asset_subsystem.save_loaded_asset(asset)
      except Exception:
        saved = False
    if not saved and asset_subsystem and hasattr(asset_subsystem, 'save_asset'):
      try:
        saved = asset_subsystem.save_asset(asset_path, only_if_is_dirty=False)
      except Exception:
        saved = False
    if not saved:
      try:
        if hasattr(editor_lib, 'save_loaded_asset'):
          saved = editor_lib.save_loaded_asset(asset)
        else:
          saved = editor_lib.save_asset(asset_path, only_if_is_dirty=False)
      except Exception:
        saved = False

    if not saved:
      return False

    asset_dir = asset_path.rsplit('/', 1)[0]
    try:
      registry = unreal.AssetRegistryHelpers.get_asset_registry()
      if hasattr(registry, 'scan_paths_synchronous'):
        registry.scan_paths_synchronous([asset_dir], True)
    except Exception:
      pass

    for _ in range(5):
      if editor_lib.does_asset_exist(asset_path):
        return True
      time.sleep(0.2)
      try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        if hasattr(registry, 'scan_paths_synchronous'):
          registry.scan_paths_synchronous([asset_dir], True)
      except Exception:
        pass
    return False
  except Exception as e:
    print(f"Error ensuring persistence: {e}")
    return False

def resolve_parent_class(explicit_name, blueprint_type):
  editor_lib = unreal.EditorAssetLibrary
  name = (explicit_name or '').strip()
  if name:
    try:
      if name.startswith('/Script/'):
        try:
          loaded = unreal.load_class(None, name)
          if loaded:
            return loaded
        except Exception:
          pass
      if name.startswith('/Game/'):
        loaded_asset = editor_lib.load_asset(name)
        if loaded_asset:
          if hasattr(loaded_asset, 'generated_class'):
            try:
              generated = loaded_asset.generated_class()
              if generated:
                return generated
            except Exception:
              pass
          return loaded_asset
      candidate = getattr(unreal, name, None)
      if candidate:
        return candidate
    except Exception:
      pass
    return None

  mapping = {
    'Actor': unreal.Actor,
    'Pawn': unreal.Pawn,
    'Character': unreal.Character,
    'GameMode': unreal.GameModeBase,
    'PlayerController': unreal.PlayerController,
    'HUD': unreal.HUD,
    'ActorComponent': unreal.ActorComponent,
  }
  return mapping.get(blueprint_type, unreal.Actor)

result = {
  'success': False,
  'message': '',
  'path': '',
  'error': '',
  'exists': False,
  'parent': '',
  'verifyError': '',
  'warnings': [],
  'details': []
}

success_message = ''

def record_detail(message):
  result['details'].append(str(message))

def record_warning(message):
  result['warnings'].append(str(message))

def set_message(message):
  global success_message
  if not success_message:
    success_message = str(message)

def set_error(message):
  result['error'] = str(message)

asset_path = "${escapedPath}"
asset_name = "${escapedName}"
full_path = f"{asset_path}/{asset_name}"
result['path'] = full_path

asset_subsystem = None
try:
  asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
except Exception:
  asset_subsystem = None

editor_lib = unreal.EditorAssetLibrary

try:
  level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
  play_subsystem = None
  try:
    play_subsystem = unreal.get_editor_subsystem(unreal.EditorPlayWorldSubsystem)
  except Exception:
    play_subsystem = None

  is_playing = False
  if level_subsystem and hasattr(level_subsystem, 'is_in_play_in_editor'):
    is_playing = bool(level_subsystem.is_in_play_in_editor())
  elif play_subsystem and hasattr(play_subsystem, 'is_playing_in_editor'):
    is_playing = bool(play_subsystem.is_playing_in_editor())

  if is_playing:
    print('Stopping Play In Editor mode...')
    record_detail('Stopping Play In Editor mode')
    if level_subsystem and hasattr(level_subsystem, 'editor_request_end_play'):
      level_subsystem.editor_request_end_play()
    elif play_subsystem and hasattr(play_subsystem, 'stop_playing_session'):
      play_subsystem.stop_playing_session()
    elif play_subsystem and hasattr(play_subsystem, 'end_play'):
      play_subsystem.end_play()
    else:
      record_warning('Unable to stop Play In Editor via modern subsystems; please stop PIE manually.')
    time.sleep(0.5)
except Exception as stop_err:
  record_warning(f'PIE stop check failed: {stop_err}')

try:
  try:
    if asset_subsystem and hasattr(asset_subsystem, 'does_asset_exist'):
      asset_exists = asset_subsystem.does_asset_exist(full_path)
    else:
      asset_exists = editor_lib.does_asset_exist(full_path)
  except Exception:
    asset_exists = editor_lib.does_asset_exist(full_path)

  result['exists'] = bool(asset_exists)

  if asset_exists:
    existing = None
    try:
      if asset_subsystem and hasattr(asset_subsystem, 'load_asset'):
        existing = asset_subsystem.load_asset(full_path)
      elif asset_subsystem and hasattr(asset_subsystem, 'get_asset'):
        existing = asset_subsystem.get_asset(full_path)
      else:
        existing = editor_lib.load_asset(full_path)
    except Exception:
      existing = editor_lib.load_asset(full_path)

    if existing:
      result['success'] = True
      result['message'] = f"Blueprint already exists at {full_path}"
      set_message(result['message'])
      record_detail(result['message'])
      try:
        result['parent'] = str(existing.generated_class())
      except Exception:
        try:
          result['parent'] = str(type(existing))
        except Exception:
          pass
    else:
      set_error(f"Asset exists but could not be loaded: {full_path}")
      record_warning(result['error'])
  else:
    factory = unreal.BlueprintFactory()
    explicit_parent = "${escapedParent}"
    parent_class = None

    if explicit_parent.strip():
      parent_class = resolve_parent_class(explicit_parent, "${params.blueprintType}")
      if not parent_class:
        set_error(f"Parent class not found: {explicit_parent}")
        record_warning(result['error'])
        raise RuntimeError(result['error'])
    else:
      parent_class = resolve_parent_class('', "${params.blueprintType}")

    if parent_class:
      result['parent'] = str(parent_class)
      record_detail(f"Resolved parent class: {result['parent']}")
      try:
        factory.set_editor_property('parent_class', parent_class)
      except Exception:
        try:
          factory.set_editor_property('ParentClass', parent_class)
        except Exception:
          try:
            factory.ParentClass = parent_class
          except Exception:
            pass

    new_asset = None
    try:
      if asset_subsystem and hasattr(asset_subsystem, 'create_asset'):
        new_asset = asset_subsystem.create_asset(
          asset_name=asset_name,
          package_path=asset_path,
          asset_class=unreal.Blueprint,
          factory=factory
        )
      else:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        new_asset = asset_tools.create_asset(
          asset_name=asset_name,
          package_path=asset_path,
          asset_class=unreal.Blueprint,
          factory=factory
        )
    except Exception as create_error:
      set_error(f"Asset creation failed: {create_error}")
      record_warning(result['error'])
      traceback.print_exc()
      new_asset = None

    if new_asset:
      result['message'] = f"Blueprint created at {full_path}"
      set_message(result['message'])
      record_detail(result['message'])
      if ensure_asset_persistence(full_path):
        verified = False
        try:
          if asset_subsystem and hasattr(asset_subsystem, 'does_asset_exist'):
            verified = asset_subsystem.does_asset_exist(full_path)
          else:
            verified = editor_lib.does_asset_exist(full_path)
        except Exception as verify_error:
          result['verifyError'] = str(verify_error)
          verified = editor_lib.does_asset_exist(full_path)

        if not verified:
          time.sleep(0.2)
          verified = editor_lib.does_asset_exist(full_path)
          if not verified:
            try:
              verified = editor_lib.load_asset(full_path) is not None
            except Exception:
              verified = False

        if verified:
          result['success'] = True
          result['error'] = ''
          set_message(result['message'])
        else:
          set_error(f"Blueprint not found after save: {full_path}")
          record_warning(result['error'])
      else:
        set_error('Failed to persist blueprint to disk')
        record_warning(result['error'])
    else:
      if not result['error']:
        set_error(f"Failed to create Blueprint {asset_name}")
except Exception as e:
  set_error(str(e))
  record_warning(result['error'])
  traceback.print_exc()

# Finalize messaging
default_success_message = f"Blueprint created at {full_path}"
default_failure_message = f"Failed to create blueprint {asset_name}"

if result['success'] and not success_message:
  set_message(default_success_message)

if not result['success'] and not result['error']:
  set_error(default_failure_message)

if not result['message']:
  if result['success']:
    result['message'] = success_message or default_success_message
  else:
    result['message'] = result['error'] or default_failure_message

result['error'] = None if result['success'] else result['error']

if not result['warnings']:
  result.pop('warnings')
if not result['details']:
  result.pop('details')
if result.get('error') is None:
  result.pop('error')

print('RESULT:' + json.dumps(result))
`.trim();

      // Prefer automation bridge for heavy editor Python operations so we can
      // specify a larger timeout. Fall back to direct bridge.executePython
      // when automationBridge is not available.
      let response: any;
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        // Allow up to 90s for heavy blueprint creation tasks on slow machines
        response = await this.automationBridge.sendAutomationRequest('execute_editor_python', { script: pythonScript }, { timeoutMs: 90000 });
        // The automation bridge returns an automation_response payload
        // Parse result shape to feed into our output parser
        response = response?.result ?? response;
      } else {
        response = await this.bridge.executePython(pythonScript);
      }
      return this.parseBlueprintCreationOutput(response, sanitizedParams.name, path);
    } catch (err) {
      return { success: false, error: `Failed to create blueprint: ${err}` };
    }
  }

  private parseBlueprintCreationOutput(response: any, blueprintName: string, blueprintPath: string) {
    const defaultPath = `${blueprintPath}/${blueprintName}`;
    const interpreted = interpretStandardResult(response, {
      successMessage: `Blueprint ${blueprintName} created`,
      failureMessage: `Failed to create blueprint ${blueprintName}`
    });

    const payload = interpreted.payload ?? {};
    const hasPayload = Object.keys(payload).length > 0;
    const warnings = interpreted.warnings ?? coerceStringArray((payload as any).warnings) ?? undefined;
    const details = interpreted.details ?? coerceStringArray((payload as any).details) ?? undefined;
    const path = coerceString((payload as any).path) ?? defaultPath;
    const parent = coerceString((payload as any).parent);
    const verifyError = coerceString((payload as any).verifyError);
    const exists = coerceBoolean((payload as any).exists);
    const errorValue = coerceString((payload as any).error) ?? interpreted.error;

    if (hasPayload) {
      if (interpreted.success) {
        const outcome: {
          success: true;
          message: string;
          path: string;
          exists?: boolean;
          parent?: string;
          verifyError?: string;
          warnings?: string[];
          details?: string[];
        } = {
          success: true,
          message: interpreted.message,
          path
        };

        if (typeof exists === 'boolean') {
          outcome.exists = exists;
        }
        if (parent) {
          outcome.parent = parent;
        }
        if (verifyError) {
          outcome.verifyError = verifyError;
        }
        if (warnings && warnings.length > 0) {
          outcome.warnings = warnings;
        }
        if (details && details.length > 0) {
          outcome.details = details;
        }

        return outcome;
      }

      const fallbackMessage = errorValue ?? interpreted.message;

      const failureOutcome: {
        success: false;
        message: string;
        error: string;
        path: string;
        exists?: boolean;
        parent?: string;
        verifyError?: string;
        warnings?: string[];
        details?: string[];
      } = {
        success: false,
        message: `Failed to create blueprint: ${fallbackMessage}`,
        error: fallbackMessage,
        path
      };

      if (typeof exists === 'boolean') {
        failureOutcome.exists = exists;
      }
      if (parent) {
        failureOutcome.parent = parent;
      }
      if (verifyError) {
        failureOutcome.verifyError = verifyError;
      }
      if (warnings && warnings.length > 0) {
        failureOutcome.warnings = warnings;
      }
      if (details && details.length > 0) {
        failureOutcome.details = details;
      }

      return failureOutcome;
    }

  const cleanedText = bestEffortInterpretedText(interpreted) ?? '';
  const failureMessage = extractTaggedLine(cleanedText, 'FAILED:');
    if (failureMessage) {
      return {
        success: false,
        message: `Failed to create blueprint: ${failureMessage}`,
        error: failureMessage,
        path: defaultPath
      };
    }

  if (cleanedText.includes('SUCCESS')) {
      return {
        success: true,
        message: `Blueprint ${blueprintName} created`,
        path: defaultPath
      };
    }

    return {
      success: false,
      message: interpreted.message,
  error: interpreted.error ?? (cleanedText || JSON.stringify(response)),
      path: defaultPath
    };
  }

  /**
   * Add Component to Blueprint using automation bridge SCS operations
   */
  async addComponent(params: {
    blueprintName: string;
    componentType: string;
    componentName: string;
    attachTo?: string;
    transform?: TransformInput;
    properties?: Record<string, unknown>;
    compile?: boolean;
    save?: boolean;
    timeoutMs?: number;
  }) {
    const blueprintName = coerceString(params.blueprintName);
    if (!blueprintName) {
      return {
        success: false as const,
        message: 'Blueprint name is required',
        error: 'INVALID_BLUEPRINT'
      };
    }

    const componentClass = coerceString(params.componentType);
    if (!componentClass) {
      return {
        success: false as const,
        message: 'Component class is required',
        error: 'INVALID_COMPONENT_CLASS'
      };
    }

    const rawComponentName = coerceString(params.componentName) ?? params.componentName;
    if (!rawComponentName) {
      return {
        success: false as const,
        message: 'Component name is required',
        error: 'INVALID_COMPONENT_NAME'
      };
    }

    const sanitizedComponentName = rawComponentName.replace(/[^A-Za-z0-9_]/g, '_');

    const operation: BlueprintScsOperationInput = {
      type: 'add_component',
      componentName: sanitizedComponentName,
      componentClass,
      attachTo: params.attachTo,
      transform: params.transform,
      properties: params.properties
    };

    // Resolve the blueprint name to a concrete path so the automation bridge
    // can reliably locate the asset regardless of whether the caller passed a
    // bare name (BP_TestActor) or a full path (/Game/Blueprints/BP_TestActor).
    const { primary: resolvedBlueprintPath } = this.resolveBlueprintCandidates(blueprintName);
    const blueprintPathToUse = resolvedBlueprintPath ?? blueprintName;

    const result = await this.modifyConstructionScript({
      blueprintPath: blueprintPathToUse,
      operations: [operation],
      compile: params.compile,
      save: params.save,
      timeoutMs: params.timeoutMs
    });

    if (!result.success) {
      if (this.shouldAttemptPythonFallback(result.error, result.message)) {
        const fallback = await this.addComponentViaPython({
          blueprintRef: blueprintName,
          componentClass,
          componentName: sanitizedComponentName,
          attachTo: params.attachTo,
          transform: this.normalizeTransformInput(params.transform),
          properties: params.properties && typeof params.properties === 'object' ? params.properties : undefined,
          compile: params.compile,
          save: params.save
        });

        if (fallback.success) {
          return {
            ...fallback,
            component: sanitizedComponentName,
            componentName: sanitizedComponentName,
            componentType: componentClass,
            componentClass,
            blueprintPath: fallback.blueprintPath ?? blueprintName
          } as const;
        }

        return {
          ...fallback,
          component: sanitizedComponentName,
          componentName: sanitizedComponentName,
          componentType: componentClass,
          componentClass,
          blueprintPath: fallback.blueprintPath ?? blueprintName
        } as const;
      }

      return {
        ...result,
        component: sanitizedComponentName,
        componentName: sanitizedComponentName,
        componentType: componentClass,
        componentClass,
        blueprintPath: result.blueprintPath ?? blueprintName
      } as const;
    }

    return {
      ...result,
      component: sanitizedComponentName,
      componentName: sanitizedComponentName,
      componentType: componentClass,
      componentClass,
      blueprintPath: result.blueprintPath ?? blueprintName
    } as const;
  }

  async modifyConstructionScript(params: {
    blueprintPath: string;
    operations: BlueprintScsOperationInput[];
    compile?: boolean;
    save?: boolean;
    timeoutMs?: number;
  }) {
    const blueprintPath = coerceString(params.blueprintPath);
    if (!blueprintPath) {
      return {
        success: false as const,
        message: 'Blueprint path is required',
        error: 'INVALID_BLUEPRINT_PATH'
      };
    }

    if (!Array.isArray(params.operations) || params.operations.length === 0) {
      return {
        success: false as const,
        message: 'At least one SCS operation is required',
        error: 'MISSING_OPERATIONS'
      };
    }

    const automationBridge = this.automationBridge;
    if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
      return {
        success: false as const,
        message: 'Automation bridge is not available',
        error: 'AUTOMATION_BRIDGE_UNAVAILABLE'
      };
    }

    const sanitizedOperations: Record<string, unknown>[] = [];
    for (let index = 0; index < params.operations.length; index += 1) {
      const op = params.operations[index];
      const sanitized = this.sanitizeScsOperation(op, index);
      if (!sanitized.ok) {
        return {
          success: false as const,
          message: sanitized.error,
          error: 'INVALID_OPERATION'
        };
      }
      sanitizedOperations.push(sanitized.operation);
    }

    const payload: Record<string, unknown> = {
      blueprintPath,
      operations: sanitizedOperations
    };

    if (typeof params.compile === 'boolean') {
      payload.compile = params.compile;
    }
    if (typeof params.save === 'boolean') {
      payload.save = params.save;
    }

    await concurrencyDelay();

    try {
        // Default SCS modify timeout can be tuned via env var to allow longer
        // automation operations on slower machines or large projects.
        const envTimeout = Number(process.env.MCP_AUTOMATION_SCS_TIMEOUT_MS || '30000');
        const defaultTimeout = Number.isFinite(envTimeout) && envTimeout > 0 ? envTimeout : 30000;
        const response = await automationBridge.sendAutomationRequest(
        'blueprint_modify_scs',
        payload,
        { timeoutMs: params.timeoutMs && params.timeoutMs > 0 ? params.timeoutMs : defaultTimeout }
      );

      const success = response.success !== false;
      const message = coerceString(response.message) ?? (success ? 'Blueprint SCS updated.' : 'Blueprint SCS update failed.');
      const errorCode = success ? undefined : (coerceString(response.error) ?? 'AUTOMATION_BRIDGE_FAILURE');

      const resultPayload = response.result && typeof response.result === 'object' ? (response.result as Record<string, unknown>) : {};
      const resultBlueprintPath = coerceString(resultPayload.blueprintPath) ?? blueprintPath;
      const warnings = coerceStringArray(resultPayload.warnings);
      const compiled = typeof resultPayload.compiled === 'boolean' ? resultPayload.compiled : undefined;
      const saved = typeof resultPayload.saved === 'boolean' ? resultPayload.saved : undefined;

      const operations = Array.isArray(resultPayload.operations)
        ? resultPayload.operations.filter((entry): entry is Record<string, unknown> => !!entry && typeof entry === 'object')
        : undefined;

      return {
        success,
        message,
        error: errorCode,
        blueprintPath: resultBlueprintPath,
        compiled,
        saved,
        warnings,
        operations,
        transport: 'automation_bridge' as const,
        bridge: {
          requestId: response.requestId,
          success,
          error: response.error
        }
      };
    } catch (error: any) {
      return {
        success: false as const,
        message: 'Automation bridge request failed',
        error: error?.message || String(error),
        blueprintPath,
        transport: 'automation_bridge' as const
      };
    }
  }

  async setBlueprintDefault(params: { blueprintName: string; propertyName: string; value: unknown }) {
    const blueprintName = params.blueprintName?.trim();
    const propertyName = params.propertyName?.trim();

    if (!blueprintName) {
      return {
        success: false as const,
        error: 'Invalid blueprint name'
      };
    }

    if (!propertyName) {
      return {
        success: false as const,
        error: 'Invalid property name'
      };
    }

    const { primary, candidates } = this.resolveBlueprintCandidates(blueprintName);
    if (!primary || candidates.length === 0) {
      return {
        success: false as const,
        error: 'Blueprint path could not be determined',
        warnings: ['Ensure the Blueprint asset is available under /Game before calling set_default.']
      };
    }

    try {
      const result = await this.python.setBlueprintDefault({
        blueprintCandidates: candidates,
        requestedPath: primary,
        propertyName,
        value: params.value
      });

      if (!result?.success) {
        // If the property was not found, try to create it programmatically
        // using the addVariable console command path and then retry setting
        // the default. This helps tests that expect a property to be created
        // implicitly before setting its default.
        const err = coerceString(result?.error) ?? '';
        const msg = (coerceString(result?.message) ?? '').toLowerCase();
        const propertyNotFound = err.includes('property') && err.includes('not found') || msg.includes('not found') || err.includes('PROPERTY_NOT_FOUND');
        if (propertyNotFound) {
          try {
            const varType = this.inferVariableTypeFromValue(params.value);
            const addVar = await this.addVariable({
              blueprintName: primary,
              variableName: propertyName,
              variableType: varType || 'Float',
              defaultValue: params.value
            });
            if (addVar && (addVar as any).success) {
              // Give the editor a moment to persist created variable before retry
              await concurrencyDelay();
              const retry = await this.python.setBlueprintDefault({
                blueprintCandidates: candidates,
                requestedPath: primary,
                propertyName,
                value: params.value,
                save: true
              });
              if (retry?.success) {
                return {
                  success: true as const,
                  message: retry.message ?? `Updated ${propertyName} on ${primary}`,
                  blueprintPath: coerceString(retry.blueprintPath) ?? primary,
                  propertyName,
                  value: params.value,
                  cdoPath: coerceString(retry.cdoPath),
                  warnings: coerceStringArray(retry.warnings)
                };
              }
            }
          } catch (_e) {
            // swallow and fall through to original error response
          }
        }
        return {
          success: false as const,
          error: coerceString(result?.error) ?? `Failed to set ${propertyName}`,
          blueprintPath: coerceString((result as any)?.blueprintPath) ?? primary,
          warnings: coerceStringArray((result as any)?.warnings)
        };
      }

      return {
        success: true as const,
        message: result.message ?? `Updated ${propertyName} on ${coerceString(result.cdoPath) ?? primary}`,
        blueprintPath: coerceString(result.blueprintPath) ?? primary,
        propertyName: coerceString(result.propertyName) ?? propertyName,
        value: params.value,
        cdoPath: coerceString(result.cdoPath),
        warnings: coerceStringArray(result.warnings)
      };
    } catch (err) {
      return {
        success: false as const,
        error: `Failed to set Blueprint default: ${err}`
      };
    }
  }

  private inferVariableTypeFromValue(value: unknown): string | undefined {
    if (value === null || value === undefined) return undefined;
    if (typeof value === 'boolean') return 'Bool';
    if (typeof value === 'number') return Number.isInteger(value) ? 'Int' : 'Float';
    if (typeof value === 'string') return 'String';
    if (Array.isArray(value)) return 'Array';
    if (typeof value === 'object') {
      const keys = Object.keys(value as Record<string, unknown>);
      if (keys.includes('x') && keys.includes('y') && keys.includes('z')) return 'Vector';
      return 'Struct';
    }
    return undefined;
  }
  /**
   * Add Variable to Blueprint
   */
  async addVariable(params: {
    blueprintName: string;
    variableName: string;
    variableType: string;
    defaultValue?: any;
    category?: string;
    isReplicated?: boolean;
    isPublic?: boolean;
    /** Optional explicit EdGraphPinType descriptor to control complex variable types */
    variablePinType?: Record<string, unknown>;
  }) {
    // Prefer Python-based variable creation which is more robust than console
    // string parsing. Fall back to existing console path when Python helpers
    // are unavailable.
    try {
      // Build a simple payload for Python helper that will use
      // BlueprintEditorLibrary.AddMemberVariableWithValue or similar APIs when
      // present in the engine. This avoids console command parsing and supports
      // value-typed defaults.
      // If the caller provided a simple variableType string but did not supply
      // an explicit EdGraphPinType descriptor, translate common scalar types
      // into a minimal descriptor so the Python helper can build a proper
      // EdGraphPinType. This avoids the Python nativization errors observed
      // when passing raw strings into APIs that expect structured types.
      const explicitPinType = params.variablePinType ?? (() => {
        const t = (params.variableType || '').toString().trim();
        if (!t) return undefined;
        const normalized = t.toLowerCase();
        switch (normalized) {
          case 'float': return { pin_category: 'float' };
          case 'int':
          case 'integer': return { pin_category: 'int' };
          case 'bool':
          case 'boolean': return { pin_category: 'bool' };
          case 'string': return { pin_category: 'string' };
          case 'vector': return { pin_category: 'struct', pin_sub_category: 'Vector' };
          case 'actor': return { pin_category: 'object', pin_sub_category_object: '/Script/Engine.Actor' };
          default: return undefined;
        }
      })();

      const payload = {
        blueprintCandidates: this.resolveBlueprintCandidates(params.blueprintName).candidates,
        requestedPath: this.resolveBlueprintCandidates(params.blueprintName).primary,
        variableName: params.variableName,
        variableType: params.variableType,
        defaultValue: params.defaultValue,
        category: params.category,
        isReplicated: params.isReplicated === true,
        isPublic: params.isPublic === true,
        edGraphPinType: explicitPinType
      };

      const pyScript = `
      import unreal, json
      payload = json.loads(r'''${JSON.stringify(payload)}''')
      result = { 'success': False, 'message': '', 'error': '' }

      def add_var_via_blueprint_lib(bp, name, pin_type, default_value):
        bel = getattr(unreal, 'BlueprintEditorLibrary', None)
        if not bel:
          return False, 'BlueprintEditorLibrary not available'
        # Try the most capable API first
        try:
          if pin_type is not None and hasattr(bel, 'add_member_variable_with_value'):
            bel.add_member_variable_with_value(bp, name, pin_type, default_value)
            return True, ''
        except Exception as e:
          return False, str(e)

        # Fallback: Try basic add_member_variable which may require an EdGraphPinType
        try:
          if hasattr(bel, 'add_member_variable'):
            bel.add_member_variable(bp, name, pin_type if pin_type is not None else payload.get('variableType'))
            # Attempt to set default if API exists
            try:
              if hasattr(bel, 'set_member_variable_default_value'):
                bel.set_member_variable_default_value(bp, name, default_value)
            except Exception:
              pass
            return True, ''
        except Exception as e:
          return False, str(e)

        return False, 'No suitable BlueprintEditorLibrary API available'

      def build_pin_type(descriptor):
        if not descriptor:
          return None
        try:
          pin = unreal.EdGraphPinType()
        except Exception as e:
          # EdGraphPinType not available in this Python runtime
          return None
        # Assign common fields if present
        for k, v in descriptor.items():
          try:
            if hasattr(pin, k):
              setattr(pin, k, v)
            else:
              # Try alternate naming conventions
              alt = ''.join([part.capitalize() for part in k.split('_')])
              if hasattr(pin, alt):
                setattr(pin, alt, v)
          except Exception:
            # Non-fatal
            pass
        return pin

      def resolve_blueprint(candidates):
        editor_lib = unreal.EditorAssetLibrary
        for p in candidates:
          try:
            bp = editor_lib.load_asset(p)
            if bp:
              return bp, p
          except Exception:
            continue
        return None, None

      bp, bp_path = resolve_blueprint(payload.get('blueprintCandidates', []))
      if not bp:
        result['error'] = f'Blueprint not found: {payload.get("requestedPath")}'
      else:
        try:
          pin_type_desc = payload.get('edGraphPinType')
          pin_type = build_pin_type(pin_type_desc) if pin_type_desc else None
          ok, msg = add_var_via_blueprint_lib(bp, payload.get('variableName'), pin_type, payload.get('defaultValue'))
          if ok:
            result['success'] = True
            result['message'] = f"Variable {payload.get('variableName')} added to {bp_path}"
          else:
            result['error'] = msg or 'Variable addition via BlueprintEditorLibrary failed'
        except Exception as e:
          result['error'] = str(e)

      print('RESULT:' + json.dumps(result))
      `;

      const pyResult = await this.bridge.executePythonWithResult(pyScript);
      if (pyResult && pyResult.success) {
        return { success: true, message: pyResult.message };
      }

      // Python helper not present or failed - fall back to console commands
      const commands = [
        `AddBlueprintVariable ${params.blueprintName} ${params.variableName} ${params.variableType}`
      ];
      if (params.defaultValue !== undefined) {
        commands.push(`SetVariableDefault ${params.blueprintName} ${params.variableName} ${JSON.stringify(params.defaultValue)}`);
      }
      if (params.category) {
        commands.push(`SetVariableCategory ${params.blueprintName} ${params.variableName} ${params.category}`);
      }
      if (params.isReplicated) {
        commands.push(`SetVariableReplicated ${params.blueprintName} ${params.variableName} true`);
      }
      if (params.isPublic !== undefined) {
        commands.push(`SetVariablePublic ${params.blueprintName} ${params.variableName} ${params.isPublic}`);
      }
      await this.bridge.executeConsoleCommands(commands);
      return { success: true, message: `Variable ${params.variableName} added to ${params.blueprintName}` };
    } catch (err) {
      return { success: false, error: `Failed to add variable: ${err}` };
    }
  }

  /**
   * Add Function to Blueprint
   */
  async addFunction(params: {
    blueprintName: string;
    functionName: string;
    inputs?: Array<{ name: string; type: string }>;
    outputs?: Array<{ name: string; type: string }>;
    isPublic?: boolean;
    category?: string;
  }) {
    try {
      const commands = [
        `AddBlueprintFunction ${params.blueprintName} ${params.functionName}`
      ];
      
      // Add inputs
      if (params.inputs) {
        for (const input of params.inputs) {
          commands.push(
            `AddFunctionInput ${params.blueprintName} ${params.functionName} ${input.name} ${input.type}`
          );
        }
      }
      
      // Add outputs
      if (params.outputs) {
        for (const output of params.outputs) {
          commands.push(
            `AddFunctionOutput ${params.blueprintName} ${params.functionName} ${output.name} ${output.type}`
          );
        }
      }
      
      if (params.isPublic !== undefined) {
        commands.push(
          `SetFunctionPublic ${params.blueprintName} ${params.functionName} ${params.isPublic}`
        );
      }
      
      if (params.category) {
        commands.push(
          `SetFunctionCategory ${params.blueprintName} ${params.functionName} ${params.category}`
        );
      }
      
      await this.bridge.executeConsoleCommands(commands);
      
      return { 
        success: true, 
        message: `Function ${params.functionName} added to ${params.blueprintName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to add function: ${err}` };
    }
  }

  /**
   * Add Event to Blueprint
   */
  async addEvent(params: {
    blueprintName: string;
    eventType: 'BeginPlay' | 'Tick' | 'EndPlay' | 'BeginOverlap' | 'EndOverlap' | 'Hit' | 'Custom';
    customEventName?: string;
    parameters?: Array<{ name: string; type: string }>;
  }) {
    try {
      const eventName = params.eventType === 'Custom' ? (params.customEventName || 'CustomEvent') : params.eventType;
      
      const commands = [
        `AddBlueprintEvent ${params.blueprintName} ${params.eventType} ${eventName}`
      ];
      
      // Add parameters for custom events
      if (params.eventType === 'Custom' && params.parameters) {
        for (const param of params.parameters) {
          commands.push(
            `AddEventParameter ${params.blueprintName} ${eventName} ${param.name} ${param.type}`
          );
        }
      }
      
      await this.bridge.executeConsoleCommands(commands);
      
      return { 
        success: true, 
        message: `Event ${eventName} added to ${params.blueprintName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to add event: ${err}` };
    }
  }

  /**
   * Set metadata for an existing variable (Tooltip, Category, etc.)
   */
  async setVariableMetadata(params: { blueprintName: string; variableName: string; metadata: Record<string, unknown> }) {
    const { primary, candidates } = this.resolveBlueprintCandidates(params.blueprintName);
    if (!primary || candidates.length === 0) {
      return { success: false as const, error: 'Invalid blueprint name' };
    }

    try {
      const payload = {
        blueprintCandidates: candidates,
        requestedPath: primary,
        variableName: params.variableName,
        metadata: params.metadata
      };
      const py = `
import unreal, json
payload = json.loads(r'''${JSON.stringify(payload)}''')
res = {'success': False, 'error': '', 'message': ''}
bp = None
try:
  for p in payload.get('blueprintCandidates', []):
    try:
      bp = unreal.EditorAssetLibrary.load_asset(p)
      if bp: break
    except Exception:
      bp = None
  if not bp:
    res['error'] = f"Blueprint not found: {payload.get('requestedPath')}"
  else:
    bel = getattr(unreal, 'BlueprintEditorLibrary', None)
    if bel and hasattr(bel, 'set_member_variable_meta_data'):
      for k,v in (payload.get('metadata') or {}).items():
        try:
          bel.set_member_variable_meta_data(bp, payload.get('variableName'), k, str(v))
        except Exception as e:
          pass
      res['success'] = True
      res['message'] = 'Variable metadata updated'
    else:
      res['error'] = 'BlueprintEditorLibrary metadata API unavailable'
except Exception as e:
  res['error'] = str(e)
print('RESULT:' + json.dumps(res))
`;
      const pyRes = await this.bridge.executePythonWithResult(py);
      const success = pyRes && pyRes.success === true;
      if (success) return { success: true as const, message: pyRes.message };
      // Fallback to console no-op: report unsupported but not fatal
      return { success: false as const, error: pyRes?.error ?? 'Metadata API not available' };
    } catch (err) {
      return { success: false as const, error: String(err) };
    }
  }

  /**
   * Add a construction script graph container — best-effort: many editor APIs
   * don't expose creating a named construction script via Python; this will
   * create a function or marker node instead as a fallback.
   */
  async addConstructionScript(params: { blueprintName: string; scriptName: string }) {
    const { primary, candidates } = this.resolveBlueprintCandidates(params.blueprintName);
    if (!primary || candidates.length === 0) {
      return { success: false as const, error: 'Invalid blueprint name' };
    }

    try {
      const payload = { blueprintCandidates: candidates, requestedPath: primary, scriptName: params.scriptName };
      const py = `
import unreal, json
payload = json.loads(r'''${JSON.stringify(payload)}''')
res = {'success': False, 'error': '', 'message': ''}
try:
  editor_lib = unreal.EditorAssetLibrary
  bp = None
  for p in payload.get('blueprintCandidates'):
    try:
      bp = editor_lib.load_asset(p)
      if bp: break
    except Exception:
      bp = None
  if not bp:
    res['error'] = f"Blueprint not found: {payload.get('requestedPath')}"
  else:
    bel = getattr(unreal, 'BlueprintEditorLibrary', None)
    if bel and hasattr(bel, 'add_function_to_blueprint'):
      try:
        bel.add_function_to_blueprint(bp, payload.get('scriptName'))
        res['success'] = True
        res['message'] = 'Construction script placeholder created as function'
      except Exception as e:
        res['error'] = str(e)
    else:
      res['error'] = 'BlueprintEditorLibrary function creation API unavailable'
except Exception as e:
  res['error'] = str(e)
print('RESULT:' + json.dumps(res))
`;
      const pyRes = await this.bridge.executePythonWithResult(py);
      if (pyRes && pyRes.success) {
        return { success: true as const, message: pyRes.message };
      }
      return { success: false as const, error: pyRes?.error ?? 'Unable to create construction script' };
    } catch (err) {
      return { success: false as const, error: String(err) };
    }
  }

  /**
   * Compile Blueprint
   */
  async compileBlueprint(params: {
    blueprintName: string;
    saveAfterCompile?: boolean;
  }) {
    try {
      const commands = [
        `CompileBlueprint ${params.blueprintName}`
      ];
      
      if (params.saveAfterCompile) {
        commands.push(`SaveAsset ${params.blueprintName}`);
      }
      
      await this.bridge.executeConsoleCommands(commands);
      
      return { 
        success: true, 
        message: `Blueprint ${params.blueprintName} compiled successfully` 
      };
    } catch (err) {
      return { success: false, error: `Failed to compile blueprint: ${err}` };
    }
  }

}
