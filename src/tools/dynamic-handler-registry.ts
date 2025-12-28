import { ITools } from '../types/tool-interfaces.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('DynamicHandlerRegistry');

type ToolHandler = (args: any, tools: ITools) => Promise<any>;

export class DynamicHandlerRegistry {
  private handlers = new Map<string, ToolHandler>();

  register(toolName: string, handler: ToolHandler) {
    if (this.handlers.has(toolName)) {
      log.warn(`Handler for tool '${toolName}' is being overwritten.`);
    }
    this.handlers.set(toolName, handler);
  }

  getHandler(toolName: string): ToolHandler | undefined {
    return this.handlers.get(toolName);
  }

  hasHandler(toolName: string): boolean {
    return this.handlers.has(toolName);
  }

  removeHandler(toolName: string): boolean {
    return this.handlers.delete(toolName);
  }

  getAllRegisteredTools(): string[] {
    return Array.from(this.handlers.keys());
  }
}

// Global registry instance
export const toolRegistry = new DynamicHandlerRegistry();
