import { createYoga } from 'graphql-yoga';
import { createServer } from 'http';
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
  }

  async start(): Promise<void> {
    if (!this.config.enabled) {
      this.log.info('GraphQL server is disabled');
      return;
    }

    try {
      // Create GraphQL schema
      const schema = createGraphQLSchema(this.bridge, this.automationBridge);

      // Create Yoga server
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

      // Create HTTP server with Yoga's request handler
      this.server = createServer(
        yoga as any
      );

      // Start server
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

      // Setup graceful shutdown
      this.setupShutdown();
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

  private setupShutdown(): void {
    const gracefulShutdown = async (signal: string) => {
      this.log.info(`Received ${signal}, shutting down GraphQL server...`);
      try {
        await this.stop();
        process.exit(0);
      } catch (error) {
        this.log.error('Error during GraphQL server shutdown:', error);
        process.exit(1);
      }
    };

    process.on('SIGINT', () => gracefulShutdown('SIGINT'));
    process.on('SIGTERM', () => gracefulShutdown('SIGTERM'));
  }

  getConfig() {
    return this.config;
  }

  isRunning(): boolean {
    return this.server !== null && this.server.listening;
  }
}
