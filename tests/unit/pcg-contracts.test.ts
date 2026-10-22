import { describe, expect, it } from 'vitest';
import { MANAGE_PCG_SOURCES, MANAGE_PCG_UNFOLDED_SOURCES } from '../../src/tools/catalog/capabilities/records/world/manage-pcg.index.js';

const byAction = (action: string) => MANAGE_PCG_SOURCES.find(record => record.legacyIds.some(pair => pair.action === action));

describe('PCG discovery and reliable editing contracts', () => {
  it('adds a read family without changing the original folded families', () => {
    expect(MANAGE_PCG_SOURCES.filter(record => record.routing.dispatchBy?.param === 'view')).toHaveLength(1);
    for (const action of ['add_pcg_node', 'edit_pcg_graph', 'execute_pcg_graph']) expect(byAction(action)).toBeDefined();
    for (const action of ['list_pcg_node_types', 'get_pcg_settings_schema', 'get_pcg_graph', 'get_pcg_editor_graph', 'validate_pcg_graph']) {
      const record = byAction(action);
      expect(record?.behavior.effect).toBe('read');
      expect(record?.policy).toMatchObject({ requiredScope: 'read', consent: 'none' });
      expect(record?.routing.dispatchBy?.param).toBe('view');
      expect(record?.schemas.input.properties.limit).toMatchObject({ type: 'integer', minimum: 1, maximum: 100 });
    }
  });

  it('preserves the numeric scheduling receipt and does not pretend it is completion', () => {
    const execute = byAction('execute_pcg_graph');
    expect(execute?.schemas.output.properties.taskId).toMatchObject({ type: 'number' });
    expect(execute?.behavior).toMatchObject({ effect: 'write', safeToRetry: false });
    expect(execute?.schemas.input.required).toContain('graphPath');
    expect(execute?.schemas.output.properties.operationId).toMatchObject({ type: 'string' });
    expect(execute?.schemas.output.properties.operationTracking).toMatchObject({ type: 'boolean' });
    const generate = byAction('generate_pcg_graph');
    expect(generate?.behavior).toMatchObject({ effect: 'write', longRunning: true, safeToRetry: false });
    expect(generate?.schemas.input.required).toEqual(['action', 'componentPath']);
    expect(generate?.schemas.input.properties.timeoutSeconds).toMatchObject({ minimum: 0.1, maximum: 600 });
    expect(generate?.schemas.output.properties).toHaveProperty('engineStillRunning');
    expect(generate?.schemas.output.properties).toHaveProperty('diagnosticsAvailable');
  });

  it('requires the graph locator only when adding a graph parameter', () => {
    const add = MANAGE_PCG_UNFOLDED_SOURCES.find(record => record.legacyIds.some(pair => pair.action === 'add_pcg_parameter'));
    expect(add?.schemas.input.required).toContain('graphPath');
    for (const action of ['set_pcg_parameter', 'rename_pcg_parameter', 'reset_pcg_parameter_override']) {
      const record = MANAGE_PCG_UNFOLDED_SOURCES.find(source => source.legacyIds.some(pair => pair.action === action));
      expect(record?.schemas.input.required).not.toContain('graphPath');
    }
  });

  it('keeps graph concurrency checks optional for all existing node edit aliases', () => {
    const edits = MANAGE_PCG_UNFOLDED_SOURCES.filter(record => {
      const action = record.legacyIds[0]?.action ?? '';
      return action.startsWith('add_') || action === 'set_pcg_node_settings' || action === 'connect_pcg_pins';
    });
    expect(edits.length).toBeGreaterThan(20);
    for (const record of edits) {
      expect(record.schemas.input.properties.expectedGraphRevision).toMatchObject({ type: 'string' });
      expect(record.schemas.input.required).not.toContain('expectedGraphRevision');
      expect(record.schemas.output.properties).toMatchObject({
        committed: { type: 'boolean' }, saved: { type: 'boolean' }, rolledBack: { type: 'boolean' }, revision: { type: 'string' },
      });
    }
  });

  it('does not mix destructive lifecycle or mutation actions into write/read families', () => {
    for (const action of ['disconnect_pcg_pins', 'remove_pcg_node', 'batch_edit_pcg_graph_destructive', 'remove_pcg_parameter', 'cleanup_pcg_component', 'cancel_pcg_operation']) {
      expect(byAction(action)?.policy).toMatchObject({ requiredScope: 'destructive', consent: 'explicit' });
      expect(byAction(action)?.behavior.effect).toBe('destructive');
    }
    expect(byAction('batch_edit_pcg_graph')?.schemas.input.required).toContain('expectedGraphRevision');
    expect(byAction('batch_edit_pcg_graph')?.policy).toMatchObject({ requiredScope: 'write', consent: 'none' });
    for (const action of ['get_pcg_parameters', 'get_pcg_operation_status', 'get_pcg_component_results']) {
      expect(byAction(action)?.policy).toMatchObject({ requiredScope: 'read', consent: 'none' });
    }
    for (const action of ['add_pcg_parameter', 'set_pcg_parameter', 'rename_pcg_parameter', 'reset_pcg_parameter_override']) {
      expect(byAction(action)?.routing.dispatchBy?.param).toBe('parameterEdit');
      expect(byAction(action)?.policy).toMatchObject({ requiredScope: 'write', consent: 'none' });
    }
    expect(byAction('cleanup_pcg_component')).toBe(byAction('cancel_pcg_operation'));
    expect(byAction('cleanup_pcg_component')).not.toBe(byAction('generate_pcg_graph'));
  });

  it('retains typed aliases and the original selector defaults', () => {
    expect(byAction('add_static_mesh_spawner')).toBe(byAction('add_pcg_node'));
    expect(byAction('connect_pcg_pins')).toBe(byAction('edit_pcg_graph'));
    expect(byAction('add_pcg_node')?.schemas.input.properties.nodeKind).toMatchObject({ default: 'node' });
    expect(byAction('edit_pcg_graph')?.schemas.input.required).toContain('edit');
    expect(byAction('get_pcg_graph')?.schemas.output.properties).toHaveProperty('revision');
  });
});
