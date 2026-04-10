import { describe, expect, it } from 'vitest';

import { consolidatedToolDefinitions } from './consolidated-tool-definitions.js';

type ObjectSchema = {
  type?: string;
  description?: string;
  properties?: Record<string, unknown>;
  items?: unknown;
};

function getToolDefinition(name: string) {
  const tool = consolidatedToolDefinitions.find((entry) => entry.name === name);
  expect(tool, `Expected tool definition for ${name}`).toBeDefined();
  return tool!;
}

function getActionEnum(toolName: string): string[] {
  const tool = getToolDefinition(toolName);
  const inputSchema = tool.inputSchema as ObjectSchema;
  const actionSchema = inputSchema.properties?.action as { enum?: string[] } | undefined;
  expect(Array.isArray(actionSchema?.enum), `Expected action enum for ${toolName}`).toBe(true);
  return actionSchema!.enum!;
}

function getOutputSchema(toolName: string): ObjectSchema {
  const tool = getToolDefinition(toolName);
  return tool.outputSchema as ObjectSchema;
}

describe('consolidated inspection contract', () => {
  it('keeps merged graph inspection actions exposed on manage_blueprint', () => {
    const actionEnum = getActionEnum('manage_blueprint');
    const outputSchema = getOutputSchema('manage_blueprint');

    expect(actionEnum).toContain('get_graph_details');
    expect(actionEnum).toContain('get_pin_details');
    expect(actionEnum).toContain('get_node_details_batch');
    expect(actionEnum).toContain('get_graph_review_summary');
    expect(outputSchema.properties?.blueprint).toBeDefined();
    expect(outputSchema.properties?.nodes).toBeDefined();
    expect(outputSchema.properties?.shown).toBeDefined();
    expect(outputSchema.properties?.totalRequested).toBeDefined();
    expect(outputSchema.properties?.truncated).toBeDefined();
    expect(outputSchema.properties?.nextCursor).toBeDefined();
    expect(outputSchema.properties?.entryNodes).toBeDefined();
    expect(outputSchema.properties?.commentGroups).toBeDefined();
    expect(outputSchema.properties?.highFanOutNodes).toBeDefined();
    expect(outputSchema.properties?.reviewTargets).toBeDefined();
    expect(outputSchema.properties?.connectionCount).toBeDefined();
  });

  it('exposes get_widget_tree on manage_widget_authoring', () => {
    const actionEnum = getActionEnum('manage_widget_authoring');

    expect(actionEnum).toContain('get_widget_tree');
  });

  it('exposes get_widget_designer_state on manage_widget_authoring', () => {
    const actionEnum = getActionEnum('manage_widget_authoring');

    expect(actionEnum).toContain('get_widget_designer_state');
  });

  it('advertises recursive widgetTree output metadata for widget inspection', () => {
    const tool = getToolDefinition('manage_widget_authoring');
    const outputSchema = tool.outputSchema as ObjectSchema;
    const widgetTreeSchema = outputSchema.properties?.widgetTree as ObjectSchema | undefined;

    expect(widgetTreeSchema).toBeDefined();
    expect(widgetTreeSchema?.type).toBe('object');
    expect(widgetTreeSchema?.description).toMatch(/widget tree/i);

    const childrenSchema = widgetTreeSchema?.properties?.children as ObjectSchema | undefined;
    expect(childrenSchema?.type).toBe('array');
    expect(childrenSchema?.items).toBeDefined();

    expect(outputSchema.properties?.widgetCount).toBeDefined();
    expect(outputSchema.properties?.rootWidgetName).toBeDefined();
  });

  it('advertises live Designer state output metadata for combined widget inspection', () => {
    const outputSchema = getOutputSchema('manage_widget_authoring');

    expect(outputSchema.properties?.widgetTree).toBeDefined();
    expect(outputSchema.properties?.currentMode).toBeDefined();
    expect(outputSchema.properties?.designerViewFound).toBeDefined();
    expect(outputSchema.properties?.liveEditorContextFound).toBeDefined();
    expect(outputSchema.properties?.selectedWidgetCount).toBeDefined();
    expect(outputSchema.properties?.selectedWidgets).toBeDefined();
  });

  it('advertises layout metadata for geometry-aware Designer-state readback', () => {
    const outputSchema = getOutputSchema('manage_widget_authoring');
    const widgetTreeSchema = outputSchema.properties?.widgetTree as ObjectSchema | undefined;
    const widgetTreeProperties = widgetTreeSchema?.properties as Record<string, unknown> | undefined;
    const widgetTreeLayoutSchema = widgetTreeProperties?.layout as ObjectSchema | undefined;

    expect(widgetTreeLayoutSchema).toBeDefined();
    expect(widgetTreeLayoutSchema?.properties?.slotClass).toBeDefined();
    expect(widgetTreeLayoutSchema?.properties?.anchors).toBeDefined();
    expect(widgetTreeLayoutSchema?.properties?.designerBounds).toBeDefined();
    expect(widgetTreeLayoutSchema?.properties?.zOrder).toBeDefined();

    const widgetTreeChildrenSchema = widgetTreeProperties?.children as ObjectSchema | undefined;
    const widgetTreeChildSchema = widgetTreeChildrenSchema?.items as ObjectSchema | undefined;
    const childLayoutSchema = widgetTreeChildSchema?.properties?.layout as ObjectSchema | undefined;

    expect(childLayoutSchema).toBeDefined();
    expect(childLayoutSchema?.properties?.slotClass).toBeDefined();
    expect(childLayoutSchema?.properties?.anchors).toBeDefined();
    expect(childLayoutSchema?.properties?.designerBounds).toBeDefined();
    expect(childLayoutSchema?.properties?.zOrder).toBeDefined();

    const selectedWidgetsSchema = outputSchema.properties?.selectedWidgets as ObjectSchema | undefined;
    const selectedWidgetSchema = selectedWidgetsSchema?.items as ObjectSchema | undefined;
    const selectedWidgetLayoutSchema = selectedWidgetSchema?.properties?.layout as ObjectSchema | undefined;
    const selectedWidgetAnchors = selectedWidgetLayoutSchema?.properties?.anchors as ObjectSchema | undefined;

    expect(selectedWidgetLayoutSchema).toBeDefined();
    expect(selectedWidgetLayoutSchema?.properties?.slotClass).toBeDefined();
    expect(selectedWidgetLayoutSchema?.properties?.designerBounds).toBeDefined();
    expect(selectedWidgetAnchors?.properties?.minimum).toBeDefined();
    expect(selectedWidgetAnchors?.properties?.maximum).toBeDefined();
    expect(selectedWidgetLayoutSchema?.properties?.zOrder).toBeDefined();
  });
});