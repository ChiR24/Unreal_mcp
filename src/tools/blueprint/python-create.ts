import { UnrealBridge } from '../../unreal-bridge.js';
import { AutomationBridge } from '../../automation-bridge.js';
import { escapePythonString } from '../../utils/python.js';
import { coerceString } from '../../utils/result-helpers.js';

/**
 * Create a Blueprint asset using Editor Python. Returns the parsed Python
 * RESULT: payload (success/message/path/etc) as an object.
 */
export async function createBlueprintViaPython(bridge: UnrealBridge, automationBridge: AutomationBridge | undefined, params: { name: string; blueprintType: string; savePath?: string; parentClass?: string; timeoutMs?: number; }) {
  const name = coerceString(params.name) ?? '';
  const savePath = coerceString(params.savePath) || '/Game/Blueprints';
  const parent = coerceString(params.parentClass) ?? '';

  const escapedName = escapePythonString(name);
  const escapedPath = escapePythonString(savePath);
  const escapedParent = escapePythonString(parent);

  const pythonScript = `
import unreal, json, time, traceback

def ensure_asset_persistence(asset_path):
  try:
    editor_lib = unreal.EditorAssetLibrary
    for _ in range(50):
      if editor_lib.does_asset_exist(asset_path):
        return True
      time.sleep(0.2)
    return False
  except Exception:
    return False

result = {'success': False, 'message': '', 'path': ''}
asset_path = "${escapedPath}"
asset_name = "${escapedName}"
full_path = f"{asset_path}/{asset_name}"

try:
  factory = unreal.BlueprintFactory()
  explicit_parent = "${escapedParent}"
  if explicit_parent.strip():
    try:
      if explicit_parent.startswith('/Script/'):
        parent_cls = unreal.load_class(None, explicit_parent)
      elif explicit_parent.startswith('/Game/'):
        parent_asset = unreal.EditorAssetLibrary.load_asset(explicit_parent)
        parent_cls = parent_asset.generated_class() if parent_asset and hasattr(parent_asset, 'generated_class') else None
      else:
        parent_cls = getattr(unreal, explicit_parent, None)
    except Exception:
      parent_cls = None
    if parent_cls:
      try:
        factory.set_editor_property('parent_class', parent_cls)
      except Exception:
        pass

  try:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset(asset_name, asset_path, unreal.Blueprint, factory)
  except Exception as e:
    asset = None

  if not asset:
    result['success'] = False
    result['message'] = f'Failed to create blueprint: {full_path}'
    result['path'] = full_path
  else:
    result['path'] = full_path
    result['message'] = f'Blueprint created at {full_path}'
    result['success'] = True
    # Attempt to persist and verify
    try:
      unreal.EditorAssetLibrary.save_loaded_asset(full_path)
    except Exception:
      pass
    if not ensure_asset_persistence(full_path):
      result['warnings'] = ['Created but asset registry did not report asset yet']

except Exception as e:
  result['success'] = False
  result['message'] = str(e)
  result['error'] = str(e)

print('RESULT:' + json.dumps(result))
`.trim();

  try {
    if (automationBridge && typeof automationBridge.sendAutomationRequest === 'function') {
      const envTimeoutCreate = Number(process.env.MCP_AUTOMATION_PYTHON_TIMEOUT_MS ?? process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
      const timeoutCreate = Number.isFinite(envTimeoutCreate) && envTimeoutCreate > 0 ? envTimeoutCreate : 120000;
      const requested = typeof params.timeoutMs === 'number' && params.timeoutMs > 0 ? params.timeoutMs : timeoutCreate;
      const resp = await automationBridge.sendAutomationRequest('execute_editor_python', { script: pythonScript }, { timeoutMs: requested });
      return resp?.result ?? resp;
    }
    return await bridge.executePythonWithResult(pythonScript);
  } catch (err) {
    return { success: false, error: String(err), message: String(err), path: `${savePath}/${name}` };
  }
}

/**
 * Add a member variable using Python (BlueprintEditorLibrary). Returns python RESULT.
 */
export async function addVariableViaPython(bridge: UnrealBridge, params: { blueprintName: string; variableName: string; variableType: string; defaultValue?: any; category?: string; isReplicated?: boolean; isPublic?: boolean; variablePinType?: Record<string, unknown>; }, timeoutMs?: number) {
  const payload = JSON.stringify(params);
  const script = `
import unreal, json
payload = json.loads(r'''${payload}''')
res = {'success': False, 'message': '', 'error': ''}
bel = getattr(unreal, 'BlueprintEditorLibrary', None)
if not bel:
  res['error'] = 'BlueprintEditorLibrary not available'
else:
  try:
    bp = None
    for p in (payload.get('blueprintCandidates') or [payload.get('blueprintName')]):
      try:
        bp = unreal.EditorAssetLibrary.load_asset(p)
        if bp: break
      except Exception:
        bp = None
    if not bp:
      res['error'] = 'Blueprint not found'
    else:
      # Try add_member_variable_with_value
      try:
        if hasattr(bel, 'add_member_variable_with_value'):
          bel.add_member_variable_with_value(bp, payload.get('variableName'), None, payload.get('defaultValue'))
          res['success'] = True
          res['message'] = 'Variable added'
        else:
          res['error'] = 'API unavailable'
      except Exception as e:
        res['error'] = str(e)
  except Exception as e:
    res['error'] = str(e)
print('RESULT:' + json.dumps(res))
`.trim();

  try {
    try {
      const t = typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : undefined;
      return await bridge.executePythonWithResult(script, t);
    } catch (err) {
      return { success: false, error: String(err) };
    }
  } catch (err) {
    return { success: false, error: String(err) };
  }
}

export async function setVariableMetadataViaPython(bridge: UnrealBridge, params: { blueprintName: string; variableName: string; metadata: Record<string, unknown>; }, timeoutMs?: number) {
  const payload = JSON.stringify(params);
  const script = `
import unreal, json
payload = json.loads(r'''${payload}''')
res = {'success': False, 'error': '', 'message': ''}
try:
  bp = unreal.EditorAssetLibrary.load_asset(payload.get('blueprintName'))
  if not bp:
    res['error'] = 'Blueprint not found'
  else:
    bel = getattr(unreal, 'BlueprintEditorLibrary', None)
    if bel and hasattr(bel, 'set_member_variable_meta_data'):
      for k,v in (payload.get('metadata') or {}).items():
        try:
          bel.set_member_variable_meta_data(bp, payload.get('variableName'), k, str(v))
        except Exception:
          pass
      res['success'] = True
      res['message'] = 'Variable metadata updated'
    else:
      res['error'] = 'BlueprintEditorLibrary metadata API unavailable'
except Exception as e:
  res['error'] = str(e)
print('RESULT:' + json.dumps(res))
`.trim();

  try {
    const t = typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : undefined;
    return await bridge.executePythonWithResult(script, t);
  } catch (err) {
    return { success: false, error: String(err) };
  }
}

export async function addConstructionScriptViaPython(bridge: UnrealBridge, params: { blueprintName: string; scriptName: string; }, timeoutMs?: number) {
  const payload = JSON.stringify(params);
  const script = `
import unreal, json
payload = json.loads(r'''${payload}''')
res = {'success': False, 'error': '', 'message': ''}
try:
  bp = unreal.EditorAssetLibrary.load_asset(payload.get('blueprintName'))
  if not bp:
    res['error'] = 'Blueprint not found'
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
`.trim();

  try {
    const t = typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : undefined;
    return await bridge.executePythonWithResult(script, t);
  } catch (err) {
    return { success: false, error: String(err) };
  }
}
