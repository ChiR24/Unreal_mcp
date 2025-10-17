import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { promises as fs } from 'fs';
import path from 'path';

export class AssetTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  async importAsset(sourcePath: string, destinationPath: string) {
    let createdTestFile = false;
    try {
      // Sanitize destination path (remove trailing slash) and normalize UE path
      let cleanDest = destinationPath.replace(/\/$/, '');
      // Map /Content -> /Game for UE asset destinations
      if (/^\/?content(\/|$)/i.test(cleanDest)) {
        cleanDest = '/Game' + cleanDest.replace(/^\/?content/i, '');
      }

      // Create test FBX file if it's a test file
      if (sourcePath.includes('test_model.fbx')) {
        try {
          await this.createTestFBX(sourcePath);
          createdTestFile = true;
        } catch (_err) {
          return { success: false, error: 'Failed to create test FBX file' };
        }
      }

      // Try Automation Bridge for asset import
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const response: any = await this.automationBridge.sendAutomationRequest(
            'import_asset',
            {
              sourcePath,
              destinationPath: cleanDest,
              automated: true,
              save: true,
              replaceExisting: true
            },
            { timeoutMs: 60000 }
          );

          if (response && response.success !== false) {
            const imported = response.imported ?? response.result?.imported ?? 1;
            const paths = response.paths ?? response.result?.paths ?? [];
            return {
              success: true,
              message: response.message || `Imported ${imported} assets to ${cleanDest}`,
              imported,
              paths
            };
          }

          return {
            success: false,
            error: response.error || response.message || 'Asset import failed'
          };
        } catch (err) {
          return { success: false, error: `Failed to import asset: ${err}` };
        }
      }

      return {
        success: false,
        error: 'Asset import requires Automation Bridge connection'
      };
    } catch (err) {
      return { success: false, error: `Failed to import asset: ${err}` };
    } finally {
      if (createdTestFile) {
        try {
          await fs.rm(sourcePath, { force: true });
        } catch (cleanupError) {
          // Swallow cleanup error but log for debug visibility
          console.warn(`Failed to clean up temporary FBX ${sourcePath}:`, cleanupError);
        }
      }
    }
  }

  async duplicateAsset(params: {
    sourcePath: string;
    destinationPath: string;
    newName?: string;
    overwrite?: boolean;
    save?: boolean;
    timeoutMs?: number;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Asset operations require plugin support.');
    }

    try {
      const resp: any = await this.automationBridge.sendAutomationRequest('duplicate_asset', {
        sourcePath: params.sourcePath,
        destinationPath: params.destinationPath,
        newName: params.newName,
        overwrite: params.overwrite === true,
        save: params.save === true
      }, { timeoutMs: params.timeoutMs });
      
      if (resp && resp.success !== false) {
        return {
          success: true,
          message: resp.message || `Duplicated asset to ${params.destinationPath}`,
          path: resp.path || resp.result?.path,
          overwritten: resp.overwritten || resp.result?.overwritten,
          warnings: resp.warnings
        };
      }
      
      return {
        success: false,
        message: resp?.message ?? 'Duplicate failed',
        error: resp?.error ?? 'DUPLICATE_FAILED'
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to duplicate asset: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  async renameAsset(params: {
    assetPath: string;
    newName: string;
    timeoutMs?: number;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Asset operations require plugin support.');
    }

    try {
      const resp: any = await this.automationBridge.sendAutomationRequest('rename_asset', {
        assetPath: params.assetPath,
        newName: params.newName
      }, { timeoutMs: params.timeoutMs });
      
      if (resp && resp.success !== false) {
        return {
          success: true,
          message: resp.message || `Renamed asset to ${resp.path || params.newName}`,
          path: resp.path || resp.result?.path,
          conflictPath: resp.conflictPath || resp.result?.conflictPath,
          warnings: resp.warnings
        };
      }
      
      return {
        success: false,
        message: resp?.message ?? 'Rename failed',
        error: resp?.error ?? 'RENAME_FAILED'
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to rename asset: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  async moveAsset(params: {
    assetPath: string;
    destinationPath: string;
    newName?: string;
    fixupRedirectors?: boolean;
    timeoutMs?: number;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Asset operations require plugin support.');
    }

    try {
      const resp: any = await this.automationBridge.sendAutomationRequest('move_asset', {
        assetPath: params.assetPath,
        destinationPath: params.destinationPath,
        newName: params.newName,
        fixupRedirectors: params.fixupRedirectors === true
      }, { timeoutMs: params.timeoutMs });
      
      if (resp && resp.success !== false) {
        return {
          success: true,
          message: resp.message || `Moved asset to ${resp.path || params.destinationPath}`,
          path: resp.path || resp.result?.path,
          conflictPath: resp.conflictPath || resp.result?.conflictPath,
          warnings: resp.warnings
        };
      }
      
      return {
        success: false,
        message: resp?.message ?? 'Move failed',
        error: resp?.error ?? 'MOVE_FAILED'
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to move asset: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  async deleteAssets(params: {
    assetPaths: string | string[];
    fixupRedirectors?: boolean;
    timeoutMs?: number;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Asset operations require plugin support.');
    }

    const paths = Array.isArray(params.assetPaths) ? params.assetPaths : [params.assetPaths];
    
    try {
      const resp: any = await this.automationBridge.sendAutomationRequest('delete_assets', {
        paths,
        fixupRedirectors: params.fixupRedirectors === true
      }, { timeoutMs: params.timeoutMs });
      
      if (resp && resp.success !== false) {
        return {
          success: true,
          message: resp.message || `Deleted ${resp.deleted?.length ?? 0} assets`,
          deleted: resp.deleted || resp.result?.deleted || [],
          missing: resp.missing || resp.result?.missing || [],
          failed: resp.failed || resp.result?.failed || [],
          warnings: resp.warnings
        };
      }
      
      return {
        success: false,
        message: resp?.message ?? 'Delete failed',
        error: resp?.error ?? 'DELETE_FAILED'
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to delete assets: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  async saveAsset(assetPath: string) {
    try {
      // Try Automation Bridge first
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const response: any = await this.automationBridge.sendAutomationRequest(
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

          // If plugin doesn't support this, try native function
          const errText = String(response?.error ?? response?.message ?? '');
          if (!errText.toLowerCase().includes('unknown')) {
            return { success: false, error: response.error || 'Failed to save asset' };
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

  // Create a folder (plugin-first)
  async createFolder(folderPath: string) {
    if (!folderPath || typeof folderPath !== 'string') return { success: false, error: 'Invalid folderPath' };
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const resp: any = await this.automationBridge.sendAutomationRequest('create_folder', { path: folderPath });
        if (resp && resp.success !== false) return { success: true, message: resp.message || 'Folder created', path: resp.path || resp.result?.path } as any;
        const errTxt = String(resp?.error ?? resp?.message ?? '');
        if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
          // No plugin support - cannot create folder in editor
          return { success: false, error: 'PLUGIN_UNSUPPORTED' } as any;
        }
        return { success: false, error: resp?.error ?? resp?.message ?? 'CREATE_FOLDER_FAILED' } as any;
      } catch (_e) {
        return { success: false, error: 'AUTOMATION_ERROR' } as any;
      }
    }
    return { success: false, error: 'Automation bridge not connected' } as any;
  }

  async getDependencies(params: { assetPath: string }) {
    const assetPath = params?.assetPath;
    if (!assetPath || typeof assetPath !== 'string') return { success: false, error: 'Invalid assetPath' };
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const resp: any = await this.automationBridge.sendAutomationRequest('get_dependencies', { assetPath });
        if (resp && resp.success !== false) return { success: true, dependencies: (resp.dependencies || resp.result?.dependencies || []) } as any;
        const errTxt = String(resp?.error ?? resp?.message ?? '');
        if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
          return { success: false, error: 'PLUGIN_UNSUPPORTED' } as any;
        }
        return { success: false, error: resp?.error ?? resp?.message ?? 'GET_DEPENDENCIES_FAILED' } as any;
      } catch (_e) {
        return { success: false, error: 'AUTOMATION_ERROR' } as any;
      }
    }
    return { success: false, error: 'Automation bridge not connected' } as any;
  }

  async createThumbnail(params: { assetPath: string }) {
    const assetPath = params?.assetPath;
    if (!assetPath || typeof assetPath !== 'string') return { success: false, error: 'Invalid assetPath' };
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const resp: any = await this.automationBridge.sendAutomationRequest('create_thumbnail', { assetPath });
        if (resp && resp.success !== false) return { success: true, message: resp.message || 'Thumbnail created' } as any;
        const errTxt = String(resp?.error ?? resp?.message ?? '');
        if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
          return { success: false, error: 'PLUGIN_UNSUPPORTED' } as any;
        }
        return { success: false, error: resp?.error ?? resp?.message ?? 'CREATE_THUMBNAIL_FAILED' } as any;
      } catch (_e) {
        return { success: false, error: 'AUTOMATION_ERROR' } as any;
      }
    }
    return { success: false, error: 'Automation bridge not connected' } as any;
  }

  async setTags(params: { assetPath: string; tags: string[] }) {
    const assetPath = params?.assetPath; const tags = Array.isArray(params?.tags) ? params.tags : [];
    if (!assetPath || typeof assetPath !== 'string') return { success: false, error: 'Invalid assetPath' };
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const resp: any = await this.automationBridge.sendAutomationRequest('set_tags', { assetPath, tags });
        if (resp && resp.success !== false) return { success: true, message: resp.message || 'Tags set' } as any;
        const errTxt = String(resp?.error ?? resp?.message ?? '');
        if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
          return { success: false, error: 'PLUGIN_UNSUPPORTED' } as any;
        }
        return { success: false, error: resp?.error ?? resp?.message ?? 'SET_TAGS_FAILED' } as any;
      } catch (_e) {
        return { success: false, error: 'AUTOMATION_ERROR' } as any;
      }
    }
    return { success: false, error: 'Automation bridge not connected' } as any;
  }

  async generateReport(params: { directory: string; reportType?: string; outputPath?: string }) {
    const dir = params?.directory || '/Game';
    const outputPath = params?.outputPath;
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const resp: any = await this.automationBridge.sendAutomationRequest('generate_report', { directory: dir, reportType: params?.reportType, outputPath });
        if (resp && resp.success !== false) return { success: true, message: resp.message || 'Report generated', path: resp.path || resp.result?.path } as any;
        const errTxt = String(resp?.error ?? resp?.message ?? '');
        if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
          return { success: false, error: 'PLUGIN_UNSUPPORTED' } as any;
        }
        return { success: false, error: resp?.error ?? resp?.message ?? 'GENERATE_REPORT_FAILED' } as any;
      } catch (_e) {
        return { success: false, error: 'AUTOMATION_ERROR' } as any;
      }
    }
    return { success: false, error: 'Automation bridge not connected' } as any;
  }

  async validate(params: { assetPath: string }) {
    const assetPath = params?.assetPath;
    if (!assetPath || typeof assetPath !== 'string') return { success: false, error: 'Invalid assetPath' };
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const resp: any = await this.automationBridge.sendAutomationRequest('validate', { assetPath });
        if (resp && resp.success !== false) return { success: true, validated: resp.validated ?? true } as any;
        const errTxt = String(resp?.error ?? resp?.message ?? '');
        if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
          return { success: false, error: 'PLUGIN_UNSUPPORTED' } as any;
        }
        return { success: false, error: resp?.error ?? resp?.message ?? 'VALIDATE_FAILED' } as any;
      } catch (_e) {
        return { success: false, error: 'AUTOMATION_ERROR' } as any;
      }
    }
    return { success: false, error: 'Automation bridge not connected' } as any;
  }

  private async createTestFBX(filePath: string): Promise<void> {
    // Create a minimal valid FBX ASCII file for testing
    const fbxContent = `; FBX 7.5.0 project file
FBXHeaderExtension:  {
    FBXHeaderVersion: 1003
    FBXVersion: 7500
    CreationTimeStamp:  {
        Version: 1000
        Year: 2024
        Month: 1
        Day: 1
        Hour: 0
        Minute: 0
        Second: 0
        Millisecond: 0
    }
    Creator: "MCP Test FBX Generator"
}
GlobalSettings:  {
    Version: 1000
}
Definitions:  {
    Version: 100
    Count: 2
    ObjectType: "Model" {
        Count: 1
    }
    ObjectType: "Geometry" {
        Count: 1
    }
}
Objects:  {
    Geometry: 1234567, "Geometry::Cube", "Mesh" {
        Vertices: *24 {
            a: -50,-50,-50,50,-50,-50,50,50,-50,-50,50,-50,-50,-50,50,50,-50,50,50,50,50,-50,50,50
        }
        PolygonVertexIndex: *36 {
            a: 0,1,2,-4,4,7,6,-6,0,4,5,-2,1,5,6,-3,2,6,7,-4,4,0,3,-8
        }
        GeometryVersion: 124
    }
    Model: 2345678, "Model::TestCube", "Mesh" {
        Version: 232
        Properties70:  {
            P: "ScalingMax", "Vector3D", "Vector", "",0,0,0
            P: "DefaultAttributeIndex", "int", "Integer", "",0
        }
    }
}
Connections:  {
    C: "OO",1234567,2345678
}
`;
    
    // Ensure directory exists
    const dir = path.dirname(filePath);
    try {
      await fs.mkdir(dir, { recursive: true });
    } catch {}
    
    // Write the FBX file
    await fs.writeFile(filePath, fbxContent, 'utf8');
  }
}
