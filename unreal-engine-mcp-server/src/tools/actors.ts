import { UnrealBridge } from '../unreal-bridge.js';

export class ActorTools {
  constructor(private bridge: UnrealBridge) {}

  async spawn(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    const worldCtx = '/Script/Engine.Default__GameplayStatics';

    // BeginDeferredActorSpawnFromClass requires a specific struct; Remote Control may vary by version.
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
  }
}
