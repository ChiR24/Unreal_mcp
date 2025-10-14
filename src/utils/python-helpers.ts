import { UnrealBridge } from '../unreal-bridge.js';
import { allowPythonFallbackFromEnv } from './env.js';

export class PythonHelper {
  constructor(private bridge: UnrealBridge) {}

  private allowPythonFallback(): boolean {
    return allowPythonFallbackFromEnv();
  }

  /**
   * Resolve a UObject path to a simple info payload using the editor function
   * templates. Falls back to Python only when explicitly allowed by env.
   */
  async resolveObject(path: string): Promise<any> {
    if (!path || typeof path !== 'string') throw new Error('Invalid path');
    return this.bridge.executeEditorFunction('RESOLVE_OBJECT', { path }, { allowPythonFallback: this.allowPythonFallback() });
  }

  /**
   * List immediate components for an actor via the centralized template.
   */
  async listActorComponents(actorPath: string): Promise<any> {
    if (!actorPath || typeof actorPath !== 'string') throw new Error('Invalid actorPath');
    return this.bridge.executeEditorFunction('LIST_ACTOR_COMPONENTS', { actorPath }, { allowPythonFallback: this.allowPythonFallback() });
  }

  /**
   * Get Blueprint CDO path and class info via the GET_BLUEPRINT_CDO template.
   */
  async getBlueprintCdo(blueprintPath: string): Promise<any> {
    if (!blueprintPath || typeof blueprintPath !== 'string') throw new Error('Invalid blueprintPath');
    return this.bridge.executeEditorFunction('GET_BLUEPRINT_CDO', { blueprintPath }, { allowPythonFallback: this.allowPythonFallback() });
  }

  async setBlueprintDefault(params: {
    blueprintCandidates: string[];
    requestedPath: string;
    propertyName: string;
    value: unknown;
    save?: boolean;
  }): Promise<any> {
    const payload = JSON.stringify({
      blueprintCandidates: params.blueprintCandidates,
      requestedPath: params.requestedPath,
      propertyName: params.propertyName,
      value: params.value ?? null,
      save: params.save === true
    });
    return this.bridge.executeEditorFunction('SET_BLUEPRINT_DEFAULT', { payload }, { allowPythonFallback: this.allowPythonFallback() });
  }

  /**
   * Set a property on an arbitrary UObject using the plugin-native
   * setObjectProperty action. This provides typed conversion and marking
   * dirty semantics without falling back to raw Python here.
   */
  async setProperty(params: {
    objectPath: string;
    propertyName: string;
    value: unknown;
    valueType?: 'auto' | 'string' | 'number' | 'bool' | 'vector' | 'rotator' | 'path';
  }): Promise<any> {
    if (!params || !params.objectPath || !params.propertyName) {
      throw new Error('Invalid setProperty parameters');
    }
    return this.bridge.setObjectProperty({
      objectPath: params.objectPath,
      propertyName: params.propertyName,
      value: params.value,
      markDirty: true
    });
  }
}