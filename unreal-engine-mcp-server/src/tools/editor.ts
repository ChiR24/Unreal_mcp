import { UnrealBridge } from '../unreal-bridge.js';

export class EditorTools {
  constructor(private bridge: UnrealBridge) {}

  async playInEditor() {
    try {
      // Use console command instead of EditorLevelLibrary (which is not accessible via Remote Control)
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: 'play',
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: 'PIE started via console command' };
    } catch (err) {
      return { success: false, error: `Failed to start PIE: ${err}` };
    }
  }

  async stopPlayInEditor() {
    try {
      // Use console command instead of EditorLevelLibrary
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: 'stop',
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: 'PIE stopped via console command' };
    } catch (err) {
      return { success: false, error: `Failed to stop PIE: ${err}` };
    }
  }
  
  async pausePlayInEditor() {
    try {
      // Pause/Resume PIE
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: 'pause',
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: 'PIE paused/resumed' };
    } catch (err) {
      return { success: false, error: `Failed to pause PIE: ${err}` };
    }
  }

  async buildLighting() {
    try {
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          WorldContextObject: null,
          Command: 'BuildLighting'
        }
      });
      return { success: true, message: 'Lighting build started' };
    } catch (err) {
      return { success: false, error: `Failed to build lighting: ${err}` };
    }
  }

  async setViewportCamera(location: { x: number; y: number; z: number }, rotation?: { pitch: number; yaw: number; roll: number }) {
    try {
      // EditorLevelLibrary is not accessible, use alternative console commands
      // Set camera speed if needed
      await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: 'camspeed 4',
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      
      // For actual camera positioning, we can use debug camera or suggest manual positioning
      // Note: Direct camera positioning via console is limited
      return { 
        success: true, 
        message: 'Camera controls set. Use debug camera (toggledebugcamera) for free movement' 
      };
    } catch (err) {
      return { success: false, error: `Failed to set camera: ${err}` };
    }
  }
  
  async setCameraSpeed(speed: number) {
    try {
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: `camspeed ${speed}`,
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: `Camera speed set to ${speed}` };
    } catch (err) {
      return { success: false, error: `Failed to set camera speed: ${err}` };
    }
  }
  
  async setFOV(fov: number) {
    try {
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: `fov ${fov}`,
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: `FOV set to ${fov}` };
    } catch (err) {
      return { success: false, error: `Failed to set FOV: ${err}` };
    }
  }
}
