import { UnrealBridge } from '../unreal-bridge.js';

export class ActorResources {
  constructor(private bridge: UnrealBridge) {}

  async listActors() {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorLevelLibrary',
        functionName: 'GetAllLevelActors',
        parameters: {}
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to list actors: ${err}` };
    }
  }

  async getActorByName(actorName: string) {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/Engine.Default__GameplayStatics',
        functionName: 'GetActorOfClass',
        parameters: {
          WorldContextObject: null,
          ActorName: actorName
        }
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to get actor: ${err}` };
    }
  }

  async getActorTransform(actorPath: string) {
    try {
      const res = await this.bridge.httpCall('/remote/object/property', 'GET', {
        objectPath: actorPath,
        propertyName: 'ActorTransform'
      });
      return res;
    } catch (err) {
      return { error: `Failed to get transform: ${err}` };
    }
  }
}
