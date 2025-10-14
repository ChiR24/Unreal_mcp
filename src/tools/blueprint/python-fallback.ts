import { UnrealBridge } from '../../unreal-bridge.js';
import * as BlueprintHelpers from './helpers.js';
import { allowPythonFallbackFromEnv } from '../../utils/env.js';

/**
 * Add a component to a Blueprint. Prefers plugin-native handlers and
 * centralized editor functions; only runs guarded Python fallback when
 * explicitly enabled via MCP_ALLOW_PYTHON_FALLBACKS.
 */
export async function addComponentViaPython(
  bridge: UnrealBridge,
  params: {
    blueprintRef: string;
    componentClass: string;
    componentName: string;
    attachTo?: string;
    transform?: Record<string, unknown>;
    properties?: Record<string, unknown>;
    compile?: boolean;
    save?: boolean;
    timeoutMs?: number;
  }
) {
  const { primary, candidates } = BlueprintHelpers.resolveBlueprintCandidates(params.blueprintRef);
  if (!primary || !candidates || candidates.length === 0) {
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

  const allowPython = allowPythonFallbackFromEnv();
  const timeout = typeof params.timeoutMs === 'number' && params.timeoutMs > 0 ? params.timeoutMs : undefined;

  // 1) Plugin-first: try blueprint_add_component action
  try {
    const automation = (bridge as any).automationBridge as any | undefined;
    if (automation && typeof automation.sendAutomationRequest === 'function') {
      try {
        const pluginResp = await automation.sendAutomationRequest('blueprint_add_component', payload, { timeoutMs: timeout });
        if (pluginResp && pluginResp.success !== false) return pluginResp.result ?? pluginResp;
        const errTxt = String(pluginResp?.error ?? pluginResp?.message ?? '');
        if (!(errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION'))) {
          return { success: false as const, error: pluginResp?.error ?? pluginResp?.message ?? 'BLUEPRINT_ADD_COMPONENT_FAILED' };
        }
      } catch (_err) {
        if (!allowPython) return { success: false as const, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge not available; Python fallback disabled' };
      }
    }
  } catch (_) {
    // ignore and continue to editor-function / Python fallback
  }

  // 2) Try centralized editor function (ADD_COMPONENT_TO_BLUEPRINT)
  try {
    const funcRes = await bridge.executeEditorFunction('ADD_COMPONENT_TO_BLUEPRINT', { payload: JSON.stringify(payload) }, { allowPythonFallback: allowPython });
    if (funcRes && funcRes.success !== false) return funcRes.result ?? funcRes;
    const errTxt = String((funcRes as any)?.error ?? (funcRes as any)?.message ?? '');
    if (!(errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION'))) {
      return { success: false as const, error: (funcRes as any)?.error ?? (funcRes as any)?.message ?? 'ADD_COMPONENT_FAILED' };
    }
    if (!allowPython) return { success: false as const, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_component or ADD_COMPONENT_TO_BLUEPRINT' };
  } catch (err) {
    if (!allowPython) return { success: false as const, error: String(err), message: 'Editor function call failed and Python fallback disabled' };
  }

  // 3) Python fallback (guarded)
  const scriptPayload = JSON.stringify(payload);
  const pythonScript = `
import unreal, json
result = {'success': False, 'message': '', 'error': '', 'blueprintPath': None}
try:
  data = json.loads(r'''${scriptPayload}''')
  requested = data.get('requestedPath')
  if not requested:
    raise RuntimeError('requestedPath missing')
  bp_asset = unreal.EditorAssetLibrary.load_asset(requested)
  if not bp_asset:
    raise RuntimeError(f'Blueprint asset not found: {requested}')
  bel = getattr(unreal, 'BlueprintEditorLibrary', None)
  if bel and hasattr(bel, 'add_component_to_blueprint'):
    comp = bel.add_component_to_blueprint(bp_asset, data.get('componentClass'), data.get('componentName'))
    if comp:
      if data.get('compile'):
        try:
          bel.compile_blueprint(bp_asset)
        except Exception:
          pass
      if data.get('save'):
        try:
          unreal.EditorAssetLibrary.save_asset(requested)
        except Exception:
          pass
      result['success'] = True
      result['message'] = 'Component added via Python fallback'
      result['blueprintPath'] = requested
  else:
    raise RuntimeError('BlueprintEditorLibrary.add_component_to_blueprint unavailable')
except Exception as e:
  result['error'] = str(e)
  if not result['message']:
    result['message'] = result['error']
print('RESULT:' + json.dumps(result))
`.trim();

  try {
    const resp = await bridge.executeEditorPython(pythonScript, { allowPythonFallback: allowPython, timeoutMs: timeout });
    const success = Boolean(resp?.success);
    if (!success) return { success: false as const, error: resp?.error ?? resp?.message ?? 'PYTHON_EXEC_FAILED', message: resp?.message ?? 'Python fallback failed', blueprintPath: resp?.blueprintPath };
    return { success: true as const, message: resp?.message ?? 'Component added via Python', blueprintPath: resp?.blueprintPath };
  } catch (err) {
    return { success: false as const, error: String(err), message: 'Failed to execute Python fallback' };
  }
}
