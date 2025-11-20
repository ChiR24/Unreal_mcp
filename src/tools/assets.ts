
import { BaseTool } from './base-tool.js';
import { IAssetTools } from '../types/tool-interfaces.js';

export class AssetTools extends BaseTool implements IAssetTools {
  async importAsset(params: { sourcePath: string; destinationPath: string; overwrite?: boolean; save?: boolean }) {
    return this.sendRequest('manage_asset', {
      action: 'import',
      ...params
    });
  }

  async duplicateAsset(params: { sourcePath: string; destinationPath: string }) {
    return this.sendRequest('manage_asset', {
      action: 'duplicate',
      ...params
    });
  }

  async renameAsset(params: { sourcePath: string; destinationPath: string }) {
    return this.sendRequest('manage_asset', {
      action: 'rename',
      ...params
    });
  }

  async moveAsset(params: { sourcePath: string; destinationPath: string }) {
    return this.sendRequest('manage_asset', {
      action: 'move',
      ...params
    });
  }

  async deleteAssets(params: { paths: string[]; fixupRedirectors?: boolean; timeoutMs?: number }) {
    return this.sendRequest('manage_asset', {
      action: 'delete',
      ...params
    });
  }

  async saveAsset(assetPath: string) {
    try {
      // Try Automation Bridge first
      const bridge = this.getAutomationBridge();
      if (bridge && typeof bridge.sendAutomationRequest === 'function') {
        try {
          const response: any = await bridge.sendAutomationRequest(
            'save_asset',
            { assetPath },
            { timeoutMs: 30000 }
          );

          if (response && response.success !== false) {
            return {
              success: true,
              saved: response.saved ?? true,
              message: response.message || 'Asset saved'
            };
          }
        } catch (_err) {
          // Fall through to executeEditorFunction
        }
      }

      // Fallback to executeEditorFunction
      const res = await this.bridge.executeEditorFunction('SAVE_ASSET', { path: assetPath });
      if (res && typeof res === 'object' && (res.success === true || (res.result && res.result.success === true))) {
        const saved = Boolean(res.saved ?? (res.result && res.result.saved));
        return { success: true, saved };
      }

      return { success: false, error: (res as any)?.error ?? 'Failed to save asset' };
    } catch (err) {
      return { success: false, error: `Failed to save asset: ${err}` };
    }
  }

  async createFolder(folderPath: string) {
    return this.sendRequest('manage_asset', {
      action: 'create_folder',
      path: folderPath
    });
  }

  async getDependencies(params: { assetPath: string; recursive?: boolean }) {
    return this.sendRequest('manage_asset', {
      action: 'get_dependencies',
      ...params
    });
  }

  async createThumbnail(params: { assetPath: string }) {
    return this.sendRequest('manage_asset', {
      action: 'create_thumbnail',
      ...params
    });
  }

  async setTags(params: { assetPath: string; tags: string[] }) {
    return this.sendRequest('manage_asset', {
      action: 'set_tags',
      ...params
    });
  }

  async generateReport(params: { directory: string; reportType?: string; outputPath?: string }) {
    return this.sendRequest('manage_asset', {
      action: 'generate_report',
      ...params
    });
  }

  async validate(params: { assetPath: string }) {
    return this.sendRequest('manage_asset', {
      action: 'validate',
      ...params
    });
  }


}
