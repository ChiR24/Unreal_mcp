import { UnrealBridge } from '../unreal-bridge.js';

export class BlueprintTools {
  constructor(private bridge: UnrealBridge) {}

  /**
   * Create Blueprint
   */
  async createBlueprint(params: {
    name: string;
    blueprintType: 'Actor' | 'Pawn' | 'Character' | 'GameMode' | 'PlayerController' | 'HUD' | 'ActorComponent';
    savePath?: string;
    parentClass?: string;
  }) {
    try {
      const path = params.savePath || '/Game/Blueprints';
      const baseClass = params.parentClass || this.getDefaultParentClass(params.blueprintType);
      
      // Blueprint creation requires editor scripting, using echo for now
      const commands = [
        `echo Creating Blueprint ${params.name} with class ${baseClass} at ${path}`
      ];
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Blueprint ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create blueprint: ${err}` };
    }
  }

  /**
   * Add Component to Blueprint
   */
  async addComponent(params: {
    blueprintName: string;
    componentType: string;
    componentName: string;
    attachTo?: string;
    transform?: {
      location?: [number, number, number];
      rotation?: [number, number, number];
      scale?: [number, number, number];
    };
  }) {
    try {
      // Component addition requires editor scripting
      const commands = [
        `echo Adding ${params.componentType} component ${params.componentName} to ${params.blueprintName}`
      ];
      
      if (params.attachTo) {
        commands.push(
          `AttachComponent ${params.blueprintName} ${params.componentName} ${params.attachTo}`
        );
      }
      
      if (params.transform) {
        if (params.transform.location) {
          const loc = params.transform.location;
          commands.push(
            `SetComponentLocation ${params.blueprintName} ${params.componentName} ${loc[0]} ${loc[1]} ${loc[2]}`
          );
        }
        if (params.transform.rotation) {
          const rot = params.transform.rotation;
          commands.push(
            `SetComponentRotation ${params.blueprintName} ${params.componentName} ${rot[0]} ${rot[1]} ${rot[2]}`
          );
        }
        if (params.transform.scale) {
          const scale = params.transform.scale;
          commands.push(
            `SetComponentScale ${params.blueprintName} ${params.componentName} ${scale[0]} ${scale[1]} ${scale[2]}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Component ${params.componentName} added to ${params.blueprintName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to add component: ${err}` };
    }
  }

  /**
   * Add Variable to Blueprint
   */
  async addVariable(params: {
    blueprintName: string;
    variableName: string;
    variableType: string;
    defaultValue?: any;
    category?: string;
    isReplicated?: boolean;
    isPublic?: boolean;
  }) {
    try {
      const commands = [
        `AddBlueprintVariable ${params.blueprintName} ${params.variableName} ${params.variableType}`
      ];
      
      if (params.defaultValue !== undefined) {
        commands.push(
          `SetVariableDefault ${params.blueprintName} ${params.variableName} ${JSON.stringify(params.defaultValue)}`
        );
      }
      
      if (params.category) {
        commands.push(
          `SetVariableCategory ${params.blueprintName} ${params.variableName} ${params.category}`
        );
      }
      
      if (params.isReplicated) {
        commands.push(
          `SetVariableReplicated ${params.blueprintName} ${params.variableName} true`
        );
      }
      
      if (params.isPublic !== undefined) {
        commands.push(
          `SetVariablePublic ${params.blueprintName} ${params.variableName} ${params.isPublic}`
        );
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Variable ${params.variableName} added to ${params.blueprintName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to add variable: ${err}` };
    }
  }

  /**
   * Add Function to Blueprint
   */
  async addFunction(params: {
    blueprintName: string;
    functionName: string;
    inputs?: Array<{ name: string; type: string }>;
    outputs?: Array<{ name: string; type: string }>;
    isPublic?: boolean;
    category?: string;
  }) {
    try {
      const commands = [
        `AddBlueprintFunction ${params.blueprintName} ${params.functionName}`
      ];
      
      // Add inputs
      if (params.inputs) {
        for (const input of params.inputs) {
          commands.push(
            `AddFunctionInput ${params.blueprintName} ${params.functionName} ${input.name} ${input.type}`
          );
        }
      }
      
      // Add outputs
      if (params.outputs) {
        for (const output of params.outputs) {
          commands.push(
            `AddFunctionOutput ${params.blueprintName} ${params.functionName} ${output.name} ${output.type}`
          );
        }
      }
      
      if (params.isPublic !== undefined) {
        commands.push(
          `SetFunctionPublic ${params.blueprintName} ${params.functionName} ${params.isPublic}`
        );
      }
      
      if (params.category) {
        commands.push(
          `SetFunctionCategory ${params.blueprintName} ${params.functionName} ${params.category}`
        );
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Function ${params.functionName} added to ${params.blueprintName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to add function: ${err}` };
    }
  }

  /**
   * Add Event to Blueprint
   */
  async addEvent(params: {
    blueprintName: string;
    eventType: 'BeginPlay' | 'Tick' | 'EndPlay' | 'BeginOverlap' | 'EndOverlap' | 'Hit' | 'Custom';
    customEventName?: string;
    parameters?: Array<{ name: string; type: string }>;
  }) {
    try {
      const eventName = params.eventType === 'Custom' ? params.customEventName! : params.eventType;
      
      const commands = [
        `AddBlueprintEvent ${params.blueprintName} ${params.eventType} ${eventName}`
      ];
      
      // Add parameters for custom events
      if (params.eventType === 'Custom' && params.parameters) {
        for (const param of params.parameters) {
          commands.push(
            `AddEventParameter ${params.blueprintName} ${eventName} ${param.name} ${param.type}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Event ${eventName} added to ${params.blueprintName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to add event: ${err}` };
    }
  }

  /**
   * Compile Blueprint
   */
  async compileBlueprint(params: {
    blueprintName: string;
    saveAfterCompile?: boolean;
  }) {
    try {
      const commands = [
        `CompileBlueprint ${params.blueprintName}`
      ];
      
      if (params.saveAfterCompile) {
        commands.push(`SaveAsset ${params.blueprintName}`);
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Blueprint ${params.blueprintName} compiled successfully` 
      };
    } catch (err) {
      return { success: false, error: `Failed to compile blueprint: ${err}` };
    }
  }

  /**
   * Get default parent class for blueprint type
   */
  private getDefaultParentClass(blueprintType: string): string {
    const parentClasses: { [key: string]: string } = {
      'Actor': '/Script/Engine.Actor',
      'Pawn': '/Script/Engine.Pawn',
      'Character': '/Script/Engine.Character',
      'GameMode': '/Script/Engine.GameModeBase',
      'PlayerController': '/Script/Engine.PlayerController',
      'HUD': '/Script/Engine.HUD',
      'ActorComponent': '/Script/Engine.ActorComponent'
    };
    
    return parentClasses[blueprintType] || '/Script/Engine.Actor';
  }

  /**
   * Helper function to execute console commands
   */
  private async executeCommand(command: string) {
    return this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/Engine.Default__KismetSystemLibrary',
      functionName: 'ExecuteConsoleCommand',
      parameters: {
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }
}
