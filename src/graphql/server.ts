import { createYoga } from 'graphql-yoga';
import { createServer, type RequestListener } from 'http';
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

    // Security check: Forcefully disable credentials if origin is wildcard
    if (this.config.cors.origin === '*' && this.config.cors.credentials) {
      this.log.error(
        "SECURITY: CORS 'origin: *' cannot be used with 'credentials: true'. Disabling credentials to prevent vulnerability."
      );
      this.config.cors.credentials = false;
    }
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
        this.log.warn(
          "GraphQL server is binding to a remote host with permissive CORS origin '*'. " +
            'Set GRAPHQL_CORS_ORIGIN to specific origins for production. Using permissive CORS for now.'
        );
      }
    }

    try {
      const schema = createGraphQLSchema(this.bridge, this.automationBridge);

      const yoga = createYoga({
        schema,
        graphqlEndpoint: this.config.path,
        cors: {
          origin: this.config.cors.origin,
          credentials: this.config.cors.credentials,
          methods: ['GET', 'POST', 'OPTIONS'],
          allowedHeaders: ['Content-Type', 'Authorization']
        },
        context: (): GraphQLContext => ({
          bridge: this.bridge,
          automationBridge: this.automationBridge,
          loaders: createLoaders(this.automationBridge)
        }),
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
    } catch (error) {
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
}
