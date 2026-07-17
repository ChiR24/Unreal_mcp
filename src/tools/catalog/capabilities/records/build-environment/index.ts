/**
 * build_environment pilot catalog: exactly 150 canonical CapabilityRecord
 * entries aggregated from family-sharded data files.
 *
 * Each record is a CapabilityRecordSource (hashes are computed at parse time
 * by createCapabilityRecord / parseCapabilityCatalog). The canonical IDs
 * exactly equal the 150-action source set from the normalization inventory.
 *
 * Family shards:
 *   landscape      16   foliage       14   procedural      5
 *   lighting       15   render-raytrace 18  render-postprocess 22
 *   render-screen  13   spline        22   atmosphere     12
 *   weather         5   water          8
 *   Total: 150
 */
import type { CapabilityRecordSource } from '../../index.js';
import { ATMOSPHERE_RECORDS } from './atmosphere.data.js';
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

export const BUILD_ENVIRONMENT_RECORDS: readonly CapabilityRecordSource[] = [
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
];

export const BUILD_ENVIRONMENT_EXPECTED_IDS: readonly string[] =
  BUILD_ENVIRONMENT_RECORDS.map((r) => r.id);
