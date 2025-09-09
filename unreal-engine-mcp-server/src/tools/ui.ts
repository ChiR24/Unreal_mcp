// UI tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class UITools {
  constructor(private bridge: UnrealBridge) {}

  // Execute console command
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

  // Create widget blueprint
  async createWidget(params: {
    name: string;
    type?: 'HUD' | 'Menu' | 'Inventory' | 'Dialog' | 'Custom';
    savePath?: string;
  }) {
    const path = params.savePath || '/Game/UI/Widgets';
    const type = params.type || 'Custom';
    
    const command = `CreateWidgetBlueprint ${params.name} ${type} ${path}`;
    return this.executeCommand(command);
  }

  // Add widget component
  async addWidgetComponent(params: {
    widgetName: string;
    componentType: 'Button' | 'Text' | 'Image' | 'ProgressBar' | 'Slider' | 'CheckBox' | 'ComboBox' | 'TextBox' | 'ScrollBox' | 'Canvas' | 'VerticalBox' | 'HorizontalBox' | 'Grid' | 'Overlay';
    componentName: string;
    slot?: {
      position?: [number, number];
      size?: [number, number];
      anchor?: [number, number, number, number];
      alignment?: [number, number];
    };
  }) {
    const commands = [];
    
    commands.push(`AddWidgetComponent ${params.widgetName} ${params.componentType} ${params.componentName}`);
    
    if (params.slot) {
      if (params.slot.position) {
        commands.push(`SetWidgetPosition ${params.widgetName}.${params.componentName} ${params.slot.position.join(' ')}`);
      }
      if (params.slot.size) {
        commands.push(`SetWidgetSize ${params.widgetName}.${params.componentName} ${params.slot.size.join(' ')}`);
      }
      if (params.slot.anchor) {
        commands.push(`SetWidgetAnchor ${params.widgetName}.${params.componentName} ${params.slot.anchor.join(' ')}`);
      }
      if (params.slot.alignment) {
        commands.push(`SetWidgetAlignment ${params.widgetName}.${params.componentName} ${params.slot.alignment.join(' ')}`);
      }
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Component ${params.componentName} added to widget` };
  }

  // Set text
  async setWidgetText(params: {
    widgetName: string;
    componentName: string;
    text: string;
    fontSize?: number;
    color?: [number, number, number, number];
    fontFamily?: string;
  }) {
    const commands = [];
    
    commands.push(`SetWidgetText ${params.widgetName}.${params.componentName} "${params.text}"`);
    
    if (params.fontSize !== undefined) {
      commands.push(`SetWidgetFontSize ${params.widgetName}.${params.componentName} ${params.fontSize}`);
    }
    
    if (params.color) {
      commands.push(`SetWidgetTextColor ${params.widgetName}.${params.componentName} ${params.color.join(' ')}`);
    }
    
    if (params.fontFamily) {
      commands.push(`SetWidgetFont ${params.widgetName}.${params.componentName} ${params.fontFamily}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Widget text updated' };
  }

  // Set image
  async setWidgetImage(params: {
    widgetName: string;
    componentName: string;
    imagePath: string;
    tint?: [number, number, number, number];
    sizeToContent?: boolean;
  }) {
    const commands = [];
    
    commands.push(`SetWidgetImage ${params.widgetName}.${params.componentName} ${params.imagePath}`);
    
    if (params.tint) {
      commands.push(`SetWidgetImageTint ${params.widgetName}.${params.componentName} ${params.tint.join(' ')}`);
    }
    
    if (params.sizeToContent !== undefined) {
      commands.push(`SetWidgetSizeToContent ${params.widgetName}.${params.componentName} ${params.sizeToContent}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Widget image updated' };
  }

  // Create HUD
  async createHUD(params: {
    name: string;
    elements?: Array<{
      type: 'HealthBar' | 'AmmoCounter' | 'Score' | 'Timer' | 'Minimap' | 'Crosshair';
      position: [number, number];
      size?: [number, number];
    }>;
  }) {
    const commands = [];
    
    commands.push(`CreateHUDClass ${params.name}`);
    
    if (params.elements) {
      for (const element of params.elements) {
        const size = element.size || [100, 50];
        commands.push(`AddHUDElement ${params.name} ${element.type} ${element.position.join(' ')} ${size.join(' ')}`);
      }
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `HUD ${params.name} created` };
  }

  // Show/Hide widget
  async setWidgetVisibility(params: {
    widgetName: string;
    visible: boolean;
    playerIndex?: number;
  }) {
    const playerIndex = params.playerIndex ?? 0;
    const command = params.visible 
      ? `ShowWidget ${params.widgetName} ${playerIndex}`
      : `HideWidget ${params.widgetName} ${playerIndex}`;
    
    return this.executeCommand(command);
  }

  // Add widget to viewport
  async addWidgetToViewport(params: {
    widgetClass: string;
    zOrder?: number;
    playerIndex?: number;
  }) {
    const zOrder = params.zOrder ?? 0;
    const playerIndex = params.playerIndex ?? 0;
    
    const command = `AddWidgetToViewport ${params.widgetClass} ${zOrder} ${playerIndex}`;
    return this.executeCommand(command);
  }

  // Remove widget from viewport
  async removeWidgetFromViewport(params: {
    widgetName: string;
    playerIndex?: number;
  }) {
    const playerIndex = params.playerIndex ?? 0;
    const command = `RemoveWidgetFromViewport ${params.widgetName} ${playerIndex}`;
    return this.executeCommand(command);
  }

  // Create menu
  async createMenu(params: {
    name: string;
    menuType: 'Main' | 'Pause' | 'Settings' | 'Inventory';
    buttons?: Array<{
      text: string;
      action: string;
      position?: [number, number];
    }>;
  }) {
    const commands = [];
    
    commands.push(`CreateMenuWidget ${params.name} ${params.menuType}`);
    
    if (params.buttons) {
      for (const button of params.buttons) {
        const pos = button.position || [0, 0];
        commands.push(`AddMenuButton ${params.name} "${button.text}" ${button.action} ${pos.join(' ')}`);
      }
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Menu ${params.name} created` };
  }

  // Set widget animation
  async createWidgetAnimation(params: {
    widgetName: string;
    animationName: string;
    duration: number;
    tracks?: Array<{
      componentName: string;
      property: 'Position' | 'Scale' | 'Rotation' | 'Opacity' | 'Color';
      keyframes: Array<{
        time: number;
        value: number | [number, number] | [number, number, number] | [number, number, number, number];
      }>;
    }>;
  }) {
    const commands = [];
    
    commands.push(`CreateWidgetAnimation ${params.widgetName} ${params.animationName} ${params.duration}`);
    
    if (params.tracks) {
      for (const track of params.tracks) {
        commands.push(`AddAnimationTrack ${params.widgetName}.${params.animationName} ${track.componentName} ${track.property}`);
        
        for (const keyframe of track.keyframes) {
          const value = Array.isArray(keyframe.value) ? keyframe.value.join(' ') : keyframe.value;
          commands.push(`AddAnimationKeyframe ${params.widgetName}.${params.animationName} ${track.componentName} ${keyframe.time} ${value}`);
        }
      }
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Animation ${params.animationName} created` };
  }

  // Play widget animation
  async playWidgetAnimation(params: {
    widgetName: string;
    animationName: string;
    playMode?: 'Forward' | 'Reverse' | 'PingPong';
    loops?: number;
  }) {
    const playMode = params.playMode || 'Forward';
    const loops = params.loops ?? 1;
    
    const command = `PlayWidgetAnimation ${params.widgetName} ${params.animationName} ${playMode} ${loops}`;
    return this.executeCommand(command);
  }

  // Set widget style
  async setWidgetStyle(params: {
    widgetName: string;
    componentName: string;
    style: {
      backgroundColor?: [number, number, number, number];
      borderColor?: [number, number, number, number];
      borderWidth?: number;
      padding?: [number, number, number, number];
      margin?: [number, number, number, number];
    };
  }) {
    const commands = [];
    
    if (params.style.backgroundColor) {
      commands.push(`SetWidgetBackgroundColor ${params.widgetName}.${params.componentName} ${params.style.backgroundColor.join(' ')}`);
    }
    
    if (params.style.borderColor) {
      commands.push(`SetWidgetBorderColor ${params.widgetName}.${params.componentName} ${params.style.borderColor.join(' ')}`);
    }
    
    if (params.style.borderWidth !== undefined) {
      commands.push(`SetWidgetBorderWidth ${params.widgetName}.${params.componentName} ${params.style.borderWidth}`);
    }
    
    if (params.style.padding) {
      commands.push(`SetWidgetPadding ${params.widgetName}.${params.componentName} ${params.style.padding.join(' ')}`);
    }
    
    if (params.style.margin) {
      commands.push(`SetWidgetMargin ${params.widgetName}.${params.componentName} ${params.style.margin.join(' ')}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Widget style updated' };
  }

  // Bind widget event
  async bindWidgetEvent(params: {
    widgetName: string;
    componentName: string;
    eventType: 'OnClicked' | 'OnPressed' | 'OnReleased' | 'OnHovered' | 'OnUnhovered' | 'OnTextChanged' | 'OnTextCommitted' | 'OnValueChanged';
    functionName: string;
  }) {
    const command = `BindWidgetEvent ${params.widgetName}.${params.componentName} ${params.eventType} ${params.functionName}`;
    return this.executeCommand(command);
  }

  // Set input mode
  async setInputMode(params: {
    mode: 'GameOnly' | 'UIOnly' | 'GameAndUI';
    showCursor?: boolean;
    lockCursor?: boolean;
  }) {
    const commands = [];
    
    commands.push(`SetInputMode ${params.mode}`);
    
    if (params.showCursor !== undefined) {
      commands.push(`ShowMouseCursor ${params.showCursor}`);
    }
    
    if (params.lockCursor !== undefined) {
      commands.push(`SetMouseLockMode ${params.lockCursor}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Input mode set to ${params.mode}` };
  }

  // Create tooltip
  async createTooltip(params: {
    widgetName: string;
    componentName: string;
    text: string;
    delay?: number;
  }) {
    const delay = params.delay ?? 0.5;
    const command = `SetWidgetTooltip ${params.widgetName}.${params.componentName} "${params.text}" ${delay}`;
    return this.executeCommand(command);
  }

  // Create drag and drop
  async setupDragDrop(params: {
    widgetName: string;
    componentName: string;
    dragVisual?: string;
    dropTargets?: string[];
  }) {
    const commands = [];
    
    commands.push(`EnableDragDrop ${params.widgetName}.${params.componentName}`);
    
    if (params.dragVisual) {
      commands.push(`SetDragVisual ${params.widgetName}.${params.componentName} ${params.dragVisual}`);
    }
    
    if (params.dropTargets) {
      for (const target of params.dropTargets) {
        commands.push(`AddDropTarget ${params.widgetName}.${params.componentName} ${target}`);
      }
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Drag and drop configured' };
  }

  // Create notification
  async showNotification(params: {
    text: string;
    duration?: number;
    type?: 'Info' | 'Success' | 'Warning' | 'Error';
    position?: 'TopLeft' | 'TopCenter' | 'TopRight' | 'BottomLeft' | 'BottomCenter' | 'BottomRight';
  }) {
    const duration = params.duration ?? 3.0;
    const type = params.type || 'Info';
    const position = params.position || 'TopRight';
    
    const command = `ShowNotification "${params.text}" ${duration} ${type} ${position}`;
    return this.executeCommand(command);
  }
}
