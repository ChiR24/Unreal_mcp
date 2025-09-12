import WebSocket from 'ws';
import { createHttpClient } from './utils/http.js';
import { Logger } from './utils/logger.js';
import { loadEnv } from './types/env.js';

interface RcMessage {
  MessageName: string;
  Parameters?: any;
}

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
  private readonly SAFE_VIEWMODES = [
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
# Use LevelEditorSubsystem for viewport operations
subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
viewport_client = subsys.get_active_viewport_config_key()
# Alternative approach using viewport
unreal.EditorLevelLibrary.set_level_viewport_camera_info(location, rotation)
print(f"RESULT:{{'success': True, 'location': [{x}, {y}, {z}], 'rotation': [{pitch}, {yaw}, {roll}]}}")
      `.trim()
    },
    BUILD_LIGHTING: {
      name: 'build_lighting',
      script: `
import unreal
quality = unreal.LightingBuildQuality.{quality}
unreal.EditorLevelLibrary.build_lighting(quality, True)
print(f"RESULT:{{'success': True, 'quality': '{quality}'}}")
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
          this.ws.close();
          this.ws = undefined;
        }
        reject(new Error(`Connection timeout: Unreal Engine may not be running or Remote Control is not enabled`));
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
          this.ws.close();
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
    
    // Retry logic with exponential backoff
    let lastError: any;
    for (let attempt = 0; attempt < 3; attempt++) {
      try {
        // For GET requests, send payload as query parameters (not in body)
        const config: any = { url, method };
        if (method === 'GET' && body && typeof body === 'object') {
          config.params = body;
        } else if (body !== undefined) {
          config.data = body;
        }
        const resp = await this.http.request<T>(config);
        const ms = Date.now() - started;
        this.log.debug(`[HTTP ${method}] ${url} -> ${ms}ms`);
        return resp.data;
      } catch (error: any) {
        lastError = error;
        const delay = Math.min(1000 * Math.pow(2, attempt), 5000); // Exponential backoff with 5s max
        
        if (attempt < 2) {
          this.log.warn(`HTTP request failed (attempt ${attempt + 1}/3), retrying in ${delay}ms...`);
          await new Promise(resolve => setTimeout(resolve, delay));
          
          // If connection error, try to reconnect
          if (error.code === 'ECONNREFUSED' || error.code === 'ETIMEDOUT') {
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
      'r.gpucrash'
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
    } catch (err1) {
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
      } catch (err2) {
        // Final fallback: execute via console py command
        this.log.warn('PythonScriptLibrary not available or failed, falling back to console `py` command');
        
        // For simple single-line commands
        if (!isMultiLine) {
          return await this.executeConsoleCommand(`py ${command}`);
        }
        
        // For multi-line scripts, try to execute as a block
        try {
          // Try executing as a single exec block
          const escapedScript = command.replace(/\"/g, '\\\"').replace(/\n/g, '\\n');
          return await this.executeConsoleCommand(`py exec(\"${escapedScript}\")`);
        } catch (err3) {
          // If that fails, execute line by line
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
   * SOLUTION 1: Enhanced EditorLevelLibrary Access
   * Use Python scripting as a bridge to access EditorLevelLibrary functions
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
      // Wrap script to capture output
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
      `.trim();

      const response = await this.executePython(wrappedScript);
      
      // Parse result from output
      if (response && typeof response === 'string') {
        const resultMatch = response.match(/RESULT:(.+)/);
        if (resultMatch) {
          try {
            return JSON.parse(resultMatch[1].replace(/'/g, '"'));
          } catch {
            return { raw: resultMatch[1] };
          }
        }
      }
      
      return response;
    } catch (error) {
      this.log.warn('Python execution failed, trying direct execution');
      return this.executePython(script);
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
      const item = this.commandQueue.shift()!;
      
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
            ...item,
            priority: Math.max(1, item.priority - 1)
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
  private calculateDelay(priority: number, command?: any): number {
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
