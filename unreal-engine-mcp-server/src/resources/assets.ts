import { UnrealBridge } from '../unreal-bridge.js';

export class AssetResources {
  constructor(private bridge: UnrealBridge) {}

  async list(dir = '/Game', recursive = true) {
    const res = await this.bridge.call({
      objectPath: '/Script/UnrealEd.Default__EditorAssetLibrary',
      functionName: 'ListAssets',
      parameters: { DirectoryPath: dir, bRecursive: recursive }
    });
    return res?.Result ?? res;
  }

  async find(assetPath: string) {
    const res = await this.bridge.call({
      objectPath: '/Script/UnrealEd.Default__EditorAssetLibrary',
      functionName: 'DoesAssetExist',
      parameters: { AssetPath: assetPath }
    });
    return res?.Result ?? res;
  }
}
