import { beforeEach, describe, expect, it } from 'vitest';
import { dynamicToolManager } from './dynamic-tool-manager.js';

describe('dynamicToolManager', () => {
  beforeEach(() => {
    dynamicToolManager.reset();
  });

  it('reopens a disabled category when enabling an individual tool', () => {
    dynamicToolManager.disableCategory('gameplay');

    expect(dynamicToolManager.isToolEnabled('manage_ai')).toBe(false);

    const result = dynamicToolManager.enableTools(['manage_ai']);

    expect(result.enabled).toContain('manage_ai');
    expect(dynamicToolManager.isToolEnabled('manage_ai')).toBe(true);
    expect(dynamicToolManager.isToolEnabled('manage_gas')).toBe(false);
  });

  it('reports disabled categories without hiding protected core tools', () => {
    dynamicToolManager.disableCategory('gameplay');

    expect(dynamicToolManager.listCategories()).toContainEqual(
      expect.objectContaining({ name: 'gameplay', enabled: false, enabledCount: 0 })
    );

    dynamicToolManager.disableCategory('core');

    expect(dynamicToolManager.isToolEnabled('manage_tools')).toBe(true);
    expect(dynamicToolManager.isToolEnabled('inspect')).toBe(true);
    expect(dynamicToolManager.listCategories()).toContainEqual(
      expect.objectContaining({ name: 'core', enabled: true })
    );
  });
});
