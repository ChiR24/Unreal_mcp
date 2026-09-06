/**
 * manage_blueprint pilot capability records: widget/graph distinct handles,
 * schema/hash parity, and availability/plugin gate tests.
 *
 * Companion to manage-blueprint-pilot-records.test.ts (exact-set,
 * hidden-disposition, SCS ownership). Split out so every focused test file
 * stays within the 250 pure-LOC ceiling.
 *
 * Verifies:
 * - Widget animation and component-add have distinct handles/constraints
 * - graph create_node returns nodeGuid (distinct from widget slotName)
 * - All records pass recomputed/hash parity and deterministic manifest checks
 * - Core/widget records require the expected plugins (EditorScriptingUtilities / UMG)
 * - All records target UE 5.0 through 5.8 Preview and are active
 */
import { describe, expect, it } from 'vitest';
import { hashManifestContent } from '../../scripts/gateway-manifest/hash.js';
import {
  buildPilotManifest,
  pilotJson,
  pilotTsText,
} from '../../scripts/gateway-manifest/pilot.js';
import {
  computeCapabilityHashes,
  createCapabilityRecord,
} from '../../src/tools/catalog/capabilities/index.js';
import {
  MANAGE_BLUEPRINT_RECORDS,
} from '../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';

describe('manage_blueprint pilot: widget and graph distinct handles', () => {
  it('graph create_node returns nodeGuid (distinct from widget slotName handle)', () => {
    const createNode = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.create_node');
    expect(createNode?.schemas.output.properties).toHaveProperty('nodeGuid');
    expect(createNode?.schemas.output.required).toContain('nodeGuid');
  });

  it('widget panel/content actions return slotName (distinct from graph nodeGuid)', () => {
    const addButton = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.add_button');
    expect(addButton?.schemas.output.properties).toHaveProperty('slotName');
    expect(addButton?.schemas.output.required).toContain('slotName');
  });

  it('widget animation actions use widgetPath + animationName handles (distinct from graph node handles)', () => {
    const createAnim = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.create_widget_animation');
    expect(createAnim?.schemas.input.properties).toHaveProperty('widgetPath');
    expect(createAnim?.schemas.input.properties).toHaveProperty('animationName');
    expect(createAnim?.schemas.input.required).toContain('animationName');
  });

  it('widget layout actions require widgetPath + slotName (distinct from SCS componentName)', () => {
    const setAnchor = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.set_anchor');
    expect(setAnchor?.schemas.input.properties).toHaveProperty('widgetPath');
    expect(setAnchor?.schemas.input.properties).toHaveProperty('slotName');
    expect(setAnchor?.schemas.input.required).toContain('slotName');
    // SCS uses componentName, not slotName
    const setScsProp = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.set_scs_property');
    expect(setScsProp?.schemas.input.properties).toHaveProperty('componentName');
    expect(setScsProp?.schemas.input.required).toContain('componentName');
  });
});

describe('manage_blueprint pilot: schema and hash parity', () => {
  it('recomputed hashes match stored hashes for every record', () => {
    for (const record of MANAGE_BLUEPRINT_RECORDS) {
      const { hashes, ...source } = record;
      const recomputed = computeCapabilityHashes(source);
      expect(recomputed.schema).toBe(hashes.schema);
      expect(recomputed.content).toBe(hashes.content);
    }
  });

  it('recomputing createCapabilityRecord produces identical hashes', () => {
    for (const record of MANAGE_BLUEPRINT_RECORDS) {
      const { hashes, ...source } = record;
      const recreated = createCapabilityRecord(source);
      expect(recreated.hashes.schema).toBe(hashes.schema);
      expect(recreated.hashes.content).toBe(hashes.content);
    }
  });

  it('pilot manifest JSON and TS output is deterministic across two runs', () => {
    const json1 = pilotJson(MANAGE_BLUEPRINT_RECORDS);
    const json2 = pilotJson(MANAGE_BLUEPRINT_RECORDS);
    const ts1 = pilotTsText(MANAGE_BLUEPRINT_RECORDS);
    const ts2 = pilotTsText(MANAGE_BLUEPRINT_RECORDS);
    expect(json1).toBe(json2);
    expect(ts1).toBe(ts2);
  });

  it('pilot manifest hash is deterministic across two runs', () => {
    const hash1 = hashManifestContent(pilotJson(MANAGE_BLUEPRINT_RECORDS));
    const hash2 = hashManifestContent(pilotJson(MANAGE_BLUEPRINT_RECORDS));
    expect(hash1).toBe(hash2);
  });

  it('pilot manifest has 121 tools (1:1 canonical ID keying)', () => {
    const manifest = buildPilotManifest(MANAGE_BLUEPRINT_RECORDS);
    expect(manifest.tools.length).toBe(121);
    expect(manifest.source).toBe('pilot:capabilityRecords');
  });

  it('pilot tool names are canonical IDs (not legacy parentTool)', () => {
    const manifest = buildPilotManifest(MANAGE_BLUEPRINT_RECORDS);
    for (const tool of manifest.tools) {
      expect(tool.name).toMatch(/^blueprint\./);
      expect(tool.name).not.toBe('manage_blueprint');
    }
  });
});

describe('manage_blueprint pilot: availability and plugin gates', () => {
  it('core records require EditorScriptingUtilities plugin', () => {
    const createRecord = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.create');
    expect(createRecord?.availability.requiredPlugins).toContain('EditorScriptingUtilities');
  });

  it('widget records require UMG plugin in addition to EditorScriptingUtilities', () => {
    const widgetRecord = MANAGE_BLUEPRINT_RECORDS.find((r) => r.id === 'blueprint.create_widget_blueprint');
    expect(widgetRecord?.availability.requiredPlugins).toContain('EditorScriptingUtilities');
    expect(widgetRecord?.availability.requiredPlugins).toContain('UMG');
  });

  it('all records target UE 5.0 through 5.8 Preview', () => {
    for (const record of MANAGE_BLUEPRINT_RECORDS) {
      expect(record.availability.unreal.min.major).toBe(5);
      expect(record.availability.unreal.min.minor).toBe(0);
      expect(record.availability.unreal.max.major).toBe(5);
      expect(record.availability.unreal.max.channel).toBe('preview');
    }
  });

  it('all records are active (not deprecated or removed)', () => {
    for (const record of MANAGE_BLUEPRINT_RECORDS) {
      expect(record.deprecation.status).toBe('active');
    }
  });
});
