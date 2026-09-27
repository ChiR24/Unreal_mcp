import type { UnrealBridge } from '../unreal-bridge.js';
import { coerceString } from '../utils/validation/type-guards.js';
import { AutomationResponse } from '../types/automation/automation-responses.js';
import { Logger } from '../utils/logging/logger.js';
import { normalizeAndSanitizeAssetPath } from '../utils/validation/validation.js';

const log = new Logger('AssetResources');

type Row = Record<string, unknown>;
const toRow = (entry: Row, fallbackClass: string): Record<string, string> => ({
  Name: coerceString(entry?.n ?? entry?.Name ?? entry?.name) ?? '',
  Path: coerceString(entry?.p ?? entry?.Path ?? entry?.path) ?? '',
  Class: coerceString(entry?.c ?? entry?.Class ?? entry?.class) ?? fallbackClass,
});

/** Backs ue://assets: the immediate children (folders and assets) of one content directory. */
export class AssetResources {
  constructor(private readonly bridge: UnrealBridge) {}

  async list(dir = '/Game', limit = 50): Promise<Record<string, unknown>> {
    if (!this.bridge.isConnected) {
      return {
        success: false,
        assets: [],
        warning: 'Unreal Engine is not connected. Please ensure Unreal Engine is running with the MCP server enabled.',
        connectionStatus: 'disconnected'
      };
    }
    const path = normalizeAndSanitizeAssetPath(dir);
    try {
      const response = await this.bridge.getAutomationBridge().sendAutomationRequest<AutomationResponse>(
        'manage_asset',
        { action: 'list', path, limit, recursive: false },
        { timeoutMs: 30000 }
      );
      if (response.success !== false && response.result) {
        const payload = response.result as Row;
        const folders = Array.isArray(payload.folders_list)
          ? (payload.folders_list as Row[]).map((entry) => ({ ...toRow(entry, 'Folder'), Class: 'Folder', isFolder: true }))
          : [];
        const assets = Array.isArray(payload.assets) ? (payload.assets as Row[]).map((entry) => toRow(entry, 'Object')) : [];
        return {
          success: true,
          assets: [...folders, ...assets],
          count: folders.length + assets.length,
          folders: folders.length,
          files: assets.length,
          path,
          recursive: false,
          method: 'automation_bridge'
        };
      }
    } catch (error) {
      log.debug('AssetRegistry list via automation bridge failed', error instanceof Error ? error : String(error));
    }
    return {
      success: false,
      path,
      assets: [],
      error: 'Asset registry returned no payload.',
      warning: 'No items returned from AssetRegistry request.',
      method: 'automation_bridge'
    };
  }
}
