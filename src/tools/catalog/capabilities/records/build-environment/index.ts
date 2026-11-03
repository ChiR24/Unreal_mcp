/**
 * build_environment catalog: 151 authored CapabilityRecordSource entries
 * aggregated from family-sharded data files, folded by BUILD_ENVIRONMENT_FOLDS
 * into the 41 shipped records.
 *
 * Each authored entry is a CapabilityRecordSource (hashes are computed at parse
 * time by createCapabilityRecord / parseCapabilityCatalog). The authored set
 * equals the 151-action source set from the normalization inventory; the folded
 * records count toward ALL_CAPABILITY_RECORD_COUNT (records/aggregate.ts), which
 * throws on any drift.
 *
 * Family shards:
 *   landscape      16   foliage       14   procedural      5
 *   lighting       15   render-raytrace 18  render-postprocess 22
 *   render-screen  13   spline        22   atmosphere     12
 *   weather         5   water          8   decal           1
 *   Total: 151
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ATMOSPHERE_RECORDS } from './atmosphere.data.js';
import { DECAL_RECORDS } from './decal.data.js';
import { FOLIAGE_RECORDS } from './foliage.data.js';
import { LANDSCAPE_RECORDS } from './landscape.data.js';
import { LIGHTING_RECORDS } from './lighting.data.js';
import { PROCEDURAL_RECORDS } from './procedural.data.js';
import { RENDER_POSTPROCESS_RECORDS } from './render-postprocess.data.js';
import { RENDER_RAYTRACE_RECORDS } from './render-raytrace.data.js';
import { RENDER_SCREEN_RECORDS } from './render-screen.data.js';
import { SPLINE_RECORDS } from './spline.data.js';
import { WATER_RECORDS } from './water.data.js';
import { WEATHER_RECORDS } from './weather.data.js';
import { applyFolds } from '../shared/fold.js';
import { BUILD_ENVIRONMENT_FOLDS } from '../folds/build-environment.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const BUILD_ENVIRONMENT_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...LANDSCAPE_RECORDS,
  ...FOLIAGE_RECORDS,
  ...PROCEDURAL_RECORDS,
  ...LIGHTING_RECORDS,
  ...RENDER_RAYTRACE_RECORDS,
  ...RENDER_POSTPROCESS_RECORDS,
  ...RENDER_SCREEN_RECORDS,
  ...SPLINE_RECORDS,
  ...ATMOSPHERE_RECORDS,
  ...WEATHER_RECORDS,
  ...WATER_RECORDS,
  ...DECAL_RECORDS,
];

export const BUILD_ENVIRONMENT_SOURCES: readonly CapabilityRecordSource[] = applyFolds(BUILD_ENVIRONMENT_UNFOLDED_SOURCES, BUILD_ENVIRONMENT_FOLDS, 'build_environment');

