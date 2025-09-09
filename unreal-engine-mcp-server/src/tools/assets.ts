import { UnrealBridge } from '../unreal-bridge.js';

export class AssetTools {
  constructor(private bridge: UnrealBridge) {}

  async importAsset(sourcePath: string, destinationPath: string) {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorAssetLibrary',
        functionName: 'ImportAsset',
        parameters: {
          SourcePath: sourcePath,
          DestinationPath: destinationPath
        }
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to import asset: ${err}` };
    }
  }

  async duplicateAsset(sourcePath: string, destinationPath: string) {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorAssetLibrary',
        functionName: 'DuplicateAsset',
        parameters: {
          SourceAssetPath: sourcePath,
          DestinationAssetPath: destinationPath
        }
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to duplicate asset: ${err}` };
    }
  }

  async deleteAsset(assetPath: string) {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorAssetLibrary',
        functionName: 'DeleteAsset',
        parameters: {
          AssetPathToDelete: assetPath
        }
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to delete asset: ${err}` };
    }
  }

  async saveAsset(assetPath: string) {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorAssetLibrary',
        functionName: 'SaveAsset',
        parameters: {
          AssetToSave: assetPath
        }
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to save asset: ${err}` };
    }
  }
}
