import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';

export class MaterialTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  async createMaterial(name: string, path: string) {
    try {
      if (!name || name.trim() === '') {
        return { success: false, error: 'Material name cannot be empty' };
      }

      if (name.length > 100) {
        return { success: false, error: `Material name too long (${name.length} chars). Maximum is 100 characters.` };
      }

      const invalidChars = /[\s./<>|{}[\]()@#\\]/;
      if (invalidChars.test(name)) {
        const foundChars = name.match(invalidChars);
        return { success: false, error: `Material name contains invalid characters: '${foundChars?.[0]}'. Avoid spaces, dots, slashes, backslashes, brackets, and special symbols.` };
      }

      if (typeof path !== 'string') {
        return { success: false, error: `Invalid path type: expected string, got ${typeof path}` };
      }

      const trimmedPath = path.trim();
      const effectivePath = trimmedPath.length === 0 ? '/Game' : trimmedPath;
      const cleanPath = effectivePath.replace(/\/$/, '');

      if (!cleanPath.startsWith('/Game') && !cleanPath.startsWith('/Engine')) {
        return { success: false, error: `Invalid path: must start with /Game or /Engine, got ${cleanPath}` };
      }

      const normalizedPath = cleanPath.toLowerCase();
      const restrictedPrefixes = ['/engine/restricted', '/engine/generated', '/engine/transient'];
      if (restrictedPrefixes.some(prefix => normalizedPath.startsWith(prefix))) {
        const errorMessage = `Destination path is read-only and cannot be used for material creation: ${cleanPath}`;
        return { success: false, error: errorMessage, message: errorMessage };
      }

      const materialPath = `${cleanPath}/${name}`;
      
      // Use Automation Bridge for material creation
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp: any = await this.automationBridge.sendAutomationRequest('create_material', {
            name,
            destinationPath: cleanPath
          });
          
          if (resp && resp.success !== false) {
            return {
              success: true,
              path: resp.path || resp.result?.path || materialPath,
              message: resp.message || `Material ${name} created at ${materialPath}`,
              warnings: resp.warnings
            };
          }
          
          return {
            success: false,
            error: resp?.error ?? resp?.message ?? 'Failed to create material'
          };
        } catch (err) {
          return {
            success: false,
            error: `Failed to create material: ${err}`
          };
        }
      }
      
      return {
        success: false,
        error: 'Material creation requires Automation Bridge connection'
      };
    } catch (err) {
      return { success: false, error: `Failed to create material: ${err}` };
    }
  }

  async applyMaterialToActor(actorPath: string, materialPath: string, slotIndex = 0) {
    try {
      const result = await this.bridge.setObjectProperty({
        objectPath: actorPath,
        propertyName: `StaticMeshComponent.Materials[${slotIndex}]`,
        value: materialPath
      });

      if (result.success) {
        return {
          success: true,
          message: 'Material applied',
          transport: result.transport,
          warnings: Array.isArray(result.warnings) ? result.warnings : undefined
        };
      }

      return {
        success: false,
        error: result.error ?? `Failed to apply material to ${actorPath}`,
        transport: result.transport
      };
    } catch (err) {
      return { success: false, error: `Failed to apply material: ${err}` };
    }
  }

  async createMaterialInstance(name: string, path: string, parentMaterial: string, parameters?: Record<string, any>) {
    try {
      if (!name || name.trim() === '') {
        return { success: false, error: 'Material instance name cannot be empty' };
      }
      const cleanPath = (path || '/Game').replace(/\/$/, '');
      if (!cleanPath.startsWith('/Game') && !cleanPath.startsWith('/Engine')) {
        return { success: false, error: `Invalid path: must start with /Game or /Engine, got ${cleanPath}` };
      }

      // Plugin-first attempt
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp: any = await this.automationBridge.sendAutomationRequest('create_material_instance', {
            name,
            destinationPath: cleanPath,
            parentMaterial,
            parameters
          });
          if (resp && resp.success !== false) {
            return { success: true, path: resp.path || resp.result?.path || `${cleanPath}/${name}`, message: resp.message || `Material instance ${name} created at ${cleanPath}/${name}`, warnings: resp.warnings } as any;
          }
          const errTxt = String(resp?.error ?? resp?.message ?? '');
          if (!(errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION'))) {
            return { success: false, error: resp?.error ?? resp?.message ?? 'CREATE_MATERIAL_INSTANCE_FAILED' } as any;
          }
        } catch (_e) {
          // fall back to python path below
        }
      }

      // Fallback: use the centralized editor function template for material instance
      const createParams = {
        asset_name: name,
        package_path: cleanPath,
        factory_class: 'MaterialInstanceConstantFactoryNew',
        asset_class: 'MaterialInstanceConstant',
        parent_material: parentMaterial,
        parameters: parameters || {}
      };

      // Try a plugin-native asset creation path via executeEditorFunction
      try {
        const res = await this.bridge.executeEditorFunction('CREATE_ASSET', createParams);
        if (res && res.success !== false) {
          const createdPath = res.path || res.result?.path || `${cleanPath}/${name}`;
          // Attempt to set parent via plugin-native setObjectProperty when available
          if (parentMaterial) {
            try {
              await this.bridge.setObjectProperty({ objectPath: createdPath, propertyName: 'parent', value: parentMaterial, markDirty: true });
            } catch (_) {
              // Ignore failure to set parent - template may have already set it
            }
          }
          // Save asset (plugin-first via SAVE_ASSET template)
          try {
            await this.bridge.executeEditorFunction('SAVE_ASSET', { path: createdPath });
          } catch (_) {}
          return { success: true, path: createdPath, message: `Material instance ${name} created at ${createdPath}` } as any;
        }
        // If plugin indicated unknown action and python fallback not allowed, surface explicit failure
        const errTxt = String((res as any)?.error ?? (res as any)?.message ?? '');
        if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
          return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement create_material_instance' } as any;
        }
        return { success: false, error: (res as any)?.error ?? (res as any)?.message ?? 'CREATE_MATERIAL_INSTANCE_FAILED' } as any;
      } catch (err) {
        // If executeEditorFunction threw due to bridge not connected or python disabled,
        // preserve previous behavior by returning structured failure.
        return { success: false, error: String(err) || 'CREATE_MATERIAL_INSTANCE_FAILED' } as any;
      }
    } catch (err) {
      return { success: false, error: `Failed to create material instance: ${err}` };
    }
  }
}
