import WebSocket from 'ws';
import { createHttpClient } from './utils/http.js';
import { Logger } from './utils/logger.js';
import { loadEnv } from './types/env.js';

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
  
  // Command queue for throttling
  private commandQueue: CommandQueueItem[] = [];
  private isProcessing = false;
  private readonly MIN_COMMAND_DELAY = 100; // Increased to prevent console spam
  private readonly MAX_COMMAND_DELAY = 500; // Maximum delay for heavy operations
  private readonly STAT_COMMAND_DELAY = 300; // Special delay for stat commands to avoid warnings
  private lastCommandTime = 0;
  private lastStatCommandTime = 0; // Track stat commands separately
  
  // Safe viewmodes that won't cause crashes (per docs and testing)
  private readonly _SAFE_VIEWMODES = [
    'Lit', 'Unlit', 'Wireframe', 'DetailLighting',
    'LightingOnly', 'ReflectionOverride', 'ShaderComplexity'
  ];
  
  // Unsafe viewmodes that can cause crashes or instability via visualizeBuffer
  private readonly UNSAFE_VIEWMODES = [
    'BaseColor', 'WorldNormal', 'Metallic', 'Specular',
    'Roughness', 'SubsurfaceColor', 'Opacity',
    'LightComplexity', 'LightmapDensity',
    'StationaryLightOverlap', 'CollisionPawn', 'CollisionVisibility'
  ];
  
  // Python script templates for EditorLevelLibrary access
  private readonly PYTHON_TEMPLATES: Record<string, PythonScriptTemplate> = {
    GET_ALL_ACTORS: {
      name: 'get_all_actors',
      script: `
import unreal
# Use EditorActorSubsystem instead of deprecated EditorLevelLibrary
subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = subsys.get_all_level_actors()
result = [{'name': a.get_name(), 'class': a.get_class().get_name(), 'path': a.get_path_name()} for a in actors]
print(f"RESULT:{result}")
      `.trim()
    },
    SPAWN_ACTOR_AT_LOCATION: {
      name: 'spawn_actor',
      script: `
import unreal
location = unreal.Vector({x}, {y}, {z})
rotation = unreal.Rotator({pitch}, {yaw}, {roll})
actor_class = unreal.EditorAssetLibrary.load_asset("{class_path}")
if actor_class:
    # Use EditorActorSubsystem instead of deprecated EditorLevelLibrary
    subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    spawned = subsys.spawn_actor_from_object(actor_class, location, rotation)
    print(f"RESULT:{{'success': True, 'actor': spawned.get_name()}}")
else:
    print(f"RESULT:{{'success': False, 'error': 'Failed to load actor class'}}")
      `.trim()
    },
    DELETE_ACTOR: {
      name: 'delete_actor',
      script: `
import unreal
import json
subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = subsys.get_all_level_actors()
found = False
for actor in actors:
    if not actor:
        continue
    label = actor.get_actor_label()
    name = actor.get_name()
    if label == "{actor_name}" or name == "{actor_name}" or label.lower().startswith("{actor_name}".lower()+"_"):
        subsys.destroy_actor(actor)
        print("RESULT:" + json.dumps({'success': True, 'deleted': label}))
        found = True
        break
if not found:
    print("RESULT:" + json.dumps({'success': False, 'error': 'Actor not found'}))
      `.trim()
    },
    CREATE_ASSET: {
      name: 'create_asset',
      script: `
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.{factory_class}()
asset = asset_tools.create_asset("{asset_name}", "{package_path}", {asset_class}, factory)
if asset:
    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
    print(f"RESULT:{{'success': True, 'path': asset.get_path_name()}}")
else:
    print(f"RESULT:{{'success': False, 'error': 'Failed to create asset'}}")
      `.trim()
    },
    SET_VIEWPORT_CAMERA: {
      name: 'set_viewport_camera',
      script: `
import unreal
location = unreal.Vector({x}, {y}, {z})
rotation = unreal.Rotator({pitch}, {yaw}, {roll})
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
    print(f"RESULT:{{'success': True, 'location': [{x}, {y}, {z}], 'rotation': [{pitch}, {yaw}, {roll}]}}")
else:
    print(f"RESULT:{{'success': False, 'error': 'UnrealEditorSubsystem not available'}}")
      `.trim()
    },
    BUILD_LIGHTING: {
      name: 'build_lighting',
      script: `
import unreal
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les:
        q = unreal.LightingBuildQuality.{quality}
        les.build_light_maps(q, True)
        print(f"RESULT:{{'success': True, 'quality': '{quality}', 'method': 'LevelEditorSubsystem'}}")
    else:
        print(f"RESULT:{{'success': False, 'error': 'LevelEditorSubsystem not available'}}")
except Exception as e:
    print(f"RESULT:{{'success': False, 'error': str(e)}}")
      `.trim()
    },
    SAVE_ALL_DIRTY_PACKAGES: {
      name: 'save_dirty_packages',
      script: `
import unreal
saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
print(f"RESULT:{{'success': {saved}, 'message': 'All dirty packages saved'}}")
      `.trim()
    }
  };

  get isConnected() { return this.connected; }
  
  /**
   * Attempt to connect with retries
   * @param maxAttempts Maximum number of connection attempts
   * @param timeoutMs Timeout for each connection attempt in milliseconds
   * @param retryDelayMs Delay between retry attempts in milliseconds
   * @returns Promise that resolves when connected or rejects after all attempts fail
   */
  async tryConnect(maxAttempts: number = 3, timeoutMs: number = 5000, retryDelayMs: number = 2000): Promise<boolean> {
    for (let attempt = 1; attempt <= maxAttempts; attempt++) {
      try {
        this.log.info(`Connection attempt ${attempt}/${maxAttempts}`);
        await this.connect(timeoutMs);
        return true; // Successfully connected
      } catch (err) {
        this.log.warn(`Connection attempt ${attempt} failed:`, err);
        
        if (attempt < maxAttempts) {
          this.log.info(`Retrying in ${retryDelayMs}ms...`);
          await new Promise(resolve => setTimeout(resolve, retryDelayMs));
        } else {
          this.log.error(`All ${maxAttempts} connection attempts failed`);
          return false; // All attempts failed
        }
      }
    }
    return false;
  }

  async connect(timeoutMs: number = 5000): Promise<void> {
    const wsUrl = `ws://${this.env.UE_HOST}:${this.env.UE_RC_WS_PORT}`;
    const httpBase = `http://${this.env.UE_HOST}:${this.env.UE_RC_HTTP_PORT}`;
    this.http = createHttpClient(httpBase);

    this.log.info(`Connecting to UE Remote Control: ${wsUrl}`);
    this.ws = new WebSocket(wsUrl);

    await new Promise<void>((resolve, reject) => {
      if (!this.ws) return reject(new Error('WS not created'));
      
      // Setup timeout
      const timeout = setTimeout(() => {
        this.log.warn(`Connection timeout after ${timeoutMs}ms`);
        if (this.ws) {
          this.ws.removeAllListeners();
          // Only close if the websocket is in CONNECTING state
          if (this.ws.readyState === WebSocket.CONNECTING) {
            try {
              this.ws.terminate(); // Use terminate instead of close for immediate cleanup
} catch (_e) {
              // Ignore close errors
            }
          }
          this.ws = undefined;
        }
        reject(new Error('Connection timeout: Unreal Engine may not be running or Remote Control is not enabled'));
      }, timeoutMs);
      
      // Success handler
      const onOpen = () => {
        clearTimeout(timeout);
        this.connected = true;
        this.log.info('Connected to Unreal Remote Control');
        this.startCommandProcessor(); // Start command processor on connect
        resolve();
      };
      
      // Error handler
      const onError = (err: Error) => {
        clearTimeout(timeout);
        this.log.error('WebSocket error', err);
        if (this.ws) {
          this.ws.removeAllListeners();
          try {
            if (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING) {
              this.ws.terminate();
            }
} catch (_e) {
            // Ignore close errors
          }
          this.ws = undefined;
        }
        reject(new Error(`Failed to connect: ${err.message}`));
      };
      
      // Close handler (if closed before open)
      const onClose = () => {
        if (!this.connected) {
          clearTimeout(timeout);
          reject(new Error('Connection closed before establishing'));
        } else {
          // Normal close after connection was established
          this.connected = false;
          this.log.warn('WebSocket closed');
          this.scheduleReconnect();
        }
      };
      
      // Message handler (currently best-effort logging)
      const onMessage = (raw: WebSocket.RawData) => {
        try {
          const msg = JSON.parse(String(raw));
          this.log.debug('WS message', msg);
        } catch (e) {
          this.log.error('Failed parsing WS message', e);
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
    const url = path.startsWith('/') ? path : `/${path}`;
    const started = Date.now();
    
    // Fix Content-Length header issue - ensure body is properly handled
    if (body === undefined || body === null) {
      body = method === 'GET' ? undefined : {};
    }
    
    // Add timeout wrapper to prevent hanging
    const CALL_TIMEOUT = 10000; // 10 seconds timeout
    
    // CRITICAL: Intercept and block dangerous console commands at HTTP level
    if (url === '/remote/object/call' && body?.functionName === 'ExecuteConsoleCommand') {
      const command = body?.parameters?.Command;
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
        if (method === 'GET' && body && typeof body === 'object') {
          config.params = body;
        } else if (body !== undefined) {
          config.data = body;
        }
        
        // Wrap with timeout promise to ensure we don't hang
        const requestPromise = this.http.request<T>(config);
        const timeoutPromise = new Promise<never>((_, reject) => {
          setTimeout(() => {
            reject(new Error(`Request timeout after ${CALL_TIMEOUT}ms`));
          }, CALL_TIMEOUT);
        });
        
        const resp = await Promise.race([requestPromise, timeoutPromise]);
        const ms = Date.now() - started;
        this.log.debug(`[HTTP ${method}] ${url} -> ${ms}ms`);
        return resp.data;
      } catch (error: any) {
        lastError = error;
        const delay = Math.min(1000 * Math.pow(2, attempt), 5000); // Exponential backoff with 5s max
        
        // Log timeout errors specifically
        if (error.message?.includes('timeout')) {
          this.log.warn(`HTTP request timed out (attempt ${attempt + 1}/3): ${url}`);
        }
        
        if (attempt < 2) {
          this.log.warn(`HTTP request failed (attempt ${attempt + 1}/3), retrying in ${delay}ms...`);
          await new Promise(resolve => setTimeout(resolve, delay));
          
          // If connection error, try to reconnect
          if (error.code === 'ECONNREFUSED' || error.code === 'ETIMEDOUT' || error.message?.includes('timeout')) {
            this.scheduleReconnect();
          }
        }
      }
    }
    
    throw lastError;
  }

  // Generic function call via Remote Control HTTP API
  async call(body: RcCallBody): Promise<any> {
    // Using HTTP endpoint /remote/object/call
    const result = await this.httpCall<any>('/remote/object/call', 'PUT', {
      generateTransaction: false,
      ...body
    });
    return result;
  }

  async getExposed(): Promise<any> {
    return this.httpCall('/remote/preset', 'GET');
  }

  // Execute a console command safely with validation and throttling
  async executeConsoleCommand(command: string): Promise<any> {
    // Validate command is not empty
    if (!command || typeof command !== 'string') {
      throw new Error('Invalid command: must be a non-empty string');
    }
    
    const cmdTrimmed = command.trim();
    if (cmdTrimmed.length === 0) {
      // Return success for empty commands to match UE behavior
      return { success: true, message: 'Empty command ignored' };
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
    
    const cmdLower = cmdTrimmed.toLowerCase();
    if (dangerousCommands.some(dangerous => cmdLower.includes(dangerous))) {
      throw new Error(`Dangerous command blocked: ${command}`);
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

  // Try to execute a Python command via the PythonScriptPlugin, fallback to `py` console command.
  async executePython(command: string): Promise<any> {
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
          return await this.executeConsoleCommand(`py ${command}`);
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
          return await this.executeConsoleCommand(`py exec("${escapedScript}")`);
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
                result = await this.executeConsoleCommand(`py ${miniScript}`);
              } else {
                result = await this.executeConsoleCommand(`py ${stmt}`);
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
              result = await this.executeConsoleCommand(`py ${line.trim()}`);
              // Small delay between commands to ensure execution order
              await new Promise(resolve => setTimeout(resolve, 50));
            }
            
            return result;
          }
        }
      }
    }
  }
  
  // Connection recovery
  private scheduleReconnect(): void {
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
    
    this.log.info(`Scheduling reconnection attempt ${this.reconnectAttempts + 1}/${this.MAX_RECONNECT_ATTEMPTS} in ${Math.round(delay)}ms`);
    
    this.reconnectTimer = setTimeout(async () => {
      this.reconnectTimer = undefined;
      this.reconnectAttempts++;
      
      try {
        await this.connect();
        this.reconnectAttempts = 0;
        this.log.info('Successfully reconnected to Unreal Engine');
      } catch (err) {
        this.log.error('Reconnection attempt failed:', err);
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
      this.ws.close();
      this.ws = undefined;
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
  private async executePythonWithResult(script: string): Promise<any> {
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
          // Common RC Python response contains LogOutput array entries with .Output strings
          if (Array.isArray((response as any).LogOutput)) {
            out = (response as any).LogOutput.map((l: any) => l.Output || '').join('');
          } else if (typeof (response as any).Output === 'string') {
            out = (response as any).Output;
          } else if (typeof (response as any).result === 'string') {
            out = (response as any).result;
          } else {
            // Fallback to stringifying object (may still include RESULT in nested fields)
            out = JSON.stringify(response);
          }
        }
      } catch {
        out = String(response || '');
      }

      // Find the last RESULT: JSON block in the output for robustness
      const matches = Array.from(out.matchAll(/RESULT:({[\s\S]*?})/g));
      if (matches.length > 0) {
        const last = matches[matches.length - 1][1];
        try {
          // Accept single quotes and True/False from Python repr if present
          const normalized = last
            .replace(/'/g, '"')
            .replace(/\bTrue\b/g, 'true')
            .replace(/\bFalse\b/g, 'false');
          return JSON.parse(normalized);
        } catch {
          return { raw: last };
        }
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
      return { version, major, minor, patch, isUE56OrAbove };
    } catch (error) {
      this.log.warn('Failed to get engine version via Python', error);
      return { version: 'unknown', major: 0, minor: 0, patch: 0, isUE56OrAbove: false };
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
    const normalizedMode = mode.charAt(0).toUpperCase() + mode.slice(1).toLowerCase();
    
    // Check if the viewmode is known to be unsafe
    if (this.UNSAFE_VIEWMODES.includes(normalizedMode)) {
      this.log.warn(`Viewmode '${normalizedMode}' is known to cause crashes. Using safe alternative.`);
      
      // For visualizeBuffer modes, we need special handling
      if (normalizedMode === 'BaseColor' || normalizedMode === 'WorldNormal') {
        // First ensure we're in a safe state
        await this.executeConsoleCommand('viewmode Lit');
        await this.delay(100);
        
        // Try to use a safer alternative or skip
        this.log.info(`Skipping unsafe visualizeBuffer mode: ${normalizedMode}`);
        return {
          success: false,
          message: `Viewmode ${normalizedMode} skipped for safety`,
          alternative: 'Lit'
        };
      }
      
      // For other unsafe modes, switch to safe alternatives
      const safeAlternative = this.getSafeAlternative(normalizedMode);
      return this.executeConsoleCommand(`viewmode ${safeAlternative}`);
    }
    
    // Safe mode - execute with delay
    return this.executeThrottledCommand(() => 
      this.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          WorldContextObject: null,
          Command: `viewmode ${normalizedMode}`,
          SpecificPlayer: null
        },
        generateTransaction: false
      })
    );
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
      } catch (error) {
        // Retry logic for transient failures
        if (item.retryCount === undefined) {
          item.retryCount = 0;
        }
        
        if (item.retryCount < 3) {
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
      return this.MIN_COMMAND_DELAY;
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
    const results: any[] = [];
    
    for (const cmd of commands) {
      const result = await this.executeConsoleCommand(cmd.command);
      results.push(result);
    }
    
    return results;
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
