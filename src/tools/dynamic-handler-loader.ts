import fs from 'node:fs';
import path from 'node:path';
import { Logger } from '../utils/logger.js';
import { config } from '../config.js';
import { consolidatedToolDefinitions } from './consolidated-tool-definitions.js';
import { toolRegistry } from './dynamic-handler-registry.js';
import { dynamicToolManager } from './dynamic-tool-manager.js';
import { executeAutomationRequest } from './handlers/common-handlers.js';

const log = new Logger('DynamicHandlerLoader');

export function loadDynamicHandlersFromJson() {
  let configDir = '';
  
  if (config.UE_PROJECT_PATH) {
    let p = config.UE_PROJECT_PATH;
    if (p.toLowerCase().endsWith('.uproject')) {
      p = path.dirname(p);
    }
    configDir = path.join(p, 'Config');
  } else {
    // Fallback if not provided
    configDir = path.join(process.cwd(), 'Config');
  }

  const jsonPath = path.join(configDir, 'McpHandlers.json');
  
  if (!fs.existsSync(jsonPath)) {
    return;
  }

  try {
    const content = fs.readFileSync(jsonPath, 'utf8');
    const data = JSON.parse(content);
    
    if (data.handlers && Array.isArray(data.handlers)) {
      let addedCount = 0;
      
      for (const handler of data.handlers) {
        if (handler.action && handler.type && handler.target) {
          const actionName = handler.action as string;
          
          // Add to tool schema definitions if it doesn't exist
          if (!consolidatedToolDefinitions.some(t => t.name === actionName)) {
            const def = {
              name: actionName,
              description: handler.description || `Dynamic ${handler.type} handler: ${handler.target}`,
              category: 'utility',
              inputSchema: handler.parameters || {
                type: 'object',
                properties: {},
                required: []
              }
            };
            consolidatedToolDefinitions.push(def);
            dynamicToolManager.registerDynamicTool(def);
            
            // Register handler in TS toolRegistry
            toolRegistry.register(actionName, async (args, tools) => {
              // We just forward the call to UE. The C++ Extensibility framework 
              // we wrote will catch the ActionName and execute it.
              return executeAutomationRequest(tools, actionName, args);
            });
            
            addedCount++;
          }
        }
      }
      
      if (addedCount > 0) {
        log.info(`Loaded ${addedCount} dynamic handlers from McpHandlers.json`);
      }
    }
  } catch (e) {
    log.warn(`Failed to parse McpHandlers.json: ${e}`);
  }
}
