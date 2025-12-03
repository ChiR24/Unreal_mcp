import type { GraphQLContext } from './types.js';
import type { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';

export const scalarResolvers = {
  Vector: {
    serialize: (value: any) => {
      if (!value) return null;
      return typeof value === 'object' && 'x' in value && 'y' in value && 'z' in value
        ? value
        : null;
    },
    parseValue: (value: any) => value,
    parseLiteral: (ast: any) => {
      if (ast.kind === 'ObjectValue') {
        const value: any = {};
        ast.fields.forEach((field: any) => {
          value[field.name.value] = field.value.value;
        });
        return value;
      }
      return null;
    }
  },
  Rotator: {
    serialize: (value: any) => {
      if (!value) return null;
      return typeof value === 'object' && 'pitch' in value && 'yaw' in value && 'roll' in value
        ? value
        : null;
    },
    parseValue: (value: any) => value,
    parseLiteral: (ast: any) => {
      if (ast.kind === 'ObjectValue') {
        const value: any = {};
        ast.fields.forEach((field: any) => {
          value[field.name.value] = field.value.value;
        });
        return value;
      }
      return null;
    }
  },
  Transform: {
    serialize: (value: any) => value,
    parseValue: (value: any) => value,
    parseLiteral: (ast: any) => {
      if (ast.kind === 'ObjectValue') {
        const value: any = {};
        ast.fields.forEach((field: any) => {
          if (field.value.kind === 'ObjectValue') {
            value[field.name.value] = {};
            field.value.fields.forEach((f: any) => {
              value[field.name.value][f.name.value] = f.value.value;
            });
          } else {
            value[field.name.value] = field.value.value;
          }
        });
        return value;
      }
      return null;
    }
  },
  JSON: {
    serialize: (value: any) => value,
    parseValue: (value: any) => value,
    parseLiteral: (ast: any) => {
      switch (ast.kind) {
        case 'StringValue':
          return ast.value;
        case 'BooleanValue':
          return ast.value;
        case 'IntValue':
        case 'FloatValue':
          return Number(ast.value);
        case 'ObjectValue':
          const value: any = {};
          ast.fields.forEach((field: any) => {
            value[field.name.value] = (scalarResolvers as any).JSON.parseLiteral(field.value);
          });
          return value;
        case 'ListValue':
          return ast.values.map((v: any) => (scalarResolvers as any).JSON.parseLiteral(v));
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
  metadata?: Record<string, any>;
  tags?: string[];
}

interface Actor {
  name: string;
  class: string;
  location?: { x: number; y: number; z: number };
  rotation?: { pitch: number; yaw: number; roll: number };
  scale?: { x: number; y: number; z: number };
  tags?: string[];
  properties?: Record<string, any>;
}

interface Blueprint {
  name: string;
  path: string;
  parentClass?: string;
  variables?: Array<{
    name: string;
    type: string;
    defaultValue?: any;
    metadata?: Record<string, any>;
  }>;
  functions?: Array<{
    name: string;
    inputs?: Array<{ name: string; type: string }>;
    outputs?: Array<{ name: string; type: string }>;
  }>;
  events?: Array<{ name: string; type: string }>;
  components?: Array<{ name: string; type: string; properties?: Record<string, any> }>;
  scsHierarchy?: Record<string, any>;
}

function logAutomationFailure(source: string, response: any) {
  try {
    if (!response || response.success !== false) {
      return;
    }
    const errorText = (response.error || response.message || '').toString();
    if (errorText.length === 0) {
      return;
    }
    console.error(`[GraphQL] ${source} automation failure:`, errorText);
  } catch {
  }
}

/**
 * Helper to get actor properties from Unreal Bridge
 */
async function getActorProperties(
  bridge: UnrealBridge,
  actorName: string
): Promise<Record<string, any>> {
  try {
    const result = await bridge.getObjectProperty({
      objectPath: actorName,
      propertyName: '*',
      timeoutMs: 5000
    });
    return result.success ? result.value || {} : {};
  } catch (error) {
    console.error('Failed to get actor properties:', error);
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
    const response = await automationBridge.sendAutomationRequest(
      'list_assets',
      {
        filter: filter || {},
        pagination: pagination || { offset: 0, limit: 50 }
      },
      { timeoutMs: 30000 }
    );

    if (response.success && response.result) {
      const result = response.result as any;
      return {
        assets: result.assets || [],
        totalCount: result.totalCount || 0
      };
    }

    logAutomationFailure('list_assets', response);
    console.error('Failed to list assets:', response);
    return { assets: [], totalCount: 0 };
  } catch (error) {
    console.error('Failed to list assets:', error);
    return { assets: [], totalCount: 0 };
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
      const result = response.result as any;
      return {
        actors: result.actors || []
      };
    }

    logAutomationFailure('list_actors', response);
    return { actors: [] };
  } catch (error) {
    console.error('Failed to list actors:', error);
    return { actors: [] };
  }
}

/**
 * Helper to get blueprint details
 */
async function getBlueprint(
  automationBridge: AutomationBridge,
  blueprintPath: string
): Promise<Blueprint | null> {
  try {
    const response = await automationBridge.sendAutomationRequest(
      'get_blueprint',
      {
        blueprintPath
      },
      { timeoutMs: 30000 }
    );

    if (response.success && response.result) {
      return response.result as Blueprint;
    }

    logAutomationFailure('get_blueprint', response);
    return null;
  } catch (error) {
    console.error('Failed to get blueprint:', error);
    return null;
  }
}

/**
 * GraphQL Resolvers Implementation
 */
export const resolvers = {
  // Query resolvers
  Query: {
    assets: async (_: any, args: any, context: GraphQLContext) => {
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

    asset: async (_: any, { path }: { path: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_asset',
          { assetPath: path },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        return null;
      } catch (error) {
        console.error('Failed to get asset:', error);
        return null;
      }
    },

    actors: async (_: any, args: any, context: GraphQLContext) => {
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

    actor: async (_: any, { name }: { name: string }, context: GraphQLContext) => {
      try {
        const actors = await listActors(context.automationBridge, { tag: name });
        if (actors.actors.length > 0) {
          return actors.actors[0];
        }
        return null;
      } catch (error) {
        console.error('Failed to get actor:', error);
        return null;
      }
    },

    blueprints: async (_: any, args: any, context: GraphQLContext) => {
      const { filter, pagination } = args;
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'list_blueprints',
          {
            filter: filter || {},
            pagination: pagination || { offset: 0, limit: 50 }
          },
          { timeoutMs: 30000 }
        );

        const blueprints: Blueprint[] = response.success && response.result
          ? (response.result as any).blueprints || []
          : [];

        const edges = blueprints.map((blueprint, index) => ({
          node: blueprint,
          cursor: Buffer.from(`${blueprint.path}:${index}`).toString('base64')
        }));

        return {
          edges,
          pageInfo: {
            hasNextPage: (pagination?.offset || 0) + blueprints.length < (response.result as any)?.totalCount || 0,
            hasPreviousPage: (pagination?.offset || 0) > 0,
            startCursor: edges.length > 0 ? edges[0].cursor : null,
            endCursor: edges.length > 0 ? edges[edges.length - 1].cursor : null
          },
          totalCount: blueprints.length
        };
      } catch (error) {
        console.error('Failed to list blueprints:', error);
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

    blueprint: async (_: any, { path }: { path: string }, context: GraphQLContext) => {
      return await getBlueprint(context.automationBridge, path);
    },

    levels: async (_: any, __: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'list_levels',
          {},
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return (response.result as any).levels || [];
        }

        return [];
      } catch (error) {
        console.error('Failed to list levels:', error);
        return [];
      }
    },

    currentLevel: async (_: any, __: any, context: GraphQLContext) => {
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
        console.error('Failed to get current level:', error);
        return null;
      }
    },

    materials: async (_: any, args: any, context: GraphQLContext) => {
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
        console.error('Failed to list materials:', error);
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

    sequences: async (_: any, args: any, context: GraphQLContext) => {
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
        console.error('Failed to list sequences:', error);
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

    worldPartitionCells: async (_: any, __: any, _context: GraphQLContext) => {
      try {
        // Mock response as actual tool for listing cells might not exist or is complex
        // Using a dummy request to test connectivity or just return empty for now if not implemented
        return [];
      } catch (error) {
        console.error('Failed to list world partition cells:', error);
        return [];
      }
    },

    niagaraSystems: async (_: any, args: any, context: GraphQLContext) => {
      const { filter, pagination } = args;
      try {
        // Re-use list_assets with filter for NiagaraSystem class
        const niagaraFilter = { ...filter, class: 'NiagaraSystem' };
        const { assets, totalCount } = await listAssets(
          context.automationBridge,
          niagaraFilter,
          pagination
        );

        // Enrich assets with emitters/parameters if possible, or return basic asset shape
        // For GraphQL we might need to fetch details for each if fields are requested, 
        // but for list we return the assets cast as NiagaraSystem.

        const offset = pagination?.offset ?? 0;
        const edges = assets.map((asset, index) => ({
          node: {
            ...asset,
            emitters: [], // Placeholder, would require fetch details
            parameters: [] // Placeholder
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
        console.error('Failed to list niagara systems:', error);
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

    niagaraSystem: async (_: any, { path }: { path: string }, context: GraphQLContext) => {
      try {
        // Check if it's a niagara system
        const asset = await context.automationBridge.sendAutomationRequest(
          'get_asset',
          { assetPath: path },
          { timeoutMs: 10000 }
        );
        if (asset.success && asset.result && (asset.result as any).class === 'NiagaraSystem') {
          return {
            ...(asset.result as any),
            emitters: [],
            parameters: []
          };
        }
        return null;
      } catch (error) {
        console.error('Failed to get niagara system:', error);
        return null;
      }
    },

    search: async (_: any, { query, type }: any, context: GraphQLContext) => {
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
          return (response.result as any).results || [];
        }

        return [];
      } catch (error) {
        console.error('Failed to search:', error);
        return [];
      }
    }
  },

  // Mutation resolvers
  Mutation: {
    duplicateAsset: async (_: any, { path, newName }: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'duplicate_asset',
          {
            assetPath: path,
            newName
          },
          { timeoutMs: 60000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to duplicate asset');
      } catch (error) {
        console.error('Failed to duplicate asset:', error);
        throw error;
      }
    },

    moveAsset: async (_: any, { path, newPath }: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'move_asset',
          {
            assetPath: path,
            destinationPath: newPath
          },
          { timeoutMs: 60000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to move asset');
      } catch (error) {
        console.error('Failed to move asset:', error);
        throw error;
      }
    },

    deleteAsset: async (_: any, { path }: { path: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'delete_asset',
          {
            assetPath: path
          },
          { timeoutMs: 30000 }
        );

        return response.success || false;
      } catch (error) {
        console.error('Failed to delete asset:', error);
        return false;
      }
    },

    spawnActor: async (_: any, { input }: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'spawn_actor',
          input,
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to spawn actor');
      } catch (error) {
        console.error('Failed to spawn actor:', error);
        throw error;
      }
    },

    deleteActor: async (_: any, { name }: { name: string }, context: GraphQLContext) => {
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
        console.error('Failed to delete actor:', error);
        return false;
      }
    },

    setActorTransform: async (_: any, { name, transform }: any, context: GraphQLContext) => {
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
        console.error('Failed to set actor transform:', error);
        throw error;
      }
    },

    createBlueprint: async (_: any, { input }: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'create_blueprint',
          input,
          { timeoutMs: 60000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to create blueprint');
      } catch (error) {
        console.error('Failed to create blueprint:', error);
        throw error;
      }
    },

    addVariableToBlueprint: async (_: any, { path, input }: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'add_variable_to_blueprint',
          {
            blueprintPath: path,
            ...input
          },
          { timeoutMs: 30000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to add variable to blueprint');
      } catch (error) {
        console.error('Failed to add variable to blueprint:', error);
        throw error;
      }
    },

    addFunctionToBlueprint: async (_: any, { path, input }: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'add_function_to_blueprint',
          {
            blueprintPath: path,
            ...input
          },
          { timeoutMs: 30000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to add function to blueprint');
      } catch (error) {
        console.error('Failed to add function to blueprint:', error);
        throw error;
      }
    },

    loadLevel: async (_: any, { path }: { path: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'load_level',
          {
            levelPath: path
          },
          { timeoutMs: 30000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to load level');
      } catch (error) {
        console.error('Failed to load level:', error);
        throw error;
      }
    },

    saveLevel: async (_: any, { path }: { path?: string }, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'save_level',
          {
            levelPath: path
          },
          { timeoutMs: 30000 }
        );

        return response.success || false;
      } catch (error) {
        console.error('Failed to save level:', error);
        return false;
      }
    },

    createMaterialInstance: async (_: any, { parentPath, name, parameters }: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'create_material_instance',
          {
            parentMaterialPath: parentPath,
            instanceName: name,
            parameters: parameters || {}
          },
          { timeoutMs: 30000 }
        );

        if (response.success && response.result) {
          return response.result;
        }

        throw new Error(response.error || 'Failed to create material instance');
      } catch (error) {
        console.error('Failed to create material instance:', error);
        throw error;
      }
    }
  },

  // Field resolvers
  Asset: {
    dependencies: async (parent: Asset, _: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_asset_dependencies',
          {
            assetPath: parent.path
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return (response.result as any).dependencies || [];
        }

        return [];
      } catch (error) {
        console.error('Failed to get asset dependencies:', error);
        return [];
      }
    },

    dependents: async (parent: Asset, _: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_asset_dependents',
          {
            assetPath: parent.path
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return (response.result as any).dependents || [];
        }

        return [];
      } catch (error) {
        console.error('Failed to get asset dependents:', error);
        return [];
      }
    }
  },

  Actor: {
    properties: async (parent: Actor, _: any, context: GraphQLContext) => {
      return await getActorProperties(context.bridge, parent.name);
    },

    components: async (parent: Actor, _: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_actor_components',
          {
            actorName: parent.name
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return (response.result as any).components || [];
        }

        return [];
      } catch (error) {
        console.error('Failed to get actor components:', error);
        return [];
      }
    }
  },

  Blueprint: {
    variables: async (parent: Blueprint, _args: any, _context: GraphQLContext) => {
      return parent.variables || [];
    },

    functions: async (parent: Blueprint, _args: any, _context: GraphQLContext) => {
      return parent.functions || [];
    },

    events: async (parent: Blueprint, _args: any, _context: GraphQLContext) => {
      return parent.events || [];
    },

    components: async (parent: Blueprint, _args: any, _context: GraphQLContext) => {
      return parent.components || [];
    }
  },

  Material: {
    parameters: async (parent: any, _: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_material_parameters',
          {
            materialPath: parent.path
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return (response.result as any).parameters || [];
        }

        return [];
      } catch (error) {
        console.error('Failed to get material parameters:', error);
        return [];
      }
    }
  },

  Sequence: {
    tracks: async (parent: any, _: any, context: GraphQLContext) => {
      try {
        const response = await context.automationBridge.sendAutomationRequest(
          'get_sequence_tracks',
          {
            sequencePath: parent.path
          },
          { timeoutMs: 10000 }
        );

        if (response.success && response.result) {
          return (response.result as any).tracks || [];
        }

        return [];
      } catch (error) {
        console.error('Failed to get sequence tracks:', error);
        return [];
      }
    }
  }
};
