/**
 * DataLoader instances for GraphQL N+1 query optimization
 * 
 * DataLoaders batch and cache requests within a single GraphQL request,
 * preventing the N+1 query problem when resolving nested fields.
 */

import DataLoader from 'dataloader';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('GraphQL:Loaders');

// ============================================================================
// Types
// ============================================================================

export interface Actor {
    name: string;
    label?: string;
    class?: string;
    path?: string;
    location?: { x: number; y: number; z: number };
    rotation?: { pitch: number; yaw: number; roll: number };
    scale?: { x: number; y: number; z: number };
    tags?: string[];
}

export interface Asset {
    name: string;
    path: string;
    class?: string;
    packagePath?: string;
    size?: number;
}

export interface Blueprint {
    name: string;
    path: string;
    parentClass?: string;
    variables?: Array<{ name: string; type: string; defaultValue?: unknown }>;
    functions?: Array<{ name: string; parameters?: Array<{ name: string; type: string }> }>;
    components?: Array<{ name: string; type: string }>;
}

// ============================================================================
// Loader Factory
// ============================================================================

export interface GraphQLLoaders {
    actorLoader: DataLoader<string, Actor | null>;
    assetLoader: DataLoader<string, Asset | null>;
    blueprintLoader: DataLoader<string, Blueprint | null>;
    actorComponentsLoader: DataLoader<string, Array<{ name: string; type: string }> | null>;
}

/**
 * Creates DataLoader instances for a GraphQL request context.
 * 
 * Each GraphQL request should have its own set of loaders to ensure
 * proper request-scoped caching and batching.
 */
export function createLoaders(automationBridge: AutomationBridge): GraphQLLoaders {
    return {
        /**
         * Batches actor fetches by name
         */
        actorLoader: new DataLoader<string, Actor | null>(
            async (names: readonly string[]) => {
                log.debug(`Batching actor fetch for ${names.length} actors`);

                try {
                    // Use batch fetch if available, otherwise fall back to individual fetches
                    const result = await automationBridge.sendAutomationRequest<{
                        success: boolean;
                        actors?: Actor[];
                    }>('control_actor', {
                        action: 'batch_get',
                        actorNames: [...names]
                    });

                    if (result.success && result.actors) {
                        // Map results back to input order
                        return names.map(name =>
                            result.actors?.find(a => a.name === name || a.label === name) ?? null
                        );
                    }
                } catch (error) {
                    log.debug('Batch fetch not supported, falling back to individual fetches');
                }

                // Fallback: fetch individually
                const results = await Promise.all(
                    names.map(async (name) => {
                        try {
                            const result = await automationBridge.sendAutomationRequest<{
                                success: boolean;
                                actor?: Actor;
                            }>('control_actor', {
                                action: 'find_by_name',
                                actorName: name
                            });
                            return result.success ? (result.actor ?? null) : null;
                        } catch {
                            return null;
                        }
                    })
                );
                return results;
            },
            {
                cache: true,
                maxBatchSize: 50
            }
        ),

        /**
         * Batches asset fetches by path
         */
        assetLoader: new DataLoader<string, Asset | null>(
            async (paths: readonly string[]) => {
                log.debug(`Batching asset fetch for ${paths.length} assets`);

                try {
                    const result = await automationBridge.sendAutomationRequest<{
                        success: boolean;
                        assets?: Asset[];
                    }>('manage_asset', {
                        action: 'batch_get',
                        assetPaths: [...paths]
                    });

                    if (result.success && result.assets) {
                        return paths.map(path =>
                            result.assets?.find(a => a.path === path) ?? null
                        );
                    }
                } catch {
                    log.debug('Batch asset fetch not supported');
                }

                // Fallback: check existence individually
                const results = await Promise.all(
                    paths.map(async (path) => {
                        try {
                            const result = await automationBridge.sendAutomationRequest<{
                                success: boolean;
                                exists?: boolean;
                                asset?: Asset;
                            }>('manage_asset', {
                                action: 'exists',
                                assetPath: path
                            });

                            if (result.success && result.exists) {
                                return result.asset ?? { name: path.split('/').pop() || '', path };
                            }
                            return null;
                        } catch {
                            return null;
                        }
                    })
                );
                return results;
            },
            {
                cache: true,
                maxBatchSize: 100
            }
        ),

        /**
         * Batches blueprint fetches by path
         */
        blueprintLoader: new DataLoader<string, Blueprint | null>(
            async (paths: readonly string[]) => {
                log.debug(`Batching blueprint fetch for ${paths.length} blueprints`);

                const results = await Promise.all(
                    paths.map(async (path) => {
                        try {
                            const result = await automationBridge.sendAutomationRequest<{
                                success: boolean;
                                blueprint?: Blueprint;
                            }>('manage_blueprint', {
                                action: 'get_blueprint',
                                blueprintPath: path
                            });
                            return result.success ? (result.blueprint ?? null) : null;
                        } catch {
                            return null;
                        }
                    })
                );
                return results;
            },
            {
                cache: true,
                maxBatchSize: 20
            }
        ),

        /**
         * Batches actor component fetches
         */
        actorComponentsLoader: new DataLoader<string, Array<{ name: string; type: string }> | null>(
            async (actorNames: readonly string[]) => {
                log.debug(`Batching component fetch for ${actorNames.length} actors`);

                const results = await Promise.all(
                    actorNames.map(async (actorName) => {
                        try {
                            const result = await automationBridge.sendAutomationRequest<{
                                success: boolean;
                                components?: Array<{ name: string; type: string }>;
                            }>('control_actor', {
                                action: 'get_components',
                                actorName
                            });
                            return result.success ? (result.components ?? null) : null;
                        } catch {
                            return null;
                        }
                    })
                );
                return results;
            },
            {
                cache: true,
                maxBatchSize: 30
            }
        )
    };
}

/**
 * Clears all loader caches (useful between mutations)
 */
export function clearLoaders(loaders: GraphQLLoaders): void {
    loaders.actorLoader.clearAll();
    loaders.assetLoader.clearAll();
    loaders.blueprintLoader.clearAll();
    loaders.actorComponentsLoader.clearAll();
}
