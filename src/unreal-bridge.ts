import WebSocket from 'ws';
import { createHttpClient } from './utils/http.js';
import { Logger } from './utils/logger.js';
import { loadEnv } from './types/env.js';
import { ErrorHandler } from './utils/error-handler.js';

// RcMessage interface reserved for future WebSocket message handling
// interface RcMessage {
//   MessageName: string;
//   Parameters?: any;
// }

interface RcCallBody {
  objectPath: string; // e.g. "/Script/UnrealEd.Default__EditorAssetLibrary"
  functionName: string; // e.g. "ListAssets"
  parameters?: Record<string, any>;
  generateTransaction?: boolean;
}

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
  private ws?: WebSocket;
  private http = createHttpClient('');
  private env = loadEnv();
  private log = new Logger('UnrealBridge');
  private connected = false;
  private reconnectTimer?: NodeJS.Timeout;
  private reconnectAttempts = 0;
  private readonly MAX_RECONNECT_ATTEMPTS = 5;
  private readonly BASE_RECONNECT_DELAY = 1000;
  private autoReconnectEnabled = false; // disabled by default to prevent looping retries
  private engineVersionCache?: { value: { version: string; major: number; minor: number; patch: number; isUE56OrAbove: boolean }; timestamp: number };
  private readonly ENGINE_VERSION_TTL_MS = 5 * 60 * 1000;
  
  // WebSocket health monitoring (best practice from WebSocket optimization guides)
  private lastPongReceived = 0;
  private pingInterval?: NodeJS.Timeout;
  private readonly PING_INTERVAL_MS = 30000; // 30 seconds
  private readonly PONG_TIMEOUT_MS = 10000; // 10 seconds
  
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
  
  // Python script templates for EditorLevelLibrary access
  private readonly PYTHON_TEMPLATES: Record<string, PythonScriptTemplate> = {
    GET_ALL_ACTORS: {
      name: 'get_all_actors',
      script: `
import unreal
import json

# Use EditorActorSubsystem instead of deprecated EditorLevelLibrary
try:
   subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
   if subsys:
       actors = subsys.get_all_level_actors()
       result = [{'name': a.get_name(), 'class': a.get_class().get_name(), 'path': a.get_path_name()} for a in actors]
       print(f"RESULT:{json.dumps(result)}")
   else:
       print("RESULT:[]")
except Exception as e:
   print(f"RESULT:{json.dumps({'error': str(e)})}")
      `.trim()
    },
    SPAWN_ACTOR_AT_LOCATION: {
      name: 'spawn_actor',
      script: `
import unreal
import json

location = unreal.Vector({x}, {y}, {z})
rotation = unreal.Rotator({pitch}, {yaw}, {roll})

try:
   # Use EditorActorSubsystem instead of deprecated EditorLevelLibrary
   subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
   if subsys:
       # Try to load asset class
       actor_class = unreal.EditorAssetLibrary.load_asset("{class_path}")
       if actor_class:
           spawned = subsys.spawn_actor_from_object(actor_class, location, rotation)
           if spawned:
               print(f"RESULT:{json.dumps({'success': True, 'actor': spawned.get_name(), 'location': [{x}, {y}, {z}]}})}")
           else:
               print(f"RESULT:{json.dumps({'success': False, 'error': 'Failed to spawn actor'})}")
       else:
           print(f"RESULT:{json.dumps({'success': False, 'error': 'Failed to load actor class: {class_path}'})}")
   else:
       print(f"RESULT:{json.dumps({'success': False, 'error': 'EditorActorSubsystem not available'})}")
except Exception as e:
   print(f"RESULT:{json.dumps({'success': False, 'error': str(e)})}")
      `.trim()
    },
    DELETE_ACTOR: {
      name: 'delete_actor',
      script: `
import unreal
import json

try:
   subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
   if subsys:
       actors = subsys.get_all_level_actors()
       found = False
       for actor in actors:
           if not actor:
               continue
           label = actor.get_actor_label()
           name = actor.get_name()
           if label == "{actor_name}" or name == "{actor_name}" or label.lower().startswith("{actor_name}".lower()+"_"):
               success = subsys.destroy_actor(actor)
               print(f"RESULT:{json.dumps({'success': success, 'deleted': label})}")
               found = True
               break
       if not found:
           print(f"RESULT:{json.dumps({'success': False, 'error': 'Actor not found: {actor_name}'})}")
   else:
       print(f"RESULT:{json.dumps({'success': False, 'error': 'EditorActorSubsystem not available'})}")
except Exception as e:
   print(f"RESULT:{json.dumps({'success': False, 'error': str(e)})}")
      `.trim()
    },
    CREATE_ASSET: {
      name: 'create_asset',
      script: `
import unreal
import json

try:
   asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
   if asset_tools:
       # Create factory based on asset type
       factory_class = getattr(unreal, '{factory_class}', None)
       asset_class = getattr(unreal, '{asset_class}', None)

       if factory_class and asset_class:
           factory = factory_class()
           # Clean up the path - remove trailing slashes and normalize
           package_path = "{package_path}".rstrip('/').replace('//', '/')

           # Ensure package path is valid (starts with /Game or /Engine)
           if not package_path.startswith('/Game') and not package_path.startswith('/Engine'):
               if not package_path.startswith('/'):
                   package_path = f"/Game/{package_path}"
               else:
                   package_path = f"/Game{package_path}"

           # Create full asset path for verification
           full_asset_path = f"{package_path}/{asset_name}" if package_path != "/Game" else f"/Game/{asset_name}"

           # Create the asset with cleaned path
           asset = asset_tools.create_asset("{asset_name}", package_path, asset_class, factory)
           if asset:
               # Save the asset
               saved = unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
               # Enhanced verification with retry logic
                asset_path = asset.get_path_name()
                verification_attempts = 0
                max_verification_attempts = 5
                asset_verified = False

                while verification_attempts < max_verification_attempts and not asset_verified:
                    verification_attempts += 1
                    # Wait a bit for the asset to be fully saved
                    import time
                    time.sleep(0.1)

                    # Check if asset exists
                    asset_exists = unreal.EditorAssetLibrary.does_asset_exist(asset_path)

                    if asset_exists:
                        asset_verified = True
                    elif verification_attempts < max_verification_attempts:
                        # Try to reload the asset registry
                        try:
                            unreal.AssetRegistryHelpers.get_asset_registry().scan_modified_asset_files([asset_path])
                        except:
                            pass

                if asset_verified:
                    print(f"RESULT:{json.dumps({'success': saved, 'path': asset_path, 'verified': True})}")
                else:
                    print(f"RESULT:{json.dumps({'success': saved, 'path': asset_path, 'warning': 'Asset created but verification pending'})}")
           else:
               print(f"RESULT:{json.dumps({'success': False, 'error': 'Failed to create asset'})}")
       else:
           print(f"RESULT:{json.dumps({'success': False, 'error': 'Invalid factory or asset class'})}")
   else:
       print(f"RESULT:{json.dumps({'success': False, 'error': 'AssetToolsHelpers not available'})}")
except Exception as e:
   print(f"RESULT:{json.dumps({'success': False, 'error': str(e)})}")
      `.trim()
    },
    SET_VIEWPORT_CAMERA: {
      name: 'set_viewport_camera',
      script: `
import unreal
import json

location = unreal.Vector({x}, {y}, {z})
rotation = unreal.Rotator({pitch}, {yaw}, {roll})

try:
   # Use UnrealEditorSubsystem for viewport operations (UE5.1+)
   ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
   les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

   if ues:
       ues.set_level_viewport_camera_info(location, rotation)
       try:
           if les:
               les.editor_invalidate_viewports()
       except Exception:
           pass
       print(f"RESULT:{json.dumps({'success': True, 'location': [{x}, {y}, {z}], 'rotation': [{pitch}, {yaw}, {roll}]}})}")
   else:
       print(f"RESULT:{json.dumps({'success': False, 'error': 'UnrealEditorSubsystem not available'})}")
except Exception as e:
   print(f"RESULT:{json.dumps({'success': False, 'error': str(e)})}")
      `.trim()
    },
    BUILD_LIGHTING: {
      name: 'build_lighting',
      script: `
import unreal
import json

try:
   les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
   if les:
       # Use UE 5.6 enhanced lighting quality settings
       quality_map = {
           'Preview': unreal.LightingBuildQuality.PREVIEW,
           'Medium': unreal.LightingBuildQuality.MEDIUM,
           'High': unreal.LightingBuildQuality.HIGH,
           'Production': unreal.LightingBuildQuality.PRODUCTION
       }
       q = quality_map.get('{quality}', unreal.LightingBuildQuality.PREVIEW)
       les.build_light_maps(q, True)
       print(f"RESULT:{json.dumps({'success': True, 'quality': '{quality}', 'method': 'LevelEditorSubsystem'})}")
   else:
       print(f"RESULT:{json.dumps({'success': False, 'error': 'LevelEditorSubsystem not available'})}")
except Exception as e:
   print(f"RESULT:{json.dumps({'success': False, 'error': str(e)})}")
      `.trim()
    },
    SAVE_ALL_DIRTY_PACKAGES: {
      name: 'save_dirty_packages',
      script: `
import unreal
import json

try:
   # Use UE 5.6 enhanced saving with better error handling
   saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
   print(f"RESULT:{json.dumps({'success': bool(saved), 'saved_count': saved if isinstance(saved, int) else 0, 'message': 'All dirty packages saved'})}")
except Exception as e:
   print(f"RESULT:{json.dumps({'success': False, 'error': str(e), 'message': 'Failed to save dirty packages'})}")
      `.trim()
    }
  };

  get isConnected() { return this.connected; }
  
  /**
   * Attempt to connect with exponential backoff retry strategy
   * Uses optimized retry pattern from TypeScript best practices
   * @param maxAttempts Maximum number of connection attempts
   * @param timeoutMs Timeout for each connection attempt in milliseconds
   * @param retryDelayMs Initial delay between retry attempts in milliseconds
   * @returns Promise that resolves to true if connected, false otherwise
   */
  private connectPromise?: Promise<void>;

  async tryConnect(maxAttempts: number = 3, timeoutMs: number = 5000, retryDelayMs: number = 2000): Promise<boolean> {
    if (this.connected) return true;

    if (this.connectPromise) {
      try {
        await this.connectPromise;
      } catch {
        // swallow, we'll return connected flag
      }
      return this.connected;
    }

    // Use ErrorHandler's retryWithBackoff for consistent retry behavior
    this.connectPromise = ErrorHandler.retryWithBackoff(
      () => this.connect(timeoutMs),
      {
        maxRetries: maxAttempts - 1,
        initialDelay: retryDelayMs,
        maxDelay: 10000,
        backoffMultiplier: 1.5,
        shouldRetry: (error) => {
          // Only retry on connection-related errors
          const msg = (error as Error)?.message?.toLowerCase() || '';
          return msg.includes('timeout') || msg.includes('connection') || msg.includes('econnrefused');
        }
      }
    ).then(() => {
      // Success
    }).catch((err) => {
      this.log.warn(`Connection failed after ${maxAttempts} attempts:`, err.message);
    });

    try {
      await this.connectPromise;
    } finally {
      this.connectPromise = undefined;
    }

    return this.connected;
  }

  async connect(timeoutMs: number = 5000): Promise<void> {
    // If already connected and socket is open, do nothing
    if (this.connected && this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.log.debug('connect() called but already connected; skipping');
      return;
    }

    const wsUrl = `ws://${this.env.UE_HOST}:${this.env.UE_RC_WS_PORT}`;
    const httpBase = `http://${this.env.UE_HOST}:${this.env.UE_RC_HTTP_PORT}`;
    this.http = createHttpClient(httpBase);

    this.log.debug(`Connecting to UE Remote Control: ${wsUrl}`);
    this.ws = new WebSocket(wsUrl);

    await new Promise<void>((resolve, reject) => {
      if (!this.ws) return reject(new Error('WS not created'));
      
      // Guard against double-resolution/rejection
      let settled = false;
      const safeResolve = () => { if (!settled) { settled = true; resolve(); } };
      const safeReject = (err: Error) => { if (!settled) { settled = true; reject(err); } };
      
      // Setup timeout
      const timeout = setTimeout(() => {
        this.log.warn(`Connection timeout after ${timeoutMs}ms`);
        if (this.ws) {
          try {
            // Attach a temporary error handler to avoid unhandled 'error' events on abort
            this.ws.on('error', () => {});
            // Prefer graceful close; terminate as a fallback
            if (this.ws.readyState === WebSocket.CONNECTING || this.ws.readyState === WebSocket.OPEN) {
              try { this.ws.close(); } catch {}
              try { this.ws.terminate(); } catch {}
            }
          } finally {
            try { this.ws.removeAllListeners(); } catch {}
            this.ws = undefined;
          }
        }
        safeReject(new Error('Connection timeout: Unreal Engine may not be running or Remote Control is not enabled'));
      }, timeoutMs);
      
      // Success handler
      const onOpen = () => {
        clearTimeout(timeout);
        this.connected = true;
        this.log.info('Connected to Unreal Remote Control');
        this.startCommandProcessor(); // Start command processor on connect
        safeResolve();
      };
      
      // Error handler
      const onError = (err: Error) => {
        clearTimeout(timeout);
        // Keep error logs concise to avoid stack spam when UE is not running
        this.log.debug(`WebSocket error during connect: ${(err && (err as any).code) || ''} ${err.message}`);
        if (this.ws) {
          try {
            // Attach a temporary error handler to avoid unhandled 'error' events while aborting
            this.ws.on('error', () => {});
            if (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING) {
              try { this.ws.close(); } catch {}
              try { this.ws.terminate(); } catch {}
            }
          } finally {
            try { this.ws.removeAllListeners(); } catch {}
            this.ws = undefined;
          }
        }
        safeReject(new Error(`Failed to connect: ${err.message}`));
      };
      
      // Close handler (if closed before open)
      const onClose = () => {
        if (!this.connected) {
          clearTimeout(timeout);
          safeReject(new Error('Connection closed before establishing'));
        } else {
          // Normal close after connection was established
          this.connected = false;
          this.ws = undefined;
          this.log.warn('WebSocket closed');
          if (this.autoReconnectEnabled) {
            this.scheduleReconnect();
          }
        }
      };
      
      // Message handler (currently best-effort logging)
      const onMessage = (raw: WebSocket.RawData) => {
        try {
          const msg = JSON.parse(String(raw));
          this.log.debug('WS message', msg);
        } catch (_e) {
          // Noise reduction: keep at debug and do nothing on parse errors
        }
      };
      
      // Attach listeners
      this.ws.once('open', onOpen);
      this.ws.once('error', onError);
      this.ws.on('close', onClose);
      this.ws.on('message', onMessage);
    });
  }


  async httpCall<T = any>(path: string, method: 'GET' | 'POST' | 'PUT' = 'POST', body?: any): Promise<T> {
    // Guard: if not connected, do not attempt HTTP
    if (!this.connected) {
      throw new Error('Not connected to Unreal Engine');
    }

    const url = path.startsWith('/') ? path : `/${path}`;
    const started = Date.now();
    
    // Fix Content-Length header issue - ensure body is properly handled
    let payload = body;
    if ((payload === undefined || payload === null) && method !== 'GET') {
      payload = {};
    }
    
    // Add timeout wrapper to prevent hanging - adjust based on operation type
    let CALL_TIMEOUT = 10000; // Default 10 seconds timeout
    const longRunningTimeout = 10 * 60 * 1000; // 10 minutes for heavy editor jobs

    // Use payload contents to detect long-running editor operations
    let payloadSignature = '';
    if (typeof payload === 'string') {
      payloadSignature = payload;
    } else if (payload && typeof payload === 'object') {
      try {
        payloadSignature = JSON.stringify(payload);
      } catch {
        payloadSignature = '';
      }
    }

    // Allow explicit override via meta property when provided
    let sanitizedPayload = payload;
    if (payload && typeof payload === 'object' && '__callTimeoutMs' in payload) {
      const overrideRaw = (payload as any).__callTimeoutMs;
      const overrideMs = typeof overrideRaw === 'number'
        ? overrideRaw
        : Number.parseInt(String(overrideRaw), 10);
      if (Number.isFinite(overrideMs) && overrideMs > 0) {
        CALL_TIMEOUT = Math.max(CALL_TIMEOUT, overrideMs);
      }
      sanitizedPayload = { ...(payload as any) };
      delete (sanitizedPayload as any).__callTimeoutMs;
    }

    // For heavy operations, use longer timeout based on URL or payload signature
    if (url.includes('build') || url.includes('create') || url.includes('asset')) {
      CALL_TIMEOUT = Math.max(CALL_TIMEOUT, 30000); // 30 seconds for heavy operations
    }
    if (url.includes('light') || url.includes('BuildLighting')) {
      CALL_TIMEOUT = Math.max(CALL_TIMEOUT, 60000); // Base 60 seconds for lighting builds
    }

    if (payloadSignature) {
      const longRunningPatterns = [
        /build_light_maps/i,
        /lightingbuildquality/i,
        /editorbuildlibrary/i,
        /buildlighting/i,
        /"command"\s*:\s*"buildlighting/i
      ];
      if (longRunningPatterns.some(pattern => pattern.test(payloadSignature))) {
        if (CALL_TIMEOUT < longRunningTimeout) {
          this.log.debug(`Detected long-running lighting operation, extending HTTP timeout to ${longRunningTimeout}ms`);
        }
        CALL_TIMEOUT = Math.max(CALL_TIMEOUT, longRunningTimeout);
      }
    }
    
    // CRITICAL: Intercept and block dangerous console commands at HTTP level
    if (url === '/remote/object/call' && (payload as any)?.functionName === 'ExecuteConsoleCommand') {
      const command = (payload as any)?.parameters?.Command;
      if (command && typeof command === 'string') {
        const cmdLower = command.trim().toLowerCase();
        
        // List of commands that cause crashes
        const crashCommands = [
          'buildpaths',           // Causes access violation 0x0000000000000060
          'rebuildnavigation',    // Can crash without nav system
          'buildhierarchicallod', // Can crash without proper setup
          'buildlandscapeinfo',   // Can crash without landscape
          'rebuildselectednavigation' // Nav-related crash
        ];
        
        // Check if this is a crash-inducing command
        if (crashCommands.some(dangerous => cmdLower === dangerous || cmdLower.startsWith(dangerous + ' '))) {
          this.log.warn(`BLOCKED dangerous command that causes crashes: ${command}`);
          // Return a safe error response instead of executing
          return {
            success: false,
            error: `Command '${command}' blocked: This command can cause Unreal Engine to crash. Use the Python API alternatives instead.`
          } as any;
        }
        
        // Also block other dangerous commands
        const dangerousPatterns = [
          'quit', 'exit', 'r.gpucrash', 'debug crash',
          'viewmode visualizebuffer' // These can crash in certain states
        ];
        
        if (dangerousPatterns.some(pattern => cmdLower.includes(pattern))) {
          this.log.warn(`BLOCKED potentially dangerous command: ${command}`);
          return {
            success: false,
            error: `Command '${command}' blocked for safety.`
          } as any;
        }
      }
    }
    
    // Retry logic with exponential backoff and timeout
    let lastError: any;
    for (let attempt = 0; attempt < 3; attempt++) {
      try {
        // For GET requests, send payload as query parameters (not in body)
        const config: any = { url, method, timeout: CALL_TIMEOUT };
        if (method === 'GET' && sanitizedPayload && typeof sanitizedPayload === 'object') {
          config.params = sanitizedPayload;
        } else if (sanitizedPayload !== undefined) {
          config.data = sanitizedPayload;
        }

        // Wrap with timeout promise to ensure we don't hang
  const requestPromise = this.http.request<T>(config);
  const resp = await new Promise<Awaited<typeof requestPromise>>((resolve, reject) => {
          const timer = setTimeout(() => {
            const err = new Error(`Request timeout after ${CALL_TIMEOUT}ms`);
            (err as any).code = 'UE_HTTP_TIMEOUT';
            reject(err);
          }, CALL_TIMEOUT);
          requestPromise.then(result => {
            clearTimeout(timer);
            resolve(result);
          }).catch(err => {
            clearTimeout(timer);
            reject(err);
          });
        });
        const ms = Date.now() - started;

        // Add connection health check for long-running requests
        if (ms > 5000) {
          this.log.debug(`[HTTP ${method}] ${url} -> ${ms}ms (long request)`);
        } else {
          this.log.debug(`[HTTP ${method}] ${url} -> ${ms}ms`);
        }

        return resp.data;
      } catch (error: any) {
        lastError = error;
        const delay = Math.min(1000 * Math.pow(2, attempt), 5000); // Exponential backoff with 5s max
        
        // Log timeout errors specifically
        if (error.message?.includes('timeout')) {
          this.log.debug(`HTTP request timed out (attempt ${attempt + 1}/3): ${url}`);
        }
        
        if (attempt < 2) {
          this.log.debug(`HTTP request failed (attempt ${attempt + 1}/3), retrying in ${delay}ms...`);
          await new Promise(resolve => setTimeout(resolve, delay));
          
          // If connection error, try to reconnect
          if (error.code === 'ECONNREFUSED' || error.code === 'ETIMEDOUT' || error.message?.includes('timeout')) {
            if (this.autoReconnectEnabled) {
              this.scheduleReconnect();
            }
          }
        }
      }
    }
    
    throw lastError;
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
    if (!pluginNames || pluginNames.length === 0) {
      return [];
    }

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

def get_plugin_manager():
  try:
    return unreal.PluginManager.get()
  except AttributeError:
    return None
  except Exception:
    return None

def get_plugins_subsystem():
  try:
    return unreal.get_editor_subsystem(unreal.PluginsEditorSubsystem)
  except AttributeError:
    pass
  except Exception:
    pass
  try:
    return unreal.PluginsSubsystem()
  except Exception:
    return None

pm = get_plugin_manager()
ps = get_plugins_subsystem()

def is_enabled(plugin_name):
  if pm:
    try:
      if pm.is_plugin_enabled(plugin_name):
        return True
    except Exception:
      try:
        plugin = pm.find_plugin(plugin_name)
        if plugin and plugin.is_enabled():
          return True
      except Exception:
        pass
  if ps:
    try:
      return bool(ps.is_plugin_enabled(plugin_name))
    except Exception:
      try:
        plugin = ps.find_plugin(plugin_name)
        if plugin and plugin.is_enabled():
          return True
      except Exception:
        pass
  return False

for plugin_name in plugins:
  enabled = False
  try:
    enabled = is_enabled(plugin_name)
  except Exception:
    enabled = False
  status[plugin_name] = bool(enabled)

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
      } catch (error) {
        this.log.warn('Plugin status check failed', { context, pluginsToCheck, error: (error as Error)?.message ?? error });
      }
    }

    for (const name of pluginNames) {
      if (!this.pluginStatusCache.has(name)) {
        this.pluginStatusCache.set(name, { enabled: false, timestamp: now });
      }
    }

    const missing = pluginNames.filter((name) => !this.pluginStatusCache.get(name)?.enabled);
    if (missing.length && context) {
      this.log.warn(`Missing required Unreal plugins for ${context}: ${missing.join(', ')}`);
    }
    return missing;
  }

  // Generic function call via Remote Control HTTP API
  async call(body: RcCallBody): Promise<any> {
    if (!this.connected) throw new Error('Not connected to Unreal Engine');
    // Using HTTP endpoint /remote/object/call
    const result = await this.httpCall<any>('/remote/object/call', 'PUT', {
      generateTransaction: false,
      ...body
    });
    return result;
  }

  async getExposed(): Promise<any> {
    if (!this.connected) throw new Error('Not connected to Unreal Engine');
    return this.httpCall('/remote/preset', 'GET');
  }

  // Execute a console command safely with validation and throttling
  async executeConsoleCommand(command: string, options: { allowPython?: boolean } = {}): Promise<any> {
    if (!this.connected) {
      throw new Error('Not connected to Unreal Engine');
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
    
    try {
      const result = await this.executeThrottledCommand(
        () => this.httpCall('/remote/object/call', 'PUT', {
          objectPath: '/Script/Engine.Default__KismetSystemLibrary',
          functionName: 'ExecuteConsoleCommand',
          parameters: {
            WorldContextObject: null,
            Command: cmdTrimmed,
            SpecificPlayer: null
          },
          generateTransaction: false
        }),
        priority
      );
      
      return result;
    } catch (error) {
      this.log.error(`Console command failed: ${cmdTrimmed}`, error);
      throw error;
    }
  }

  summarizeConsoleCommand(command: string, response: any) {
    const trimmedCommand = command.trim();
    const logLines = Array.isArray(response?.LogOutput)
      ? (response.LogOutput as any[]).map(entry => {
          if (entry === null || entry === undefined) {
            return '';
          }
          if (typeof entry === 'string') {
            return entry;
          }
          return typeof entry.Output === 'string' ? entry.Output : '';
        }).filter(Boolean)
      : [];

    let output = logLines.join('\n').trim();
    if (!output) {
      if (typeof response === 'string') {
        output = response.trim();
      } else if (response && typeof response === 'object') {
        if (typeof response.Output === 'string') {
          output = response.Output.trim();
        } else if ('result' in response && response.result !== undefined) {
          output = String(response.result).trim();
        } else if ('ReturnValue' in response && typeof response.ReturnValue === 'string') {
          output = response.ReturnValue.trim();
        }
      }
    }

    const returnValue = response && typeof response === 'object' && 'ReturnValue' in response
      ? (response as any).ReturnValue
      : undefined;

    return {
      command: trimmedCommand,
      output,
      logLines,
      returnValue,
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

  // Try to execute a Python command via the PythonScriptPlugin, fallback to `py` console command.
  async executePython(command: string): Promise<any> {
    if (!this.connected) {
      throw new Error('Not connected to Unreal Engine');
    }
    const isMultiLine = /[\r\n]/.test(command) || command.includes(';');
    try {
      // Use ExecutePythonCommandEx with appropriate mode based on content
      return await this.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/PythonScriptPlugin.Default__PythonScriptLibrary', 
        functionName: 'ExecutePythonCommandEx',
        parameters: {
          PythonCommand: command,
          ExecutionMode: isMultiLine ? 'ExecuteFile' : 'ExecuteStatement',
          FileExecutionScope: 'Private'
        },
        generateTransaction: false
      });
    } catch {
      try {
        // Fallback to ExecutePythonCommand (more tolerant for multi-line)
        return await this.httpCall('/remote/object/call', 'PUT', {
          objectPath: '/Script/PythonScriptPlugin.Default__PythonScriptLibrary',
          functionName: 'ExecutePythonCommand',
          parameters: {
            Command: command
          },
          generateTransaction: false
        });
      } catch {
        // Final fallback: execute via console py command
        this.log.warn('PythonScriptLibrary not available or failed, falling back to console `py` command');
        
        // For simple single-line commands
        if (!isMultiLine) {
          return await this.executeConsoleCommand(`py ${command}`, { allowPython: true });
        }
        
        // For multi-line scripts, try to execute as a block
        try {
          // Try executing as a single exec block
          // Properly escape the script for Python exec
          const escapedScript = command
            .replace(/\\/g, '\\\\')
            .replace(/"/g, '\\"')
            .replace(/\n/g, '\\n')
            .replace(/\r/g, '');
          return await this.executeConsoleCommand(`py exec("${escapedScript}")`, { allowPython: true });
        } catch {
          // If that fails, break into smaller chunks
          try {
            // First ensure unreal is imported
            await this.executeConsoleCommand('py import unreal');
            
            // For complex multi-line scripts, execute in logical chunks
            const commandWithoutImport = command.replace(/^\s*import\s+unreal\s*;?\s*/m, '');
            
            // Split by semicolons first, then by newlines
            const statements = commandWithoutImport
              .split(/[;\n]/)  
              .map(s => s.trim())
              .filter(s => s.length > 0 && !s.startsWith('#'));
            
            let result = null;
            for (const stmt of statements) {
              // Skip if statement is too long for console
              if (stmt.length > 200) {
                // Try to execute as a single exec block
                const miniScript = `exec("""${stmt.replace(/"/g, '\\"')}""")`;
                result = await this.executeConsoleCommand(`py ${miniScript}`, { allowPython: true });
              } else {
                result = await this.executeConsoleCommand(`py ${stmt}`, { allowPython: true });
              }
              // Small delay between commands
              await new Promise(resolve => setTimeout(resolve, 30));
            }
            
            return result;
          } catch {
            // Final fallback: execute line by line
            const lines = command.split('\n').filter(line => line.trim().length > 0);
            let result = null;
            
            for (const line of lines) {
              // Skip comments
              if (line.trim().startsWith('#')) {
                continue;
              }
              result = await this.executeConsoleCommand(`py ${line.trim()}`, { allowPython: true });
              // Small delay between commands to ensure execution order
              await new Promise(resolve => setTimeout(resolve, 50));
            }
            
            return result;
          }
        }
      }
    }
  }
  
  // Allow callers to enable/disable auto-reconnect behavior
  setAutoReconnectEnabled(enabled: boolean): void {
    this.autoReconnectEnabled = enabled;
  }

  // Connection recovery
  private scheduleReconnect(): void {
    if (!this.autoReconnectEnabled) {
      this.log.info('Auto-reconnect disabled; not scheduling reconnection');
      return;
    }
    if (this.reconnectTimer || this.connected) {
      return;
    }
    
    if (this.reconnectAttempts >= this.MAX_RECONNECT_ATTEMPTS) {
      this.log.error('Max reconnection attempts reached. Please check Unreal Engine.');
      return;
    }
    
    // Exponential backoff with jitter
    const delay = Math.min(
      this.BASE_RECONNECT_DELAY * Math.pow(2, this.reconnectAttempts) + Math.random() * 1000,
      30000 // Max 30 seconds
    );
    
    this.log.debug(`Scheduling reconnection attempt ${this.reconnectAttempts + 1}/${this.MAX_RECONNECT_ATTEMPTS} in ${Math.round(delay)}ms`);
    
    this.reconnectTimer = setTimeout(async () => {
      this.reconnectTimer = undefined;
      this.reconnectAttempts++;
      
      try {
        await this.connect();
        this.reconnectAttempts = 0;
        this.log.info('Successfully reconnected to Unreal Engine');
      } catch (err) {
        this.log.warn('Reconnection attempt failed:', err);
        this.scheduleReconnect();
      }
    }, delay);
  }
  
  // Graceful shutdown
  async disconnect(): Promise<void> {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = undefined;
    }
    
    if (this.ws) {
      try {
        // Avoid unhandled error during shutdown
        this.ws.on('error', () => {});
        try { this.ws.close(); } catch {}
        try { this.ws.terminate(); } catch {}
      } finally {
        try { this.ws.removeAllListeners(); } catch {}
        this.ws = undefined;
      }
    }
    
    this.connected = false;
  }
  
  /**
   * Enhanced Editor Function Access
   * Use Python scripting as a bridge to access modern Editor Subsystem functions
   */
  async executeEditorFunction(functionName: string, params?: Record<string, any>): Promise<any> {
    const template = this.PYTHON_TEMPLATES[functionName];
    if (!template) {
      throw new Error(`Unknown editor function: ${functionName}`);
    }

    let script = template.script;
    
    // Replace parameters in the script
    if (params) {
      for (const [key, value] of Object.entries(params)) {
        const placeholder = `{${key}}`;
        script = script.replace(new RegExp(placeholder, 'g'), String(value));
      }
    }

    try {
      // Execute Python script with result parsing
      const result = await this.executePythonWithResult(script);
      return result;
    } catch (error) {
      this.log.error(`Failed to execute editor function ${functionName}:`, error);
      
      // Fallback to console command if Python fails
      return this.executeFallbackCommand(functionName, params);
    }
  }

  /**
   * Execute Python script and parse the result
   */
  // Expose for internal consumers (resources) that want parsed RESULT blocks
  public async executePythonWithResult(script: string): Promise<any> {
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

      const response = await this.executePython(wrappedScript);

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

      // Fallback to previous regex approach (best-effort)
      const matches = Array.from(out.matchAll(/RESULT:({[\s\S]*})/g));
      if (matches.length > 0) {
        const last = matches[matches.length - 1][1];
        try { return JSON.parse(last); } catch { return { raw: last }; }
      }

      // If no RESULT: marker, return the best-effort textual output or original response
      return typeof response !== 'undefined' ? response : out;
    } catch {
      this.log.warn('Python execution failed, trying direct execution');
      return this.executePython(script);
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
      const fallback = { version: 'unknown', major: 0, minor: 0, patch: 0, isUE56OrAbove: false };
      this.engineVersionCache = { value: fallback, timestamp: now };
      return fallback;
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
   * Fallback commands when Python is not available
   */
  private async executeFallbackCommand(functionName: string, params?: Record<string, any>): Promise<any> {
    switch (functionName) {
      case 'SPAWN_ACTOR_AT_LOCATION':
        return this.executeConsoleCommand(
          `summon ${params?.class_path || 'StaticMeshActor'} ${params?.x || 0} ${params?.y || 0} ${params?.z || 0}`
        );
      
      case 'DELETE_ACTOR':
        // Use Python-based deletion to avoid unsafe console command and improve reliability
        return this.executePythonWithResult(this.PYTHON_TEMPLATES.DELETE_ACTOR.script.replace('{actor_name}', String(params?.actor_name || '')));
      
      case 'BUILD_LIGHTING':
        return this.executeConsoleCommand('BuildLighting');
      
      default:
        throw new Error(`No fallback available for ${functionName}`);
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
        // Retry logic for transient failures
        const msg = (error?.message || String(error)).toLowerCase();
        const notConnected = msg.includes('not connected to unreal');
        if (item.retryCount === undefined) {
          item.retryCount = 0;
        }
        
        if (!notConnected && item.retryCount < 3) {
          item.retryCount++;
          this.log.warn(`Command failed, retrying (${item.retryCount}/3)`);
          
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
