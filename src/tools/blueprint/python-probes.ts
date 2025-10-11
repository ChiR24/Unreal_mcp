import { UnrealBridge } from '../../unreal-bridge.js';
// no local helpers required from result-helpers here

/**
 * Wait for a blueprint asset to be visible to the Editor's Asset Registry.
 * This mirrors the previous implementation but is now a small reusable helper
 * that accepts a bridge instance.
 */
export async function waitForBlueprint(bridge: UnrealBridge, blueprintCandidates: string | string[], timeoutMs?: number) {
  const candidatesArray = Array.isArray(blueprintCandidates) ? blueprintCandidates : [blueprintCandidates];
  const candidatesPayload = JSON.stringify({ candidates: candidatesArray, timeoutMs: typeof timeoutMs === 'number' ? timeoutMs : undefined });
  const python = `
import unreal, json, time
payload = json.loads(r'''${candidatesPayload}''')
result = { 'success': False, 'found': None, 'checked': [], 'warnings': [] }
start = time.time()
timeout = payload.get('timeoutMs') or ${Number(process.env.MCP_AUTOMATION_SCS_TIMEOUT_MS ?? process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000')}
timeout = float(timeout) / 1000.0
# Poll at a conservative rate to avoid log spam when invalid paths are
# provided. Each iteration will attempt a small set of normalized path
# variants for each candidate.
poll_interval = 1.0
  while time.time() - start < timeout:
    for raw in payload.get('candidates', []):
      # Normalize and skip empty candidates
      try:
        s = str(raw).strip()
      except Exception:
        s = ''
      if not s:
        continue
      # Prefer absolute /Game/ paths first, then common variants
      to_check = []
      if s.startswith('/'):
        to_check.append(s)
      else:
        # Only probe common absolute content roots. Avoid passing bare
        # names like 'BP_TestPawn' to EditorAssetLibrary which will
        # produce repeated error logs when polled frequently.
        to_check.append(f"/Game/Blueprints/{s}")
        to_check.append(f"/Game/{s}")
      # Try each candidate/variant once per poll loop
      for p in to_check:
        try:
          result['checked'].append(p)
          if unreal.EditorAssetLibrary.does_asset_exist(p):
            result['success'] = True
            result['found'] = p
            print('RESULT:' + json.dumps(result))
            raise SystemExit
        except Exception as e:
          try:
            result['warnings'].append(str(e))
          except Exception:
            pass
    # Conservative polling interval to avoid rapid repeated queries that
    # produce engine log spam when callers pass invalid or bare names.
    time.sleep(poll_interval)
print('RESULT:' + json.dumps(result))
`.trim();

  try {
  const resp = await bridge.executePythonWithResult(python, typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : undefined);
    return resp as Record<string, any>;
  } catch (err) {
    return { success: false, error: String(err) } as any;
  }
}

/**
 * Resolve a parent class specification using a small Python probe.
 * Returns { success, resolved, error } payload similar to other helpers.
 */
export async function resolveParentClass(bridge: UnrealBridge, parentSpec: string, blueprintType: string) {
  const payload = JSON.stringify({ parentSpec, blueprintType });
  const python = `
import unreal, json
payload = json.loads(r'''${payload}''')
result = {'success': False, 'resolved': '', 'error': ''}
try:
  spec = (payload.get('parentSpec') or '').strip()
  if not spec:
    result['success'] = True
    print('RESULT:' + json.dumps(result))
    raise SystemExit
  editor = unreal.EditorAssetLibrary
  resolved = None
  try:
    if spec.startswith('/Script/'):
      resolved = unreal.load_class(None, spec)
  except Exception:
    resolved = None
  if not resolved:
    try:
      if spec.startswith('/Game/'):
        asset = editor.load_asset(spec)
        if asset:
          if hasattr(asset, 'generated_class'):
            try:
              gen = asset.generated_class()
              if gen:
                resolved = gen
            except Exception:
              pass
          else:
            resolved = asset
    except Exception:
      resolved = None
  if not resolved:
    try:
      candidate = getattr(unreal, spec, None)
      if candidate:
        resolved = candidate
    except Exception:
      resolved = None
  if resolved:
    try:
      result['resolved'] = str(resolved.get_path_name())
    except Exception:
      try:
        result['resolved'] = str(resolved)
      except Exception:
        result['resolved'] = ''
    result['success'] = True
  else:
    result['error'] = f'Parent class not found: {spec}'
except Exception as e:
  result['error'] = str(e)
print('RESULT:' + json.dumps(result))
`.trim();

  try {
  return await bridge.executePythonWithResult(python, undefined);
  } catch (err) {
    return { success: false, error: String(err) } as any;
  }
}

/**
 * Probe SubobjectDataSubsystem shapes by creating a disposable temporary
 * blueprint and invoking minimal add_new_subobject calls. Returns a JSON
 * description of discovered handle shapes to aid retries/coercion.
 */
export async function probeSubobjectDataHandle(bridge: UnrealBridge, componentClass?: string) {
  const cls = (componentClass || 'StaticMeshComponent').toString();
  const payload = JSON.stringify({ componentClass: cls });
  const py = `
import unreal, json, time
payload = json.loads(r'''${payload}''')
result = {
  'success': False,
  'componentClass': payload.get('componentClass'),
  'createdBlueprint': None,
  'subsystemAvailable': False,
  'gatheredHandles': [],
  'add_new_subobject': None,
  'subobject_data_handle_type': None,
  'subobject_data_handle_fields': [],
  'rename_result': None,
  'warnings': []
}
def add_warn(m):
  try:
    result['warnings'].append(str(m))
  except Exception:
    pass
try:
  try:
    if not unreal.EditorAssetLibrary.does_directory_exist('/Game/Temp/MCPProbe'):
      unreal.EditorAssetLibrary.make_directory('/Game/Temp/MCPProbe')
  except Exception:
    pass
  unique = str(int(time.time() * 1000))
  name = f"MCP_Probe_BP_{unique}"
  path = '/Game/Temp/MCPProbe'
  full = f"{path}/{name}"
  created = None
  try:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    created = tools.create_asset(name, path, unreal.Blueprint, factory)
  except Exception as e:
    add_warn(f"Blueprint probe creation failed: {e}")
    created = None
  if not created:
    add_warn('Probe blueprint creation failed; will attempt to probe using an existing blueprint if available.')
    result['success'] = False
    print('RESULT:' + json.dumps(result))
  else:
    result['createdBlueprint'] = full
    try:
      unreal.EditorAssetLibrary.save_loaded_asset(full)
    except Exception:
      pass
    time.sleep(0.2)
    subsystem = None
    try:
      subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    except Exception as e:
      add_warn(f'Failed to get SubobjectDataSubsystem: {e}')
      subsystem = None
    result['subsystemAvailable'] = bool(subsystem)
    if subsystem:
      try:
        handles = subsystem.k2_gather_subobject_data_for_blueprint(created) or []
        result['gatheredHandles'] = [str(h) for h in handles]
      except Exception as e:
        add_warn(f'Gather handles failed: {e}')
    # Try a minimal add_new_subobject flow to inspect returned value
    try:
      bp = created
      if bp:
        clsobj = getattr(unreal, payload.get('componentClass'), None)
        if not clsobj:
          add_warn('Component class not found on unreal module')
        else:
          try:
            subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
            if subsystem:
              created_handle = subsystem.k2_add_new_subobject(bp, clsobj, None)
              result['add_new_subobject'] = str(created_handle)
              # Inspect possible fields on a SubobjectDataHandle-like object
              try:
                typ = type(created_handle)
                result['subobject_data_handle_type'] = str(typ)
                try:
                  for k in dir(created_handle):
                    if k.startswith('_'): continue
                    result['subobject_data_handle_fields'].append(k)
              except Exception:
                pass
          except Exception as e:
            add_warn(f'add_new_subobject failed: {e}')
    except Exception:
      pass
    # Cleanup probe blueprint
    try:
      unreal.EditorAssetLibrary.delete_loaded_asset(full)
    except Exception:
      try:
        unreal.EditorAssetLibrary.delete_asset(full)
      except Exception as e:
        add_warn(f'Failed to delete probe asset {full}: {e}')
    result['success'] = True
    print('RESULT:' + json.dumps(result))
except Exception as e:
  result['success'] = False
  add_warn(f'Unhandled probe exception: {e}')
  try:
    import traceback as _tb
    add_warn(_tb.format_exc())
  except Exception:
    pass
print('RESULT:' + json.dumps(result))
`.trim();

  try {
    const resp = await bridge.executePythonWithResult(py);
    return resp as Record<string, any>;
  } catch (err) {
    return { success: false, error: String(err) } as any;
  }
}
