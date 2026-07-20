/**
 * Focused unit tests for the Task 5 normalization inventory — route
 * dispositions (v2 exact-key completeness, aggregate metrics, concrete-file
 * evidence, group-row citations, structural invariants), and public/non-public
 * separation.
 *
 * Determinism of generation and byte-stability of the committed artifact live in
 * `normalization-artifact.test.ts`. These tests read the authoritative source
 * through `buildInventory`; the v2 route-audit expected key set is pinned
 * below, and any deviation (missing, extra, or invented key) fails the test.
 */

import { readFileSync, statSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { buildInventory } from '../../src/tools/catalog/capabilities/normalization/build.js';
import { REVIEWED_ROUTE_KEYS } from '../../src/tools/catalog/capabilities/normalization/routedispositions.js';

/**
 * v2 deterministic expected key set (77 non-public routes after Task 21 geometry expansion).
 * Source: .omo/evidence/task-5-route-coverage-audit-v2.json
 * This is the authoritative route-audit v2 expected set; any deviation
 * (missing, extra, or invented key) fails the test.
 */
const V2_EXPECTED_ROUTE_KEYS: readonly string[] = [
  'route:widget:add_quest_tracker', 'route:widget:add_safe_zone', 'route:widget:add_spacer', 'route:widget:add_widget_component', 'route:widget:add_widget_switcher', 'route:widget:bind_localized_text', 'route:widget:create_credits_screen', 'route:widget:create_shop_ui', 'route:widget:create_widget_style', 'route:widget:delete_animation', 'route:widget:get_widget_slot_info', 'route:widget:remove_widget', 'route:widget:rename_widget', 'route:widget:reparent_widget', 'route:widget:set_font', 'route:widget:set_localization_key', 'route:widget:set_margin', 'route:widget:set_widget_binding',
  'route:widget:create_widget', 'route:widget:get_animation_info', 'route:widget:show_widget', 'route:widget:apply_style_to_widget', 'route:widget:set_animation_speed',
  'route:graph:get_nodes',
  'route:skeleton:add_socket', 'route:skeleton:modify_socket', 'route:skeleton:modify_physics_body', 'route:skeleton:set_physics_asset', 'route:skeleton:remove_physics_body', 'route:skeleton:get_physics_asset_info', 'route:skeleton:list_morph_targets', 'route:skeleton:delete_morph_target', 'route:skeleton:delete_socket', 'route:skeleton:remove_socket', 'route:skeleton:get_bone_transform', 'route:skeleton:list_virtual_bones', 'route:skeleton:delete_virtual_bone', 'route:skeleton:set_physics_constraint', 'route:skeleton:set_morph_target_value', 'route:skeleton:preview_physics',
  'route:animation:create_pose_library', 'route:animation:add_notify', 'route:animation:set_retarget_chain_mapping', 'route:animation:assign_cloth_asset_to_mesh',
  'route:gas:create_ability_set', 'route:gas:add_ability', 'route:gas:grant_ability', 'route:gas:create_execution_calculation', 'route:gas:set_activation_policy',
  'route:ai:create_nav_modifier', 'route:ai:set_ai_movement', 'route:ai:set_ai_perception', 'route:ai:create_nav_link_proxy',
  'route:effect:niagara', 'route:effect:activate', 'route:effect:shadowed_stubs',
  'route:geometry:difference', 'route:geometry:bridge', 'route:geometry:loft',
  'route:geometry:create_procedural_mesh', 'route:geometry:append_triangle', 'route:geometry:append_vertex', 'route:geometry:set_uvs', 'route:geometry:set_vertex_color', 'route:geometry:split_normals', 'route:geometry:delete_vertex', 'route:geometry:delete_triangle', 'route:geometry:get_vertex_position', 'route:geometry:set_vertex_position', 'route:geometry:translate_mesh',
  'route:audio:create_sound_cue', 'route:audio:create_sound_class', 'route:audio:create_sound_mix', 'route:audio:add_source_effect',
  'route:asset:analyze_graph', 'route:asset:get_source_control_state', 'route:asset:material_overlap_residual',
];

/** Invented MRQ audio routes that must NEVER appear (v2 rejected: no source token). */
const V2_INVENTED_AUDIO_KEYS: readonly string[] = [
  'route:audio:cancel_render',
  'route:audio:get_render_progress',
  'route:audio:get_render_status',
];

/**
 * Extract a verifiable token from an evidence symbol string. The token is
 * guaranteed to appear in the cited evidence source file if the evidence is
 * truthful. Handles SubAction.Equals(TEXT("...")), case '...' TS patterns,
 * Handle* function names, and semicolon-separated group symbols.
 */
function extractVerificationToken(symbol: string): string {
  // SubAction.Equals(TEXT("route_name")) or SubAction == TEXT("route_name")
  const textMatch = symbol.match(/TEXT\("([^"]+)"\)/);
  if (textMatch) return textMatch[1];
  // case 'route_name': (TypeScript)
  const caseMatch = symbol.match(/case '([^']+)'/);
  if (caseMatch) return caseMatch[1];
  // Handle* function name (first one in a semicolon-separated list)
  const handleMatch = symbol.match(/\b(Handle[A-Za-z0-9_]+)\b/);
  if (handleMatch) return handleMatch[1];
  // snake_case identifier with underscore (first one; skips English words)
  const snakeMatch = symbol.match(/\b([a-z][a-z0-9]*_[a-z0-9_]+)\b/);
  if (snakeMatch) return snakeMatch[1];
  return symbol;
}

const here = dirname(fileURLToPath(import.meta.url));

describe('route dispositions: v2 exact-key completeness (77 non-public routes)', () => {
  const inv = buildInventory();
  const keys = inv.routeDispositions.map((r) => r.dispositionKey);
  const keySet = new Set(keys);

  it('has exactly 77 non-public route dispositions (expected total)', () => {
    expect(inv.routeDispositions.length).toBe(77);
    expect(REVIEWED_ROUTE_KEYS.length).toBe(77);
  });

  it('contains every v2 expected key (no missing routes)', () => {
    const missing = V2_EXPECTED_ROUTE_KEYS.filter((k) => !keySet.has(k));
    expect(missing, `missing v2 route keys: ${missing.join(', ')}`).toEqual([]);
  });

  it('contains no extra keys beyond v2 expected (no invented routes)', () => {
    const expected = new Set(V2_EXPECTED_ROUTE_KEYS);
    const extra = keys.filter((k) => !expected.has(k));
    expect(extra, `extra/invented route keys: ${extra.join(', ')}`).toEqual([]);
  });

  it('rejects the 3 invented MRQ audio routes (no source token exists)', () => {
    for (const invented of V2_INVENTED_AUDIO_KEYS) {
      expect(keySet.has(invented), `invented route ${invented} must not be present`).toBe(false);
    }
  });

  it('has no duplicate keys', () => {
    expect(keySet.size).toBe(keys.length);
  });
});

describe('route dispositions: v2 aggregate metrics (internally consistent totals)', () => {
  const inv = buildInventory();
  const m = inv.metrics;

  it('byDomain matches: widget23/graph1/skeleton16/animation4/gas5/ai4/effect3/geometry14/audio4/asset3', () => {
    const byDomain: Record<string, number> = {};
    for (const r of inv.routeDispositions) byDomain[r.domain] = (byDomain[r.domain] ?? 0) + 1;
    expect(byDomain).toEqual({
      widget: 23, graph: 1, skeleton: 16, animation: 4, gas: 5,
      ai: 4, effect: 3, geometry: 14, audio: 4, asset: 3,
    });
  });

  it('byStatus matches: dead20/raw24/hidden33 (sum=77)', () => {
    expect(m.routeStatusCounts).toEqual({ dead: 20, raw: 24, hidden: 33 });
    expect(m.routeStatusCounts.dead + m.routeStatusCounts.raw + m.routeStatusCounts.hidden).toBe(77);
  });

  it('byDisposition matches: promote53/map16/remove8 (sum=77)', () => {
    expect(m.routeDispositionCounts).toEqual({ promote: 53, map: 16, remove: 8 });
    expect(m.routeDispositionCounts.promote + m.routeDispositionCounts.map + m.routeDispositionCounts.remove).toBe(77);
  });

  it('skeleton: 15 hidden/promote + 1 raw/remove (not blanket raw)', () => {
    const skeleton = inv.routeDispositions.filter((r) => r.domain === 'skeleton');
    expect(skeleton.length).toBe(16);
    const hidden = skeleton.filter((r) => r.status === 'hidden' && r.disposition === 'promote');
    const rawRemove = skeleton.filter((r) => r.status === 'raw' && r.disposition === 'remove');
    expect(hidden.length).toBe(15);
    expect(rawRemove.length).toBe(1);
    expect(rawRemove[0].route).toBe('preview_physics');
  });

  it('audio: exactly 4 hidden/map (3 invented MRQ routes absent)', () => {
    const audio = inv.routeDispositions.filter((r) => r.domain === 'audio');
    expect(audio.length).toBe(4);
    expect(audio.every((r) => r.status === 'hidden' && r.disposition === 'map')).toBe(true);
  });
});

describe('route dispositions: concrete-file evidence (no directories or placeholders)', () => {
  const inv = buildInventory();
  const repoRoot = resolve(here, '../../');

  it('every evidence source is a concrete file (not a directory)', () => {
    for (const r of inv.routeDispositions) {
      const abs = resolve(repoRoot, r.evidence.source);
      const stat = statSync(abs);
      expect(stat.isFile(), `${r.dispositionKey}: evidence source must be a file, not a directory: ${r.evidence.source}`).toBe(true);
    }
  });

  it('every evidence symbol token exists in its cited file', () => {
    for (const r of inv.routeDispositions) {
      const abs = resolve(repoRoot, r.evidence.source);
      const content = readFileSync(abs, 'utf8');
      const token = extractVerificationToken(r.evidence.symbol);
      expect(
        content,
        `${r.dispositionKey}: token "${token}" not found in ${r.evidence.source}`,
      ).toContain(token);
    }
  });

  it('every evidence source path is non-empty and relative', () => {
    for (const r of inv.routeDispositions) {
      expect(r.evidence.source.length).toBeGreaterThan(0);
      expect(r.evidence.source.startsWith('/')).toBe(false);
    }
  });
});

describe('route dispositions: group-row citations (every constituent symbol source-backed)', () => {
  const inv = buildInventory();
  const repoRoot = resolve(here, '../../');

  /**
   * material_overlap_residual is a group row citing 3 translated aliases:
   * connect_material_pins, break_material_connections, rebuild_material.
   * Each lives in a DISTINCT concrete Materials/ cpp file. The row must
   * carry a citations list (source+symbol pairs) so every constituent
   * symbol is verifiably literal in its own paired file - not just the
   * primary pin-connections file.
   */
  it('material_overlap_residual carries 3 citations covering all constituent symbols', () => {
    const row = inv.routeDispositions.find(
      (r) => r.dispositionKey === 'route:asset:material_overlap_residual',
    );
    expect(row).toBeDefined();
    const citations = row?.evidence.citations;
    expect(citations, 'material_overlap_residual must carry a citations list').toBeDefined();
    expect(citations?.length).toBe(3);
    const tokens = citations?.map((c) => extractVerificationToken(c.symbol));
    expect(tokens).toContain('connect_material_pins');
    expect(tokens).toContain('break_material_connections');
    expect(tokens).toContain('rebuild_material');
  });

  it('every citation source is a concrete file and every full symbol is literal in it', () => {
    const row = inv.routeDispositions.find(
      (r) => r.dispositionKey === 'route:asset:material_overlap_residual',
    );
    expect(row).toBeDefined();
    const citations = row?.evidence.citations;
    expect(citations, 'citations must be present').toBeDefined();
    for (const c of citations ?? []) {
      const abs = resolve(repoRoot, c.source);
      const stat = statSync(abs);
      expect(stat.isFile(), `citation source must be a file: ${c.source}`).toBe(true);
      const content = readFileSync(abs, 'utf8');
      expect(
        content,
        `citation full symbol not literal in ${c.source}: "${c.symbol}"`,
      ).toContain(c.symbol);
    }
  });
});

describe('route dispositions: structural invariants', () => {
  const inv = buildInventory();

  it('has zero unresolved rows (evidence + target/guidance for every record)', () => {
    expect(inv.metrics.routeDispositionUnresolved).toBe(0);
    for (const r of inv.routeDispositions) {
      expect(r.resolved).toBe(true);
      expect(r.evidence.source.length).toBeGreaterThan(0);
      expect(r.evidence.symbol.length).toBeGreaterThan(0);
      if (r.disposition === 'remove') {
        expect(r.removalGuidance && r.removalGuidance.length > 0).toBe(true);
      } else {
        expect(r.targetCanonicalId && r.targetCanonicalId.length > 0).toBe(true);
      }
    }
  });

  it('completeness: every reviewed route key is present and resolved', () => {
    const led = inv.routeDispositions.map((r) => r.dispositionKey);
    const keys = new Set(led);
    expect(keys.size).toBe(led.length);
    for (const r of inv.routeDispositions) {
      expect(REVIEWED_ROUTE_KEYS).toContain(r.dispositionKey);
    }
  });

  it('promote/map target a canonical id; remove carries removal guidance (mutually exclusive)', () => {
    for (const r of inv.routeDispositions) {
      if (r.disposition === 'remove') {
        expect(r.removalGuidance && r.removalGuidance.length > 0).toBe(true);
        expect(r.targetCanonicalId, `${r.dispositionKey} remove must not set targetCanonicalId`).toBeUndefined();
      } else {
        expect(r.targetCanonicalId && r.targetCanonicalId.length > 0, `${r.dispositionKey} ${r.disposition} must set targetCanonicalId`).toBe(true);
        expect(r.removalGuidance, `${r.dispositionKey} ${r.disposition} must not set removalGuidance`).toBeUndefined();
      }
    }
  });
});

describe('public/non-public separation (1,335 public vs 77 non-public)', () => {
  const inv = buildInventory();

  it('keeps the 1,335 public occurrences structurally separate from the 77 non-public routes', () => {
    expect(inv.occurrences.length).toBe(1335);
    expect(inv.routeDispositions.length).toBe(77);
    const occKeys = new Set(inv.occurrences.map((o) => o.occurrenceKey));
    const routeKeys = new Set(inv.routeDispositions.map((r) => r.dispositionKey));
    const overlap = [...occKeys].filter((k) => routeKeys.has(k));
    expect(overlap, `public/non-public key overlap: ${overlap.join(', ')}`).toEqual([]);
  });

  it('P classification is absent; role field is separate from A-F taxonomy', () => {
    const cc = inv.metrics.classificationCounts;
    expect('P' in cc).toBe(false);
    expect(cc.A + cc.B + cc.C + cc.D + cc.E + cc.F).toBe(1335);
    for (const o of inv.occurrences) {
      expect(['A', 'B', 'C', 'D', 'E', 'F']).toContain(o.classification);
      expect(['primary', 'alias']).toContain(o.role);
    }
  });
});
