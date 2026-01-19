import type { GraphQLContext } from './types.js';
import type { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { sanitizePath } from '../utils/path-security.js';
import { sanitizeAssetName } from '../utils/validation.js';

// GraphQL AST node types for parseLiteral
interface ASTFieldNode {
  name: { value: string };
  value: { value: unknown; kind?: string; fields?: ASTFieldNode[] };
}

interface ASTNode {
  kind: string;
  value?: unknown;
  fields?: ASTFieldNode[];
  values?: ASTNode[];
}

export const scalarResolvers = {
  Vector: {
    serialize: (value: unknown) => {
      if (!value) return null;
      return typeof value === 'object' && 'x' in value && 'y' in value && 'z' in value
        ? value
        : null;
    },
    parseValue: (value: unknown) => value,
    parseLiteral: (ast: ASTNode) => {
      if (ast.kind === 'ObjectValue' && ast.fields) {
        const result: Record<string, unknown> = {};
        ast.fields.forEach((field: ASTFieldNode) => {
          result[field.name.value] = field.value.value;
        });
        return result;
      }
      return null;
    }
  },
  Rotator: {
    serialize: (value: unknown) => {
      if (!value) return null;
      return typeof value === 'object' && 'pitch' in value && 'yaw' in value && 'roll' in value
        ? value
        : null;
    },
    parseValue: (value: unknown) => value,
    parseLiteral: (ast: ASTNode) => {
      if (ast.kind === 'ObjectValue' && ast.fields) {
        const result: Record<string, unknown> = {};
        ast.fields.forEach((field: ASTFieldNode) => {
          result[field.name.value] = field.value.value;
        });
        return result;
      }
      return null;
    }
  },
  Transform: {
    serialize: (value: unknown) => value,
    parseValue: (value: unknown) => value,
    parseLiteral: (ast: ASTNode) => {
      if (ast.kind === 'ObjectValue' && ast.fields) {
        const result: Record<string, unknown> = {};
        ast.fields.forEach((field: ASTFieldNode) => {
          if (field.value.kind === 'ObjectValue' && field.value.fields) {
            result[field.name.value] = {};
            const nestedObj = result[field.name.value] as Record<string, unknown>;
            field.value.fields.forEach((f: ASTFieldNode) => {
              nestedObj[f.name.value] = f.value.value;
            });
          } else {
            result[field.name.value] = field.value.value;
          }
        });
        return result;
      }
      return null;
    }
  },
  JSON: {
    serialize: (value: unknown) => value,
    parseValue: (value: unknown) => value,
    parseLiteral: (ast: ASTNode): unknown => {
      switch (ast.kind) {
        case 'StringValue':
          return ast.value;
        case 'BooleanValue':
          return ast.value;
        case 'IntValue':
        case 'FloatValue':
          return Number(ast.value);
        case 'ObjectValue':
          if (ast.fields) {
            const result: Record<string, unknown> = {};
            ast.fields.forEach((field: ASTFieldNode) => {
              result[field.name.value] = scalarResolvers.JSON.parseLiteral(field.value as ASTNode);
            });
            return result;
          }
          return {};
        case 'ListValue':
          return (ast.values ?? []).map((v: ASTNode) => scalarResolvers.JSON.parseLiteral(v));
        default:
          return null;
      }
    }
  }
};

interface Asset {
  name: string;
  path: string;
  class: string;
  packagePath: string;
  size?: number;
  metadata?: Record<string, unknown>;
  tags?: string[];
}

interface Actor {
  name: string;
  class: string;
  location?: { x: number; y: number; z: number };
  rotation?: { pitch: number; yaw: number; roll: number };
  scale?: { x: number; y: number; z: number };
  tags?: string[];
  properties?: Record<string, unknown>;
}

interface Blueprint {
  name: string;
  path: string;
  parentClass?: string;
  variables?: Array<{
    name: string;
    type: string;
    defaultValue?: unknown;
    metadata?: Record<string, unknown>;
  }>;
  functions?: Array<{
    name: string;
    inputs?: Array<{ name: string; type: string }>;
    outputs?: Array<{ name: string; type: string }>;
  }>;
  events?: Array<{ name: string; type: string }>;
  components?: Array<{ name: string; type: string; properties?: Record<string, unknown> }>;
  scsHierarchy?: Record<string, unknown>;
}

import { Logger } from '../utils/logger.js';

const log = new Logger('GraphQL:Resolvers');

/**
 * Creates a GraphQL-friendly error with proper extensions
 */
class GraphQLResolverError extends Error {
  extensions: { code: string; originalError?: string };

  constructor(message: string, code: string = 'UNREAL_ENGINE_ERROR', originalError?: Error) {
    super(message);
    this.name = 'GraphQLResolverError';
    this.extensions = {
      code,
      originalError: originalError?.message
    };
  }
}

/**
 * Helper to create resolver errors with proper logging
 */
function createResolverError(operation: string, error: unknown): GraphQLResolverError {
  const message = error instanceof Error ? error.message : String(error);
  log.error(`${operation} failed:`, message);
  return new GraphQLResolverError(`${operation} failed: ${message}`, 'UNREAL_ENGINE_ERROR', error instanceof Error ? error : undefined);
}

function logAutomationFailure(source: string, response: Record<string, unknown> | null) {
  try {
    if (!response || response.success !== false) {
      return;
    }
    const errorText = ((response.error || response.message || '') as string).toString();
    if (errorText.length === 0) {
      return;
    }
    log.error(`${source} automation failure:`, errorText);
  } catch {
  }
}

/**
 * Helper to get actor properties from Unreal Bridge
 */
async function getActorProperties(
  bridge: UnrealBridge,
  actorName: string
): Promise<Record<string, unknown>> {
  try {
    const result = await bridge.getObjectProperty({
      objectPath: actorName,
      propertyName: '*',
      timeoutMs: 5000
    });
    return result.success ? (result.value as Record<string, unknown>) || {} : {};
  } catch (error) {
    log.error('Failed to get actor properties:', error);
    return {};
  }
}

/**
 * Helper to list assets
 */
async function listAssets(
  automationBridge: AutomationBridge,
  filter?: { class?: string; tag?: string; pathStartsWith?: string },
  pagination?: { offset?: number; limit?: number }
): Promise<{ assets: Asset[]; totalCount: number }> {
  try {
    const safeFilter = { ...filter };
    if (safeFilter.pathStartsWith) {
      safeFilter.pathStartsWith = sanitizePath(safeFilter.pathStartsWith);
    }

    const response = await automationBridge.sendAutomationRequest(
      'list_assets',
      {
        filter: safeFilter,
        pagination: pagination || { offset: 0, limit: 50 }
      },
      { timeoutMs: 30000 }
    );

    if (response.success && response.result) {
      const result = response.result as Record<string, unknown>;
      return {
        assets: (result.assets || []) as Asset[],
        totalCount: (result.totalCount || 0) as number
      };
    }

    logAutomationFailure('list_assets', response);
    log.warn('Failed to list assets - returning empty set');
    return { assets: [], totalCount: 0 };
  } catch (error) {
    log.error('Failed to list assets:', error);
    throw createResolverError('listAssets', error);
  }
}

/**
 * Helper to list actors
 */
async function listActors(
  automationBridge: AutomationBridge,
  filter?: { class?: string; tag?: string }
): Promise<{ actors: Actor[] }> {
  try {
    const response = await automationBridge.sendAutomationRequest(
      'list_actors',
      {
        filter: filter || {}
      },
      { timeoutMs: 30000 }
    );

    if (response.success && response.result) {
      const result = response.result as Record<string, unknown>;
      return {
        actors: (result.actors || []) as Actor[]
      };
    }

    logAutomationFailure('list_actors', response);
    return { actors: [] };
  } catch (error) {
    log.error('Failed to list actors:', error);
    return { actors: [] };
  }
}



/**
 * GraphQL Resolvers Implementation
 */

// Argument types for resolvers
interface AssetFilter {
  class?: string;
  tag?: string;
  pathStartsWith?: string;
}

interface ActorFilter {
  class?: string;
  tag?: string;
}

interface PaginationArgs {
  offset?: number;
  limit?: number;
}

interface ListArgs {
  filter?: AssetFilter | ActorFilter;
  pagination?: PaginationArgs;
}

interface SpawnActorInput {
  class: string;
  name?: string;
  location?: { x: number; y: number; z: number };
  rotation?: { pitch: number; yaw: number; roll: number };
}

interface TransformInput {
  location?: { x: number; y: number; z: number };
  rotation?: { pitch: number; yaw: number; roll: number };
  scale?: { x: number; y: number; z: number };
}

interface BlueprintInput {
  name: string;
  parentClass?: string;
  path?: string;
}

interface VariableInput {
  name: string;
  type: string;
  defaultValue?: unknown;
}

interface FunctionInput {
  name: string;
  inputs?: Array<{ name: string; type: string }>;
  outputs?: Array<{ name: string; type: string }>;
}

export const resolvers = {
  // Query resolvers
  Query: {
    assets: async (_: unknown, args: ListArgs, context: GraphQLContext) => {
      const { filter, pagination } = args;
      const { assets, totalCount } = await listAssets(
        context.automationBridge,
        filter,
        pagination
      );

      // Convert to connection format for pagination
      const edges = assets.map((asset, index) => ({
        node: asset,
        cursor: Buffer.from(`${asset.path}:${index}`).toString('base64')
      }));

      return {
        edges,
        pageInfo: {
          hasNextPage: (pagination?.offset || 0) + assets.length < totalCount,
          hasPreviousPage: (pagination?.offset || 0) > 0,
          startCursor: edges.length > 0 ? edges[0].cursor : null,
          endCursor: edges.length > 0 ? edges[edges.length - 1].cursor : null
        },
        totalCount
      };
    },

    asset: async (_: unknown, { path }: { path: string }, context: GraphQLContext) => {
      try {
        if (!context.loaders) {
          throw new Error('Loaders not initialized');
        }
        return await context.loaders.assetLoader.load(sanitizePath(path));
      } catch (error) {
        log.error('Failed to get asset:', error);
        return null;
      }
    },

    actors: async (_: unknown, args: ListArgs, context: GraphQLContext) => {
      const { filter, pagination } = args;
      const { actors } = await listActors(context.automationBridge, filter);

      const offset = pagination?.offset ?? 0;
      const limit = pagination?.limit ?? 50;

      const paginatedActors = actors.slice(offset, offset + limit);

      const edges = paginatedActors.map((actor, index) => ({
        node: actor,
        cursor: Buffer.from(`${actor.name}:${offset + index}`).toString('base64')
      }));

      return {
        edges,
        pageInfo: {
          hasNextPage: offset + paginatedActors.length < actors.length,
          hasPreviousPage: offset > 0,
          startCursor: edges.length > 0 ? edges[0].cursor : null,
          endCursor: edges.length > 0 ? edges[edges.length - 1].cursor : null
        },
        totalCount: actors.length
      };
    },

    actor: async (_: unknown, { name }: { name: string }, context: GraphQLContext) => {
      try {
        if (!context.loaders) {
          throw new Error('Loaders not initialized');
        }
        return await context.loaders.actorLoader.load(name);
      } catch (error) {
        log.error('Failed to get actor:', error);
        return null;
      }
    },

    blueprints: async (_: unknown, args: ListArgs, context: GraphQLContext) => {
      const { filter, pagination } = args;
      try {
        const safeFilter = { ...filter };
        if (safeFilter.pathStartsWith) {
          safeFilter.pathStartsWith = sanitizePath(safeFilter.pathStartsWith);
        }

        const response = await context.automationBridge.sendAutomationRequest(
          'list_blueprints',
          {
            filter: safeFilter,
            pagination: pagination || { offset: 0, limit: 50 }
          },
          { timeoutMs: 30000 }
        );

        const resultObj = (response.result ?? {}) as Record<string, unknown>;
        const blueprints: Blueprint[] = response.success && response.result
          ? (resultObj.blueprints || []) as Blueprint[]
          : [];

        const edges = blueprints.map((blueprint, index) => ({
          node: blueprint,
          cursor: Buffer.from(`${blueprint.path}:${index}`).toString('base64')
        }));

        const totalCount = (resultObj.totalCount as number) || 0;
        return {
          edges,
          pageInfo: {
            hasNextPage: (pagination?.offset || 0) + blueprints.length < totalCount,
            hasPreviousPage: (pagination?.offset || 0) > 0,
            startCursor: edges.length > 0 ? edges[0].cursor : null,
            endCursor: edges.length > 0 ? edges[edges.length - 1].cursor : null
          },
          totalCount: blueprints.length
        };
      } catch (error) {
        log.error('Failed to list blueprints:', error);

        return {
          edges: [],
          pageInfo: {

            hasNextPage: false,
            hasPreviousPage: false,
            startCursor: null,
            endCursor: null
          },
          totalCount: 0
        };
      }
    },

    blueprint: async (_: unknown, { path }: { path: string }, context: GraphQLContext) => {
      if (!context.loaders) {
        throw new Error('Loaders not initialized');
      }
      return await context.loaders.blueprintLoader.load(sanitizePath(path));
    },

    levels: async (_: unknown, __: unknown, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'list_levels',
          {},
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          const resultObj = response.result as Record<string, unknown>;
          return (resultObj.levels || []) as unknown[];
        }

        return [];
      } catch (error) {
        log.error('Failed to list levels:', error);
        return [];
      }
    },

    currentLevel: async (_: unknown, __: unknown, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_current_level',
          {},
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        return null;
      } catch (error) {
        log.error('Failed to get current level:', error);
        return null;
      }
    },

    materials: async (_: unknown, args: ListArgs, context: GraphQLContext) => {
      const { filter, pagination } = args;
      try {
        const materialFilter = { ...filter, class: 'MaterialInterface' };
        const { assets, totalCount } = await listAssets(
          context.automationBridge,
          materialFilter,
          pagination
        );

        const offset = pagination?.offset ?? 0;
        const edges = assets.map((material, index) => ({
          node: material, // Material type in schema matches Asset mostly
          cursor: Buffer.from(`${material.path}:${offset + index}`).toString('base64')
        }));

        return {
          edges,
          pageInfo: {
            hasNextPage: (pagination?.offset || 0) + assets.length < totalCount,
            hasPreviousPage: (pagination?.offset || 0) > 0,
            startCursor: edges.length > 0 ? edges[0].cursor : null,
            endCursor: edges.length > 0 ? edges[edges.length - 1].cursor : null
          },
          totalCount
        };
      } catch (error) {
        log.error('Failed to list materials:', error);
        return {
          edges: [],
          pageInfo: {
            hasNextPage: false,
            hasPreviousPage: false,
            startCursor: null,
            endCursor: null
          },
          totalCount: 0
        };
      }
    },

    sequences: async (_: unknown, args: ListArgs, context: GraphQLContext) => {
      const { filter, pagination } = args;
      try {
        const sequenceFilter = { ...filter, class: 'LevelSequence' };
        const { assets, totalCount } = await listAssets(
          context.automationBridge,
          sequenceFilter,
          pagination
        );

        const offset = pagination?.offset ?? 0;
        const edges = assets.map((sequence, index) => ({
          node: sequence,
          cursor: Buffer.from(`${sequence.path}:${offset + index}`).toString('base64')
        }));

        return {
          edges,
          pageInfo: {
            hasNextPage: (pagination?.offset || 0) + assets.length < totalCount,
            hasPreviousPage: (pagination?.offset || 0) > 0,
            startCursor: edges.length > 0 ? edges[0].cursor : null,
            endCursor: edges.length > 0 ? edges[edges.length - 1].cursor : null
          },
          totalCount
        };
      } catch (error) {
        log.error('Failed to list sequences:', error);
        return {
          edges: [],
          pageInfo: {
            hasNextPage: false,
            hasPreviousPage: false,
            startCursor: null,
            endCursor: null
          },
          totalCount: 0
        };
      }
    },

    worldPartitionCells: async (_: unknown, __: unknown, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'manage_world_partition',
          { subAction: 'get_cells' },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          const resultObj = response.result as Record<string, unknown>;
          return (resultObj.cells || []) as unknown[];
        }
        return [];
      } catch (error) {
        log.error('Failed to list world partition cells:', error);
        return [];
      }
    },

    niagaraSystems: async (_: unknown, args: ListArgs, context: GraphQLContext) => {
      const { filter, pagination } = args;
      try {
        // Re-use list_assets with filter for NiagaraSystem class
        const niagaraFilter = { ...filter, class: 'NiagaraSystem' };
        const { assets, totalCount } = await listAssets(
          context.automationBridge,
          niagaraFilter,
          pagination
        );

        const offset = pagination?.offset ?? 0;
        const edges = assets.map((asset, index) => ({
          node: {
            ...asset,
            emitters: [],
            parameters: []
          },
          cursor: Buffer.from(`${asset.path}:${offset + index}`).toString('base64')
        }));

        return {
          edges,
          pageInfo: {
            hasNextPage: (pagination?.offset || 0) + assets.length < totalCount,
            hasPreviousPage: (pagination?.offset || 0) > 0,
            startCursor: edges.length > 0 ? edges[0].cursor : null,
            endCursor: edges.length > 0 ? edges[edges.length - 1].cursor : null
          },
          totalCount
        };
      } catch (error) {
        log.error('Failed to list niagara systems:', error);
        return {
          edges: [],
          pageInfo: {
            hasNextPage: false,
            hasPreviousPage: false,
            startCursor: null,
            endCursor: null
          },
          totalCount: 0
        };
      }
    },

    niagaraSystem: async (_: unknown, { path }: { path: string }, context: GraphQLContext) => {
      try {
        // Check if it's a niagara system
        const asset = await context.automationBridge.sendAutomationRequest(
          'get_asset',
          { assetPath: sanitizePath(path) },
          { timeoutMs: 10000 }
        );
        const resultObj = (asset.result ?? {}) as Record<string, unknown>;
        if (asset.success && asset.result && resultObj.class === 'NiagaraSystem') {
          return {
            ...resultObj,
            emitters: [],
            parameters: []
          };
        }
        return null;
      } catch (error) {
        log.error('Failed to get niagara system:', error);
        return null;
      }
    },

    search: async (_: unknown, { query, type }: { query: string; type?: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'search',
          {
            query,
            type: type || 'ALL'
          },
          { timeoutMs: 30000 }
        );

        if (response.success && response.result) {
          const resultObj = response.result as Record<string, unknown>;
          return (resultObj.results || []) as unknown[];
        }

        return [];
      } catch (error) {
        log.error('Failed to search:', error);
        return [];
      }
    }
  },

  // Mutation resolvers
  Mutation: {
    duplicateAsset: async (_: unknown, { path, newName }: { path: string; newName: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'duplicate_asset',
          {
            assetPath: sanitizePath(path),
            newName: sanitizeAssetName(newName)
          },
          { timeoutMs: 60000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to duplicate asset');
      } catch (error) {
        log.error('Failed to duplicate asset:', error);
        throw error;
      }
    },

    moveAsset: async (_: unknown, { path, newPath }: { path: string; newPath: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'move_asset',
          {
            assetPath: sanitizePath(path),
            destinationPath: sanitizePath(newPath)
          },
          { timeoutMs: 60000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to move asset');
      } catch (error) {
        log.error('Failed to move asset:', error);
        throw error;
      }
    },

    deleteAsset: async (_: unknown, { path }: { path: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'delete_asset',
          {
            assetPath: sanitizePath(path)
          },
          { timeoutMs: 30000 }
        );

        return response.success || false;
      } catch (error) {
        log.error('Failed to delete asset:', error);
        return false;
      }
    },

    spawnActor: async (_: unknown, { input }: { input: SpawnActorInput }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'spawn_actor',
          { ...input },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to spawn actor');
      } catch (error) {
        log.error('Failed to spawn actor:', error);
        throw error;
      }
    },

    deleteActor: async (_: unknown, { name }: { name: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'delete_actor',
          {
            actorName: name
          },
          { timeoutMs: 10000 }
        );

        return response.success || false;
      } catch (error) {
        log.error('Failed to delete actor:', error);
        return false;
      }
    },

    setActorTransform: async (_: unknown, { name, transform }: { name: string; transform: TransformInput }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'set_actor_transform',
          {
            actorName: name,
            transform
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to set actor transform');
      } catch (error) {
        log.error('Failed to set actor transform:', error);
        throw error;
      }
    },

    createBlueprint: async (_: unknown, { input }: { input: BlueprintInput }, context: GraphQLContext) => {
      try {
        // Sanitize input
        const sanitizedInput = {
          ...input,
          name: sanitizeAssetName(input.name),
          path: input.path ? sanitizePath(input.path) : undefined
        };

        const response = await context.automationBridge.sendAutomationRequest(
          'create_blueprint',
          sanitizedInput,
          { timeoutMs: 60000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to create blueprint');
      } catch (error) {
        log.error('Failed to create blueprint:', error);
        throw error;
      }
    },

    addVariableToBlueprint: async (_: unknown, { path, input }: { path: string; input: VariableInput }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'add_variable_to_blueprint',
          {
            blueprintPath: sanitizePath(path),
            ...input
          },
          { timeoutMs: 30000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to add variable to blueprint');
      } catch (error) {
        log.error('Failed to add variable to blueprint:', error);
        throw error;
      }
    },

    addFunctionToBlueprint: async (_: unknown, { path, input }: { path: string; input: FunctionInput }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'add_function_to_blueprint',
          {
            blueprintPath: sanitizePath(path),
            ...input
          },
          { timeoutMs: 30000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to add function to blueprint');
      } catch (error) {
        log.error('Failed to add function to blueprint:', error);
        throw error;
      }
    },

    loadLevel: async (_: unknown, { path }: { path: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'load_level',
          {
            levelPath: sanitizePath(path)
          },
          { timeoutMs: 30000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to load level');
      } catch (error) {
        log.error('Failed to load level:', error);
        throw error;
      }
    },

    saveLevel: async (_: unknown, { path }: { path?: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'save_level',
          {
            levelPath: path ? sanitizePath(path) : undefined
          },
          { timeoutMs: 30000 }
        );

        return response.success || false;
      } catch (error) {
        log.error('Failed to save level:', error);
        return false;
      }
    },

    createMaterialInstance: async (_: unknown, { parentPath, name, parameters }: { parentPath: string; name: string; parameters?: Record<string, unknown> }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'create_material_instance',
          {
            parentMaterialPath: sanitizePath(parentPath),
            instanceName: sanitizeAssetName(name),
            parameters: parameters || {}
          },
          { timeoutMs: 30000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to create material instance');
      } catch (error) {
        log.error('Failed to create material instance:', error);
        throw error;
      }
    }
  },

  // Field resolvers
  Asset: {
    dependencies: async (parent: Asset, _: unknown, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_asset_dependencies',
          {
            assetPath: parent.path
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          const resultObj = response.result as Record<string, unknown>;
          return (resultObj.dependencies || []) as string[];
        }

        return [];
      } catch (error) {
        log.error('Failed to get asset dependencies:', error);
        return [];
      }
    },

    dependents: async (parent: Asset, _: unknown, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_asset_dependents',
          {
            assetPath: parent.path
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          const resultObj = response.result as Record<string, unknown>;
          return (resultObj.dependents || []) as string[];
        }

        return [];
      } catch (error) {
        log.error('Failed to get asset dependents:', error);
        return [];
      }
    }
  },

  Actor: {
    properties: async (parent: Actor, _: unknown, context: GraphQLContext) => {
      return await getActorProperties(context.bridge, parent.name);
    },

    components: async (parent: Actor, _: unknown, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_actor_components',
          {
            actorName: parent.name
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          const resultObj = response.result as Record<string, unknown>;
          return (resultObj.components || []) as unknown[];
        }

        return [];
      } catch (error) {
        log.error('Failed to get actor components:', error);
        return [];
      }
    }
  },

  Blueprint: {
    variables: async (parent: Blueprint, _args: unknown, _context: GraphQLContext) => {
      return parent.variables || [];
    },

    functions: async (parent: Blueprint, _args: unknown, _context: GraphQLContext) => {
      return parent.functions || [];
    },

    events: async (parent: Blueprint, _args: unknown, _context: GraphQLContext) => {
      return parent.events || [];
    },

    components: async (parent: Blueprint, _args: unknown, _context: GraphQLContext) => {
      return parent.components || [];
    }
  },

  Material: {
    parameters: async (parent: Asset, _: unknown, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_material_parameters',
          {
            materialPath: parent.path
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          const resultObj = response.result as Record<string, unknown>;
          return (resultObj.parameters || []) as unknown[];
        }

        return [];
      } catch (error) {
        log.error('Failed to get material parameters:', error);
        return [];
      }
    }
  },

  Sequence: {
    tracks: async (parent: Asset, _: unknown, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_sequence_tracks',
          {
            sequencePath: parent.path
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          const resultObj = response.result as Record<string, unknown>;
          return (resultObj.tracks || []) as unknown[];
        }

        return [];
      } catch (error) {
        log.error('Failed to get sequence tracks:', error);
        return [];
      }
    }
  }
};
