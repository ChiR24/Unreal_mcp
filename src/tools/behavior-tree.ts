import { BaseTool } from './base-tool.js';

export class BehaviorTreeTools extends BaseTool {
  private async sendAction(subAction: string, payload: Record<string, unknown> = {}) {
    try {
        const response = await this.sendAutomationRequest('manage_behavior_tree', {
            subAction,
            ...payload
        }) as Record<string, unknown> | null;
        
        return {
            success: (response?.success as boolean) ?? false,
            message: response?.message as string | undefined,
            error: response?.error as string | undefined,
            result: response?.result as unknown
        };
    } catch (e: unknown) {
        return { success: false, error: String(e) };
    }
  }

  async addNode(params: { assetPath: string; nodeType: string; x: number; y: number }) {
    return this.sendAction('add_node', params);
  }

  async connectNodes(params: { assetPath: string; parentNodeId: string; childNodeId: string }) {
    return this.sendAction('connect_nodes', params);
  }

  async removeNode(params: { assetPath: string; nodeId: string }) {
    return this.sendAction('remove_node', params);
  }
  
  async breakConnections(params: { assetPath: string; nodeId: string }) {
    return this.sendAction('break_connections', params);
  }

  async setNodeProperties(params: { assetPath: string; nodeId: string; comment?: string; properties?: Record<string, unknown> }) {
    return this.sendAction('set_node_properties', params);
  }

  async create(params: { name: string; savePath?: string }) {
    return this.sendAction('create', params);
  }
}
