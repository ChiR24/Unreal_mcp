// UI tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { bestEffortInterpretedText, interpretStandardResult } from '../utils/result-helpers.js';

export class UITools {
  private automationBridge?: AutomationBridge;

  constructor(private bridge: UnrealBridge, automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  // Create widget blueprint
  async createWidget(params: {
    name: string;
    type?: 'HUD' | 'Menu' | 'Inventory' | 'Dialog' | 'Custom';
    savePath?: string;
  }) {
    const path = params.savePath || '/Game/UI/Widgets';
    // Plugin-first: attempt to create the widget asset via the Automation Bridge.
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const resp = await this.automationBridge.sendAutomationRequest('system_control', {
          action: 'create_widget',
          name: params.name,
          widgetType: params.type,
          savePath: path
        });
        if (resp && resp.success !== false) {
          const result = resp.result ?? resp;
          const resultObj = result && typeof result === 'object' ? (result as Record<string, unknown>) : undefined;
          const widgetPath = typeof resultObj?.widgetPath === 'string' ? (resultObj.widgetPath as string) : `${path}/${params.name}`;
          const message = resp.message || `Widget created at ${widgetPath}`;
          return {
            success: true,
            message,
            widgetPath,
            exists: Boolean(resultObj?.exists)
          };
        }
      } catch (error) {
        console.warn('UITools.createWidget automation bridge request failed, falling back to editor function:', error);
      }
    }

    // Fallback: attempt to create the widget asset via the Automation
    // Bridge plugin using the generic CREATE_ASSET function. If the plugin
    // does not implement the action the bridge will fall back to executing
    // the Python template (deprecated and gated by server opt-in).
    try {
      const payload = {
        asset_name: params.name,
        package_path: path,
        factory_class: 'WidgetBlueprintFactory',
        asset_class: 'unreal.WidgetBlueprint'
      } as Record<string, any>;

      const resp = await this.bridge.executeEditorFunction('CREATE_ASSET', payload as any);
      const result = resp && typeof resp === 'object' ? (resp.result ?? resp) : resp;

      // Interpret common success shapes returned by plugin or Python template
      if (result && (result.success === true || result.created === true || Boolean(result.path))) {
        return { success: true, message: result.message ?? `Widget created at ${result.path ?? `${path}/${params.name}`}` };
      }

      // If plugin/template returned a structured failure, surface it
      if (result && result.success === false) {
        return { success: false, error: result.error ?? result.message ?? 'Failed to create widget blueprint', details: result };
      }

      // Fallback: if no structured response, return generic failure
      return { success: false, error: 'Failed to create widget blueprint' };
    } catch (e) {
      return { success: false, error: `Failed to create widget blueprint: ${e}` };
    }
  }

  // Add widget component (requires C++ plugin)
  async addWidgetComponent(_params: {
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
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Widget component operations require C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('add_widget_component', {
        widgetName: _params.widgetName,
        componentType: _params.componentType,
        componentName: _params.componentName,
        slot: _params.slot
      });

      return response.success
        ? { success: true, message: response.message || 'Widget component added' }
        : { success: false, error: response.error || response.message || 'Failed to add widget component' };
    } catch (error) {
      return { success: false, error: `Failed to add widget component: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Set text (requires C++ plugin)
  async setWidgetText(_params: {
    widgetName: string;
    componentName: string;
    text: string;
    fontSize?: number;
    color?: [number, number, number, number];
    fontFamily?: string;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Setting widget text requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('set_widget_text', {
        widgetName: _params.widgetName,
        componentName: _params.componentName,
        text: _params.text,
        fontSize: _params.fontSize,
        color: _params.color,
        fontFamily: _params.fontFamily
      });

      return response.success
        ? { success: true, message: response.message || 'Widget text set' }
        : { success: false, error: response.error || response.message || 'Failed to set widget text' };
    } catch (error) {
      return { success: false, error: `Failed to set widget text: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Set image (requires C++ plugin)
  async setWidgetImage(_params: {
    widgetName: string;
    componentName: string;
    imagePath: string;
    tint?: [number, number, number, number];
    sizeToContent?: boolean;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Setting widget images requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('set_widget_image', {
        widgetName: _params.widgetName,
        componentName: _params.componentName,
        imagePath: _params.imagePath,
        tint: _params.tint,
        sizeToContent: _params.sizeToContent
      });

      return response.success
        ? { success: true, message: response.message || 'Widget image set' }
        : { success: false, error: response.error || response.message || 'Failed to set widget image' };
    } catch (error) {
      return { success: false, error: `Failed to set widget image: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Create HUD (requires C++ plugin)
  async createHUD(_params: {
    name: string;
    elements?: Array<{
      type: 'HealthBar' | 'AmmoCounter' | 'Score' | 'Timer' | 'Minimap' | 'Crosshair';
      position: [number, number];
      size?: [number, number];
    }>;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating HUDs requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_hud', {
        name: _params.name,
        elements: _params.elements
      });

      return response.success
        ? { success: true, message: response.message || 'HUD created' }
        : { success: false, error: response.error || response.message || 'Failed to create HUD' };
    } catch (error) {
      return { success: false, error: `Failed to create HUD: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Show/Hide widget (requires C++ plugin)
  async setWidgetVisibility(_params: {
    widgetName: string;
    visible: boolean;
    playerIndex?: number;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Showing/hiding widgets requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('set_widget_visibility', {
        widgetName: _params.widgetName,
        visible: _params.visible,
        playerIndex: _params.playerIndex ?? 0
      });

      return response.success
        ? { success: true, message: response.message || `Widget ${_params.visible ? 'shown' : 'hidden'}` }
        : { success: false, error: response.error || response.message || 'Failed to set widget visibility' };
    } catch (error) {
      return { success: false, error: `Failed to set widget visibility: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Add widget to viewport
  async addWidgetToViewport(params: {
    widgetClass: string;
    zOrder?: number;
    playerIndex?: number;
  }) {
    const zOrder = params.zOrder ?? 0;
    const playerIndex = params.playerIndex ?? 0;
    
    try {
      const allowPythonFallback = false;
      const resp = await this.bridge.executeEditorFunction('ADD_WIDGET_TO_VIEWPORT', { widget_path: params.widgetClass, z_order: zOrder, player_index: playerIndex } as any, { allowPythonFallback });
      const interpreted = interpretStandardResult(resp, {
        successMessage: `Widget added to viewport with z-order ${zOrder}`,
        failureMessage: 'Failed to add widget to viewport'
      });
      if (interpreted.success) {
        return { success: true, message: interpreted.message };
      }
      return { success: false, error: interpreted.error ?? 'Failed to add widget to viewport', details: bestEffortInterpretedText(interpreted) };
    } catch (e) {
      return { success: false, error: `Failed to add widget to viewport: ${e}` };
    }
  }

  // Remove widget from viewport (requires C++ plugin)
  async removeWidgetFromViewport(_params: {
    widgetName: string;
    playerIndex?: number;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Removing widgets requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('remove_widget_from_viewport', {
        widgetName: _params.widgetName,
        playerIndex: _params.playerIndex ?? 0
      });

      return response.success
        ? { success: true, message: response.message || 'Widget removed from viewport' }
        : { success: false, error: response.error || response.message || 'Failed to remove widget from viewport' };
    } catch (error) {
      return { success: false, error: `Failed to remove widget from viewport: ${error instanceof Error ? error.message : String(error)}` };
    }
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
  const commands: string[] = [];
    
    commands.push(`CreateMenuWidget ${params.name} ${params.menuType}`);
    
    if (params.buttons) {
      for (const button of params.buttons) {
        const pos = button.position || [0, 0];
        commands.push(`AddMenuButton ${params.name} "${button.text}" ${button.action} ${pos.join(' ')}`);
      }
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
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
  const commands: string[] = [];
    
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
    
    await this.bridge.executeConsoleCommands(commands);
    
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
    return this.bridge.executeConsoleCommand(command);
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
  const commands: string[] = [];
    
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
    
    await this.bridge.executeConsoleCommands(commands);
    
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
    return this.bridge.executeConsoleCommand(command);
  }

  // Set input mode
  async setInputMode(params: {
    mode: 'GameOnly' | 'UIOnly' | 'GameAndUI';
    showCursor?: boolean;
    lockCursor?: boolean;
  }) {
  const commands: string[] = [];
    
    commands.push(`SetInputMode ${params.mode}`);
    
    if (params.showCursor !== undefined) {
      commands.push(`ShowMouseCursor ${params.showCursor}`);
    }
    
    if (params.lockCursor !== undefined) {
      commands.push(`SetMouseLockMode ${params.lockCursor}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
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
    return this.bridge.executeConsoleCommand(command);
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
    
    await this.bridge.executeConsoleCommands(commands);
    
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
    return this.bridge.executeConsoleCommand(command);
  }
}
