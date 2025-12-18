import { Logger } from './utils/logger.js';
import { ErrorHandler } from './utils/error-handler.js';
import type { AutomationBridge } from './automation/index.js';
import { DEFAULT_AUTOMATION_HOST, DEFAULT_AUTOMATION_PORT } from './constants.js';
import { UnrealCommandQueue } from './utils/unreal-command-queue.js';
import { CommandValidator } from './utils/command-validator.js';

/** Connection event payload for automation bridge events */
interface ConnectionEventInfo {
  host?: string;
  port?: number;
  reason?: string;
  error?: string;
  [key: string]: unknown;
}

/** Result object from automation requests */
interface AutomationResult {
  value?: unknown;
  propertyValue?: unknown;
  message?: string;
  warnings?: string[];
  [key: string]: unknown;
}

/** Subsystems feature flags */
interface SubsystemFlags {
  unrealEditor?: boolean;
  levelEditor?: boolean;
  editorActor?: boolean;
  [key: string]: unknown;
}

/** Engine version result */
interface EngineVersionResult {
  version?: string;
  major?: number;
  minor?: number;
  patch?: number;
  isUE56OrAbove?: boolean;
  [key: string]: unknown;
}

/** Console command response */
interface ConsoleCommandResponse {
  success?: boolean;
  message?: string;
  error?: string;
  transport?: string;
  [key: string]: unknown;
}

export class UnrealBridge {
  private log = new Logger('UnrealBridge');
  private connected = false;
  private automationBridge?: AutomationBridge;
  private automationBridgeListeners?: {
    connected: (info: ConnectionEventInfo) => void;
    disconnected: (info: ConnectionEventInfo) => void;
    handshakeFailed: (info: ConnectionEventInfo) => void;
  };

  // Command queue for throttling
  private commandQueue = new UnrealCommandQueue();

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

    const onConnected = (info: ConnectionEventInfo) => {
      this.connected = true;
      this.log.debug('Automation bridge connected', info);
    };

    const onDisconnected = (info: ConnectionEventInfo) => {
      this.connected = false;
      this.log.debug('Automation bridge disconnected', info);
    };

    const onHandshakeFailed = (info: ConnectionEventInfo) => {
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

    this.connected = automationBridge.isConnected();
  }

  /**
   * Get the automation bridge instance safely.
   * Throws if not configured, but does not check connection status (use isConnected for that).
   */
  getAutomationBridge(): AutomationBridge {
    if (!this.automationBridge) {
      throw new Error('Automation bridge is not configured');
    }
    return this.automationBridge;
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
    if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
      this.log.info('🔌 MOCK MODE: Simulating active connection');
      this.connected = true;
      return true;
    }

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
      } catch { }
      return this.connected;
    }

    this.connectPromise = ErrorHandler.retryWithBackoff(
      () => {
        const envTimeout = process.env.UNREAL_CONNECTION_TIMEOUT ? parseInt(process.env.UNREAL_CONNECTION_TIMEOUT, 10) : 30000;
        const actualTimeout = envTimeout > 0 ? envTimeout : timeoutMs;
        return this.connect(actualTimeout);
      },
      {
        maxRetries: Math.max(0, maxAttempts - 1),
        initialDelay: retryDelayMs,
        maxDelay: 10000,
        backoffMultiplier: 1.5,
        shouldRetry: (error: any) => {
          const msg = (error as Error)?.message?.toLowerCase() || '';
          return msg.includes('timeout') || msg.includes('connect') || msg.includes('automation');
        }
      }
    ).catch((err: any) => {
      this.log.warn(`Automation bridge connection failed after ${maxAttempts} attempts:`, err.message);
      this.log.warn('⚠️  Ensure Unreal Editor is running with MCP Automation Bridge plugin enabled');
      this.log.warn(`⚠️  Plugin should listen on ws://${DEFAULT_AUTOMATION_HOST}:${DEFAULT_AUTOMATION_PORT} for MCP server connections`);
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

    // Start the bridge connection if it's not active
    // This supports lazy connection where the bridge doesn't start until a tool is used
    automationBridge.start();

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
        automationBridge.off('error', onError);
        automationBridge.off('disconnected', onDisconnected);
        clearTimeout(timer);
      };

      const onConnected = (info: any) => {
        cleanup();
        this.log.debug('Automation bridge connected while waiting', info);
        resolve(true);
      };

      const onHandshakeFailed = (info: any) => {
        this.log.warn('Automation bridge handshake failed while waiting', info);
        // We don't resolve false immediately here? The original code didn't. 
        // But handshake failed usually means we should stop waiting.
        cleanup();
        resolve(false);
      };

      const onError = (err: any) => {
        this.log.warn('Automation bridge error while waiting', err);
        cleanup();
        resolve(false);
      };

      const onDisconnected = (info: any) => {
        this.log.warn('Automation bridge disconnected while waiting', info);
        cleanup();
        resolve(false);
      };

      const timer = setTimeout(() => {
        cleanup();
        resolve(false);
      }, Math.max(0, timeoutMs));

      automationBridge.on('connected', onConnected);
      automationBridge.on('handshakeFailed', onHandshakeFailed);
      automationBridge.on('error', onError);
      automationBridge.on('disconnected', onDisconnected);
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

    if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
      return {
        success: true,
        objectPath,
        propertyName,
        value: 'MockValue',
        propertyValue: 'MockValue',
        transport: 'mock_bridge',
        message: 'Mock property read successful'
      };
    }

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
      const rawResult: AutomationResult | undefined =
        response.result && typeof response.result === 'object'
          ? { ...(response.result as Record<string, unknown>) }
          : undefined;
      const value =
        rawResult?.value ??
        rawResult?.propertyValue ??
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
          warnings: Array.isArray(rawResult?.warnings)
            ? rawResult.warnings
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

    if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
      return {
        success: true,
        objectPath,
        propertyName,
        message: 'Mock property set successful',
        transport: 'mock_bridge'
      };
    }

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
      const rawResult: AutomationResult | undefined =
        response.result && typeof response.result === 'object'
          ? { ...(response.result as Record<string, unknown>) }
          : undefined;

      if (success) {
        return {
          success: true,
          objectPath,
          propertyName,
          message:
            response.message ||
            (typeof rawResult?.message === 'string' ? rawResult.message : undefined),
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
  async executeConsoleCommand(command: string, _options: Record<string, never> = {}): Promise<any> {
    const automationAvailable = Boolean(
      this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function'
    );
    if (!automationAvailable) {
      throw new Error('Automation bridge not connected');
    }

    // Validate command
    CommandValidator.validate(command);
    const cmdTrimmed = command.trim();
    if (cmdTrimmed.length === 0) {
      return { success: true, message: 'Empty command ignored' };
    }

    if (CommandValidator.isLikelyInvalid(cmdTrimmed)) {
      this.log.warn(`Command appears invalid: ${cmdTrimmed}`);
    }

    const priority = CommandValidator.getPriority(cmdTrimmed);

    const executeCommand = async (): Promise<any> => {
      if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
        this.log.info(`[MOCK] Executing console command: ${cmdTrimmed}`);
        return { success: true, message: `Mock execution of '${cmdTrimmed}' successful`, transport: 'mock_bridge' };
      }

      if (!this.automationBridge || !this.automationBridge.isConnected()) {
        throw new Error('Automation bridge not connected');
      }

      const pluginResp: ConsoleCommandResponse = await this.automationBridge.sendAutomationRequest(
        'console_command',
        { command: cmdTrimmed },
        { timeoutMs: 30000 }
      );

      if (pluginResp && pluginResp.success) {
        return { ...pluginResp, transport: 'automation_bridge' };
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

  async executeEditorFunction(
    functionName: string,
    params?: Record<string, any>,
    _options?: { timeoutMs?: number }
  ): Promise<any> {
    if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
      return { success: true, result: { status: 'mock_success', function: functionName } };
    }

    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE' };
    }

    const resp: any = await this.automationBridge.sendAutomationRequest('execute_editor_function', {
      functionName,
      params: params ?? {}
    }, _options?.timeoutMs ? { timeoutMs: _options.timeoutMs } : undefined);

    return resp && resp.success !== false ? (resp.result ?? resp) : resp;
  }

  /** Get Unreal Engine version */
  async getEngineVersion(): Promise<{ version: string; major: number; minor: number; patch: number; isUE56OrAbove: boolean; }> {
    if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
      return { version: '5.6.0-Mock', major: 5, minor: 6, patch: 0, isUE56OrAbove: true };
    }

    const bridge = this.getAutomationBridge();
    try {
      const resp = await bridge.sendAutomationRequest(
        'system_control',
        { action: 'get_engine_version' },
        { timeoutMs: 15000 }
      );
      const raw: EngineVersionResult = resp && typeof resp.result === 'object'
        ? (resp.result as Record<string, unknown>)
        : (resp?.result as Record<string, unknown>) ?? resp ?? {};
      const version = typeof raw.version === 'string' ? raw.version : 'unknown';
      const major = typeof raw.major === 'number' ? raw.major : 0;
      const minor = typeof raw.minor === 'number' ? raw.minor : 0;
      const patch = typeof raw.patch === 'number' ? raw.patch : 0;
      const isUE56OrAbove =
        typeof raw.isUE56OrAbove === 'boolean'
          ? raw.isUE56OrAbove
          : (major > 5 || (major === 5 && minor >= 6));
      return { version, major, minor, patch, isUE56OrAbove };
    } catch (error) {
      this.log.warn('getEngineVersion failed', error);
      return {
        version: 'unknown',
        major: 0,
        minor: 0,
        patch: 0,
        isUE56OrAbove: false
      };
    }
  }

  /** Query feature flags */
  async getFeatureFlags(): Promise<{ subsystems: { unrealEditor: boolean; levelEditor: boolean; editorActor: boolean; } }> {
    if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
      return {
        subsystems: {
          unrealEditor: true,
          levelEditor: true,
          editorActor: true
        }
      };
    }

    const bridge = this.getAutomationBridge();
    try {
      const resp = await bridge.sendAutomationRequest(
        'system_control',
        { action: 'get_feature_flags' },
        { timeoutMs: 15000 }
      );
      const raw = resp && typeof resp.result === 'object'
        ? (resp.result as Record<string, unknown>)
        : (resp?.result as Record<string, unknown>) ?? resp ?? {};
      const subs: SubsystemFlags = raw && typeof raw.subsystems === 'object'
        ? (raw.subsystems as SubsystemFlags)
        : {};
      return {
        subsystems: {
          unrealEditor: Boolean(subs.unrealEditor),
          levelEditor: Boolean(subs.levelEditor),
          editorActor: Boolean(subs.editorActor)
        }
      };
    } catch (error) {
      this.log.warn('getFeatureFlags failed', error);
      return {
        subsystems: {
          unrealEditor: false,
          levelEditor: false,
          editorActor: false
        }
      };
    }
  }

  /**
   * SOLUTION 3: Command Throttling and Queueing
   * Prevent rapid command execution that can overwhelm the engine
   */
  private async executeThrottledCommand<T>(
    command: () => Promise<T>,
    priority: number = 5
  ): Promise<T> {
    return this.commandQueue.execute(command, priority);
  }

  /**
   * Helper delay function
   */
  private delay(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}
