import { UnrealBridge } from '../../unreal-bridge.js';
import { AutomationBridge } from '../../automation-bridge.js';
import { coerceString } from '../../utils/result-helpers.js';
import { allowPythonFallbackFromEnv } from '../../utils/env.js';

/**
 * Create a Blueprint asset. Prefers the automation bridge action
 * `blueprint_create`. If the plugin reports UNKNOWN_PLUGIN_ACTION and
 * Python fallbacks are enabled, this will call the centralized
 * CREATE_BLUEPRINT editor-function (the plugin may implement that
 * natively or run a guarded Python template).
 */
export async function createBlueprintViaPython(
  bridge: UnrealBridge,
  automationBridge: AutomationBridge | undefined,
  params: { name: string; blueprintType?: string; savePath?: string; parentClass?: string; timeoutMs?: number; }
) {
  const name = coerceString(params.name) ?? '';
  const savePath = coerceString(params.savePath) || '/Game/Blueprints';
  const parent = coerceString(params.parentClass) ?? '';

  const allowPython = allowPythonFallbackFromEnv();
  const automation = automationBridge ?? (bridge as any).automationBridge as AutomationBridge | undefined;
  if (!automation || typeof automation.sendAutomationRequest !== 'function') {
    return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available; blueprint_create cannot be performed' } as any;
  }

  const envTimeout = Number(process.env.MCP_AUTOMATION_PLUGIN_CREATE_TIMEOUT_MS ?? process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '15000');
  const pluginTimeout = Number.isFinite(envTimeout) && envTimeout > 0 ? envTimeout : 15000;
  const timeout = typeof params.timeoutMs === 'number' && params.timeoutMs > 0 ? params.timeoutMs : pluginTimeout;

  try {
    const resp: any = await automation.sendAutomationRequest('blueprint_create', { name, blueprintType: params.blueprintType ?? 'Actor', savePath, parentClass: parent }, { timeoutMs: timeout });
    if (resp && resp.success !== false) return resp.result ?? resp;

    const errTxt = String(resp?.error ?? resp?.message ?? '');
    if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
      if (!allowPython) return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_create' } as any;
      try {
  const funcResp: any = await (bridge as any).executeEditorFunction('CREATE_BLUEPRINT', { payload: JSON.stringify({ name, blueprintType: params.blueprintType ?? 'Actor', savePath, parentClass: parent }) }, { allowPythonFallback: allowPython });
        if (funcResp && funcResp.success !== false) return funcResp.result ?? funcResp;
        return { success: false, error: funcResp?.error ?? funcResp?.message ?? 'PYTHON_EXEC_FAILED' } as any;
      } catch (pyErr) {
        return { success: false, error: String(pyErr), message: String(pyErr) } as any;
      }
    }

    return { success: false, error: resp?.error ?? resp?.message ?? 'BLUEPRINT_CREATE_FAILED' } as any;
  } catch (err) {
    // If the automation bridge call threw, try the guarded editor-function fallback
    if (!allowPython) return { success: false, error: String(err), message: String(err), path: `${savePath}/${name}` } as any;
    try {
  const funcResp: any = await (bridge as any).executeEditorFunction('CREATE_BLUEPRINT', { payload: JSON.stringify({ name, blueprintType: params.blueprintType ?? 'Actor', savePath, parentClass: parent }) }, { allowPythonFallback: allowPython });
      if (funcResp && funcResp.success !== false) return funcResp.result ?? funcResp;
      return { success: false, error: funcResp?.error ?? 'PYTHON_EXEC_FAILED' } as any;
    } catch (pyErr) {
      return { success: false, error: String(pyErr), message: String(pyErr), path: `${savePath}/${name}` } as any;
    }
  }
}

/**
 * Add a variable to a Blueprint. Prefers plugin action
 * `blueprint_add_variable`. Falls back to editor-function
 * `BLUEPRINT_ADD_VARIABLE` only when allowed by env.
 */
export async function addVariableViaPython(
  bridge: UnrealBridge,
  params: { blueprintName: string; variableName: string; variableType: string; defaultValue?: any; category?: string; isReplicated?: boolean; isPublic?: boolean; variablePinType?: Record<string, unknown>; },
  timeoutMs?: number
) {
  const payload = JSON.stringify(params);
  const allowPython = allowPythonFallbackFromEnv();
  const automation = (bridge as any).automationBridge as AutomationBridge | undefined;

  if (!automation || typeof automation.sendAutomationRequest !== 'function') {
    return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available; cannot add variable' } as any;
  }

  try {
    const resp: any = await automation.sendAutomationRequest('blueprint_add_variable', {
      blueprintName: params.blueprintName,
      variableName: params.variableName,
      variableType: params.variableType,
      defaultValue: params.defaultValue,
      category: params.category,
      isReplicated: params.isReplicated,
      isPublic: params.isPublic,
      variablePinType: params.variablePinType
    }, { timeoutMs });

    if (resp && resp.success !== false) return resp.result ?? resp;

    const errTxt = String(resp?.error ?? resp?.message ?? '');
    if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
      if (!allowPython) return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_variable' } as any;
      try {
  const funcResp: any = await (bridge as any).executeEditorFunction('BLUEPRINT_ADD_VARIABLE', { payload }, { allowPythonFallback: allowPython });
        if (funcResp && funcResp.success !== false) return funcResp.result ?? funcResp;
        return { success: false, error: funcResp?.error ?? funcResp?.message ?? 'PYTHON_EXEC_FAILED' } as any;
      } catch (pyErr) {
        return { success: false, error: String(pyErr) } as any;
      }
    }

    return { success: false, error: resp?.error ?? resp?.message ?? 'BLUEPRINT_ADD_VARIABLE_FAILED' } as any;
  } catch (err) {
    if (!allowPython) return { success: false, error: String(err) } as any;
    try {
  const funcResp: any = await (bridge as any).executeEditorFunction('BLUEPRINT_ADD_VARIABLE', { payload }, { allowPythonFallback: allowPython });
      if (funcResp && funcResp.success !== false) return funcResp.result ?? funcResp;
      return { success: false, error: funcResp?.error ?? 'PYTHON_EXEC_FAILED' } as any;
    } catch (pyErr) {
      return { success: false, error: String(pyErr) } as any;
    }
  }
}

/**
 * Set metadata for a Blueprint variable. Prefers plugin action
 * `blueprint_set_variable_metadata`. Falls back to editor-function
 * `BLUEPRINT_SET_VARIABLE_METADATA` only when allowed by env.
 */
export async function setVariableMetadataViaPython(
  bridge: UnrealBridge,
  params: { blueprintName: string; variableName: string; metadata: Record<string, unknown>; },
  timeoutMs?: number
) {
  const payload = JSON.stringify(params);
  const allowPython = allowPythonFallbackFromEnv();
  const automation = (bridge as any).automationBridge as AutomationBridge | undefined;

  if (!automation || typeof automation.sendAutomationRequest !== 'function') {
    return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available; cannot set variable metadata' } as any;
  }

  try {
    const resp: any = await automation.sendAutomationRequest('blueprint_set_variable_metadata', { blueprintName: params.blueprintName, variableName: params.variableName, metadata: params.metadata }, { timeoutMs });
    if (resp && resp.success !== false) return resp.result ?? resp;

    const errTxt = String(resp?.error ?? resp?.message ?? '');
    if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
      if (!allowPython) return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_set_variable_metadata' } as any;
      try {
  const funcResp: any = await (bridge as any).executeEditorFunction('BLUEPRINT_SET_VARIABLE_METADATA', { payload }, { allowPythonFallback: allowPython });
        if (funcResp && funcResp.success !== false) return funcResp.result ?? funcResp;
        return { success: false, error: funcResp?.error ?? funcResp?.message ?? 'PYTHON_EXEC_FAILED' } as any;
      } catch (pyErr) {
        return { success: false, error: String(pyErr) } as any;
      }
    }

    return { success: false, error: resp?.error ?? resp?.message ?? 'SET_VARIABLE_METADATA_FAILED' } as any;
  } catch (err) {
    if (!allowPython) return { success: false, error: String(err) } as any;
    try {
  const funcResp: any = await (bridge as any).executeEditorFunction('BLUEPRINT_SET_VARIABLE_METADATA', { payload }, { allowPythonFallback: allowPython });
      if (funcResp && funcResp.success !== false) return funcResp.result ?? funcResp;
      return { success: false, error: funcResp?.error ?? 'PYTHON_EXEC_FAILED' } as any;
    } catch (pyErr) {
      return { success: false, error: String(pyErr) } as any;
    }
  }
}

/**
 * Add a construction script entry (or manipulate the construction script)
 * for a Blueprint. Prefers `blueprint_add_construction_script` and falls
 * back to the editor-function `BLUEPRINT_ADD_CONSTRUCTION_SCRIPT` when
 * allowed.
 */
export async function addConstructionScriptViaPython(
  bridge: UnrealBridge,
  params: { blueprintName: string; scriptName: string; },
  timeoutMs?: number
) {
  const payload = JSON.stringify(params);
  const allowPython = allowPythonFallbackFromEnv();
  const automation = (bridge as any).automationBridge as AutomationBridge | undefined;

  if (!automation || typeof automation.sendAutomationRequest !== 'function') {
    return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available; cannot add construction script' } as any;
  }

  try {
    const resp: any = await automation.sendAutomationRequest('blueprint_add_construction_script', { blueprintName: params.blueprintName, scriptName: params.scriptName }, { timeoutMs });
    if (resp && resp.success !== false) return resp.result ?? resp;

    const errTxt = String(resp?.error ?? resp?.message ?? '');
    if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
      if (!allowPython) return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_construction_script' } as any;
      try {
  const funcResp: any = await (bridge as any).executeEditorFunction('BLUEPRINT_ADD_CONSTRUCTION_SCRIPT', { payload }, { allowPythonFallback: allowPython });
        if (funcResp && funcResp.success !== false) return funcResp.result ?? funcResp;
        return { success: false, error: funcResp?.error ?? funcResp?.message ?? 'PYTHON_EXEC_FAILED' } as any;
      } catch (pyErr) {
        return { success: false, error: String(pyErr) } as any;
      }
    }

    return { success: false, error: resp?.error ?? resp?.message ?? 'ADD_CONSTRUCTION_SCRIPT_FAILED' } as any;
  } catch (err) {
    if (!allowPython) return { success: false, error: String(err) } as any;
    try {
  const funcResp: any = await (bridge as any).executeEditorFunction('BLUEPRINT_ADD_CONSTRUCTION_SCRIPT', { payload }, { allowPythonFallback: allowPython });
      if (funcResp && funcResp.success !== false) return funcResp.result ?? funcResp;
      return { success: false, error: funcResp?.error ?? 'PYTHON_EXEC_FAILED' } as any;
    } catch (pyErr) {
      return { success: false, error: String(pyErr) } as any;
    }
  }
}