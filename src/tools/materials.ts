import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('MaterialTools');

export class MaterialTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) { }

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  async createMaterial(name: string, path: string) {
    try {
      // Support unified assetPath in 'name' argument
      let effectiveName = name;
      let effectivePath = path;

      if (name && name.includes('/')) {
        const lastSlashIndex = name.lastIndexOf('/');
        effectiveName = name.substring(lastSlashIndex + 1);
        const derivedPath = name.substring(0, lastSlashIndex);
        // If path was not provided or is just root, prefer the derived path
        if (!path || path === '/Game' || path === '') {
          effectivePath = derivedPath;
        }
      }

      if (!effectiveName || effectiveName.trim() === '') {
        return { success: false, error: 'Material name cannot be empty' };
      }

      if (effectiveName.length > 100) {
        return { success: false, error: `Material name too long (${effectiveName.length} chars). Maximum is 100 characters.` };
      }

      const invalidChars = /[\s./<>|{}[\]()@#\\]/;
      if (invalidChars.test(effectiveName)) {
        const foundChars = effectiveName.match(invalidChars);
        return { success: false, error: `Material name contains invalid characters: '${foundChars?.[0]}'. Avoid spaces, dots, slashes, backslashes, brackets, and special symbols.` };
      }

      const trimmedPath = (effectivePath || '').trim();
      const cleanPathStr = trimmedPath.length === 0 ? '/Game' : trimmedPath;
      const cleanPath = cleanPathStr.replace(/\/$/, '');

      if (!cleanPath.startsWith('/Game') && !cleanPath.startsWith('/Engine')) {
        return { success: false, error: `Invalid path: must start with /Game or /Engine, got ${cleanPath}` };
      }

      const normalizedPath = cleanPath.toLowerCase();
      const restrictedPrefixes = ['/engine/restricted', '/engine/generated', '/engine/transient'];
      if (restrictedPrefixes.some(prefix => normalizedPath.startsWith(prefix))) {
        const errorMessage = `Destination path is read-only and cannot be used for material creation: ${cleanPath}`;
        return { success: false, error: errorMessage, message: errorMessage };
      }

      const materialPath = `${cleanPath}/${effectiveName}`;

      // Use Automation Bridge for material creation
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp = await this.automationBridge.sendAutomationRequest('create_material', {
            name: effectiveName,
            destinationPath: cleanPath
          }) as Record<string, unknown>;

          if (resp && resp.success !== false) {
            const result = (resp.result ?? {}) as Record<string, unknown>;
            return {
              success: true,
              path: (resp.path || result.path || materialPath) as string,
              message: (resp.message || `Material ${name} created at ${materialPath}`) as string,
              warnings: resp.warnings as string[] | undefined
            };
          }

          return {
            success: false,
            error: (resp?.error ?? resp?.message ?? 'Failed to create material') as string
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

  async createMaterialInstance(name: string, path: string, parentMaterial: string, parameters?: Record<string, unknown>) {
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
          const resp = await this.automationBridge.sendAutomationRequest('create_material_instance', {
            name,
            destinationPath: cleanPath,
            parentMaterial,
            parameters
          }) as Record<string, unknown>;
          if (resp && resp.success !== false) {
            const result = (resp.result ?? {}) as Record<string, unknown>;
            return { success: true, path: (resp.path || result.path || `${cleanPath}/${name}`) as string, message: (resp.message || `Material instance ${name} created at ${cleanPath}/${name}`) as string, warnings: resp.warnings };
          }
          const errTxt = String(resp?.error ?? resp?.message ?? '');
          if (!(errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION'))) {
            return { success: false, error: (resp?.error ?? resp?.message ?? 'CREATE_MATERIAL_INSTANCE_FAILED') as string };
          }
        } catch (e) {
          // If the error is simply generic or unknown action, we fall back.
          // But if it's a specific error, we might log it.
          // For now, let's at least not silence everything.
          const msg = e instanceof Error ? e.message : String(e);
          if (!msg.includes('unknown') && !msg.includes('UNKNOWN_PLUGIN_ACTION')) {
            log.warn(`Plugin create_material_instance failed with specific error (falling back to python): ${msg}`);
          }
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
          const resResult = (res.result ?? {}) as Record<string, unknown>;
          const createdPath = (res.path || resResult.path || `${cleanPath}/${name}`) as string;
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
          } catch (_) { /* Best-effort save - continue even if save fails */ }
          return { success: true, path: createdPath, message: `Material instance ${name} created at ${createdPath}` };
        }
        // If plugin indicated unknown action and python fallback not allowed, surface explicit failure
        const resRecord = res as Record<string, unknown>;
        const errTxt = String(resRecord?.error ?? resRecord?.message ?? '');
        if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
          return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement create_material_instance' };
        }
        return { success: false, error: (resRecord?.error ?? resRecord?.message ?? 'CREATE_MATERIAL_INSTANCE_FAILED') as string };
      } catch (err) {
        // If executeEditorFunction threw due to bridge not connected or python disabled,
        // preserve previous behavior by returning structured failure.
        return { success: false, error: String(err) || 'CREATE_MATERIAL_INSTANCE_FAILED' };
      }
    } catch (err) {
      return { success: false, error: `Failed to create material instance: ${err}` };
    }
  }
  async addNode(params: {
    materialPath: string;
    nodeType: string;
    x: number;
    y: number;
    name?: string; // For parameter nodes
    timeoutMs?: number;
  }) {
    if (!params.materialPath) return { success: false, error: 'INVALID_MATERIAL_PATH', message: 'Material path is required' } as const;
    if (!params.nodeType) return { success: false, error: 'INVALID_NODE_TYPE', message: 'Node type is required' } as const;

    const res = await this.automationBridge?.sendAutomationRequest('manage_material_graph', {
      subAction: 'add_node',
      assetPath: params.materialPath,
      nodeType: params.nodeType,
      x: params.x,
      y: params.y,
      name: params.name
    });
    return res;
  }

  async removeNode(params: {
    materialPath: string;
    nodeId: string;
    timeoutMs?: number;
  }) {
    if (!params.materialPath) return { success: false, error: 'INVALID_MATERIAL_PATH', message: 'Material path is required' } as const;
    if (!params.nodeId) return { success: false, error: 'INVALID_NODE_ID', message: 'Node ID is required' } as const;

    const res = await this.automationBridge?.sendAutomationRequest('manage_material_graph', {
      subAction: 'remove_node',
      assetPath: params.materialPath,
      nodeId: params.nodeId
    });
    return res;
  }

  async connectNodes(params: {
    materialPath: string;
    sourceNodeId: string;
    targetNodeId: string; // Use 'Main' for main material node
    inputName: string; // Input pin name on target
    timeoutMs?: number;
  }) {
    if (!params.materialPath) return { success: false, error: 'INVALID_MATERIAL_PATH', message: 'Material path is required' } as const;
    if (!params.sourceNodeId) return { success: false, error: 'INVALID_SOURCE_NODE', message: 'Source node ID is required' } as const;
    if (!params.inputName) return { success: false, error: 'INVALID_INPUT_NAME', message: 'Input name is required' } as const;

    const res = await this.automationBridge?.sendAutomationRequest('manage_material_graph', {
      subAction: 'connect_nodes',
      assetPath: params.materialPath,
      sourceNodeId: params.sourceNodeId,
      targetNodeId: params.targetNodeId,
      inputName: params.inputName
    });
    return res;
  }

  async breakConnections(params: {
    materialPath: string;
    nodeId: string;
    pinName?: string;
    timeoutMs?: number;
  }) {
    if (!params.materialPath) return { success: false, error: 'INVALID_MATERIAL_PATH', message: 'Material path is required' } as const;
    if (!params.nodeId) return { success: false, error: 'INVALID_NODE_ID', message: 'Node ID is required' } as const;

    const res = await this.automationBridge?.sendAutomationRequest('manage_material_graph', {
      subAction: 'break_connections',
      assetPath: params.materialPath,
      nodeId: params.nodeId,
      pinName: params.pinName
    });
    return res;
  }

  async getNodeDetails(params: {
    materialPath: string;
    nodeId?: string; // If omitted, lists all nodes
    timeoutMs?: number;
  }) {
    if (!params.materialPath) return { success: false, error: 'INVALID_MATERIAL_PATH', message: 'Material path is required' } as const;

    const res = await this.automationBridge?.sendAutomationRequest('manage_material_graph', {
      subAction: 'get_node_details',
      assetPath: params.materialPath,
      nodeId: params.nodeId
    });
    return res;
  }
}
