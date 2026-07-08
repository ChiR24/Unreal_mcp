import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { ComponentInfo, HandlerArgs, InspectArgs } from '../../../types/handlers/handler-types.js';

export interface InspectResponse {
  success?: boolean;
  error?: string;
  message?: string;
  components?: ComponentInfo[];
  value?: unknown;
  objects?: unknown[];
  cdo?: unknown;
  result?: unknown;
  [key: string]: unknown;
}

export interface InspectHandlerContext {
  readonly args: HandlerArgs;
  readonly inspectArgs: InspectArgs;
  readonly normalizedArgs: HandlerArgs;
  readonly originalAction: string;
  readonly tools: ITools;
}

const INSPECT_ACTION_ALIASES: Record<string, string> = {
  get_actor_details: 'inspect_object',
  get_material_details: 'inspect_object',
  get_texture_details: 'inspect_object',
  get_mesh_details: 'inspect_object',
  get_blueprint_details: 'inspect_object',
  // A level/world has no `objectPath`, so aliasing to inspect_object failed 'Invalid objectPath'. Route to
  // get_world_settings, which returns the level summary (worldName/levelName/packageName/gravity/killZ/time).
  get_level_details: 'get_world_settings',
  get_project_settings: 'get_project_settings',
  get_editor_settings: 'get_editor_settings',
  get_performance_stats: 'get_performance_stats',
  get_memory_stats: 'get_memory_stats',
  get_scene_stats: 'get_scene_stats',
  get_viewport_info: 'get_viewport_info',
  get_selected_actors: 'get_selected_actors',
  pie_report: 'runtime_report'
};

export function normalizeInspectAction(action: string): string {
  return INSPECT_ACTION_ALIASES[action] ?? action;
}

export function createInspectContext(action: string, args: HandlerArgs, tools: ITools): {
  readonly normalizedAction: string;
  readonly context: InspectHandlerContext;
} {
  const normalizedArgs: HandlerArgs = {
    ...args,
    actorName: args.actor_name ?? args.actorName ?? args.name,
    objectPath: args.object_path ?? args.objectPath ?? args.path,
    componentName: args.component_name ?? args.componentName,
    componentNames: args.component_names ?? args.componentNames,
    propertyName: args.property_name ?? args.propertyName ?? args.propertyPath,
    propertyNames: args.property_names ?? args.propertyNames
  };

  return {
    normalizedAction: normalizeInspectAction(action),
    context: {
      args,
      inspectArgs: args as InspectArgs,
      normalizedArgs,
      originalAction: action,
      tools
    }
  };
}
