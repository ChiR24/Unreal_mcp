// Level management tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class LevelTools {
  constructor(private bridge: UnrealBridge) {}

  // Execute console command
  private async executeCommand(command: string) {
    return this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/Engine.Default__KismetSystemLibrary',
      functionName: 'ExecuteConsoleCommand',
      parameters: {
        WorldContextObject: null,
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }

  // Load level
  async loadLevel(params: {
    levelPath: string;
    streaming?: boolean;
    position?: [number, number, number];
  }) {
    // Use proper streaming level commands
    const command = params.streaming 
      ? `LoadStreamLevel ${params.levelPath}` 
      : `open ${params.levelPath}`;
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Save current level
  async saveLevel(params: {
    levelName?: string;
    savePath?: string;
  }) {
    const path = params.savePath || '/Game/Maps';
    const name = params.levelName || 'CurrentLevel';
    // Save level command doesn't exist, using SaveGame
    const command = `SaveGame ${name}`;
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Create new level
  async createLevel(params: {
    levelName: string;
    template?: 'Empty' | 'Default' | 'VR' | 'TimeOfDay';
    savePath?: string;
  }) {
    const template = params.template || 'Default';
    const path = params.savePath || '/Game/Maps';
    const command = `CreateNewLevel ${params.levelName} ${template} ${path}`;
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Stream level
  async streamLevel(params: {
    levelName: string;
    shouldBeLoaded: boolean;
    shouldBeVisible: boolean;
    position?: [number, number, number];
  }) {
    const loadCmd = params.shouldBeLoaded ? 'Load' : 'Unload';
    const visCmd = params.shouldBeVisible ? 'Show' : 'Hide';
    const command = `StreamLevel ${params.levelName} ${loadCmd} ${visCmd}`;
    
    return this.bridge.executeConsoleCommand(command);
  }

  // World composition
  async setupWorldComposition(params: {
    enableComposition: boolean;
    tileSize?: number;
    distanceStreaming?: boolean;
    streamingDistance?: number;
  }) {
    const commands = [];
    
    if (params.enableComposition) {
      commands.push('EnableWorldComposition');
      if (params.tileSize) {
        commands.push(`SetWorldTileSize ${params.tileSize}`);
      }
      if (params.distanceStreaming) {
        commands.push(`EnableDistanceStreaming ${params.streamingDistance || 5000}`);
      }
    } else {
      commands.push('DisableWorldComposition');
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'World composition configured' };
  }

  // Level blueprint
  async editLevelBlueprint(params: {
    eventType: 'BeginPlay' | 'EndPlay' | 'Tick' | 'Custom';
    customEventName?: string;
    nodes?: Array<{
      nodeType: string;
      position: [number, number];
      connections?: string[];
    }>;
  }) {
    const command = `OpenLevelBlueprint ${params.eventType}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Sub-levels
  async createSubLevel(params: {
    name: string;
    type: 'Persistent' | 'Streaming' | 'Lighting' | 'Gameplay';
    parent?: string;
  }) {
    const command = `CreateSubLevel ${params.name} ${params.type} ${params.parent || 'None'}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // World settings
  async setWorldSettings(params: {
    gravity?: number;
    worldScale?: number;
    gameMode?: string;
    defaultPawn?: string;
    killZ?: number;
  }) {
    const commands = [];
    
    if (params.gravity !== undefined) {
      commands.push(`SetWorldGravity ${params.gravity}`);
    }
    if (params.worldScale !== undefined) {
      commands.push(`SetWorldToMeters ${params.worldScale}`);
    }
    if (params.gameMode) {
      commands.push(`SetGameMode ${params.gameMode}`);
    }
    if (params.defaultPawn) {
      commands.push(`SetDefaultPawn ${params.defaultPawn}`);
    }
    if (params.killZ !== undefined) {
      commands.push(`SetKillZ ${params.killZ}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'World settings updated' };
  }

  // Level bounds
  async setLevelBounds(params: {
    min: [number, number, number];
    max: [number, number, number];
  }) {
    const command = `SetLevelBounds ${params.min.join(',')} ${params.max.join(',')}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Navigation mesh
  async buildNavMesh(params: {
    rebuildAll?: boolean;
    selectedOnly?: boolean;
  }) {
    const command = params.rebuildAll 
      ? 'RebuildNavigation' 
      : 'BuildPaths';
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Level visibility
  async setLevelVisibility(params: {
    levelName: string;
    visible: boolean;
  }) {
    const command = `SetLevelVisibility ${params.levelName} ${params.visible}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // World origin
  async setWorldOrigin(params: {
    location: [number, number, number];
  }) {
    const command = `SetWorldOriginLocation ${params.location.join(' ')}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Level streaming volumes
  async createStreamingVolume(params: {
    levelName: string;
    position: [number, number, number];
    size: [number, number, number];
    streamingDistance?: number;
  }) {
    const command = `CreateStreamingVolume ${params.levelName} ${params.position.join(' ')} ${params.size.join(' ')} ${params.streamingDistance || 0}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Level LOD
  async setLevelLOD(params: {
    levelName: string;
    lodLevel: number;
    distance: number;
  }) {
    const command = `SetLevelLOD ${params.levelName} ${params.lodLevel} ${params.distance}`;
    return this.bridge.executeConsoleCommand(command);
  }
}
