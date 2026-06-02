import { ITools } from '../types/tool-interfaces.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('DynamicHandlerRegistry');

type ToolHandler = (args: Record<string, unknown>, tools: ITools) => Promise<unknown>;

export class DynamicHandlerRegistry {
  private handlers = new Map<string, ToolHandler>();

  register(toolName: string, handler: ToolHandler): void {
    const normalizedToolName = this.normalizeToolName(toolName);
    if (this.handlers.has(normalizedToolName)) {
      log.warn(`Handler for tool '${normalizedToolName}' is being overwritten.`);
    }
    this.handlers.set(normalizedToolName, handler);
  }

  getHandler(toolName: string): ToolHandler | undefined {
    return this.handlers.get(this.normalizeToolName(toolName));
  }

  hasHandler(toolName: string): boolean {
    return this.handlers.has(this.normalizeToolName(toolName));
  }

  removeHandler(toolName: string): boolean {
    return this.handlers.delete(this.normalizeToolName(toolName));
  }

  getAllRegisteredTools(): string[] {
    return Array.from(this.handlers.keys()).sort();
  }

  private normalizeToolName(toolName: string): string {
    const normalized = toolName.trim();
    if (!normalized) {
      throw new Error('toolName is required');
    }
    return normalized;
  }
}

export const toolRegistry = new DynamicHandlerRegistry();
