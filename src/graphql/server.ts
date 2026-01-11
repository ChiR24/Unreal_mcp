import { createYoga, type Plugin } from 'graphql-yoga';
import { createServer, type RequestListener } from 'http';
import { GraphQLError, type DocumentNode, Kind, type SelectionSetNode, type SelectionNode } from 'graphql';
import { Logger } from '../utils/logger.js';
import { createGraphQLSchema } from './schema.js';
import type { GraphQLContext } from './types.js';
import type { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { createLoaders } from './loaders.js';

export interface GraphQLServerConfig {
  enabled?: boolean;
  port?: number;
  host?: string;
  path?: string;
  cors?: {
    origin?: string | string[];
    credentials?: boolean;
  };
}

export class GraphQLServer {
  private log = new Logger('GraphQLServer');
  private server: ReturnType<typeof createServer> | null = null;
  private config: Required<GraphQLServerConfig>;
  private bridge: UnrealBridge;
  private automationBridge: AutomationBridge;

  // Rate limiting
  private requestCounts = new Map<string, { count: number; resetAt: number }>();
  private readonly RATE_LIMIT_WINDOW_MS = 60000;
  private readonly RATE_LIMIT_MAX_REQUESTS = 60;

  // Depth limiting
  private static readonly MAX_QUERY_DEPTH = 10;

  constructor(
    bridge: UnrealBridge,
    automationBridge: AutomationBridge,
    config: GraphQLServerConfig = {}
  ) {
    this.bridge = bridge;
    this.automationBridge = automationBridge;
    this.config = {
      enabled: config.enabled ?? process.env.GRAPHQL_ENABLED === 'true',
      port: config.port ?? Number(process.env.GRAPHQL_PORT ?? 4000),
      host: config.host ?? process.env.GRAPHQL_HOST ?? '127.0.0.1',
      path: config.path ?? process.env.GRAPHQL_PATH ?? '/graphql',
      cors: config.cors ?? {
        origin: process.env.GRAPHQL_CORS_ORIGIN ?? '*',
        credentials: process.env.GRAPHQL_CORS_CREDENTIALS === 'true'
      }
    };
  }

  async start(): Promise<void> {
    if (!this.config.enabled) {
      this.log.info('GraphQL server is disabled');
      return;
    }

    const isLoopback = this.config.host === '127.0.0.1' ||
                        this.config.host === '::1' ||
                        this.config.host.toLowerCase() === 'localhost';

    const allowRemote = process.env.GRAPHQL_ALLOW_REMOTE === 'true';

    if (!isLoopback && !allowRemote) {
      this.log.warn(
        `GraphQL server is configured to bind to non-loopback host '${this.config.host}'. GraphQL is for local debugging only. ` +
          'To allow remote binding, set GRAPHQL_ALLOW_REMOTE=true. Aborting start.'
      );
      return;
    }

    if (!isLoopback && allowRemote) {
      if (this.config.cors.origin === '*') {
        this.log.error(
          'GraphQL server cannot bind to remote host with wildcard CORS origin. ' +
            'Set GRAPHQL_CORS_ORIGIN to specific origins.'
        );
        return; // Abort startup - security risk
      }
    }

    try {
      const schema = createGraphQLSchema(this.bridge, this.automationBridge);

      const yoga = createYoga({
        schema,
        graphqlEndpoint: this.config.path,
        plugins: [this.createDepthLimitPlugin()],
        cors: {
          origin: this.config.cors.origin,
          credentials: this.config.cors.credentials,
          methods: ['GET', 'POST', 'OPTIONS'],
          allowedHeaders: ['Content-Type', 'Authorization']
        },
        context: ({ request }): GraphQLContext => {
          // Rate limiting check
          const ip = request.headers.get('x-forwarded-for')?.split(',')[0]?.trim() || '127.0.0.1';
          if (!this.checkRateLimit(ip)) {
            this.log.warn(`Rate limit exceeded for IP: ${ip}`);
            throw new GraphQLError('Rate limit exceeded. Try again later.', {
              extensions: { code: 'RATE_LIMITED', http: { status: 429 } }
            });
          }

          return {
            bridge: this.bridge,
            automationBridge: this.automationBridge,
            loaders: createLoaders(this.automationBridge)
          };
        },
        logging: {
          debug: (...args) => this.log.debug('[GraphQL]', ...args),
          info: (...args) => this.log.info('[GraphQL]', ...args),
          warn: (...args) => this.log.warn('[GraphQL]', ...args),
          error: (...args) => this.log.error('[GraphQL]', ...args)
        }
      });

      this.server = createServer(
        yoga as RequestListener
      );

      await new Promise<void>((resolve, reject) => {
        if (!this.server) {
          reject(new Error('Server not initialized'));
          return;
        }

        this.server.on('error', (error) => {
          const errorObj = error as unknown as { code?: unknown };
          const errorCode = typeof errorObj?.code === 'string' ? errorObj.code : undefined;

          // GraphQL is optional; treat port-in-use as non-fatal (common in dev + CI smoke).
          if (errorCode === 'EADDRINUSE') {
            this.log.warn(
              `GraphQL server port ${this.config.host}:${this.config.port} is already in use; skipping GraphQL startup. ` +
                'Set GRAPHQL_ENABLED=false to disable, or change GRAPHQL_PORT to a free port.'
            );
            resolve();
            return;
          }

          this.log.error('GraphQL server error:', error);
          reject(error);
        });

        this.server.listen(this.config.port, this.config.host, () => {
          this.log.info(
            `GraphQL server started at http://${this.config.host}:${this.config.port}${this.config.path}`
          );
          this.log.info('GraphQL Playground available at the endpoint URL');
          resolve();
        });
      });
    } catch (error: unknown) {
      this.log.error('Failed to start GraphQL server:', error);
      throw error;
    }
  }

  async stop(): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.server) {
        resolve();
        return;
      }

      this.server.close((error) => {
        if (error) {
          this.log.error('Error closing GraphQL server:', error);
          reject(error);
        } else {
          this.log.info('GraphQL server stopped');
          resolve();
        }
      });
    });
  }

  getConfig() {
    return this.config;
  }

  isRunning(): boolean {
    return this.server !== null && this.server.listening;
  }

  /**
   * Check rate limit for a given IP address
   * @returns true if request is allowed, false if rate limited
   */
  private checkRateLimit(ip: string): boolean {
    const now = Date.now();
    const record = this.requestCounts.get(ip);
    if (!record || now >= record.resetAt) {
      this.requestCounts.set(ip, { count: 1, resetAt: now + this.RATE_LIMIT_WINDOW_MS });
      return true;
    }
    if (record.count >= this.RATE_LIMIT_MAX_REQUESTS) {
      return false;
    }
    record.count++;
    return true;
  }

  /**
   * Calculate the maximum depth of a GraphQL selection set
   */
  private static getQueryDepth(selectionSet: SelectionSetNode | undefined, currentDepth: number = 0): number {
    if (!selectionSet || !selectionSet.selections) {
      return currentDepth;
    }

    let maxDepth = currentDepth;
    for (const selection of selectionSet.selections) {
      if (selection.kind === Kind.FIELD) {
        const fieldDepth = GraphQLServer.getQueryDepth(selection.selectionSet, currentDepth + 1);
        maxDepth = Math.max(maxDepth, fieldDepth);
      } else if (selection.kind === Kind.INLINE_FRAGMENT) {
        const fragmentDepth = GraphQLServer.getQueryDepth(selection.selectionSet, currentDepth);
        maxDepth = Math.max(maxDepth, fragmentDepth);
      }
      // FRAGMENT_SPREAD is rejected for security (fail closed)
    }
    return maxDepth;
  }

  /**
   * Create a depth limiting plugin for graphql-yoga
   */
  private createDepthLimitPlugin(): Plugin {
    const maxDepth = GraphQLServer.MAX_QUERY_DEPTH;
    const logger = this.log;

    return {
      onParse() {
        return ({ result }: { result: DocumentNode | Error }) => {
          if (result instanceof Error) {
            return; // Parse error, let it propagate
          }

          const document = result as DocumentNode;
          for (const definition of document.definitions) {
            if (definition.kind === Kind.OPERATION_DEFINITION) {
              // Check for fragment spreads (reject for security)
              const hasFragmentSpread = (selections: readonly SelectionNode[]): boolean => {
                for (const sel of selections) {
                  if (sel.kind === Kind.FRAGMENT_SPREAD) {
                    return true;
                  }
                  if ((sel.kind === Kind.FIELD || sel.kind === Kind.INLINE_FRAGMENT) && sel.selectionSet) {
                    if (hasFragmentSpread(sel.selectionSet.selections)) {
                      return true;
                    }
                  }
                }
                return false;
              };

              if (definition.selectionSet && hasFragmentSpread(definition.selectionSet.selections)) {
                logger.warn('Query rejected: fragment spreads not allowed for security');
                throw new GraphQLError('Fragment spreads are not allowed for security reasons', {
                  extensions: { code: 'FRAGMENT_SPREAD_NOT_ALLOWED' }
                });
              }

              const depth = GraphQLServer.getQueryDepth(definition.selectionSet, 0);
              if (depth > maxDepth) {
                logger.warn(`Query rejected: depth ${depth} exceeds maximum ${maxDepth}`);
                throw new GraphQLError(`Query depth ${depth} exceeds maximum allowed depth of ${maxDepth}`, {
                  extensions: { code: 'QUERY_TOO_DEEP' }
                });
              }
            }
          }
        };
      }
    };
  }
}
