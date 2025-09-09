import { UnrealBridge } from '../unreal-bridge.js';

export class ActorTools {
  constructor(private bridge: UnrealBridge) {}

  async spawn(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    try {
      // First try the GameplayStatics method
      const worldCtx = '/Script/Engine.Default__GameplayStatics';
      const res = await this.bridge.call({
        objectPath: worldCtx,
        functionName: 'BeginDeferredActorSpawnFromClass',
        parameters: {
          WorldContextObject: null,
          ActorClass: params.classPath,
          SpawnTransform: {
            Translation: params.location ?? { x: 0, y: 0, z: 100 },
            Rotation: params.rotation ?? { pitch: 0, yaw: 0, roll: 0 },
            Scale3D: { x: 1, y: 1, z: 1 }
          }
        }
      });
      return res?.Result ?? res;
    } catch (err) {
      // Fallback to console command method
      return this.spawnViaConsole(params);
    }
  }
  
  async spawnViaConsole(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    try {
      // Use console command to spawn actor
      // Format: spawnactor <ClassPath>
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: `spawnactor ${params.classPath}`,
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: `Actor spawned via console: ${params.classPath}` };
    } catch (err) {
      throw new Error(`Failed to spawn actor: ${err}`);
    }
  }
}
