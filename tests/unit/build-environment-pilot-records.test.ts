// tests/unit/build-environment-pilot-records.test.ts
// Focused tests for the build_environment pilot capability catalog.
// Proves: exactly 150 records, minimal valid fixtures, rule-invalid fixtures,
// representative landscape/lighting/spline schemas with relevant-only params,
// deferred persistence truth for Render routes, and dispatch routing.

import { describe, expect, it } from 'vitest';
import { validatePilotCatalog } from '../../scripts/gateway-manifest/validate.js';
import { type CapabilityRecordSource, createCapabilityRecord } from '../../src/tools/catalog/capabilities/index.js';
import {
  BUILD_ENVIRONMENT_EXPECTED_IDS,
  BUILD_ENVIRONMENT_RECORDS,
} from '../../src/tools/catalog/capabilities/records/build-environment/index.js';

function findRecord(id: string): CapabilityRecordSource {
  const r = BUILD_ENVIRONMENT_RECORDS.find((x) => x.id === id);
  if (!r) throw new Error(`record ${id} not found`);
  return r;
}

const RENDER_ACTIONS = new Set([
  'configure_ray_traced_shadows', 'configure_ray_traced_gi', 'configure_ray_traced_reflections',
  'configure_ray_traced_ao', 'configure_path_tracing', 'set_light_channel', 'set_actor_light_channel',
  'configure_lightmass_settings', 'build_lighting_quality', 'configure_indirect_lighting_cache',
  'create_sphere_reflection_capture', 'create_box_reflection_capture', 'configure_reflection_capture_resolution',
  'configure_capture_resolution', 'configure_capture_offset', 'recapture_scene', 'create_planar_reflection',
  'configure_planar_reflection', 'configure_ssr_settings', 'configure_lumen_reflection_settings',
  'configure_pp_blend', 'set_pp_white_balance', 'set_pp_color_grading', 'set_pp_lut', 'configure_tonemapper',
  'set_tonemapper_type', 'configure_bloom', 'set_bloom_intensity', 'set_bloom_threshold',
  'configure_lens_flare', 'configure_dof', 'set_dof_method', 'set_focal_distance', 'set_aperture',
  'configure_bokeh', 'configure_motion_blur', 'set_motion_blur_amount', 'set_motion_blur_max',
  'configure_exposure', 'set_exposure_method', 'set_exposure_compensation', 'set_exposure_min_max',
  'configure_ssao', 'configure_gtao', 'configure_vignette', 'configure_chromatic_aberration',
  'configure_grain', 'configure_screen_percentage', 'create_scene_capture_2d', 'create_scene_capture_cube',
  'configure_capture_source', 'assign_render_target', 'capture_scene',
]);

describe('build_environment pilot catalog completeness', () => {
  it('has exactly 150 records', () => {
    expect(BUILD_ENVIRONMENT_RECORDS).toHaveLength(150);
  });

  it('has exactly 150 unique canonical IDs', () => {
    const ids = BUILD_ENVIRONMENT_RECORDS.map((r) => r.id);
    expect(new Set(ids).size).toBe(150);
  });

  it('all IDs start with build_environment.', () => {
    for (const r of BUILD_ENVIRONMENT_RECORDS) {
      expect(r.id.startsWith('build_environment.')).toBe(true);
    }
  });

  it('every record validates as a CapabilityRecord with correct hashes', () => {
    const records = BUILD_ENVIRONMENT_RECORDS.map((r) => createCapabilityRecord(r));
    expect(records).toHaveLength(150);
    for (const r of records) {
      expect(r.hashes.algorithm).toBe('sha256');
      expect(r.hashes.schema).toMatch(/^[0-9a-f]{64}$/);
      expect(r.hashes.content).toMatch(/^[0-9a-f]{64}$/);
    }
  });

  it('the full catalog validates against expected IDs', () => {
    const recordsWithHashes = BUILD_ENVIRONMENT_RECORDS.map((r) => createCapabilityRecord(r));
    const result = validatePilotCatalog(recordsWithHashes, BUILD_ENVIRONMENT_EXPECTED_IDS);
    expect(result.success).toBe(true);
    if (result.success) {
      expect(result.records).toHaveLength(150);
    }
  });
});

describe('build_environment pilot minimal valid fixture', () => {
  it('a minimal record with only required fields validates', () => {
    const minimal = findRecord('build_environment.list_light_types');
    const record = createCapabilityRecord(minimal);
    expect(record.id).toBe('build_environment.list_light_types');
    expect(record.schemas.input.required).toEqual(['action']);
    expect(record.schemas.input.properties).toHaveProperty('action');
    expect(Object.keys(record.schemas.input.properties)).toHaveLength(1);
  });
});

describe('build_environment pilot rule-invalid fixtures', () => {
  it('rejects a record with invalid behavior effect', () => {
    const source = BUILD_ENVIRONMENT_RECORDS[0];
    const malformed = {
      ...source,
      behavior: { ...source.behavior, effect: 'invalid-effect' },
    };
    const result = validatePilotCatalog([malformed]);
    expect(result.success).toBe(false);
    if (!result.success) {
      expect(result.errors[0].pointer).toContain('/0/behavior');
    }
  });

  it('rejects a record with reversed UE version range', () => {
    const source = BUILD_ENVIRONMENT_RECORDS[0];
    const malformed = {
      ...source,
      availability: {
        ...source.availability,
        unreal: {
          min: { major: 5, minor: 8, patch: 0, channel: 'preview', preview: 1 },
          max: { major: 5, minor: 0, patch: 0, channel: 'stable' },
        },
      },
    };
    const result = validatePilotCatalog([malformed]);
    expect(result.success).toBe(false);
  });

  it('rejects a record with duplicate canonical ID', () => {
    const first = createCapabilityRecord(BUILD_ENVIRONMENT_RECORDS[0]);
    const dup = createCapabilityRecord({ ...BUILD_ENVIRONMENT_RECORDS[1], id: first.id });
    const result = validatePilotCatalog([first, dup]);
    expect(result.success).toBe(false);
    if (!result.success) {
      const dupError = result.errors.find((e) => e.message.includes('duplicate'));
      expect(dupError).toBeDefined();
    }
  });

  it('rejects a record with unbounded object schema without reflection marker', () => {
    const source = BUILD_ENVIRONMENT_RECORDS[0];
    const malformed = {
      ...source,
      schemas: {
        input: {
          ...source.schemas.input,
          properties: {
            ...source.schemas.input.properties,
            bad: { type: 'object', additionalProperties: true },
          },
        },
        output: source.schemas.output,
      },
    };
    const result = validatePilotCatalog([malformed]);
    expect(result.success).toBe(false);
  });
});

describe('build_environment representative schemas expose relevant-only params', () => {
  it('landscape action (create_landscape) exposes only landscape params, not the parent union', () => {
    const r = findRecord('build_environment.create_landscape');
    const props = Object.keys(r.schemas.input.properties);
    expect(props).toContain('name');
    expect(props).toContain('location');
    expect(props).toContain('sizeX');
    expect(props).toContain('materialPath');
    expect(props).not.toContain('foliageType');
    expect(props).not.toContain('splineType');
    expect(props).not.toContain('lightType');
    expect(props).not.toContain('waveHeight');
  });

  it('lighting action (spawn_light) exposes only lighting params', () => {
    const r = findRecord('build_environment.spawn_light');
    const props = Object.keys(r.schemas.input.properties);
    expect(props).toContain('lightType');
    expect(props).toContain('location');
    expect(props).toContain('intensity');
    expect(props).not.toContain('landscapeName');
    expect(props).not.toContain('foliageType');
    expect(props).not.toContain('splineType');
  });

  it('spline action (add_spline_point) exposes only spline params', () => {
    const r = findRecord('build_environment.add_spline_point');
    const props = Object.keys(r.schemas.input.properties);
    expect(props).toContain('actorName');
    expect(props).toContain('position');
    expect(props).toContain('pointType');
    expect(props).not.toContain('landscapeName');
    expect(props).not.toContain('lightType');
    expect(props).not.toContain('foliageType');
  });

  it('landscape, lighting, and spline actions have distinct input schemas', () => {
    const landscape = findRecord('build_environment.create_landscape');
    const lighting = findRecord('build_environment.spawn_light');
    const spline = findRecord('build_environment.add_spline_point');
    const lProps = new Set(Object.keys(landscape.schemas.input.properties));
    const gProps = new Set(Object.keys(lighting.schemas.input.properties));
    const sProps = new Set(Object.keys(spline.schemas.input.properties));
    expect(lProps).not.toEqual(gProps);
    expect(lProps).not.toEqual(sProps);
    expect(gProps).not.toEqual(sProps);
  });
});

describe('build_environment deferred persistence for Render routes', () => {
  it('Render actions route through manage_render, not build_environment', () => {
    const renderRecords = BUILD_ENVIRONMENT_RECORDS.filter(
      (r) => RENDER_ACTIONS.has(r.id.replace('build_environment.', '')),
    );
    expect(renderRecords.length).toBe(53);
    for (const r of renderRecords) {
      expect(r.routing.parentTool).toBe('build_environment');
    }
  });

  it('a Render action cannot claim immediate save when implementation only marks dirty', () => {
    const captureScene = findRecord('build_environment.capture_scene');
    expect(captureScene.behavior.effect).toBe('write');
    expect(captureScene.behavior.supportsUndo).toBe(true);
    const output = captureScene.examples[0].output;
    expect(output).toHaveProperty('success');
    expect(output).toHaveProperty('message');
    expect(output).not.toHaveProperty('saved');
  });

  it('water actions (MarkPackageDirty) do not claim saved in output', () => {
    const waterRecord = findRecord('build_environment.create_water_body_ocean');
    const output = waterRecord.examples[0].output;
    expect(output).not.toHaveProperty('saved');
    expect(output).toHaveProperty('success');
  });
});

describe('build_environment pilot dispatch routing', () => {
  it('action-dispatched records use dispatchMode action', () => {
    const createAction = findRecord('build_environment.create_landscape');
    expect(createAction.routing.dispatchMode).toBe('action');
    expect(createAction.routing.dispatchAction).toBe('create_landscape');
  });

  it('tool-dispatched records use dispatchMode tool', () => {
    const configureAction = findRecord('build_environment.configure_sky_atmosphere');
    expect(configureAction.routing.dispatchMode).toBe('tool');
    expect(configureAction.routing.parentTool).toBe('build_environment');
  });

  it('sculpt dispatches to sculpt_landscape (action translation)', () => {
    const sculpt = findRecord('build_environment.sculpt');
    expect(sculpt.routing.dispatchAction).toBe('sculpt_landscape');
    expect(sculpt.routing.dispatchMode).toBe('action');
  });

  it('create_foliage_type dispatches to add_foliage_type (action translation)', () => {
    const ft = findRecord('build_environment.create_foliage_type');
    expect(ft.routing.dispatchAction).toBe('add_foliage_type');
  });
});
