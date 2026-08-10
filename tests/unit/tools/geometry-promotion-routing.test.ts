/**
 * Routing contract for the promoted DynamicMesh geometry routes.
 *
 * Twelve geometry operations are implemented and natively dispatchable in
 * `Private/Domains/Geometry/McpAutomationBridge_GeometryHandlers.cpp`, but they
 * were never named on the TypeScript surface. `handleGeometryTools` validates
 * the requested action against its own `GEOMETRY_ACTIONS` allow-list and
 * answers `INVALID_ACTION` for anything absent, so the native implementation is
 * unreachable no matter what the canonical record says.
 *
 * Unlike the widget-authoring and skeleton promotions, `manage_geometry` is
 * registered with `MCP_REGISTER_DIRECT` and carries no native predicate gate,
 * so the allow-list below is the only routing surface that has to name them.
 */

import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';

const PROMOTED_GEOMETRY_ACTIONS = [
  'difference',
  'create_procedural_mesh',
  'append_triangle',
  'append_vertex',
  'set_uvs',
  'set_vertex_color',
  'split_normals',
  'delete_vertex',
  'delete_triangle',
  'get_vertex_position',
  'set_vertex_position',
  'translate_mesh',
] as const;

const HANDLER_PATH = 'src/tools/handlers/geometry/geometry-handlers.ts';
const NATIVE_PATH =
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Geometry/'
  + 'McpAutomationBridge_GeometryHandlers.cpp';

/** The body of the `GEOMETRY_ACTIONS` array, so a match elsewhere cannot pass for membership. */
function geometryAllowList(): string {
  const source = readFileSync(HANDLER_PATH, 'utf8');
  const start = source.indexOf('const GEOMETRY_ACTIONS');
  if (start < 0) throw new Error(`GEOMETRY_ACTIONS is missing from ${HANDLER_PATH}`);
  const end = source.indexOf('] as const;', start);
  if (end < 0) throw new Error(`GEOMETRY_ACTIONS is unterminated in ${HANDLER_PATH}`);
  return source.slice(start, end);
}

describe('promoted geometry routes are reachable through manage_geometry', () => {
  it('the TypeScript allow-list names every promoted action', () => {
    const allowList = geometryAllowList();
    const missing = PROMOTED_GEOMETRY_ACTIONS.filter(
      (action) => !allowList.includes(`'${action}'`),
    );

    expect(
      missing,
      'absent from GEOMETRY_ACTIONS, so handleGeometryTools answers INVALID_ACTION '
      + 'and the native handler is never reached',
    ).toEqual([]);
  });

  it('every promoted action is already dispatchable natively', () => {
    const native = readFileSync(NATIVE_PATH, 'utf8');
    const missing = PROMOTED_GEOMETRY_ACTIONS.filter(
      (action) => !native.includes(`SubAction == TEXT("${action}")`),
    );

    expect(
      missing,
      'no native dispatch, so promoting the action would advertise behaviour that does not exist',
    ).toEqual([]);
  });
});
