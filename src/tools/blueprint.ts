import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { Logger } from '../utils/logger.js';
import { validateAssetParams, concurrencyDelay } from '../utils/validation.js';
import { coerceString } from '../utils/result-helpers.js';
// No Python fallbacks: Blueprint operations must be implemented by the
// Automation Bridge plugin. Fallbacks to editor Python or other engine
// plugins have been removed to let a dedicated MCP plugin implement the
// behavior deterministically.
import * as BlueprintProbes from './blueprint/python-probes.js';

/**
 * BlueprintTools — plugin-first implementation
 *
 * This implementation delegates all blueprint operations to the custom
 * Automation Bridge plugin rather than running Editor Python directly.
 * The plugin is expected to implement blueprint_* automation actions.
 */
export class BlueprintTools {
  private log = new Logger('BlueprintTools');
  // Cached result of whether the connected Automation Bridge plugin
  // implements specialized blueprint_* actions. `null` means unknown,
  // `true` means plugin supports them, `false` means plugin does not
  // implement these actions and we should fallback to Python probes.
  private pluginBlueprintActionsAvailable: boolean | null = null;

  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  private async sendAction(action: string, payload: Record<string, unknown> = {}, timeoutMs?: number) {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available' } as const;
    }
    const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
    const defaultTimeout = Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
    const finalTimeout = typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : defaultTimeout;
    try {
      const response: any = await this.automationBridge.sendAutomationRequest(action, payload, { timeoutMs: finalTimeout });
      const success = response && response.success !== false;
      const result = response.result ?? response;
      return { success, message: response.message ?? undefined, error: response.success === false ? (response.error ?? response.message) : undefined, result, requestId: response.requestId } as any;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  private isUnknownActionResponse(res: any): boolean {
    if (!res) return false;
    const txt = String((res.error ?? res.message ?? '')).toLowerCase();
    return txt.includes('unknown') || txt.includes('unknown_action') || txt.includes('unknown automation action');
  }

  private buildCandidates(rawName: string | undefined): string[] {
    const trimmed = coerceString(rawName)?.trim();
    if (!trimmed) return [];
    const normalized = trimmed.replace(/\\/g, '/').replace(/\/\/+/g, '/');
    const withoutLeading = normalized.replace(/^\/+/, '');
    const basename = withoutLeading.split('/').pop() ?? withoutLeading;
    const candidates: string[] = [];
    if (normalized.includes('/')) {
      if (normalized.startsWith('/')) candidates.push(normalized);
      if (basename) {
        candidates.push(`/Game/Blueprints/${basename}`);
        candidates.push(`/Game/${basename}`);
      }
      if (!normalized.startsWith('/')) candidates.push(`/${withoutLeading}`);
    } else {
      if (basename) {
        candidates.push(`/Game/Blueprints/${basename}`);
        candidates.push(`/Game/${basename}`);
      }
      candidates.push(normalized);
      candidates.push(`/${withoutLeading}`);
    }
    return candidates.filter(Boolean);
  }

  async createBlueprint(params: { name: string; blueprintType?: string; savePath?: string; parentClass?: string; timeoutMs?: number; }) {
    try {
      const validation = validateAssetParams({ name: params.name, savePath: params.savePath || '/Game/Blueprints' });
      if (!validation.valid) return { success: false, message: `Failed to create blueprint: ${validation.error}`, error: validation.error };
      const sanitized = validation.sanitized;
      const payload = { name: sanitized.name, blueprintType: params.blueprintType ?? 'Actor', savePath: sanitized.savePath ?? '/Game/Blueprints', parentClass: params.parentClass };
      await concurrencyDelay();

      // Require Automation Bridge plugin to implement blueprint_create.
      if (!this.automationBridge) {
        return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available; blueprint_create cannot be performed' } as const;
      }
      const res = await this.sendAction('blueprint_create', payload, params.timeoutMs);
      if (res && res.success) {
        this.pluginBlueprintActionsAvailable = true;
      }
      if (!res.success && this.isUnknownActionResponse(res)) {
        // Do not fall back to Python — surface a clear error so the plugin
        // owner can implement the action in their MCP plugin.
        this.pluginBlueprintActionsAvailable = false;
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_create' } as const;
      }
      return res;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) };
    }
  }

  async modifyConstructionScript(params: { blueprintPath: string; operations: any[]; compile?: boolean; save?: boolean; timeoutMs?: number; }) {
    const blueprintPath = coerceString(params.blueprintPath);
    if (!blueprintPath) return { success: false, message: 'Blueprint path is required', error: 'INVALID_BLUEPRINT_PATH' };
    if (!Array.isArray(params.operations) || params.operations.length === 0) return { success: false, message: 'At least one SCS operation is required', error: 'MISSING_OPERATIONS' };
    const payload: any = { blueprintPath, operations: params.operations };
    if (typeof params.compile === 'boolean') payload.compile = params.compile;
    if (typeof params.save === 'boolean') payload.save = params.save;
    const res = await this.sendAction('blueprint_modify_scs', payload, params.timeoutMs);
    if (res && res.success) this.pluginBlueprintActionsAvailable = true;
    if (res && this.isUnknownActionResponse(res)) {
      this.pluginBlueprintActionsAvailable = false;
    }
    return res;
  }

  async addComponent(params: { blueprintName: string; componentType: string; componentName: string; attachTo?: string; transform?: Record<string, unknown>; properties?: Record<string, unknown>; compile?: boolean; save?: boolean; timeoutMs?: number; }) {
    const blueprintName = coerceString(params.blueprintName);
    if (!blueprintName) return { success: false as const, message: 'Blueprint name is required', error: 'INVALID_BLUEPRINT' };
    const componentClass = coerceString(params.componentType);
    if (!componentClass) return { success: false as const, message: 'Component class is required', error: 'INVALID_COMPONENT_CLASS' };
    const rawComponentName = coerceString(params.componentName) ?? params.componentName;
    if (!rawComponentName) return { success: false as const, message: 'Component name is required', error: 'INVALID_COMPONENT_NAME' };
    const sanitizedComponentName = rawComponentName.replace(/[^A-Za-z0-9_]/g, '_');
    const candidates = this.buildCandidates(blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
    try {
      // Use the plugin's SCS modification endpoint which already handles
      // add_component operations. This prevents proliferation of custom
      // action names and leverages the plugin implementation directly.
      const op = { type: 'add_component', componentName: sanitizedComponentName, componentClass, attachTo: params.attachTo, transform: params.transform, properties: params.properties };
      const svcResult = await this.modifyConstructionScript({ blueprintPath: primary, operations: [op], compile: params.compile, save: params.save, timeoutMs: params.timeoutMs });
      if (svcResult && svcResult.success) {
        this.pluginBlueprintActionsAvailable = true;
        return { ...(svcResult as any), component: sanitizedComponentName, componentName: sanitizedComponentName, componentType: componentClass, componentClass, blueprintPath: svcResult.blueprintPath ?? primary } as const;
      }
      // If plugin did not implement SCS modification, surface an error.
      if (svcResult && (this.isUnknownActionResponse(svcResult) || (svcResult.error && svcResult.error === 'SCS_UNAVAILABLE'))) {
        this.pluginBlueprintActionsAvailable = false;
        return { success: false, error: 'SCS_UNAVAILABLE', message: 'Plugin does not support construction script modification (blueprint_modify_scs)'} as const;
      }
      return svcResult as any;
    } catch (err: any) {
      return { success: false, error: String(err) };
    }
  }

  async waitForBlueprint(blueprintRef: string | string[], timeoutMs?: number) {
    const candidates = Array.isArray(blueprintRef) ? blueprintRef : this.buildCandidates(blueprintRef as string | undefined);
    if (!candidates || candidates.length === 0) return { success: false, error: 'Invalid blueprint reference', checked: [] } as any;
    // Try plugin probe first unless we already know it's unavailable
    if (this.pluginBlueprintActionsAvailable === false) {
      return await BlueprintProbes.waitForBlueprint(this.bridge, candidates, timeoutMs);
    }

    const start = Date.now();
    const tot = typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : Number(process.env.MCP_AUTOMATION_SCS_TIMEOUT_MS ?? process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? 120000);
    const perCheck = Math.min(5000, Math.max(1000, Math.floor(tot / 6)));
    while (Date.now() - start < tot) {
      for (const candidate of candidates) {
        try {
          const r = await this.sendAction('blueprint_exists', { candidates: [candidate] }, Math.min(perCheck, tot));
          if (r && r.success && r.result && (r.result.exists === true || r.result.found)) {
            this.pluginBlueprintActionsAvailable = true;
            return { success: true, found: r.result.found ?? candidate } as any;
          }
          if (r && r.success === false && this.isUnknownActionResponse(r)) {
            // Plugin does not implement blueprint_exists — cache and fall back
            this.pluginBlueprintActionsAvailable = false;
            this.log.info('Automation plugin does not implement blueprint_exists; falling back to Python probe');
            return await BlueprintProbes.waitForBlueprint(this.bridge, candidates, timeoutMs);
          }
        } catch (_e) {
          // ignore and try next candidate
        }
      }
      // conservative sleep between rounds
      await new Promise((r) => setTimeout(r, 1000));
    }
    // Timeout — if plugin available we tried, otherwise fallback to Python probe
    if (this.pluginBlueprintActionsAvailable === null) {
      return await BlueprintProbes.waitForBlueprint(this.bridge, candidates, timeoutMs);
    }
    return { success: false, error: `Timeout waiting for blueprint after ${tot}ms`, checked: candidates } as any;
  }

  async probeSubobjectDataHandle(opts: { componentClass?: string } = {}) {
    return await this.sendAction('blueprint_probe_subobject_handle', { componentClass: opts.componentClass });
  }

  async setBlueprintDefault(params: { blueprintName: string; propertyName: string; value: unknown }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
    return await this.sendAction('blueprint_set_default', { blueprintCandidates: candidates, requestedPath: primary, propertyName: params.propertyName, value: params.value });
  }

  async addVariable(params: { blueprintName: string; variableName: string; variableType: string; defaultValue?: any; category?: string; isReplicated?: boolean; isPublic?: boolean; variablePinType?: Record<string, unknown>; timeoutMs?: number; }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
  // Try plugin action first; otherwise fall back to Python helpers which accept a concrete blueprintName.
    const pluginResp = await this.sendAction('blueprint_add_variable', { blueprintCandidates: candidates, requestedPath: primary, variableName: params.variableName, variableType: params.variableType, defaultValue: params.defaultValue, category: params.category, isReplicated: params.isReplicated, isPublic: params.isPublic, variablePinType: params.variablePinType }, params.timeoutMs);
    if (pluginResp && pluginResp.success) return pluginResp;
    if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_variable' } as const;
    }
    return { success: false, error: pluginResp?.error ?? 'BLUEPRINT_ADD_VARIABLE_FAILED', message: pluginResp?.message ?? 'Failed to add variable via automation bridge' } as const;
  }

  // Add event to a Blueprint (BeginPlay or custom). If plugin does not
  // implement the action, return UNKNOWN_PLUGIN_ACTION so tests fail loudly
  // and plugin authors can implement the handler.
  async addEvent(params: { blueprintName: string; eventType: string; customEventName?: string; parameters?: Array<{ name: string; type: string }>; timeoutMs?: number; }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
    const pluginResp = await this.sendAction('blueprint_add_event', { blueprintCandidates: candidates, requestedPath: primary, eventType: params.eventType, customEventName: params.customEventName, parameters: params.parameters }, params.timeoutMs);
    if (pluginResp && pluginResp.success) return pluginResp;
    if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_event' } as const;
    }
    return { success: false, error: pluginResp?.error ?? 'BLUEPRINT_ADD_EVENT_FAILED', message: pluginResp?.message ?? 'Failed to add event via automation bridge' } as const;
  }

  async addFunction(params: { blueprintName: string; functionName: string; inputs?: Array<{ name: string; type: string }>; outputs?: Array<{ name: string; type: string }>; isPublic?: boolean; category?: string; timeoutMs?: number; }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
    const pluginResp = await this.sendAction('blueprint_add_function', { blueprintCandidates: candidates, requestedPath: primary, functionName: params.functionName, inputs: params.inputs, outputs: params.outputs, isPublic: params.isPublic, category: params.category }, params.timeoutMs);
    if (pluginResp && pluginResp.success) return pluginResp;
    if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_function' } as const;
    }
    return { success: false, error: pluginResp?.error ?? 'BLUEPRINT_ADD_FUNCTION_FAILED', message: pluginResp?.message ?? 'Failed to add function via automation bridge' } as const;
  }

  async setVariableMetadata(params: { blueprintName: string; variableName: string; metadata: Record<string, unknown>; timeoutMs?: number }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
    const pluginResp = await this.sendAction('blueprint_set_variable_metadata', { blueprintCandidates: candidates, requestedPath: primary, variableName: params.variableName, metadata: params.metadata }, params.timeoutMs);
    if (pluginResp && pluginResp.success) return pluginResp;
    if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_set_variable_metadata' } as const;
    }
    return { success: false, error: pluginResp?.error ?? 'SET_VARIABLE_METADATA_FAILED', message: pluginResp?.message ?? 'Failed to set variable metadata via automation bridge' } as const;
  }

  async addConstructionScript(params: { blueprintName: string; scriptName: string; timeoutMs?: number }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
    const pluginResp = await this.sendAction('blueprint_add_construction_script', { blueprintCandidates: candidates, requestedPath: primary, scriptName: params.scriptName }, params.timeoutMs);
    if (pluginResp && pluginResp.success) return pluginResp;
    if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_construction_script' } as const;
    }
    return { success: false, error: pluginResp?.error ?? 'ADD_CONSTRUCTION_SCRIPT_FAILED', message: pluginResp?.message ?? 'Failed to add construction script via automation bridge' } as const;
  }

  async compileBlueprint(params: { blueprintName: string; saveAfterCompile?: boolean; }) {
    // Require Automation Bridge plugin to implement compile. Do not fallback to console commands.
    try {
      const candidates = this.buildCandidates(params.blueprintName);
      const primary = candidates[0] ?? params.blueprintName;
      const pluginResp = await this.sendAction('blueprint_compile', { requestedPath: primary, saveAfterCompile: params.saveAfterCompile });
      if (pluginResp && pluginResp.success) return pluginResp;
      if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
        this.pluginBlueprintActionsAvailable = false;
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_compile' } as const;
      }
      return { success: false, error: pluginResp?.error ?? 'BLUEPRINT_COMPILE_FAILED', message: pluginResp?.message ?? 'Failed to compile blueprint via automation bridge' } as const;
    } catch (err: any) {
      return { success: false, error: String(err) };
    }
  }
}
