import { Logger } from './utils/logger.js';
import { ErrorHandler } from './utils/error-handler.js';
import { escapePythonString } from './utils/python.js';
import { allowPythonFallbackFromEnv } from './utils/env.js';
import type { AutomationBridge } from './automation-bridge.js';

// RcMessage interface reserved for future WebSocket message handling
// interface RcMessage {
//   MessageName: string;
//   Parameters?: any;
// }

interface CommandQueueItem {
  command: () => Promise<any>;
  resolve: (value: any) => void;
  reject: (reason?: any) => void;
  priority: number;
  retryCount?: number;
}

interface PythonScriptTemplate {
  name: string;
  script: string;
  params?: Record<string, any>;
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
  private autoReconnectEnabled = false; // retained for API compatibility
  private commandProcessorInitialized = false;
  private engineVersionCache?: { value: { version: string; major: number; minor: number; patch: number; isUE56OrAbove: boolean }; timestamp: number };
  private readonly ENGINE_VERSION_TTL_MS = 5 * 60 * 1000;
  
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
  
  // Python script templates for essential editor helpers. Keep this map
  // intentionally small: most operations should be implemented natively
  // in the Automation Bridge plugin and routed via executeEditorFunction.
  private readonly PYTHON_TEMPLATES: Record<string, PythonScriptTemplate> = {
    GET_ALL_ACTORS: {
      name: 'get_all_actors',
      script: `
import unreal, json
try:
  subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  actors = []
  if subsys:
    for a in subsys.get_all_level_actors():
      if a:
        actors.append({'name': a.get_name(), 'label': a.get_actor_label(), 'path': a.get_path_name(), 'class': a.get_class().get_path_name() if a.get_class() else ''})
  print('RESULT:' + json.dumps({'success': True, 'actors': actors, 'count': len(actors)}))
except Exception as e:
  print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
      `.trim()
    },
    SPAWN_ACTOR_AT_LOCATION: {
      name: 'spawn_actor',
      script: `
import unreal, json
try:
  location = unreal.Vector({x}, {y}, {z})
  rotation = unreal.Rotator({pitch}, {yaw}, {roll})
  subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  if not subsys:
    print('RESULT:' + json.dumps({'success': False, 'error': 'EditorActorSubsystem not available'}))
  else:
    asset = unreal.EditorAssetLibrary.load_asset(r"{class_path}") if "{class_path}" else None
    cls = asset if asset and hasattr(asset, 'GeneratedClass') else None
    spawned = None
    if cls:
      spawned = subsys.spawn_actor_from_object(cls, location, rotation)
    if spawned:
      print('RESULT:' + json.dumps({'success': True, 'actorName': spawned.get_actor_label(), 'actorPath': spawned.get_path_name()}))
    else:
      print('RESULT:' + json.dumps({'success': False, 'error': 'Spawn failed'}))
except Exception as e:
  print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
      `.trim()
    },
    DELETE_ACTOR: {
      name: 'delete_actor',
      script: `
import unreal, json
try:
  subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  if not subsys:
    print('RESULT:' + json.dumps({'success': False, 'error': 'EditorActorSubsystem not available'}))
  else:
    target = None
    for a in subsys.get_all_level_actors():
      if not a: continue
      if a.get_actor_label() == "{actor_name}" or a.get_name() == "{actor_name}":
        target = a
        break
    if not target:
      print('RESULT:' + json.dumps({'success': False, 'error': 'Actor not found'}))
    else:
      ok = subsys.destroy_actor(target)
      print('RESULT:' + json.dumps({'success': bool(ok), 'deleted': target.get_actor_label()}))
except Exception as e:
  print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
      `.trim()
    },
    CREATE_ASSET: {
      name: 'create_asset',
      script: `
import unreal, json
try:
  asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
  factory = getattr(unreal, '{factory_class}', None)
  asset_class = getattr(unreal, '{asset_class}', None)
  package_path = '{package_path}'.rstrip('/')
  if not package_path.startswith('/Game') and not package_path.startswith('/Engine'):
    package_path = '/Game/' + package_path.strip('/')
  asset = asset_tools.create_asset('{asset_name}', package_path, asset_class, factory() if factory else None)
  if asset:
    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
    print('RESULT:' + json.dumps({'success': True, 'path': asset.get_path_name()}))
  else:
    print('RESULT:' + json.dumps({'success': False, 'error': 'Create asset failed'}))
except Exception as e:
  print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
      `.trim()
    },
    SET_VIEWPORT_CAMERA: {
      name: 'set_viewport_camera',
      script: `
import unreal, json
try:
  location = unreal.Vector({x}, {y}, {z})
  rotation = unreal.Rotator({pitch}, {yaw}, {roll})
  ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
  if ues:
    ues.set_level_viewport_camera_info(location, rotation)
    print('RESULT:' + json.dumps({'success': True}))
  else:
    print('RESULT:' + json.dumps({'success': False, 'error': 'UnrealEditorSubsystem not available'}))
except Exception as e:
  print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
      `.trim()
    },
    BUILD_LIGHTING: {
      name: 'build_lighting',
      script: `
import unreal, json
try:
  les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
  if les:
    les.build_visible_level_lighting()
    print('RESULT:' + json.dumps({'success': True, 'message': 'Build requested'}))
  else:
    print('RESULT:' + json.dumps({'success': False, 'error': 'LevelEditorSubsystem not available'}))
except Exception as e:
  print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
      `.trim()
    },
    SAVE_ALL_DIRTY_PACKAGES: {
      name: 'save_dirty_packages',
      script: `
import unreal, json
try:
  saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
  print('RESULT:' + json.dumps({'success': True, 'saved': bool(saved)}))
except Exception as e:
  print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
      `.trim()
    }
  };

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
  private parsePythonJsonResult<T = any>(raw: any): T | null {
    if (!raw) {
      return null;
    }

    const fragments: string[] = [];

    if (typeof raw === 'string') {
      fragments.push(raw);
    }

    if (typeof raw?.Output === 'string') {
      fragments.push(raw.Output);
    }

    if (typeof raw?.ReturnValue === 'string') {
      fragments.push(raw.ReturnValue);
    }

    if (Array.isArray(raw?.LogOutput)) {
      for (const entry of raw.LogOutput) {
        if (!entry) continue;
        if (typeof entry === 'string') {
          fragments.push(entry);
        } else if (typeof entry?.Output === 'string') {
          fragments.push(entry.Output);
        }
      }
    }

    const combined = fragments.join('\n');
    const match = combined.match(/RESULT:(\{.*\}|\[.*\])/s);
    if (!match) {
      return null;
    }

    try {
      return JSON.parse(match[1]);
    } catch {
      return null;
    }
  }

  async ensurePluginsEnabled(pluginNames: string[], context?: string): Promise<string[]> {
    if (!pluginNames || pluginNames.length === 0) return [];

    const now = Date.now();
    const pluginsToCheck = pluginNames.filter((name) => {
      const cached = this.pluginStatusCache.get(name);
      if (!cached) return true;
      if (now - cached.timestamp > this.PLUGIN_CACHE_TTL) {
        this.pluginStatusCache.delete(name);
        return true;
      }
      return false;
    });

    if (pluginsToCheck.length > 0) {
      const python = `
import unreal
import json

plugins = ${JSON.stringify(pluginsToCheck)}
status = {}

def is_plugin_enabled(plugin_name):
  try:
    pm = unreal.PluginManager.get() if hasattr(unreal, 'PluginManager') else None
    if pm and hasattr(pm, 'is_plugin_enabled') and callable(pm.is_plugin_enabled):
      return pm.is_plugin_enabled(plugin_name)
  except Exception:
    pass
  try:
    lib = unreal.PluginBlueprintLibrary
    if lib and hasattr(lib, 'get_enabled_plugin_names') and callable(lib.get_enabled_plugin_names):
      names = lib.get_enabled_plugin_names()
      return plugin_name in names
  except Exception:
    pass
  try:
    ps = unreal.get_editor_subsystem(unreal.PluginsEditorSubsystem)
    if ps and hasattr(ps, 'is_plugin_enabled') and callable(ps.is_plugin_enabled):
      return ps.is_plugin_enabled(plugin_name)
  except Exception:
    pass
  return False

for p in plugins:
  try:
    status[p] = bool(is_plugin_enabled(p))
  except Exception:
    status[p] = False

print('RESULT:' + json.dumps(status))
`.trim();

      try {
        const response = await this.executePython(python);
        const parsed = this.parsePythonJsonResult<Record<string, boolean>>(response);
        if (parsed) {
          for (const [name, enabled] of Object.entries(parsed)) {
            this.pluginStatusCache.set(name, { enabled: Boolean(enabled), timestamp: now });
          }
        } else {
          this.log.warn('Failed to parse plugin status response', { context, pluginsToCheck });
        }
      } catch (_err) {
        this.log.debug('Plugin status probe failed:', (_err as Error)?.message ?? _err);
      }
    }

    const missing = pluginNames.filter((name) => !this.pluginStatusCache.get(name)?.enabled);
    if (missing.length && context) {
      this.log.warn(`Missing required Unreal plugins for ${context}: ${missing.join(', ')}`);
    }
    return missing;
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
    } else if (command.startsWith('stat') || command.startsWith('show')) {
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
      // First attempt to execute via the plugin's native handler if the
      // automation bridge is available. This avoids executing arbitrary
      // Python inside the editor when the plugin can handle simple
      // console commands natively.
      if (this.automationBridge && this.automationBridge.isConnected()) {
        try {
          const pluginResp: any = await this.automationBridge.sendAutomationRequest('execute_console_command', { command: cmdTrimmed }, { timeoutMs: 30000 });
          if (pluginResp && pluginResp.success) {
            // Plugin handled the command; return the plugin result shape.
            return { ...(pluginResp as any), transport: 'automation_bridge' };
          }
          // If plugin explicitly reports NOT_IMPLEMENTED, only fall back
          // to Python when caller allowed it. Otherwise surface plugin's
          // error to the caller so they can diagnose missing plugin
          // features.
          if (pluginResp && pluginResp.error === 'NOT_IMPLEMENTED') {
            if (!allowPython) {
              throw new Error('Plugin does not implement console commands and Python fallback is disabled');
            }
            // else fall through to python fallback
          } else {
            // Plugin returned an error; prefer plugin's error unless
            // Python fallback allowed and plugin failed transiently.
            if (!allowPython) {
              const errMsg = pluginResp?.message || pluginResp?.error || 'Plugin execution failed';
              throw new Error(errMsg);
            }
            // otherwise continue to python fallback
          }
        } catch (err) {
          const errMsg = String((err as Error)?.message ?? String(err)).toLowerCase();
          // Deterministic failures (e.g. command not executed) are not
          // actionable by retries — log them at debug level to avoid
          // excessive warnings in normal test runs.
          if (errMsg.includes('command not executed') || errMsg.includes('exec_failed') || errMsg.includes('not implemented')) {
            this.log.debug('Plugin execute_console_command failed (non-retryable):', errMsg, err);
          } else {
            this.log.warn('Plugin execute_console_command failed; falling back to python if allowed', err);
          }
          if (!allowPython) {
            throw err;
          }
          // else continue to python fallback
        }
      }

      const escaped = escapePythonString(cmdTrimmed);
      const automationScript = `
import unreal
import json

command = "${escaped}"
result = {
    'success': False,
    'command': command
}

try:
    output = unreal.SystemLibrary.execute_console_command(None, command, None)
    result['success'] = True
    if output is not None:
        try:
            result['output'] = str(output)
        except Exception:
            result['output'] = ''
except Exception as exc:
    result['error'] = str(exc)

print('RESULT:' + json.dumps(result))
      `.trim();

      const automationResult = await this.executePythonWithResult(automationScript);
      if (automationResult && typeof automationResult === 'object') {
        const success = (automationResult as any).success !== false;
        const enriched = {
          ...(automationResult as Record<string, unknown>),
          success,
          command: cmdTrimmed,
          transport: 'automation_bridge' as const
        };

        if (success) {
          if (!Array.isArray((enriched as any).logLines) && typeof (enriched as any).output === 'string') {
            (enriched as any).logLines = [(enriched as any).output];
          }
          return enriched;
        }

        throw new Error((automationResult as any).error || (automationResult as any).message || 'Automation bridge console command failed');
      }

      if (automationResult !== undefined) {
        return {
          success: true,
          command: cmdTrimmed,
          result: automationResult,
          transport: 'automation_bridge' as const
        };
      }

      throw new Error('Automation bridge returned no response for console command');
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
    commands: Iterable<string | { command: string; priority?: number; allowPython?: boolean }>,
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
        const result = await this.executeConsoleCommand(command, {
          allowPython: Boolean(descriptor.allowPython)
        });
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

  // Execute a Python command via the Automation Bridge
  async executePython(command: string, timeoutMs?: number): Promise<any> {
    const trimmedCommand = command.trim();

    // By default, executing arbitrary Python via the automation bridge is
    // disabled to encourage use of native automation actions implemented
    // by the editor plugin. Set MCP_ALLOW_PYTHON_FALLBACKS=1 to opt in
    // for legacy Python fallbacks (deprecated).
    const allowEnv = allowPythonFallbackFromEnv();
    if (!allowEnv) {
      throw new Error('Editor Python execution via automation_bridge is disabled by default. Set MCP_ALLOW_PYTHON_FALLBACKS=1 to opt in (deprecated).');
    }

    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      throw new Error('Automation bridge not connected');
    }

    // Allow controlling Python execution timeout via MCP_AUTOMATION_PYTHON_TIMEOUT_MS
    // Fall back to the general automation request timeout or 120s to accommodate
    // long-running Editor operations (blueprint creation, compilation, etc.).
    const timeoutEnv = process.env.MCP_AUTOMATION_PYTHON_TIMEOUT_MS ?? process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;
    const requestedTimeout = typeof timeoutMs === 'number' && timeoutMs > 0 ? Number(timeoutMs) : (timeoutEnv ? Number(timeoutEnv) : Number.NaN);
    const finalTimeoutMs = Number.isFinite(requestedTimeout) && requestedTimeout > 0
      ? requestedTimeout
      : 120000;

    const response = await this.automationBridge.sendAutomationRequest(
      'execute_editor_python',
      { script: trimmedCommand },
      { timeoutMs: Math.max(1000, finalTimeoutMs) }
    );

    if (response.success === false) {
      throw new Error(response.error || response.message || 'Automation bridge Python execution failed');
    }

    if (response.result !== undefined) {
      return response.result;
    }

    return response.message ?? response;
  }
  
  // Allow callers to enable/disable auto-reconnect behavior
  setAutoReconnectEnabled(enabled: boolean): void {
    this.autoReconnectEnabled = enabled;
  }

  // Graceful shutdown
  async disconnect(): Promise<void> {
    this.connected = false;
  }
  
  /**
   * Enhanced Editor Function Access
   * Use Python scripting as a bridge to access modern Editor Subsystem functions
   */
  async executeEditorFunction(functionName: string, params?: Record<string, any>, options?: { allowPythonFallback?: boolean }): Promise<any> {
    const template = this.PYTHON_TEMPLATES[functionName];
  const allowPythonFallback = options?.allowPythonFallback ?? allowPythonFallbackFromEnv();

    // First, attempt to invoke a native plugin handler for this well-known
    // function. The plugin exposes a lightweight 'execute_editor_function'
    // action that implements common templates natively. If the plugin does
    // not implement the action, decide whether to fall back to the Python
    // template (deprecated) based on allowPythonFallback and whether a
    // template exists for this function.
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const resp: any = await this.automationBridge.sendAutomationRequest('execute_editor_function', {
          functionName,
          params: params ?? {}
        });
        if (resp && resp.success !== false) {
          return resp.result ?? resp;
        }
        const errTxt = String(resp?.error ?? resp?.message ?? '');
        if (!(errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION'))) {
          // Plugin explicitly returned an error for this known function
          return resp;
        }
        // Unknown plugin action -> conditionally fall through to Python fallback
        if (!allowPythonFallback || !template) {
          // Return structured failure indicating plugin does not implement this action
          return { success: false, error: 'UNKNOWN_PLUGIN_ACTION' };
        }
      } catch (err) {
        // If the plugin call threw due to connection or other reasons, decide
        // whether to fall back to Python or surface bridge-unavailable.
        if (!allowPythonFallback || !template) {
          this.log.debug(`executeEditorFunction plugin call failed for ${functionName}, automation bridge unavailable:`, (err as Error)?.message ?? err);
          return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE' };
        }
        this.log.debug(`executeEditorFunction plugin call failed for ${functionName}, falling back to Python as configured:`, (err as Error)?.message ?? err);
      }
    } else {
      if (!allowPythonFallback || !template) {
        return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE' };
      }
    }

    // Fallback: execute the Python template (deprecated). Replace params
    // placeholders in the template and execute using executePythonWithResult.
    if (!template) {
      return { success: false, error: 'UNKNOWN_EDITOR_FUNCTION_AND_NO_TEMPLATE' };
    }
    let script = template.script;
    if (params) {
      for (const [key, value] of Object.entries(params)) {
        const placeholder = `{${key}}`;
        script = script.replace(new RegExp(placeholder, 'g'), String(value));
      }
    }
    try {
      const result = await this.executePythonWithResult(script);
      return result;
    } catch (error) {
      this.log.error(`Failed to execute editor function ${functionName}:`, error);
      throw error;
    }
  }

  /**
   * Execute an arbitrary Python script via the plugin if available. This
   * centralizes raw Python execution so callsites do not directly invoke
   * executePythonWithResult. The plugin may accept, reject, or map the
   * script; when rejected we optionally fall back to the raw executor.
   */
  public async executeEditorPython(script: string, options?: { allowPythonFallback?: boolean; timeoutMs?: number }): Promise<any> {
    // Centralized mapping: map common editor Python templates to native
    // plugin handlers. Raw Python execution has been removed from the
    // plugin; callers must use executeEditorFunction for supported
    // operations. This method attempts to detect well-known templates
    // and invoke the corresponding native handler.
    const map = this.mapScriptToEditorFunction(script);
    if (map) {
      try {
        // If mapping designates a direct plugin action, call it via
        // automationBridge.sendAutomationRequest so plugin-level actions
        // (e.g. blueprint_probe_subobject_handle, import_asset_deferred)
        // can be invoked without relying on the editor-function shim.
        if ('type' in map && (map as any).type === 'plugin' && typeof (this.automationBridge as any)?.sendAutomationRequest === 'function') {
          try {
            const resp: any = await (this.automationBridge as any).sendAutomationRequest((map as any).actionName ?? (map as any).functionName ?? '', map.params ?? {}, options?.timeoutMs ? { timeoutMs: options.timeoutMs } : undefined);
            return resp && resp.success !== false ? (resp.result ?? resp) : resp;
          } catch (err) {
            this.log.debug(`executeEditorPython -> plugin-action mapping for ${(map as any).actionName ?? (map as any).functionName} failed:`, (err as Error)?.message ?? err);
            return { success: false, error: 'MAPPED_PLUGIN_ACTION_FAILED', message: String((err as Error)?.message ?? err) };
          }
        }

        // Default to the editor-function path
        return await this.executeEditorFunction((map as any).functionName ?? (map as any).actionName ?? '', map.params ?? {}, { allowPythonFallback: options?.allowPythonFallback });
      } catch (err) {
        this.log.debug(`executeEditorPython -> executeEditorFunction mapping for ${(map as any).functionName} failed:`, (err as Error)?.message ?? err);
        return { success: false, error: 'MAPPED_FUNCTION_FAILED', message: String((err as Error)?.message ?? err) };
      }
    }

    // No mapping found — raw Python execution is no longer supported.
    return {
      success: false,
      error: 'PYTHON_FALLBACK_REMOVED',
      message: 'Direct execution of arbitrary Python has been disabled. Convert the call to executeEditorFunction or implement a native handler in the plugin.'
    };
  }

  /**
   * Attempt to map an ad-hoc Python script to a known editor function
   * so the server can call the plugin-native handler instead of sending
   * raw Python. Returns null when no mapping exists.
   */
  private mapScriptToEditorFunction(script: string): { functionName: string; params?: Record<string, any> } | { type: 'plugin'; actionName: string; params?: Record<string, any> } | null {
    if (!script || typeof script !== 'string') return null;
    const lower = script.toLowerCase();

    // GET_ALL_ACTORS
    if (lower.includes('get_all_level_actors')) {
      return { functionName: 'GET_ALL_ACTORS' };
    }

    // ASSET REGISTRY DIRECTORY LISTING -> map to plugin-native 'list' action
    if (lower.includes('get_assets_by_path') && lower.includes('get_sub_paths')) {
      // Try to extract the directory literal (pattern used by templates)
      const dirMatch = script.match(/_dir\s*=\s*r?['"]([^'"]+)['"]/i);
      const limitMatch = script.match(/assets_data\s*\[:\s*(\d+)\]/i);
      const dir = dirMatch ? dirMatch[1] : '/Game';
      const params: any = { directory: dir };
      if (limitMatch && limitMatch[1]) params.limit = parseInt(limitMatch[1], 10);
      return { type: 'plugin', actionName: 'list', params };
    }

    // BUILD_LIGHTING
    if (lower.includes('build_light_maps') || lower.includes('buildvisiblelevellighting') || lower.includes('buildlighting')) {
      const match = script.match(/LightingBuildQuality\.([A-Za-z0-9_]+)/i);
      let quality = 'High';
      if (match && match[1]) {
        const token = match[1].toUpperCase();
        const map: Record<string, string> = { 'QUALITY_PREVIEW': 'Preview', 'QUALITY_MEDIUM': 'Medium', 'QUALITY_HIGH': 'High', 'QUALITY_PRODUCTION': 'Production' };
        quality = map[token] ?? token;
      }
      const withCaptures = /buildreflectioncaptures\s*=\s*True|buildreflectioncaptures\s*\)|buildreflectioncaptures\s*,\s*True/i.test(script);
      return { functionName: 'BUILD_LIGHTING', params: { quality, buildReflectionCaptures: Boolean(withCaptures) } };
    }

    // ASSET_EXISTS
    if (lower.includes('editorassetlibrary.does_asset_exist') || lower.includes('does_asset_exist(')) {
      const m = script.match(/['"]([^'"]+)['"]/);
      const path = m ? m[1] : '';
      return { functionName: 'ASSET_EXISTS', params: { path } };
    }

    // SET_VIEWPORT_CAMERA
    if (lower.includes('set_level_viewport_camera_info') || lower.includes('set_level_viewport_camera')) {
      const vec = script.match(/unreal\.Vector\s*\(\s*([^)]+)\)/i);
      const rot = script.match(/unreal\.Rotator\s*\(\s*([^)]+)\)/i);
      const parseTriple = (txt?: string) => {
        if (!txt) return [0, 0, 0];
        return txt.split(',').map(p => Number(p.trim() || 0)).slice(0, 3).map(n => Number.isFinite(n) ? n : 0);
      };
      const [x, y, z] = parseTriple(vec?.[1]);
      const [pitch, yaw, roll] = parseTriple(rot?.[1]);
      return { functionName: 'SET_VIEWPORT_CAMERA', params: { x, y, z, pitch, yaw, roll } };
    }

    // SPAWN_ACTOR (try to extract class path or class name + location)
    if (lower.includes('spawn_actor_from_class') || lower.includes('spawn_actor(')) {
      // Prefer quoted asset/class paths
      let classPath = '';
      const loadMatch = script.match(/load_asset\s*\(\s*r?['"]([^'"]+)['"]\s*\)/i);
      if (loadMatch && loadMatch[1]) classPath = loadMatch[1];
      // Look for `= unreal.ClassName` assignments
      if (!classPath) {
        const assignMatch = script.match(/=\s*unreal\.([A-Za-z0-9_]+)/i);
        if (assignMatch && assignMatch[1]) classPath = `/Script/Engine.${assignMatch[1]}`;
      }
      // Parse first Vector occurrence for location
      const vec = script.match(/unreal\.Vector\s*\(\s*([^)]+)\)/i);
      const parseTriple = (txt?: string) => {
        if (!txt) return [0, 0, 0];
        return txt.split(',').map(p => Number(p.trim() || 0)).slice(0, 3).map(n => Number.isFinite(n) ? n : 0);
      };
      const [x, y, z] = parseTriple(vec?.[1]);
      return { functionName: 'SPAWN_ACTOR_AT_LOCATION', params: { classPath, x, y, z } };
    }

    // SAVE_ALL_DIRTY_PACKAGES
    if (lower.includes('save_dirty_packages') || lower.includes('save_dirty_packages(')) {
      return { functionName: 'SAVE_ALL_DIRTY_PACKAGES' };
    }

    // CREATE_ASSET heuristics
    if (script.includes('asset_tools.create_asset') || script.includes('get_asset_tools()')) {
      const m = script.match(/create_asset\s*\(\s*['"]([^'"]+)['"]\s*,\s*['"]([^'"]+)['"]/i);
      const assetName = m ? m[1] : undefined;
      const packagePath = m ? m[2] : undefined;
      return { functionName: 'CREATE_ASSET', params: { asset_name: assetName, package_path: packagePath } };
    }

    // IMPORT ASSET
    if (script.includes('AssetImportTask') || script.includes('import_asset_tasks') || script.includes('import_asset')) {
      // Best-effort: extract source/destination paths
      const srcMatch = script.match(/task\.filename\s*=\s*r?["']([^"']+)["']/i) || script.match(/['"]sourcePath['"]\s*:\s*['"]([^'"]+)['"]/i);
      const dstMatch = script.match(/task\.destination_path\s*=\s*r?["']([^"']+)["']/i) || script.match(/['"]destinationPath['"]\s*:\s*['"]([^'"]+)['"]/i);
      return { type: 'plugin', actionName: 'import_asset_deferred', params: { sourcePath: srcMatch ? srcMatch[1] : undefined, destinationPath: dstMatch ? dstMatch[1] : undefined } };
    }

    // DUPLICATE / DELETE ASSET
    if (script.includes('duplicate_asset') || script.includes('duplicate_asset(') || script.includes('duplicate_asset(')) {
      const src = (script.match(/duplicate_asset\s*\(\s*r?["']([^"']+)["']\s*,\s*r?["']([^"']+)["']/i) || [])[1];
      const dst = (script.match(/duplicate_asset\s*\(\s*r?["']([^"']+)["']\s*,\s*r?["']([^"']+)["']/i) || [])[2];
      return { type: 'plugin', actionName: 'duplicate_asset', params: { sourcePath: src, destinationPath: dst } };
    }

    if (script.includes('delete_asset') || script.includes('delete_loaded_asset') || script.includes('delete_asset(')) {
      const pm = script.match(/delete_asset\s*\(\s*r?["']([^"']+)["']\s*\)/i) || script.match(/delete_loaded_asset\s*\(\s*r?["']([^"']+)["']\s*\)/i) || [];
      const path = pm[1] || undefined;
      return { type: 'plugin', actionName: 'delete_asset', params: { path } };
    }

    // MATERIAL creation heuristics
    if (script.includes('MaterialFactoryNew') || script.includes('create_material') || script.includes('create_material_instance')) {
      if (script.includes('MaterialInstanceConstantFactoryNew') || script.includes('create_material_instance')) {
        const nameMatch = script.match(/asset_tools\.create_asset\(\s*asset_name\s*=\s*['"]([^'"]+)['"]/i) || script.match(/name\s*=\s*r?["']([^"']+)["']/i);
        const destMatch = script.match(/package_path\s*=\s*r?["']([^"']+)["']/i) || script.match(/dest\s*=\s*r?["']([^"']+)["']/i);
        return { functionName: 'CREATE_MATERIAL_INSTANCE', params: { name: nameMatch ? nameMatch[1] : undefined, package_path: destMatch ? destMatch[1] : undefined } };
      }
      const nameMatch = script.match(/asset_tools\.create_asset\(\s*asset_name\s*=\s*['"]([^'"]+)['"]/i) || script.match(/name\s*=\s*r?["']([^"']+)["']/i);
      const destMatch = script.match(/package_path\s*=\s*r?["']([^"']+)["']/i) || script.match(/dest\s*=\s*r?["']([^"']+)["']/i);
      return { functionName: 'CREATE_MATERIAL', params: { name: nameMatch ? nameMatch[1] : undefined, destinationPath: destMatch ? destMatch[1] : undefined } };
    }

    // ANIMATION BLUEPRINT creation heuristics
    if (script.includes('AnimBlueprintFactory') || (script.includes('create_asset') && script.includes('AnimBlueprint'))) {
      const nameMatch = script.match(/asset_tools\.create_asset\(\s*asset_name\s*=\s*['"]([^'"]+)['"]/i) || script.match(/name\s*=\s*r?["']([^"']+)["']/i);
      const destMatch = script.match(/package_path\s*=\s*r?["']([^"']+)["']/i) || script.match(/path\s*=\s*r?["']([^"']+)["']/i);
      const skeletonMatch = script.match(/target_skeleton\s*=\s*[^,\n]+/i) || script.match(/skeletonPath['"]\s*:\s*['"]([^'"]+)['"]/i);
      return { functionName: 'CREATE_ANIMATION_BLUEPRINT', params: { name: nameMatch ? nameMatch[1] : undefined, package_path: destMatch ? destMatch[1] : undefined, skeleton_path: skeletonMatch ? skeletonMatch[1] : undefined } };
    }

    // REMOTE CONTROL presets and expose patterns
    if (script.includes('RemoteControlPresetFactory') || script.includes('get_remote_control') || script.includes('RemoteControlFunctionLibrary')) {
      if (script.includes('expose_property') || script.includes('expose_actor')) {
        return { functionName: 'RC_EXPOSE_PROPERTY', params: {} };
      }
      if (script.includes('list_assets') && script.includes('RemoteControlPreset')) {
        return { functionName: 'RC_LIST_PRESETS' };
      }
      if (script.includes('create_asset') && script.includes('RemoteControlPreset')) {
        return { functionName: 'RC_CREATE_PRESET' };
      }
    }

    // Resolve parent class / blueprint parent resolution probes
    if (script.includes('resolve_parent') || script.includes('parentSpec') || script.includes('parentClass')) {
      return { type: 'plugin', actionName: 'blueprint_resolve_parent', params: {} };
    }

    // Blueprint add component / modify_scs patterns
    if (script.includes('add_component_to_blueprint') || script.includes('add_component(') || script.includes('blueprint_modify_scs')) {
      return { functionName: 'ADD_COMPONENT_TO_BLUEPRINT', params: {} };
    }

    return null;
  }

  /**
   * Execute Python script and parse the result
   */
  // Expose for internal consumers (resources) that want parsed RESULT blocks
  public async executePythonWithResult(script: string, timeoutMs?: number): Promise<any> {
    try {
      // Wrap script to capture output so we can parse RESULT: lines reliably
      const wrappedScript = `
import sys
import io
old_stdout = sys.stdout
sys.stdout = buffer = io.StringIO()
try:
    ${script.split('\n').join('\n    ')}
finally:
    output = buffer.getvalue()
    sys.stdout = old_stdout
    if output:
        print(output)
      `.trim()
        .replace(/\r?\n/g, '\n');

  const response = await this.executePython(wrappedScript, timeoutMs);

      // Extract textual output from various response shapes
      let out = '';
      try {
        if (response && typeof response === 'string') {
          out = response;
        } else if (response && typeof response === 'object') {
          if (Array.isArray((response as any).LogOutput)) {
            out = (response as any).LogOutput.map((l: any) => l.Output || '').join('');
          } else if (typeof (response as any).Output === 'string') {
            out = (response as any).Output;
          } else if (typeof (response as any).result === 'string') {
            out = (response as any).result;
          } else {
            out = JSON.stringify(response);
          }
        }
      } catch {
        out = String(response || '');
      }

      // Robust RESULT parsing with bracket matching (handles nested objects)
      const marker = 'RESULT:';
      const idx = out.lastIndexOf(marker);
      if (idx !== -1) {
        // Find first '{' after the marker
        let i = idx + marker.length;
        while (i < out.length && out[i] !== '{') i++;
        if (i < out.length && out[i] === '{') {
          let depth = 0;
          let inStr = false;
          let esc = false;
          let j = i;
          for (; j < out.length; j++) {
            const ch = out[j];
            if (esc) { esc = false; continue; }
            if (ch === '\\') { esc = true; continue; }
            if (ch === '"') { inStr = !inStr; continue; }
            if (!inStr) {
              if (ch === '{') depth++;
              else if (ch === '}') { depth--; if (depth === 0) { j++; break; } }
            }
          }
          const jsonStr = out.slice(i, j);
          try { return JSON.parse(jsonStr); } catch {}
        }
      }

  // Try previous regex approach (best-effort)
      const matches = Array.from(out.matchAll(/RESULT:({[\s\S]*})/g));
      if (matches.length > 0) {
        const last = matches[matches.length - 1][1];
        try { return JSON.parse(last); } catch { return { raw: last }; }
      }

      // If no RESULT: marker, return the best-effort textual output or original response
      return typeof response !== 'undefined' ? response : out;
    } catch (err) {
  this.log.warn('Python execution failed and alternate mechanisms are disabled; propagating error', err);
      throw err;
    }
  }

  

  /**
   * Get the Unreal Engine version via Python and parse major/minor/patch.
   */
  async getEngineVersion(): Promise<{ version: string; major: number; minor: number; patch: number; isUE56OrAbove: boolean; }> {
    const now = Date.now();
    if (this.engineVersionCache && now - this.engineVersionCache.timestamp < this.ENGINE_VERSION_TTL_MS) {
      return this.engineVersionCache.value;
    }

    try {
      const script = `
import unreal, json, re
ver = str(unreal.SystemLibrary.get_engine_version())
m = re.match(r'^(\\d+)\\.(\\d+)\\.(\\d+)', ver)
major = int(m.group(1)) if m else 0
minor = int(m.group(2)) if m else 0
patch = int(m.group(3)) if m else 0
print('RESULT:' + json.dumps({'version': ver, 'major': major, 'minor': minor, 'patch': patch}))
      `.trim();
      const result = await this.executePythonWithResult(script);
      const version = String(result?.version ?? 'unknown');
      const major = Number(result?.major ?? 0) || 0;
      const minor = Number(result?.minor ?? 0) || 0;
      const patch = Number(result?.patch ?? 0) || 0;
      const isUE56OrAbove = major > 5 || (major === 5 && minor >= 6);
      const value = { version, major, minor, patch, isUE56OrAbove };
      this.engineVersionCache = { value, timestamp: now };
      return value;
    } catch (error) {
      this.log.warn('Failed to get engine version via Python', error);
  const defaultVersion = { version: 'unknown', major: 0, minor: 0, patch: 0, isUE56OrAbove: false };
  this.engineVersionCache = { value: defaultVersion, timestamp: now };
  return defaultVersion;
    }
  }

  /**
   * Query feature flags (Python availability, editor subsystems) via Python.
   */
  async getFeatureFlags(): Promise<{ pythonEnabled: boolean; subsystems: { unrealEditor: boolean; levelEditor: boolean; editorActor: boolean; } }> {
    try {
      const script = `
import unreal, json
flags = {}
# Python plugin availability (class exists)
try:
    _ = unreal.PythonScriptLibrary
    flags['pythonEnabled'] = True
except Exception:
    flags['pythonEnabled'] = False
# Editor subsystems
try:
    flags['unrealEditor'] = bool(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem))
except Exception:
    flags['unrealEditor'] = False
try:
    flags['levelEditor'] = bool(unreal.get_editor_subsystem(unreal.LevelEditorSubsystem))
except Exception:
    flags['levelEditor'] = False
try:
    flags['editorActor'] = bool(unreal.get_editor_subsystem(unreal.EditorActorSubsystem))
except Exception:
    flags['editorActor'] = False
print('RESULT:' + json.dumps(flags))
      `.trim();
      const res = await this.executePythonWithResult(script);
      return {
        pythonEnabled: Boolean(res?.pythonEnabled),
        subsystems: {
          unrealEditor: Boolean(res?.unrealEditor),
          levelEditor: Boolean(res?.levelEditor),
          editorActor: Boolean(res?.editorActor)
        }
      };
    } catch (e) {
      this.log.warn('Failed to get feature flags via Python', e);
      return { pythonEnabled: false, subsystems: { unrealEditor: false, levelEditor: false, editorActor: false } };
    }
  }

  /**
   * Check whether an asset exists in the editor (plugin-first, Python
   * fallback only when explicitly permitted). Returns true when the asset
   * exists, false otherwise.
   */
  async assetExists(assetPath: string): Promise<boolean> {
    if (!assetPath || typeof assetPath !== 'string') return false;

    try {
  const allowPythonFallback = allowPythonFallbackFromEnv();
      const result = await this.executeEditorFunction('ASSET_EXISTS', { path: assetPath }, { allowPythonFallback });
      if (result && typeof result === 'object') {
        if (typeof result.exists === 'boolean') return result.exists;
        if (typeof (result as any).result === 'object' && typeof (result as any).result.exists === 'boolean') return (result as any).result.exists;
      }
      if (typeof result === 'boolean') return result;
    } catch (err) {
      this.log.debug('assetExists plugin/editor function call failed or was rejected:', (err as Error)?.message ?? err);
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
      'HealthCheck': 'echo MCP Server Health Check',
      
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
