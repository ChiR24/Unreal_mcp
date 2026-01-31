# src/graphql

Optional GraphQL API for complex queries. Disabled by default (`GRAPHQL_ENABLED=true` to enable).

## OVERVIEW
GraphQL Yoga server providing flexible query interface over MCP tools. Supports nested queries, batching via DataLoader, depth limiting (max 10), and per-IP rate limiting (60 req/min).

## STRUCTURE
```
graphql/
├── server.ts       # GraphQLServer class, Yoga setup, rate limiting
├── schema.ts       # Type definitions (Asset, Actor, Blueprint, Vector, Rotator)
├── resolvers.ts    # Query/mutation resolvers + scalar types
├── types.ts        # GraphQLContext interface
└── loaders.ts      # DataLoaders for N+1 query optimization
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Server config | `server.ts` | Port 4000 default, depth/rate limits |
| Add type | `schema.ts` | Extend typeDefs, add to resolvers |
| Add query | `resolvers.ts` | Base resolvers + scalar resolvers |
| Batching | `loaders.ts` | DataLoader batch functions |
| Context | `types.ts` | Bridge access in resolvers |

## CONVENTIONS
- **Custom Scalars**: Vector, Rotator, Transform for UE math types
- **JSON Scalar**: Flexible metadata/properties fields
- **DataLoader**: Always use loaders for nested field resolution
- **Depth Limit**: MAX_QUERY_DEPTH = 10 prevents complex queries
- **Rate Limiting**: 60 req/min per IP, 60s window

## SECURITY
- Disabled by default (opt-in)
- Query depth limiting prevents DoS
- Per-IP rate limiting on all endpoints
- CORS configurable via env vars

## COMMANDS
```bash
# Enable GraphQL
GRAPHQL_ENABLED=true npm start

# Custom port
GRAPHQL_PORT=5000 npm start
```

## ANTI-PATTERNS
- **Deep Nesting**: Avoid >5 levels in queries (performance)
- **Missing Loaders**: Always batch with DataLoader (N+1 problem)
- **No Validation**: Validate args in resolvers before bridge calls
