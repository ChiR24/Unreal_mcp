/**
 * Dynamic Tools Handler
 * 
 * Handles the manage_tools MCP tool for dynamic tool loading/unloading.
 */

import { ITools } from '../../types/tool-interfaces.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import { dynamicToolManager, type ToolCategory } from '../dynamic-tool-manager.js';

/**
 * Handle manage_tools actions
 */
export async function handleManageToolsTools(
  action: string,
  args: Record<string, unknown>,
  _tools: ITools
): Promise<Record<string, unknown>> {
  try {
    // Helper to safely extract string array
    const getStringArray = (key: string): string[] => {
      const val = args[key];
      if (Array.isArray(val)) {
        return val.filter((v): v is string => typeof v === 'string');
      }
      return [];
    };

    // Helper to safely extract string
    const getString = (key: string): string | undefined => {
      const val = args[key];
      return typeof val === 'string' ? val : undefined;
    };

    switch (action) {
      // ===== List All Tools =====
      case 'list_tools': {
        const toolStates = dynamicToolManager.listTools();
        const tools = toolStates.map(state => ({
          name: state.name,
          enabled: state.enabled,
          category: state.category,
          description: state.description.substring(0, 100) + (state.description.length > 100 ? '...' : '')
        }));

        const status = dynamicToolManager.getStatus();

        return ResponseFactory.success({
          tools,
          totalTools: status.totalTools,
          enabledCount: status.enabledTools,
          disabledCount: status.disabledTools
        }, `Listed ${tools.length} tools (${status.enabledTools} enabled, ${status.disabledTools} disabled)`);
      }

      // ===== List Categories =====
      case 'list_categories': {
        const categories = dynamicToolManager.listCategories();
        
        return ResponseFactory.success({
          categories: categories.map(cat => ({
            name: cat.name,
            enabled: cat.enabled,
            toolCount: cat.toolCount,
            enabledCount: cat.enabledCount
          })),
          totalCategories: categories.length
        }, `Listed ${categories.length} categories`);
      }

      // ===== Enable Specific Tools =====
      case 'enable_tools': {
        const toolNames = getStringArray('tools');
        
        if (toolNames.length === 0) {
          return ResponseFactory.error('No tools specified. Provide tools array.', 'MISSING_TOOLS');
        }

        const result = dynamicToolManager.enableTools(toolNames);

        if (result.notFound.length > 0) {
          return ResponseFactory.success({
            enabled: result.enabled,
            notFound: result.notFound
          }, `Enabled ${result.enabled.length} tools. ${result.notFound.length} not found.`);
        }

        return ResponseFactory.success({
          enabled: result.enabled
        }, `Enabled ${result.enabled.length} tools`);
      }

      // ===== Disable Specific Tools =====
      case 'disable_tools': {
        const toolNames = getStringArray('tools');
        
        if (toolNames.length === 0) {
          return ResponseFactory.error('No tools specified. Provide tools array.', 'MISSING_TOOLS');
        }

        const result = dynamicToolManager.disableTools(toolNames);

        if (result.protected.length > 0 && result.disabled.length === 0) {
          return ResponseFactory.error(
            `Cannot disable protected tools: ${result.protected.join(', ')}`,
            'PROTECTED_TOOLS'
          );
        }

        const messages: string[] = [];
        if (result.disabled.length > 0) messages.push(`Disabled ${result.disabled.length} tools`);
        if (result.notFound.length > 0) messages.push(`${result.notFound.length} not found`);
        if (result.protected.length > 0) messages.push(`${result.protected.length} protected`);

        return ResponseFactory.success({
          disabled: result.disabled,
          notFound: result.notFound,
          protected: result.protected
        }, messages.join('. '));
      }

      // ===== Enable Category =====
      case 'enable_category': {
        const category = getString('category') as ToolCategory | undefined;
        
        if (!category) {
          return ResponseFactory.error('No category specified.', 'MISSING_CATEGORY');
        }

        const validCategories: ToolCategory[] = ['core', 'world', 'authoring', 'gameplay', 'utility'];
        if (!validCategories.includes(category)) {
          return ResponseFactory.error(
            `Invalid category '${category}'. Valid: ${validCategories.join(', ')}`,
            'INVALID_CATEGORY'
          );
        }

        const result = dynamicToolManager.enableCategory(category);

        if (result.notFound) {
          return ResponseFactory.error(`Category '${category}' not found`, 'CATEGORY_NOT_FOUND');
        }

        return ResponseFactory.success({
          category,
          enabled: result.enabled
        }, `Enabled category '${category}' (${result.enabled.length} tools)`);
      }

      // ===== Disable Category =====
      case 'disable_category': {
        const category = getString('category') as ToolCategory | undefined;
        
        if (!category) {
          return ResponseFactory.error('No category specified.', 'MISSING_CATEGORY');
        }

        const validCategories: ToolCategory[] = ['core', 'world', 'authoring', 'gameplay', 'utility'];
        if (!validCategories.includes(category)) {
          return ResponseFactory.error(
            `Invalid category '${category}'. Valid: ${validCategories.join(', ')}`,
            'INVALID_CATEGORY'
          );
        }

        const result = dynamicToolManager.disableCategory(category);

        if (result.notFound) {
          return ResponseFactory.error(`Category '${category}' not found`, 'CATEGORY_NOT_FOUND');
        }

        if (result.protected.length > 0 && result.disabled.length === 0) {
          return ResponseFactory.error(
            `Cannot fully disable protected category '${category}'. Protected tools: ${result.protected.join(', ')}`,
            'PROTECTED_CATEGORY'
          );
        }

        return ResponseFactory.success({
          category,
          disabled: result.disabled,
          protected: result.protected
        }, `Disabled category '${category}' (${result.disabled.length} tools disabled)`);
      }

      // ===== Get Status =====
      case 'get_status': {
        const status = dynamicToolManager.getStatus();
        
        return ResponseFactory.success({
          totalTools: status.totalTools,
          enabledTools: status.enabledTools,
          disabledTools: status.disabledTools,
          categories: status.categories.map(cat => ({
            name: cat.name,
            enabled: cat.enabled,
            toolCount: cat.toolCount,
            enabledCount: cat.enabledCount
          }))
        }, `${status.enabledTools}/${status.totalTools} tools enabled`);
      }

      // ===== Reset =====
      case 'reset': {
        const result = dynamicToolManager.reset();
        
        return ResponseFactory.success({
          enabled: result.enabled
        }, `Reset complete. ${result.enabled} tools re-enabled.`);
      }

      default:
        return ResponseFactory.error(
          `Unknown action: ${action}. Available: list_tools, list_categories, enable_tools, disable_tools, enable_category, disable_category, get_status, reset`,
          'UNKNOWN_ACTION'
        );
    }
  } catch (error) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Dynamic tools error: ${err.message}`, 'TOOLS_ERROR');
  }
}
