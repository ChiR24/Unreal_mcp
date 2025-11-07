import { Logger } from './utils/logger.js';
import { ErrorHandler } from './utils/error-handler.js';
import type { AutomationBridge } from './automation-bridge.js';

interface CommandQueueItem {
  command: () => Promise<any>;
  resolve: (value: any) => void;
  reject: (reason?: any) => void;
  priority: number;
  retryCount?: number;
}

export class UnrealBridge {
  private log = new Logger('UnrealBridge');
  private connected = false;
  private automationBridge?: AutomationBridge;
  private automationBridgeListeners?: {
    connected: (info: any) => void;
    disconnected: (info: any) => void;
    handshakeFailed: (info: any) => void;
  };
  private autoReconnectEnabled = false;
  private commandProcessorInitialized = false;
  
  // Command queue for throttling
  private commandQueue: CommandQueueItem[] = [];
  private isProcessing = false;
  private readonly MIN_COMMAND_DELAY = 100; // Increased to prevent console spam
  private readonly MAX_COMMAND_DELAY = 500; // Maximum delay for heavy operations
  private readonly STAT_COMMAND_DELAY = 300; // Special delay for stat commands to avoid warnings
  private lastCommandTime = 0;
  private lastStatCommandTime = 0; // Track stat commands separately

  // Console object cache to reduce FindConsoleObject warnings
  private consoleObjectCache = new Map<string, any>();
  private readonly CONSOLE_CACHE_TTL = 300000; // 5 minutes TTL for cached objects
  private pluginStatusCache = new Map<string, { enabled: boolean; timestamp: number }>();
  private readonly PLUGIN_CACHE_TTL = 5 * 60 * 1000;
  
  // Unsafe viewmodes that can cause crashes or instability via visualizeBuffer
  private readonly UNSAFE_VIEWMODES = [
    'BaseColor', 'WorldNormal', 'Metallic', 'Specular',
      'Roughness',
      'SubsurfaceColor',
      'Opacity',
    'LightComplexity', 'LightmapDensity',
    'StationaryLightOverlap', 'CollisionPawn', 'CollisionVisibility'
  ];
  private readonly HARD_BLOCKED_VIEWMODES = new Set([
    'BaseColor', 'WorldNormal', 'Metallic', 'Specular', 'Roughness', 'SubsurfaceColor', 'Opacity'
  ]);
  private readonly VIEWMODE_ALIASES = new Map<string, string>([
    ['lit', 'Lit'],
    ['unlit', 'Unlit'],
    ['wireframe', 'Wireframe'],
    ['brushwireframe', 'BrushWireframe'],
    ['brush_wireframe', 'BrushWireframe'],
    ['detaillighting', 'DetailLighting'],
    ['detail_lighting', 'DetailLighting'],
    ['lightingonly', 'LightingOnly'],
    ['lighting_only', 'LightingOnly'],
    ['lightonly', 'LightingOnly'],
    ['light_only', 'LightingOnly'],
    ['lightcomplexity', 'LightComplexity'],
    ['light_complexity', 'LightComplexity'],
    ['shadercomplexity', 'ShaderComplexity'],
    ['shader_complexity', 'ShaderComplexity'],
    ['lightmapdensity', 'LightmapDensity'],
    ['lightmap_density', 'LightmapDensity'],
    ['stationarylightoverlap', 'StationaryLightOverlap'],
    ['stationary_light_overlap', 'StationaryLightOverlap'],
    ['reflectionoverride', 'ReflectionOverride'],
    ['reflection_override', 'ReflectionOverride'],
    ['texeldensity', 'TexelDensity'],
    ['texel_density', 'TexelDensity'],
    ['vertexcolor', 'VertexColor'],
    ['vertex_color', 'VertexColor'],
    ['litdetail', 'DetailLighting'],
    ['lit_only', 'LightingOnly']
  ]);

  get isConnected() { return this.connected; }

  setAutomationBridge(automationBridge?: AutomationBridge): void {
    if (this.automationBridge && this.automationBridgeListeners) {
      this.automationBridge.off('connected', this.automationBridgeListeners.connected);
      this.automationBridge.off('disconnected', this.automationBridgeListeners.disconnected);
      this.automationBridge.off('handshakeFailed', this.automationBridgeListeners.handshakeFailed);
    }

    this.automationBridge = automationBridge;
    this.automationBridgeListeners = undefined;

    if (!automationBridge) {
      this.connected = false;
      return;
    }

    const onConnected = (info: any) => {
      this.connected = true;
      this.log.debug('Automation bridge connected', info);
      this.startCommandProcessor();
    };

    const onDisconnected = (info: any) => {
      this.connected = false;
      this.log.debug('Automation bridge disconnected', info);
    };

    const onHandshakeFailed = (info: any) => {
      this.connected = false;
      this.log.warn('Automation bridge handshake failed', info);
    };

    automationBridge.on('connected', onConnected);
    automationBridge.on('disconnected', onDisconnected);
    automationBridge.on('handshakeFailed', onHandshakeFailed);

    this.automationBridgeListeners = {
      connected: onConnected,
      disconnected: onDisconnected,
      handshakeFailed: onHandshakeFailed
    };

    if (automationBridge.isConnected()) {
      this.startCommandProcessor();
    }

    this.connected = automationBridge.isConnected();
  }
  
  /**
   * Attempt to connect with exponential backoff retry strategy
   * Uses optimized retry pattern from TypeScript best practices
   * @param maxAttempts Maximum number of connection attempts
   * @param timeoutMs Timeout for each connection attempt in milliseconds
   * @param retryDelayMs Initial delay between retry attempts in milliseconds
   * @returns Promise that resolves to true if connected, false otherwise
   */
  private connectPromise?: Promise<void>;

  async tryConnect(maxAttempts: number = 3, timeoutMs: number = 15000, retryDelayMs: number = 3000): Promise<boolean> {
    if (this.connected && this.automationBridge?.isConnected()) {
      return true;
    }

    if (!this.automationBridge) {
      this.log.warn('Automation bridge is not configured; cannot establish connection.');
      return false;
    }

    if (this.automationBridge.isConnected()) {
      this.connected = true;
      return true;
    }

    if (this.connectPromise) {
      try {
        await this.connectPromise;
      } catch {}
      return this.connected;
    }

    this.connectPromise = ErrorHandler.retryWithBackoff(
      () => this.connect(timeoutMs),
      {
        maxRetries: Math.max(0, maxAttempts - 1),
        initialDelay: retryDelayMs,
        maxDelay: 10000,
        backoffMultiplier: 1.5,
        shouldRetry: (error) => {
          const msg = (error as Error)?.message?.toLowerCase() || '';
          return msg.includes('timeout') || msg.includes('connect') || msg.includes('automation');
        }
      }
    ).catch((err) => {
      this.log.warn(`Automation bridge connection failed after ${maxAttempts} attempts:`, err.message);
      this.log.warn('⚠️  Ensure Unreal Editor is running with MCP Automation Bridge plugin enabled');
      this.log.warn('⚠️  Plugin should listen on ws://127.0.0.1:8091 for MCP server connections');
    });

    try {
      await this.connectPromise;
    } finally {
      this.connectPromise = undefined;
    }

    this.connected = this.automationBridge?.isConnected() ?? false;
    return this.connected;
  }

  async connect(timeoutMs: number = 15000): Promise<void> {
    const automationBridge = this.automationBridge;
    if (!automationBridge) {
      throw new Error('Automation bridge not configured');
    }

    if (automationBridge.isConnected()) {
      this.connected = true;
      return;
    }

    const success = await this.waitForAutomationConnection(timeoutMs);
    if (!success) {
      throw new Error('Automation bridge connection timeout');
    }

    this.connected = true;
  }

  private async waitForAutomationConnection(timeoutMs: number): Promise<boolean> {
    const automationBridge = this.automationBridge;
    if (!automationBridge) {
      return false;
    }

    if (automationBridge.isConnected()) {
      return true;
    }

    return new Promise<boolean>((resolve) => {
      let settled = false;

      const cleanup = () => {
        if (settled) {
          return;
        }
        settled = true;
        automationBridge.off('connected', onConnected);
        automationBridge.off('handshakeFailed', onHandshakeFailed);
        clearTimeout(timer);
      };

      const onConnected = (info: any) => {
        cleanup();
        this.log.debug('Automation bridge connected while waiting', info);
        resolve(true);
      };

      const onHandshakeFailed = (info: any) => {
        this.log.warn('Automation bridge handshake failed while waiting', info);
      };

      const timer = setTimeout(() => {
        cleanup();
        resolve(false);
      }, Math.max(0, timeoutMs));

      automationBridge.on('connected', onConnected);
      automationBridge.on('handshakeFailed', onHandshakeFailed);
    });
  }

  async getObjectProperty(params: {
    objectPath: string;
    propertyName: string;
    timeoutMs?: number;
    allowAlternate?: boolean;
  }): Promise<Record<string, any>> {
  const { objectPath, propertyName, timeoutMs } = params;
    if (!objectPath || typeof objectPath !== 'string') {
      throw new Error('Invalid objectPath: must be a non-empty string');
    }
    if (!propertyName || typeof propertyName !== 'string') {
      throw new Error('Invalid propertyName: must be a non-empty string');
    }

    const bridge = this.automationBridge;
    if (!bridge || typeof bridge.sendAutomationRequest !== 'function') {
      return {
        success: false,
        objectPath,
        propertyName,
        error: 'Automation bridge not connected',
        transport: 'automation_bridge'
      };
    }

    try {
      const response = await bridge.sendAutomationRequest(
        'get_object_property',
        {
          objectPath,
          propertyName
        },
        timeoutMs ? { timeoutMs } : undefined
      );

      const success = response.success !== false;
      const rawResult =
        response.result && typeof response.result === 'object'
          ? { ...(response.result as Record<string, unknown>) }
          : response.result;
      const value =
        (rawResult as any)?.value ??
        (rawResult as any)?.propertyValue ??
        (success ? rawResult : undefined);

      if (success) {
        return {
          success: true,
          objectPath,
          propertyName,
          value,
          propertyValue: value,
          transport: 'automation_bridge',
          message: response.message,
          warnings: Array.isArray((rawResult as any)?.warnings)
            ? (rawResult as any).warnings
            : undefined,
          raw: rawResult,
          bridge: {
            requestId: response.requestId,
            success: true,
            error: response.error
          }
        };
      }

      return {
        success: false,
        objectPath,
        propertyName,
        error: response.error || response.message || 'AUTOMATION_BRIDGE_FAILURE',
        transport: 'automation_bridge',
        raw: rawResult,
        bridge: {
          requestId: response.requestId,
          success: false,
          error: response.error
        }
      };
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      return {
        success: false,
        objectPath,
        propertyName,
        error: message,
        transport: 'automation_bridge'
      };
    }
  }

  async setObjectProperty(params: {
    objectPath: string;
    propertyName: string;
    value: unknown;
    markDirty?: boolean;
    timeoutMs?: number;
    allowAlternate?: boolean;
  }): Promise<Record<string, any>> {
  const { objectPath, propertyName, value, markDirty, timeoutMs } = params;
    if (!objectPath || typeof objectPath !== 'string') {
      throw new Error('Invalid objectPath: must be a non-empty string');
    }
    if (!propertyName || typeof propertyName !== 'string') {
      throw new Error('Invalid propertyName: must be a non-empty string');
    }

    const bridge = this.automationBridge;
    if (!bridge || typeof bridge.sendAutomationRequest !== 'function') {
      return {
        success: false,
        objectPath,
        propertyName,
        error: 'Automation bridge not connected',
        transport: 'automation_bridge'
      };
    }

    const payload: Record<string, unknown> = {
      objectPath,
      propertyName,
      value
    };
    if (markDirty !== undefined) {
      payload.markDirty = Boolean(markDirty);
    }

    try {
      const response = await bridge.sendAutomationRequest(
        'set_object_property',
        payload,
        timeoutMs ? { timeoutMs } : undefined
      );

      const success = response.success !== false;
      const rawResult =
        response.result && typeof response.result === 'object'
          ? { ...(response.result as Record<string, unknown>) }
          : response.result;

      if (success) {
        return {
          success: true,
          objectPath,
          propertyName,
          message:
            response.message ||
            (typeof (rawResult as any)?.message === 'string' ? (rawResult as any).message : undefined),
          transport: 'automation_bridge',
          raw: rawResult,
          bridge: {
            requestId: response.requestId,
            success: true,
            error: response.error
          }
        };
      }

      return {
        success: false,
        objectPath,
        propertyName,
        error: response.error || response.message || 'AUTOMATION_BRIDGE_FAILURE',
        transport: 'automation_bridge',
        raw: rawResult,
        bridge: {
          requestId: response.requestId,
          success: false,
          error: response.error
        }
      };
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      return {
        success: false,
        objectPath,
        propertyName,
        error: message,
        transport: 'automation_bridge'
      };
    }
  }

  // Execute a console command safely with validation and throttling
  async executeConsoleCommand(command: string, options: { allowPython?: boolean } = {}): Promise<any> {
    const automationAvailable = Boolean(
      this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function'
    );
    if (!automationAvailable) {
      throw new Error('Automation bridge not connected');
    }
    const { allowPython = false } = options;
    // Validate command is not empty
    if (!command || typeof command !== 'string') {
      throw new Error('Invalid command: must be a non-empty string');
    }
    
    const cmdTrimmed = command.trim();
    if (cmdTrimmed.length === 0) {
      // Return success for empty commands to match UE behavior
      return { success: true, message: 'Empty command ignored' };
    }

    if (cmdTrimmed.includes('\n') || cmdTrimmed.includes('\r')) {
      throw new Error('Multi-line console commands are not allowed. Send one command per call.');
    }

    const cmdLower = cmdTrimmed.toLowerCase();

    if (!allowPython && (cmdLower === 'py' || cmdLower.startsWith('py '))) {
      throw new Error('Python console commands are blocked from external calls for safety.');
    }
    
    // Check for dangerous commands
    const dangerousCommands = [
      'quit', 'exit', 'delete', 'destroy', 'kill', 'crash',
      'viewmode visualizebuffer basecolor',
      'viewmode visualizebuffer worldnormal',
      'r.gpucrash',
      'buildpaths', // Can cause access violation if nav system not initialized
      'rebuildnavigation' // Can also crash without proper nav setup
    ];
    if (dangerousCommands.some(dangerous => cmdLower.includes(dangerous))) {
      throw new Error(`Dangerous command blocked: ${command}`);
    }

    const forbiddenTokens = [
      'rm ', 'rm-', 'del ', 'format ', 'shutdown', 'reboot',
      'rmdir', 'mklink', 'copy ', 'move ', 'start "', 'system(',
      'import os', 'import subprocess', 'subprocess.', 'os.system',
      'exec(', 'eval(', '__import__', 'import sys', 'import importlib',
      'with open', 'open('
    ];

    if (cmdLower.includes('&&') || cmdLower.includes('||')) {
      throw new Error('Command chaining with && or || is blocked for safety.');
    }

    if (forbiddenTokens.some(token => cmdLower.includes(token))) {
      throw new Error(`Command contains unsafe token and was blocked: ${command}`);
    }
    
    // Determine priority based on command type
    let priority = 7; // Default priority
    
    if (command.includes('BuildLighting') || command.includes('BuildPaths')) {
      priority = 1; // Heavy operation
    } else if (command.includes('summon') || command.includes('spawn')) {
      priority = 5; // Medium operation
    } else if (command.startsWith('stat')) {
      priority = 8; // Dedicated throttling for stat commands
    } else if (command.startsWith('show')) {
      priority = 9; // Light operation
    }
    
    // Known invalid command patterns
    const invalidPatterns = [
      /^\d+$/,  // Just numbers
      /^invalid_command/i,
      /^this_is_not_a_valid/i,
    ];
    
    const isLikelyInvalid = invalidPatterns.some(pattern => pattern.test(cmdTrimmed));
    if (isLikelyInvalid) {
      this.log.warn(`Command appears invalid: ${cmdTrimmed}`);
    }
    
    const executeCommand = async (): Promise<any> => {
      if (!this.automationBridge || !this.automationBridge.isConnected()) {
        throw new Error('Automation bridge not connected');
      }

      const pluginResp: any = await this.automationBridge.sendAutomationRequest(
        'execute_console_command',
        { command: cmdTrimmed },
        { timeoutMs: 30000 }
      );
      
      if (pluginResp && pluginResp.success) {
        return { ...(pluginResp as any), transport: 'automation_bridge' };
      }

      const errMsg = pluginResp?.message || pluginResp?.error || 'Plugin execution failed';
      throw new Error(errMsg);
    };

    try {
      const result = await this.executeThrottledCommand(executeCommand, priority);
      return result;
    } catch (error) {
      this.log.error(`Console command failed: ${cmdTrimmed}`, error);
      throw error;
    }
  }

  summarizeConsoleCommand(command: string, response: any) {
    const trimmedCommand = command.trim();
    const logLines: string[] = [];

    if (Array.isArray(response?.LogOutput)) {
      for (const entry of response.LogOutput as any[]) {
        if (entry === null || entry === undefined) continue;
        if (typeof entry === 'string') {
          logLines.push(entry);
        } else if (typeof entry.Output === 'string') {
          logLines.push(entry.Output);
        }
      }
    }

    if (Array.isArray(response?.logLines)) {
      for (const entry of response.logLines as any[]) {
        if (typeof entry === 'string' && entry.trim().length > 0) {
          logLines.push(entry);
        }
      }
    }

    if (Array.isArray(response?.logs)) {
      for (const entry of response.logs as any[]) {
        if (typeof entry === 'string' && entry.trim().length > 0) {
          logLines.push(entry);
        }
      }
    }

    let output = logLines.join('\n').trim();
    if (!output) {
      if (typeof response === 'string') {
        output = response.trim();
      } else if (response && typeof response === 'object') {
        if (typeof response.output === 'string') {
          output = response.output.trim();
        } else if (typeof response.Output === 'string') {
          output = response.Output.trim();
        } else if ('result' in response && response.result !== undefined) {
          output = String(response.result).trim();
        } else if ('ReturnValue' in response && typeof response.ReturnValue === 'string') {
          output = response.ReturnValue.trim();
        }
      }
    }

    const returnValue = response && typeof response === 'object'
      ? ((response as any).ReturnValue ?? (response as any).returnValue ?? ((response as any).result && (response as any).result.ReturnValue))
      : undefined;

    return {
      command: trimmedCommand,
      output,
      logLines,
      returnValue,
      transport: typeof response?.transport === 'string' ? response.transport : undefined,
      raw: response
    };
  }

  async executeConsoleCommands(
    commands: Iterable<string | { command: string; priority?: number }>,
    options: { continueOnError?: boolean; delayMs?: number } = {}
  ): Promise<any[]> {
    const { continueOnError = false, delayMs = 0 } = options;
    const results: any[] = [];

    for (const rawCommand of commands) {
      const descriptor = typeof rawCommand === 'string' ? { command: rawCommand } : rawCommand;
      const command = descriptor.command?.trim();
      if (!command) {
        continue;
      }
      try {
        const result = await this.executeConsoleCommand(command);
        results.push(result);
      } catch (error) {
        if (!continueOnError) {
          throw error;
        }
        this.log.warn(`Console batch command failed: ${command}`, error);
        results.push(error);
      }

      if (delayMs > 0) {
        await this.delay(delayMs);
      }
    }

    return results;
  }

  setAutoReconnectEnabled(enabled: boolean): void {
    this.autoReconnectEnabled = enabled;
  }

  // Graceful shutdown
  async disconnect(): Promise<void> {
    this.connected = false;
  }

  async executeEditorFunction(
    functionName: string,
    params?: Record<string, any>,
    _options?: { timeoutMs?: number }
  ): Promise<any> {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE' };
    }

    const resp: any = await this.automationBridge.sendAutomationRequest('execute_editor_function', {
      functionName,
      params: params ?? {}
    }, _options?.timeoutMs ? { timeoutMs: _options.timeoutMs } : undefined);
    
    return resp && resp.success !== false ? (resp.result ?? resp) : resp;
  }

  /** Stub - plugin management removed */
  async ensurePluginsEnabled(_pluginNames: string[], _context?: string): Promise<string[]> {
    return [];
  }

  async executeEditorPython(_script: string, _options?: { timeoutMs?: number }): Promise<any> {
    return {
      success: false,
      error: 'PYTHON_EXECUTION_REMOVED',
      message: 'Python execution removed. Use native plugin handlers instead.'
    };
  }

  async executePythonWithResult(_script: string, _timeoutMs?: number): Promise<any> {
    throw new Error('Python execution removed. Use native plugin handlers instead.');
  }

  /** Get Unreal Engine version */
  async getEngineVersion(): Promise<{ version: string; major: number; minor: number; patch: number; isUE56OrAbove: boolean; }> {
    this.log.debug('[STUB] getEngineVersion called');
    return {
      version: 'unknown',
      major: 5,
      minor: 6,
      patch: 0,
      isUE56OrAbove: true
    };
  }

  /** Query feature flags (stub) */
  async getFeatureFlags(): Promise<{ pythonEnabled: boolean; subsystems: { unrealEditor: boolean; levelEditor: boolean; editorActor: boolean; } }> {
    this.log.debug('[STUB] getFeatureFlags called');
    return {
      pythonEnabled: false,
      subsystems: {
        unrealEditor: false,
        levelEditor: false,
        editorActor: false
      }
    };
  }

  /**
   * Check whether an asset exists using plugin-native methods only
   */
  async assetExists(assetPath: string): Promise<boolean> {
    if (!assetPath || typeof assetPath !== 'string') return false;

    try {
      const result = await this.executeEditorFunction('ASSET_EXISTS', { path: assetPath });
      if (result && typeof result === 'object') {
        if (typeof result.exists === 'boolean') return result.exists;
        if (typeof (result as any).result === 'object' && typeof (result as any).result.exists === 'boolean') {
          return (result as any).result.exists;
        }
      }
      if (typeof result === 'boolean') return result;
    } catch (err) {
      this.log.debug('assetExists plugin call failed:', (err as Error)?.message ?? err);
    }
    return false;
  }

  /**
   * Alternate commands when Python is not available
   */
  private async executeAlternateCommand(functionName: string, params?: Record<string, any>): Promise<any> {
    switch (functionName) {
      case 'SPAWN_ACTOR_AT_LOCATION':
        return this.executeEditorFunction('SPAWN_ACTOR_AT_LOCATION', {
          class_path: params?.class_path || 'StaticMeshActor',
          params: { location: { x: params?.x || 0, y: params?.y || 0, z: params?.z || 0 }, rotation: params?.rotation }
        });
      
      case 'DELETE_ACTOR':
        // Prefer plugin-native delete (or executeEditorFunction fallback) over direct Python
        return this.executeEditorFunction('DELETE_ACTOR', { actor_name: params?.actor_name });
      
      case 'BUILD_LIGHTING':
        // Prefer plugin / LevelEditorSubsystem to run a lighting build; fall back to console if unavailable.
        try {
          return await this.executeEditorFunction('BUILD_LIGHTING', { quality: params?.quality });
        } catch (_err) {
          return this.executeConsoleCommand('BuildLighting');
        }
      
      default:
        throw new Error(`No alternate available for ${functionName}`);
    }
  }

  /**
   * SOLUTION 2: Safe ViewMode Switching
   * Prevent crashes by validating and safely switching viewmodes
   */
  async setSafeViewMode(mode: string): Promise<any> {
    const acceptedModes = Array.from(new Set(this.VIEWMODE_ALIASES.values())).sort();

    if (typeof mode !== 'string') {
      return {
        success: false,
        error: 'View mode must be provided as a string',
        acceptedModes
      };
    }

    const key = mode.trim().toLowerCase().replace(/[\s_-]+/g, '');
    if (!key) {
      return {
        success: false,
        error: 'View mode cannot be empty',
        acceptedModes
      };
    }

    const targetMode = this.VIEWMODE_ALIASES.get(key);
    if (!targetMode) {
      return {
        success: false,
        error: `Unknown view mode '${mode}'`,
        acceptedModes
      };
    }

    if (this.HARD_BLOCKED_VIEWMODES.has(targetMode)) {
      this.log.warn(`Viewmode '${targetMode}' is blocked for safety. Using alternative.`);
      const alternative = this.getSafeAlternative(targetMode);
      const altCommand = `viewmode ${alternative}`;
      const altResult = await this.executeConsoleCommand(altCommand);
      const altSummary = this.summarizeConsoleCommand(altCommand, altResult);
      return {
        ...altSummary,
        success: false,
        requestedMode: targetMode,
        viewMode: alternative,
        message: `View mode '${targetMode}' is unsafe in remote sessions. Switched to '${alternative}'.`,
        alternative
      };
    }

    const command = `viewmode ${targetMode}`;
    const rawResult = await this.executeConsoleCommand(command);
    const summary = this.summarizeConsoleCommand(command, rawResult);
    const response: any = {
      ...summary,
      success: summary.returnValue !== false,
      requestedMode: targetMode,
      viewMode: targetMode,
      message: `View mode set to ${targetMode}`
    };

    if (this.UNSAFE_VIEWMODES.includes(targetMode)) {
      response.warning = `View mode '${targetMode}' may be unstable on some engine versions.`;
    }

    if (summary.output && /unknown|invalid/i.test(summary.output)) {
      response.success = false;
      response.error = summary.output;
    }

    return response;
  }

  /**
   * Get safe alternative for unsafe viewmodes
   */
  private getSafeAlternative(unsafeMode: string): string {
    const alternatives: Record<string, string> = {
      'BaseColor': 'Unlit',
      'WorldNormal': 'Lit',
      'Metallic': 'Lit',
      'Specular': 'Lit',
      'Roughness': 'Lit',
      'SubsurfaceColor': 'Lit',
      'Opacity': 'Lit',
      'LightComplexity': 'LightingOnly',
      'ShaderComplexity': 'Wireframe',
      'CollisionPawn': 'Wireframe',
      'CollisionVisibility': 'Wireframe'
    };
    
    return alternatives[unsafeMode] || 'Lit';
  }

  /**
   * SOLUTION 3: Command Throttling and Queueing
   * Prevent rapid command execution that can overwhelm the engine
   */
  private async executeThrottledCommand<T>(
    command: () => Promise<T>, 
    priority: number = 5
  ): Promise<T> {
    return new Promise((resolve, reject) => {
      this.commandQueue.push({
        command,
        resolve,
        reject,
        priority
      });
      
      // Sort by priority (lower number = higher priority)
      this.commandQueue.sort((a, b) => a.priority - b.priority);
      
      // Process queue if not already processing
      if (!this.isProcessing) {
        this.processCommandQueue();
      }
    });
  }

  /**
   * Process command queue with appropriate delays
   */
  private async processCommandQueue(): Promise<void> {
    if (this.isProcessing || this.commandQueue.length === 0) {
      return;
    }
    
    this.isProcessing = true;
    
    while (this.commandQueue.length > 0) {
      const item = this.commandQueue.shift();
      if (!item) continue; // Skip if undefined
      
      // Calculate delay based on time since last command
      const timeSinceLastCommand = Date.now() - this.lastCommandTime;
      const requiredDelay = this.calculateDelay(item.priority);
      
      if (timeSinceLastCommand < requiredDelay) {
        await this.delay(requiredDelay - timeSinceLastCommand);
      }
      
      try {
        const result = await item.command();
        item.resolve(result);
      } catch (error: any) {
        // Enhanced retry policy: only retry on transient transport/timeouts
        // and avoid retrying deterministic command failures such as
        // EXEC_FAILED/Command not executed or client-side validation errors.
        const msgRaw = error?.message ?? String(error);
        const msg = String(msgRaw).toLowerCase();
        if (item.retryCount === undefined) item.retryCount = 0;

        const isTransient = (
          msg.includes('timeout') ||
          msg.includes('timed out') ||
          msg.includes('connect') ||
          msg.includes('econnrefused') ||
          msg.includes('econnreset') ||
          msg.includes('broken pipe') ||
          msg.includes('automation bridge') ||
          msg.includes('not connected')
        );

        const isDeterministicFailure = (
          msg.includes('command not executed') ||
          msg.includes('exec_failed') ||
          msg.includes('invalid command') ||
          msg.includes('invalid argument') ||
          msg.includes('unknown_plugin_action') ||
          msg.includes('unknown action')
        );

        if (isTransient && item.retryCount < 3) {
          item.retryCount++;
          this.log.warn(`Command failed (transient), retrying (${item.retryCount}/3)`);
          // Re-add to queue with increased priority
          this.commandQueue.unshift({
            command: item.command,
            resolve: item.resolve,
            reject: item.reject,
            priority: Math.max(1, item.priority - 1),
            retryCount: item.retryCount
          });
          // Add extra delay before retry
          await this.delay(500);
        } else {
          if (isDeterministicFailure) {
            // Log once at warning level and do not retry deterministic
            // failures to avoid noisy repeated attempts.
            this.log.warn(`Command failed (non-retryable): ${msgRaw}`);
          }
          item.reject(error);
        }
      }
      
      this.lastCommandTime = Date.now();
    }
    
    this.isProcessing = false;
  }

  /**
   * Calculate appropriate delay based on command priority and type
   */
  private calculateDelay(priority: number): number {
    // Priority 1-3: Heavy operations (asset creation, lighting build)
    if (priority <= 3) {
      return this.MAX_COMMAND_DELAY;
    }
    // Priority 4-6: Medium operations (actor spawning, material changes)
    else if (priority <= 6) {
      return 200;
    }
    // Priority 8: Stat commands - need special handling
    else if (priority === 8) {
      // Check time since last stat command to avoid FindConsoleObject warnings
      const timeSinceLastStat = Date.now() - this.lastStatCommandTime;
      if (timeSinceLastStat < this.STAT_COMMAND_DELAY) {
        return this.STAT_COMMAND_DELAY;
      }
      this.lastStatCommandTime = Date.now();
      return 150;
    }
    // Priority 7,9-10: Light operations (console commands, queries)
    else {
      // For light operations, add some jitter to prevent thundering herd
      const baseDelay = this.MIN_COMMAND_DELAY;
      const jitter = Math.random() * 50; // Add up to 50ms random jitter
      return baseDelay + jitter;
    }
  }

  /**
   * SOLUTION 4: Enhanced Asset Creation
   * Use Python scripting for complex asset creation that requires editor scripting
   */
  async createComplexAsset(assetType: string, params: Record<string, any>): Promise<any> {
    const assetCreators: Record<string, string> = {
      'Material': 'MaterialFactoryNew',
      'MaterialInstance': 'MaterialInstanceConstantFactoryNew',
      'Blueprint': 'BlueprintFactory',
      'AnimationBlueprint': 'AnimBlueprintFactory',
      'ControlRig': 'ControlRigBlueprintFactory',
      'NiagaraSystem': 'NiagaraSystemFactoryNew',
      'NiagaraEmitter': 'NiagaraEmitterFactoryNew',
      'LandscapeGrassType': 'LandscapeGrassTypeFactory',
      'PhysicsAsset': 'PhysicsAssetFactory'
    };

    const factoryClass = assetCreators[assetType];
    if (!factoryClass) {
      throw new Error(`Unknown asset type: ${assetType}`);
    }

    const createParams = {
      factory_class: factoryClass,
      asset_class: `unreal.${assetType}`,
      asset_name: params.name || `New${assetType}`,
      package_path: params.path || '/Game/CreatedAssets',
      ...params
    };

    return this.executeEditorFunction('CREATE_ASSET', createParams);
  }

  /**
   * Start the command processor
   */
  private startCommandProcessor(): void {
    if (this.commandProcessorInitialized) {
      return;
    }
    this.commandProcessorInitialized = true;
    // Periodic queue processing to handle any stuck commands
    setInterval(() => {
      if (!this.isProcessing && this.commandQueue.length > 0) {
        this.processCommandQueue();
      }
    }, 1000);

    // Clean console cache every 5 minutes
    setInterval(() => {
      this.cleanConsoleCache();
    }, this.CONSOLE_CACHE_TTL);
  }

  /**
   * Clean expired entries from console object cache
   */
  private cleanConsoleCache(): void {
    const now = Date.now();
    let cleaned = 0;
    for (const [key, value] of this.consoleObjectCache.entries()) {
      if (now - (value.timestamp || 0) > this.CONSOLE_CACHE_TTL) {
        this.consoleObjectCache.delete(key);
        cleaned++;
      }
    }
    if (cleaned > 0) {
      this.log.debug(`Cleaned ${cleaned} expired console cache entries`);
    }
  }

  /**
   * Helper delay function
   */
  private delay(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Batch command execution with proper delays
   */
  async executeBatch(commands: Array<{ command: string; priority?: number }>): Promise<any[]> {
    return this.executeConsoleCommands(commands.map(cmd => cmd.command));
  }

  /**
   * Get safe console commands for common operations
   */
  getSafeCommands(): Record<string, string> {
    return {
      // Health check (safe, no side effects)
'HealthCheck': 'stat none',
      
      // Performance monitoring (safe)
      'ShowFPS': 'stat unit',  // Use 'stat unit' instead of 'stat fps'
      'ShowMemory': 'stat memory',
      'ShowGame': 'stat game',
      'ShowRendering': 'stat scenerendering',
      'ClearStats': 'stat none',
      
      // Safe viewmodes
      'ViewLit': 'viewmode lit',
      'ViewUnlit': 'viewmode unlit',
      'ViewWireframe': 'viewmode wireframe',
      'ViewDetailLighting': 'viewmode detaillighting',
      'ViewLightingOnly': 'viewmode lightingonly',
      
      // Safe show flags
      'ShowBounds': 'show bounds',
      'ShowCollision': 'show collision',
      'ShowNavigation': 'show navigation',
      'ShowFog': 'show fog',
      'ShowGrid': 'show grid',
      
      // PIE controls
      'PlayInEditor': 'play',
      'StopPlay': 'stop',
      'PausePlay': 'pause',
      
      // Time control
      'SlowMotion': 'slomo 0.5',
      'NormalSpeed': 'slomo 1',
      'FastForward': 'slomo 2',
      
      // Camera controls
      'CameraSpeed1': 'camspeed 1',
      'CameraSpeed4': 'camspeed 4',
      'CameraSpeed8': 'camspeed 8',
      
      // Rendering quality (safe)
      'LowQuality': 'sg.ViewDistanceQuality 0',
      'MediumQuality': 'sg.ViewDistanceQuality 1',
      'HighQuality': 'sg.ViewDistanceQuality 2',
      'EpicQuality': 'sg.ViewDistanceQuality 3'
    };
  }
}
