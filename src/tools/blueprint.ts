import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { Logger } from '../utils/logger.js';
import { validateAssetParams, concurrencyDelay } from '../utils/validation.js';
import { coerceString } from '../utils/result-helpers.js';

export class BlueprintTools {
  private log = new Logger('BlueprintTools');
  private pluginBlueprintActionsAvailable: boolean | null = null;

  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  private async sendAction(action: string, payload: Record<string, unknown> = {}, options?: { timeoutMs?: number; waitForEvent?: boolean; waitForEventTimeoutMs?: number }) {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available' } as const;
    }
    const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
    const defaultTimeout = Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
    const finalTimeout = typeof options?.timeoutMs === 'number' && options?.timeoutMs > 0 ? options.timeoutMs : defaultTimeout;
    try {
      const response: any = await this.automationBridge.sendAutomationRequest(action, payload, { timeoutMs: finalTimeout, waitForEvent: !!options?.waitForEvent, waitForEventTimeoutMs: options?.waitForEventTimeoutMs });
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

  async createBlueprint(params: { name: string; blueprintType?: string; savePath?: string; parentClass?: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }) {
    try {
      const validation = validateAssetParams({ name: params.name, savePath: params.savePath || '/Game/Blueprints' });
      if (!validation.valid) return { success: false, message: `Failed to create blueprint: ${validation.error}`, error: validation.error };
      const sanitized = validation.sanitized;
  const payload: Record<string, unknown> = { name: sanitized.name, blueprintType: params.blueprintType ?? 'Actor', savePath: sanitized.savePath ?? '/Game/Blueprints', parentClass: params.parentClass, waitForCompletion: !!params.waitForCompletion };
      await concurrencyDelay();

      // Require Automation Bridge plugin to implement blueprint_create.
      // If the plugin is unavailable or does not implement the action,
      // return an explicit error so callers know to enable or update the plugin.
      if (!this.automationBridge) {
        return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available; blueprint_create cannot be performed' } as const;
      }

      if (this.pluginBlueprintActionsAvailable === false) {
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_create' } as const;
      }

      const envPluginTimeout = Number(process.env.MCP_AUTOMATION_PLUGIN_CREATE_TIMEOUT_MS ?? process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '15000');
      const pluginTimeout = Number.isFinite(envPluginTimeout) && envPluginTimeout > 0 ? envPluginTimeout : 15000;
      try {
  const res = await this.sendAction('blueprint_create', payload, { timeoutMs: typeof params.timeoutMs === 'number' ? params.timeoutMs : pluginTimeout, waitForEvent: !!params.waitForCompletion, waitForEventTimeoutMs: params.waitForCompletionTimeoutMs });
        if (res && res.success) {
          this.pluginBlueprintActionsAvailable = true;
          return res;
        }
        if (res && this.isUnknownActionResponse(res)) {
          this.pluginBlueprintActionsAvailable = false;
          return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_create' } as const;
        }
        return res as any;
      } catch (err: any) {
        const errTxt = String(err ?? '');
        const isTimeout = errTxt.includes('Request timed out') || errTxt.includes('-32001') || errTxt.toLowerCase().includes('timeout');
        if (isTimeout) {
          this.pluginBlueprintActionsAvailable = false;
        }
        return { success: false, error: String(err), message: String(err) } as const;
      }
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) };
    }
  }

  async modifyConstructionScript(params: { blueprintPath: string; operations: any[]; compile?: boolean; save?: boolean; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }) {
    const blueprintPath = coerceString(params.blueprintPath);
    if (!blueprintPath) return { success: false, message: 'Blueprint path is required', error: 'INVALID_BLUEPRINT_PATH' };
    if (!Array.isArray(params.operations) || params.operations.length === 0) return { success: false, message: 'At least one SCS operation is required', error: 'MISSING_OPERATIONS' };
    const payload: any = { blueprintPath, operations: params.operations };
    if (typeof params.compile === 'boolean') payload.compile = params.compile;
    if (typeof params.save === 'boolean') payload.save = params.save;
  const res = await this.sendAction('blueprint_modify_scs', payload, { timeoutMs: params.timeoutMs, waitForEvent: !!params.waitForCompletion, waitForEventTimeoutMs: params.waitForCompletionTimeoutMs });
    // Defensive: the plugin may indicate SCS_UNAVAILABLE inside the result
    // object (or via an automation_event). Treat that as a top-level
    // SCS_UNAVAILABLE so callers and higher-level helpers behave
    // consistently even when older plugin binaries return the error
    // within the result payload instead of as the top-level error field.
    if (res && res.result && typeof res.result === 'object' && (res.result as any).error === 'SCS_UNAVAILABLE') {
      this.pluginBlueprintActionsAvailable = false;
      return { success: false, error: 'SCS_UNAVAILABLE', message: 'Plugin does not support construction script modification (blueprint_modify_scs)' } as const;
    }
    if (res && res.success) this.pluginBlueprintActionsAvailable = true;
    if (res && this.isUnknownActionResponse(res)) {
      this.pluginBlueprintActionsAvailable = false;
    }
    return res;
  }

  async addComponent(params: { blueprintName: string; componentType: string; componentName: string; attachTo?: string; transform?: Record<string, unknown>; properties?: Record<string, unknown>; compile?: boolean; save?: boolean; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }) {
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
  const svcResult = await this.modifyConstructionScript({ blueprintPath: primary, operations: [op], compile: params.compile, save: params.save, timeoutMs: params.timeoutMs, waitForCompletion: params.waitForCompletion, waitForCompletionTimeoutMs: params.waitForCompletionTimeoutMs });
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
    if (this.pluginBlueprintActionsAvailable === false) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_exists' } as any;
    }

    const start = Date.now();
    const tot = typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : Number(process.env.MCP_AUTOMATION_SCS_TIMEOUT_MS ?? process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? 120000);
    const perCheck = Math.min(5000, Math.max(1000, Math.floor(tot / 6)));
    while (Date.now() - start < tot) {
      for (const candidate of candidates) {
        try {
          // Use the plugin's expected payload keys: 'blueprintCandidates' and
          // include requestedPath so the plugin can resolve quickly.
          const r = await this.sendAction('blueprint_exists', { blueprintCandidates: [candidate], requestedPath: candidate }, { timeoutMs: Math.min(perCheck, tot) });
          if (r && r.success && r.result && (r.result.exists === true || r.result.found)) {
            this.pluginBlueprintActionsAvailable = true;
            return { success: true, found: r.result.found ?? candidate } as any;
          }
          if (r && r.success === false && this.isUnknownActionResponse(r)) {
            this.pluginBlueprintActionsAvailable = false;
            return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_exists' } as any;
          }
        } catch (_e) {
          // ignore and try next candidate
        }
      }
      // conservative sleep between rounds
      await new Promise((r) => setTimeout(r, 1000));
    }
    if (this.pluginBlueprintActionsAvailable === null) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin availability unknown; blueprint_exists not implemented by plugin' } as any;
    }
    return { success: false, error: `Timeout waiting for blueprint after ${tot}ms`, checked: candidates } as any;
  }

  async getBlueprint(params: { blueprintName: string; timeoutMs?: number; }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false, error: 'Invalid blueprint name' } as const;
    try {
  const pluginResp = await this.sendAction('blueprint_get', { blueprintCandidates: candidates, requestedPath: primary }, { timeoutMs: params.timeoutMs });
      if (pluginResp && pluginResp.success) {
        if (pluginResp && typeof pluginResp === 'object') {
          return { ...pluginResp, blueprint: pluginResp.result } as any;
        }
        return pluginResp;
      }
      if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_get' } as const;
      }
      return { success: false, error: pluginResp?.error ?? 'BLUEPRINT_GET_FAILED', message: pluginResp?.message ?? 'Failed to get blueprint via automation bridge' } as const;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
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

  async addVariable(params: { blueprintName: string; variableName: string; variableType: string; defaultValue?: any; category?: string; isReplicated?: boolean; isPublic?: boolean; variablePinType?: Record<string, unknown>; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
  const pluginResp = await this.sendAction('blueprint_add_variable', { blueprintCandidates: candidates, requestedPath: primary, variableName: params.variableName, variableType: params.variableType, defaultValue: params.defaultValue, category: params.category, isReplicated: params.isReplicated, isPublic: params.isPublic, variablePinType: params.variablePinType }, { timeoutMs: params.timeoutMs, waitForEvent: !!params.waitForCompletion, waitForEventTimeoutMs: params.waitForCompletionTimeoutMs });
    if (pluginResp && pluginResp.success) {
      return pluginResp;
    }
    if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_variable' } as const;
    }
    return { success: false, error: pluginResp?.error ?? 'BLUEPRINT_ADD_VARIABLE_FAILED', message: pluginResp?.message ?? 'Failed to add variable via automation bridge' } as const;
  }

  async addEvent(params: { blueprintName: string; eventType: string; customEventName?: string; parameters?: Array<{ name: string; type: string }>; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
  const pluginResp = await this.sendAction('blueprint_add_event', { blueprintCandidates: candidates, requestedPath: primary, eventType: params.eventType, customEventName: params.customEventName, parameters: params.parameters }, { timeoutMs: params.timeoutMs, waitForEvent: !!params.waitForCompletion, waitForEventTimeoutMs: params.waitForCompletionTimeoutMs });
    if (pluginResp && pluginResp.success) {
      return pluginResp;
    }
    if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_event' } as const;
    }
    return { success: false, error: pluginResp?.error ?? 'BLUEPRINT_ADD_EVENT_FAILED', message: pluginResp?.message ?? 'Failed to add event via automation bridge' } as const;
  }

  async removeEvent(params: { blueprintName: string; eventName: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
    try {
  const pluginResp = await this.sendAction('blueprint_remove_event', { blueprintCandidates: candidates, requestedPath: primary, eventName: params.eventName }, { timeoutMs: params.timeoutMs, waitForEvent: !!params.waitForCompletion, waitForEventTimeoutMs: params.waitForCompletionTimeoutMs });
      if (pluginResp && pluginResp.success) {
        return pluginResp;
      }
      if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_remove_event' } as const;
      }
      return { success: false, error: pluginResp?.error ?? 'BLUEPRINT_REMOVE_EVENT_FAILED', message: pluginResp?.message ?? 'Failed to remove event via automation bridge' } as const;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  async addFunction(params: { blueprintName: string; functionName: string; inputs?: Array<{ name: string; type: string }>; outputs?: Array<{ name: string; type: string }>; isPublic?: boolean; category?: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
  const pluginResp = await this.sendAction('blueprint_add_function', { blueprintCandidates: candidates, requestedPath: primary, functionName: params.functionName, inputs: params.inputs, outputs: params.outputs, isPublic: params.isPublic, category: params.category }, { timeoutMs: params.timeoutMs, waitForEvent: !!params.waitForCompletion, waitForEventTimeoutMs: params.waitForCompletionTimeoutMs });
    if (pluginResp && pluginResp.success) {
      return pluginResp;
    }
    if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_add_function' } as const;
    }
    return { success: false, error: pluginResp?.error ?? 'BLUEPRINT_ADD_FUNCTION_FAILED', message: pluginResp?.message ?? 'Failed to add function via automation bridge' } as const;
  }

  async setVariableMetadata(params: { blueprintName: string; variableName: string; metadata: Record<string, unknown>; timeoutMs?: number }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
  const pluginResp = await this.sendAction('blueprint_set_variable_metadata', { blueprintCandidates: candidates, requestedPath: primary, variableName: params.variableName, metadata: params.metadata }, { timeoutMs: params.timeoutMs });
    if (pluginResp && pluginResp.success) {
      // No change to session registry required for metadata
      return pluginResp;
    }
    if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement blueprint_set_variable_metadata' } as const;
    }
    return { success: false, error: pluginResp?.error ?? 'SET_VARIABLE_METADATA_FAILED', message: pluginResp?.message ?? 'Failed to set variable metadata via automation bridge' } as const;
  }

  async addConstructionScript(params: { blueprintName: string; scriptName: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
  const pluginResp = await this.sendAction('blueprint_add_construction_script', { blueprintCandidates: candidates, requestedPath: primary, scriptName: params.scriptName }, { timeoutMs: params.timeoutMs, waitForEvent: !!params.waitForCompletion, waitForEventTimeoutMs: params.waitForCompletionTimeoutMs });
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

  // ========== SCS (Simple Construction Script) Blueprint Authoring ==========

  /**
   * Get Blueprint SCS structure
   * Retrieves the complete Simple Construction Script component hierarchy
   */
  async getBlueprintSCS(params: { blueprintPath: string; timeoutMs?: number }) {
    const blueprintPath = coerceString(params.blueprintPath);
    if (!blueprintPath) {
      return { success: false, error: 'INVALID_BLUEPRINT_PATH', message: 'Blueprint path is required' } as const;
    }

    try {
      const pluginResp = await this.sendAction('get_blueprint_scs', 
        { blueprint_path: blueprintPath }, 
        { timeoutMs: params.timeoutMs });
      
      if (pluginResp && pluginResp.success) return pluginResp;
      if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement get_blueprint_scs' } as const;
      }
      return { success: false, error: pluginResp?.error ?? 'GET_SCS_FAILED', message: pluginResp?.message ?? 'Failed to get SCS via automation bridge' } as const;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  /**
   * Add component to Blueprint SCS
   * Adds a new component to the Simple Construction Script with optional parent
   */
  async addSCSComponent(params: { 
    blueprintPath: string; 
    componentClass: string; 
    componentName: string; 
    parentComponent?: string;
    timeoutMs?: number;
  }) {
    const blueprintPath = coerceString(params.blueprintPath);
    if (!blueprintPath) {
      return { success: false, error: 'INVALID_BLUEPRINT_PATH', message: 'Blueprint path is required' } as const;
    }

    const componentClass = coerceString(params.componentClass);
    if (!componentClass) {
      return { success: false, error: 'INVALID_COMPONENT_CLASS', message: 'Component class is required' } as const;
    }

    const componentName = coerceString(params.componentName);
    if (!componentName) {
      return { success: false, error: 'INVALID_COMPONENT_NAME', message: 'Component name is required' } as const;
    }

    try {
      const payload: Record<string, unknown> = {
        blueprint_path: blueprintPath,
        component_class: componentClass,
        component_name: componentName
      };

      if (params.parentComponent) {
        payload.parent_component = params.parentComponent;
      }

      const pluginResp = await this.sendAction('add_scs_component', payload, { timeoutMs: params.timeoutMs });
      
      if (pluginResp && pluginResp.success === false) {
        if ((pluginResp as any).message) {
          this.log.warn?.(`addSCSComponent reported warning: ${(pluginResp as any).message}`);
        }
      }
      if (pluginResp && pluginResp.success) return pluginResp;
      if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement add_scs_component' } as const;
      }
      return { success: false, error: pluginResp?.error ?? 'ADD_SCS_COMPONENT_FAILED', message: pluginResp?.message ?? 'Failed to add SCS component via automation bridge' } as const;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  /**
   * Remove component from Blueprint SCS
   * Removes a component from the Simple Construction Script
   */
  async removeSCSComponent(params: { blueprintPath: string; componentName: string; timeoutMs?: number }) {
    const blueprintPath = coerceString(params.blueprintPath);
    if (!blueprintPath) {
      return { success: false, error: 'INVALID_BLUEPRINT_PATH', message: 'Blueprint path is required' } as const;
    }

    const componentName = coerceString(params.componentName);
    if (!componentName) {
      return { success: false, error: 'INVALID_COMPONENT_NAME', message: 'Component name is required' } as const;
    }

    try {
      const pluginResp = await this.sendAction('remove_scs_component',
        { blueprint_path: blueprintPath, component_name: componentName },
        { timeoutMs: params.timeoutMs });
      
      if (pluginResp && pluginResp.success === false) {
        if ((pluginResp as any).message) {
          this.log.warn?.(`removeSCSComponent reported warning: ${(pluginResp as any).message}`);
        }
      }
      if (pluginResp && pluginResp.success) return pluginResp;
      if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement remove_scs_component' } as const;
      }
      return { success: false, error: pluginResp?.error ?? 'REMOVE_SCS_COMPONENT_FAILED', message: pluginResp?.message ?? 'Failed to remove SCS component via automation bridge' } as const;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  /**
   * Reparent component in Blueprint SCS
   * Changes the parent of a component in the Simple Construction Script hierarchy
   */
  async reparentSCSComponent(params: { 
    blueprintPath: string; 
    componentName: string; 
    newParent: string;
    timeoutMs?: number;
  }) {
    const blueprintPath = coerceString(params.blueprintPath);
    if (!blueprintPath) {
      return { success: false, error: 'INVALID_BLUEPRINT_PATH', message: 'Blueprint path is required' } as const;
    }

    const componentName = coerceString(params.componentName);
    if (!componentName) {
      return { success: false, error: 'INVALID_COMPONENT_NAME', message: 'Component name is required' } as const;
    }

    try {
      const pluginResp = await this.sendAction('reparent_scs_component',
        { 
          blueprint_path: blueprintPath, 
          component_name: componentName, 
          new_parent: params.newParent || ''
        },
        { timeoutMs: params.timeoutMs });
      
      if (pluginResp && pluginResp.success === false) {
        if ((pluginResp as any).message) {
          this.log.warn?.(`reparentSCSComponent reported warning: ${(pluginResp as any).message}`);
        }
      }
      if (pluginResp && pluginResp.success) return pluginResp;
      if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement reparent_scs_component' } as const;
      }
      return { success: false, error: pluginResp?.error ?? 'REPARENT_SCS_COMPONENT_FAILED', message: pluginResp?.message ?? 'Failed to reparent SCS component via automation bridge' } as const;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  /**
   * Set component transform in Blueprint SCS
   * Sets the relative transform of a component in the Simple Construction Script
   */
  async setSCSComponentTransform(params: {
    blueprintPath: string;
    componentName: string;
    location?: [number, number, number];
    rotation?: [number, number, number];
    scale?: [number, number, number];
    timeoutMs?: number;
  }) {
    const blueprintPath = coerceString(params.blueprintPath);
    if (!blueprintPath) {
      return { success: false, error: 'INVALID_BLUEPRINT_PATH', message: 'Blueprint path is required' } as const;
    }

    const componentName = coerceString(params.componentName);
    if (!componentName) {
      return { success: false, error: 'INVALID_COMPONENT_NAME', message: 'Component name is required' } as const;
    }

    try {
      const payload: Record<string, unknown> = {
        blueprint_path: blueprintPath,
        component_name: componentName
      };

      if (params.location) payload.location = params.location;
      if (params.rotation) payload.rotation = params.rotation;
      if (params.scale) payload.scale = params.scale;

      const pluginResp = await this.sendAction('set_scs_component_transform', payload, { timeoutMs: params.timeoutMs });
      
      if (pluginResp && pluginResp.success === false) {
        if ((pluginResp as any).message) {
          this.log.warn?.(`setSCSComponentTransform reported warning: ${(pluginResp as any).message}`);
        }
      }
      if (pluginResp && pluginResp.success) return pluginResp;
      if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement set_scs_component_transform' } as const;
      }
      return { success: false, error: pluginResp?.error ?? 'SET_SCS_TRANSFORM_FAILED', message: pluginResp?.message ?? 'Failed to set SCS component transform via automation bridge' } as const;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  /**
   * Set component property in Blueprint SCS
   * Sets a property value on a component in the Simple Construction Script
   */
  async setSCSComponentProperty(params: {
    blueprintPath: string;
    componentName: string;
    propertyName: string;
    propertyValue: any;
    timeoutMs?: number;
  }) {
    const blueprintPath = coerceString(params.blueprintPath);
    if (!blueprintPath) {
      return { success: false, error: 'INVALID_BLUEPRINT_PATH', message: 'Blueprint path is required' } as const;
    }

    const componentName = coerceString(params.componentName);
    if (!componentName) {
      return { success: false, error: 'INVALID_COMPONENT_NAME', message: 'Component name is required' } as const;
    }

    const propertyName = coerceString(params.propertyName);
    if (!propertyName) {
      return { success: false, error: 'INVALID_PROPERTY_NAME', message: 'Property name is required' } as const;
    }

    try {
      // Serialize property value to JSON for the C++ handler
      const propertyValueJson = JSON.stringify({ value: params.propertyValue });

      const pluginResp = await this.sendAction('set_scs_component_property',
        {
          blueprint_path: blueprintPath,
          component_name: componentName,
          property_name: propertyName,
          property_value: propertyValueJson
        },
        { timeoutMs: params.timeoutMs });
      
      if (pluginResp && pluginResp.success === false) {
        if ((pluginResp as any).message) {
          this.log.warn?.(`setSCSComponentProperty reported warning: ${(pluginResp as any).message}`);
        }
      }
      if (pluginResp && pluginResp.success) return pluginResp;
      if (pluginResp && this.isUnknownActionResponse(pluginResp)) {
        return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement set_scs_component_property' } as const;
      }
      return { success: false, error: pluginResp?.error ?? 'SET_SCS_PROPERTY_FAILED', message: pluginResp?.message ?? 'Failed to set SCS component property via automation bridge' } as const;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  // ===== Event Graph Node authoring (editor builds only) =====
  async addNode(params: {
    blueprintName: string;
    nodeType: string; // e.g., 'variableget', 'variableset', 'customevent', 'callfunction'
    graphName?: string;
    functionName?: string;
    variableName?: string;
    nodeName?: string;
    posX?: number;
    posY?: number;
    timeoutMs?: number;
  }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
    const payload: any = {
      blueprintCandidates: candidates,
      requestedPath: primary,
      nodeType: params.nodeType,
      graphName: params.graphName,
      functionName: params.functionName,
      variableName: params.variableName,
      nodeName: params.nodeName,
      posX: params.posX,
      posY: params.posY
    };
    const res = await this.sendAction('blueprint_add_node', payload, { timeoutMs: params.timeoutMs });
    return res;
  }

  async connectPins(params: {
    blueprintName: string;
    sourceNodeGuid: string;
    targetNodeGuid: string;
    sourcePinName?: string;
    targetPinName?: string;
    timeoutMs?: number;
  }) {
    const candidates = this.buildCandidates(params.blueprintName);
    const primary = candidates[0];
    if (!primary) return { success: false as const, error: 'Invalid blueprint name' };
    const res = await this.sendAction('blueprint_connect_pins', {
      requestedPath: primary,
      sourceNodeGuid: params.sourceNodeGuid,
      targetNodeGuid: params.targetNodeGuid,
      sourcePinName: params.sourcePinName,
      targetPinName: params.targetPinName
    }, { timeoutMs: params.timeoutMs });
    return res;
  }
}
