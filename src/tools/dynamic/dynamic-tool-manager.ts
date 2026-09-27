import { Logger } from '../../utils/logging/logger.js';
import { generatedParentToolDefinitions } from '../catalog/capabilities/generated/parent-tool-definitions.generated.js';

export type ToolCategory = 'core' | 'world' | 'gameplay' | 'utility' | 'all';

export interface ToolState {
  name: string;
  category: ToolCategory;
  /** Effective visibility: the tool's own flag AND its category's. */
  enabled: boolean;
}

export interface CategoryState {
  name: ToolCategory;
  enabled: boolean;
  toolCount: number;
  enabledCount: number;
}

const PROTECTED_TOOL_NAMES = new Set(['manage_tools', 'inspect']);
// Rejected outright, not partially applied: the catalog advertises that core
// cannot be disabled, so disabling its unprotected members would falsify that claim.
const PROTECTED_CATEGORY: ToolCategory = 'core';

const log = new Logger('DynamicToolManager');

class DynamicToolManager {
  /** Each tool's own flag and category; effective visibility also needs the category flag. */
  private readonly tools = new Map<string, { category: ToolCategory; enabled: boolean }>();
  private readonly categories = new Map<ToolCategory, boolean>();
  private catalogStateRevision = 0;

  constructor() {
    for (const def of generatedParentToolDefinitions) {
      const category: ToolCategory = def.category ?? 'utility';
      this.tools.set(def.name, { category, enabled: true });
      this.categories.set(category, true);
    }
    log.info(`Initialized with ${this.tools.size} tools across ${this.categories.size} categories`);
  }

  isToolEnabled(toolName: string): boolean {
    const tool = this.tools.get(toolName);
    return tool !== undefined && tool.enabled && (this.categories.get(tool.category) ?? true);
  }

  listTools(): ToolState[] {
    return [...this.tools].map(([name, { category }]) => ({ name, category, enabled: this.isToolEnabled(name) }));
  }

  listCategories(): CategoryState[] {
    const tools = this.listTools();
    return [...this.categories].map(([name, enabled]) => {
      const members = tools.filter((tool) => tool.category === name);
      return { name, enabled, toolCount: members.length, enabledCount: members.filter((tool) => tool.enabled).length };
    });
  }

  enableTools(toolNames: string[]): { enabled: string[]; notFound: string[] } {
    const enabled: string[] = [];
    const notFound: string[] = [];
    this.applyMutation(() => {
      for (const name of toolNames) {
        const tool = this.tools.get(name);
        if (tool === undefined) {
          notFound.push(name);
          continue;
        }
        this.categories.set(tool.category, true);
        tool.enabled = true;
        enabled.push(name);
      }
    });
    if (enabled.length > 0) log.info(`Enabled tools: ${enabled.join(', ')}`);
    if (notFound.length > 0) log.warn(`Tools not found: ${notFound.join(', ')}`);
    return { enabled, notFound };
  }

  disableTools(toolNames: string[]): { disabled: string[]; notFound: string[]; protected: string[] } {
    const disabled: string[] = [];
    const notFound: string[] = [];
    const protectedTools: string[] = [];
    this.applyMutation(() => {
      for (const name of toolNames) {
        const tool = this.tools.get(name);
        if (PROTECTED_TOOL_NAMES.has(name)) protectedTools.push(name);
        else if (tool === undefined) notFound.push(name);
        else {
          tool.enabled = false;
          disabled.push(name);
        }
      }
    });
    if (disabled.length > 0) log.info(`Disabled tools: ${disabled.join(', ')}`);
    if (protectedTools.length > 0) log.warn(`Cannot disable protected tools: ${protectedTools.join(', ')}`);
    return { disabled, notFound, protected: protectedTools };
  }

  enableCategory(category: ToolCategory): { enabled: string[]; notFound: boolean } {
    if (category !== 'all' && !this.categories.has(category)) return { enabled: [], notFound: true };
    const enabled: string[] = [];
    this.applyMutation(() => {
      for (const name of this.categories.keys()) {
        if (category === 'all' || name === category) this.categories.set(name, true);
      }
      for (const [name, tool] of this.tools) {
        if ((category === 'all' || tool.category === category) && !tool.enabled) {
          tool.enabled = true;
          enabled.push(name);
        }
      }
    });
    if (enabled.length > 0) log.info(`Enabled ${category === 'all' ? 'all categories' : `category '${category}'`}: ${enabled.length} tools`);
    return { enabled, notFound: false };
  }

  disableCategory(category: ToolCategory): { disabled: string[]; notFound: boolean; protected: string[] } {
    if (category !== 'all' && !this.categories.has(category)) return { disabled: [], notFound: true, protected: [] };
    const disabled: string[] = [];
    const protectedTools: string[] = [];
    if (category === PROTECTED_CATEGORY) {
      for (const [name, tool] of this.tools) {
        if (tool.category === category && PROTECTED_TOOL_NAMES.has(name)) protectedTools.push(name);
      }
      log.warn(`Cannot disable protected category: ${category}`);
      return { disabled, notFound: false, protected: protectedTools };
    }
    this.applyMutation(() => {
      for (const name of this.categories.keys()) {
        if (category === 'all' ? name !== PROTECTED_CATEGORY : name === category) this.categories.set(name, false);
      }
      for (const [name, tool] of this.tools) {
        if (category !== 'all' && tool.category !== category) continue;
        if (PROTECTED_TOOL_NAMES.has(name) || (category === 'all' && tool.category === PROTECTED_CATEGORY)) {
          protectedTools.push(name);
        } else if (tool.enabled) {
          tool.enabled = false;
          disabled.push(name);
        }
      }
    });
    if (disabled.length > 0) log.info(`Disabled ${category === 'all' ? 'all categories' : `category '${category}'`}: ${disabled.length} tools`);
    return { disabled, notFound: false, protected: protectedTools };
  }

  getStatus(): { totalTools: number; enabledTools: number; disabledTools: number; categories: CategoryState[]; catalogStateRevision: number } {
    const enabledTools = this.listTools().filter((tool) => tool.enabled).length;
    return {
      totalTools: this.tools.size,
      enabledTools,
      disabledTools: this.tools.size - enabledTools,
      categories: this.listCategories(),
      catalogStateRevision: this.catalogStateRevision
    };
  }

  getCatalogStateRevision(): number {
    return this.catalogStateRevision;
  }

  reset(): { enabled: number } {
    let count = 0;
    this.applyMutation(() => {
      for (const tool of this.tools.values()) {
        if (!tool.enabled) {
          tool.enabled = true;
          count++;
        }
      }
      for (const name of this.categories.keys()) this.categories.set(name, true);
    });
    log.info(`Reset ${count} tools to enabled state`);
    return { enabled: count };
  }

  private visibilityFingerprint(): string {
    const tools = [...this.tools].map(([name, tool]) => `${name}=${tool.enabled ? 1 : 0}`);
    const categories = [...this.categories].map(([name, enabled]) => `${name}=${enabled ? 1 : 0}`);
    return `${tools.join(',')}|${categories.join(',')}`;
  }

  // Advances the revision at most once per batch, and only when visibility really
  // moved. Compare-mutate-compare runs synchronously, so no caller can observe a
  // revision before the state change it describes.
  private applyMutation(mutate: () => void): void {
    const before = this.visibilityFingerprint();
    mutate();
    if (this.visibilityFingerprint() !== before) this.catalogStateRevision++;
  }
}

export const dynamicToolManager = new DynamicToolManager();
