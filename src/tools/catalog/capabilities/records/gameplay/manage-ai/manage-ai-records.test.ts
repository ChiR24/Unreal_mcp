import { describe, expect, it } from 'vitest';

import { createCapabilityRecord } from '../../../index.js';
import { MANAGE_AI_SOURCES } from './records.js';

/**
 * The 86 parameters the parameter-combination audit reported as
 * `extraParameters` for manage_ai: names exercised by the manage_ai test suites
 * that the compact-parent stub had stopped declaring. Every one must be back on
 * at least one action, and on an action whose handler actually reads it.
 */
const UNDECLARED_86 = [
  'agentHeight', 'agentMaxSlope', 'agentRadius', 'agentStepHeight', 'areaClass', 'areaCost',
  'bCreateBoxObstacle', 'baseObjectClass', 'behaviorTreePath', 'blackboardPath',
  'broadcastInterval', 'broadcastRadius', 'cellHeight', 'cellSize', 'childNodeId', 'comment',
  'componentName', 'compositeType', 'configPath', 'contextType', 'controllerPath', 'damageConfig',
  'decoratorType', 'definitionPath', 'direction', 'disabledAreaClass', 'dominantSense',
  'enableDamage', 'enableHearing', 'enableSight', 'enabled', 'enabledAreaClass', 'endPoint',
  'failsafeExtent', 'focusActorName', 'fromState', 'generatorSettings', 'generatorType',
  'hearingConfig', 'hearingRange', 'isInstanceSynced', 'keyName', 'keyType', 'linkEnabled',
  'linkType', 'location', 'loseSightRadius', 'maxSimplificationError', 'mergeRegionSize',
  'minRegionArea', 'nodeClass', 'nodeId', 'nodeType', 'obstacleAreaClass', 'obstacleExtent',
  'obstacleOffset', 'offset', 'parentNodeId', 'parentStateName', 'peripheralVisionAngle',
  'queryPath', 'rotation', 'save', 'savePath', 'serviceType', 'sightConfig', 'sightRadius',
  'slotIndex', 'snapRadius', 'spawnCount', 'startPoint', 'stateName', 'stateTreePath', 'stateType',
  'subnodeType', 'taskType', 'teamId', 'testIndex', 'testSettings', 'testType', 'tileSizeUU',
  'toState', 'triggerType', 'value', 'x', 'y',
] as const;

const records = MANAGE_AI_SOURCES.map((source) => createCapabilityRecord(source));

const actionOf = (record: (typeof records)[number]): string => record.legacyIds[0].action;
const propsOf = (record: (typeof records)[number]): string[] =>
  Object.keys(record.schemas.input.properties as Record<string, unknown>);

const byAction = new Map(records.map((record) => [actionOf(record), record]));
const propsFor = (action: string): string[] => {
  const record = byAction.get(action);
  if (record === undefined) throw new Error(`missing manage_ai record: ${action}`);
  return propsOf(record);
};
const requiredFor = (action: string): string[] => {
  const record = byAction.get(action);
  if (record === undefined) throw new Error(`missing manage_ai record: ${action}`);
  return [...record.schemas.input.required].sort();
};

describe('manage_ai capability records', () => {
  it('declares exactly 62 records with unique actions and parses each one', () => {
    expect(records).toHaveLength(62);
    expect(byAction.size).toBe(62);
    for (const record of records) {
      expect(record.routing.parentTool).toBe('manage_ai');
      expect(record.discovery.family).toBe('ai');
    }
  });

  it('restores all 86 previously undeclared parameters across the parent union', () => {
    const union = new Set(records.flatMap(propsOf));
    const stillMissing = UNDECLARED_86.filter((name) => !union.has(name));
    expect(stillMissing).toEqual([]);
  });

  it('does not apply the parent union wholesale to each action', () => {
    const union = new Set(records.flatMap(propsOf));
    // Every action is strictly narrower than the union it contributes to.
    for (const record of records) {
      expect(propsOf(record).length).toBeLessThan(union.size);
    }
    // The widest action still declares well under half the union.
    const widest = Math.max(...records.map((record) => propsOf(record).length));
    expect(widest).toBeLessThan(union.size / 2);
  });

  it('keeps navigation parameters off Behavior Tree actions and vice versa', () => {
    // buildRecord strips `action` from the per-record input schema (helpers.ts),
    // so a record's properties are exactly its handler-observable parameters.
    expect(propsFor('add_task_node')).toEqual(['behaviorTreePath', 'taskType']);
    expect(propsFor('set_nav_agent_properties')).not.toContain('behaviorTreePath');
    expect(propsFor('configure_nav_mesh_settings')).not.toContain('taskType');
    expect(propsFor('add_blackboard_key')).not.toContain('areaClass');
  });

  it('matches the required sets enforced by the TypeScript handlers', () => {
    expect(requiredFor('add_blackboard_key')).toEqual(['blackboardPath', 'keyName', 'keyType']);
    expect(requiredFor('assign_behavior_tree')).toEqual(['behaviorTreePath', 'controllerPath']);
    // assign_blackboard accepts controllerPath OR behaviorTreePath, so only the
    // unconditional blackboardPath check is a schema-level requirement.
    expect(requiredFor('assign_blackboard')).toEqual(['blackboardPath']);
    expect(requiredFor('add_state_tree_transition')).toEqual(['fromState', 'stateTreePath', 'toState']);
    expect(requiredFor('setup_perception')).toEqual(['blueprintPath']);
    // Navigation handlers validate nothing themselves.
    expect(requiredFor('rebuild_navigation')).toEqual([]);
    expect(requiredFor('get_navigation_info')).toEqual([]);
    expect(requiredFor('configure_nav_mesh_settings')).toEqual([]);
  });

  it('routes create_nav_link_proxy through navigation, not the AI blueprintPath check', () => {
    // NAVIGATION_ACTIONS membership means navigation-handlers.ts wins before the
    // ai-utility-actions.ts blueprintPath guard is ever reached.
    expect(propsFor('create_nav_link_proxy')).not.toContain('blueprintPath');
    expect(propsFor('create_nav_link_proxy')).toContain('startPoint');
    expect(requiredFor('create_nav_link_proxy')).toEqual([]);
  });

  it('declares every action as an editor-state operation', () => {
    // No manage_ai handler touches GEditor->PlayWorld; the navigation actions
    // resolve the EDITOR world via GEditor->GetEditorWorldContext().World().
    for (const record of records) {
      expect(record.availability.editorStates).toEqual(['edit']);
    }
  });

  it('gates plugin-dependent families and leaves core AI ungated', () => {
    const plugins = (action: string): string[] => {
      const record = byAction.get(action);
      if (record === undefined) throw new Error(`missing manage_ai record: ${action}`);
      return [...record.availability.requiredPlugins];
    };
    expect(plugins('add_task_node')).toEqual(['BehaviorTreeEditor']);
    expect(plugins('add_eqs_test')).toEqual(['EnvironmentQueryEditor']);
    expect(plugins('create_state_tree')).toEqual(['StateTree']);
    expect(plugins('add_smart_object_slot')).toEqual(['SmartObjects']);
    expect(plugins('add_mass_spawner')).toEqual(['MassAI']);
    // Perception and navigation ride engine-core modules.
    expect(plugins('setup_perception')).toEqual([]);
    expect(plugins('rebuild_navigation')).toEqual([]);
  });

  it('marks the four read actions as reads and the rest as writes', () => {
    const reads = records.filter((record) => record.behavior.effect === 'read').map(actionOf).sort();
    expect(reads).toEqual(['get_ai_info', 'get_blackboard_value', 'get_navigation_info', 'get_tree']);
  });
});
