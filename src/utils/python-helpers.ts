// DEPRECATED: Python support has been removed
// This file exists only for backwards compatibility during migration

import { UnrealBridge } from '../unreal-bridge.js';

export class PythonHelper {
  constructor(_bridge: UnrealBridge) {
    throw new Error('PythonHelper has been removed. Please use the automation bridge for all operations.');
  }

  async listActorComponents(_actorPath: string): Promise<any> {
    throw new Error('Python support has been removed. Please use the automation bridge for all operations.');
  }
}
