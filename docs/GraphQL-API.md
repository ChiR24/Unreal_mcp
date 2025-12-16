# GraphQL API Documentation

## Overview

The Unreal Engine MCP Server now includes a GraphQL API that provides a flexible, efficient alternative to the standard MCP tools for complex queries. The GraphQL API allows you to:

- Query multiple related objects in a single request
- Specify exactly what data you need (no over-fetching)
- Use nested queries to traverse object relationships
- Perform mutations (create, update, delete) through mutations
- Leverage GraphQL's strong typing and introspection

## GraphQL Server Configuration

### Environment Variables

```bash
# Enable/disable GraphQL server
GRAPHQL_ENABLED=true

# Server host and port
GRAPHQL_HOST=127.0.0.1
GRAPHQL_PORT=4000

# GraphQL endpoint path
GRAPHQL_PATH=/graphql

# CORS settings
GRAPHQL_CORS_ORIGIN=*
GRAPHQL_CORS_CREDENTIALS=false
```

### Accessing the GraphQL API

The GraphQL server runs on a separate port from the MCP server:

- **URL**: `http://127.0.0.1:4000/graphql`
- **Default Port**: 4000 (configurable via `GRAPHQL_PORT`)
- **Default Host**: 127.0.0.1 (configurable via `GRAPHQL_HOST`)

## GraphQL Schema

### Custom Scalars

- **Vector**: 3D vector with `x`, `y`, `z` fields (Float)
- **Rotator**: Rotation with `pitch`, `yaw`, `roll` fields (Float, degrees)
- **Transform**: Combined transform with `location`, `rotation`, `scale` (Vector/Rotator/Vector)
- **JSON**: Arbitrary JSON data

### Core Types

#### Asset
```graphql
type Asset {
  name: String!
  path: String!
  class: String!
  packagePath: String!
  size: Float
  dependencies: [Asset!]!
  dependents: [Asset!]!
  metadata: JSON
  tags: [String!]!
}
```

#### Actor
```graphql
type Actor {
  name: String!
  class: String!
  location: Vector
  rotation: Rotator
  scale: Vector
  components: [Component!]!
  tags: [String!]!
  properties: JSON
}
```

#### Blueprint
```graphql
type Blueprint {
  name: String!
  path: String!
  parentClass: String
  variables: [Variable!]!
  functions: [Function!]!
  events: [Event!]!
  components: [Component!]!
  scsHierarchy: JSON
}
```

## Query Examples

### 1. List All Assets

```graphql
query {
  assets {
    edges {
      node {
        name
        path
        class
        packagePath
      }
      cursor
    }
    pageInfo {
      hasNextPage
      hasPreviousPage
    }
    totalCount
  }
}
```

### 2. Filter Assets by Class

```graphql
query {
  assets(filter: { class: "Material" }) {
    edges {
      node {
        name
        path
        class
        tags
      }
    }
    totalCount
  }
}
```

### 3. Get Asset with Dependencies

```graphql
query {
  asset(path: "/Game/Materials/M_Master") {
    name
    path
    class
    dependencies {
      name
      path
    }
    dependents {
      name
      path
    }
    metadata
    tags
  }
}
```

### 4. Paginated Asset Query

```graphql
query {
  assets(pagination: { offset: 0, limit: 10 }) {
    edges {
      node {
        name
        path
        class
      }
      cursor
    }
    pageInfo {
      hasNextPage
      hasPreviousPage
      startCursor
      endCursor
    }
    totalCount
  }
}
```

### 5. List All Actors

```graphql
query {
  actors {
    edges {
      node {
        name
        class
        location {
          x
          y
          z
        }
        rotation {
          pitch
          yaw
          roll
        }
        tags
      }
    }
    totalCount
  }
}
```

### 6. Get Actor with Properties and Components

```graphql
query {
  actor(name: "SpotLight_Main") {
    name
    class
    location {
      x
      y
      z
    }
    rotation {
      pitch
      yaw
      roll
    }
    tags
    properties
    components {
      name
      type
      properties
    }
  }
}
```

### 7. Filter Actors by Tag

```graphql
query {
  actors(filter: { tag: "Enemy" }) {
    edges {
      node {
        name
        class
        tags
      }
    }
    totalCount
  }
}
```

### 8. Get Blueprint Details

```graphql
query {
  blueprint(path: "/Game/Blueprints/BP_PlayerCharacter") {
    name
    path
    parentClass
    variables {
      name
      type
      defaultValue
      metadata
    }
    functions {
      name
      inputs {
        name
        type
      }
      outputs {
        name
        type
      }
    }
    events {
      name
      type
    }
    components {
      name
      type
    }
    scsHierarchy
  }
}
```

### 9. List All Blueprints

```graphql
query {
  blueprints {
    edges {
      node {
        name
        path
        parentClass
        variables {
          name
          type
        }
        functions {
          name
        }
      }
    }
    totalCount
  }
}
```

### 10. Filter Blueprints by Parent Class

```graphql
query {
  blueprints(filter: { parentClass: "Actor" }) {
    edges {
      node {
        name
        path
        parentClass
      }
    }
    totalCount
  }
}
```

### 11. Get Current Level Information

```graphql
query {
  currentLevel {
    name
    path
    loaded
    lightingQuality
    actors {
      name
      class
    }
    streamingLevels
  }
}
```

### 12. List All Levels

```graphql
query {
  levels {
    name
    path
    loaded
    actorCount
  }
}
```

### 13. Search Across All Types

```graphql
query {
  search(query: "Player", type: ALL) {
    ... on Asset {
      name
      path
      __typename
    }
    ... on Actor {
      name
      class
      __typename
    }
    ... on Blueprint {
      name
      path
      __typename
    }
  }
}
```

### 14. Search for Assets Only

```graphql
query {
  search(query: "Material", type: ASSETS) {
    ... on Asset {
      name
      path
      class
    }
  }
}
```

## Mutation Examples

### 1. Duplicate an Asset

```graphql
mutation {
  duplicateAsset(path: "/Game/Materials/M_Master", newName: "M_Master_Copy") {
    name
    path
    class
  }
}
```

### 2. Move an Asset

```graphql
mutation {
  moveAsset(path: "/Game/Materials/M_Master", newPath: "/Game/Archived/Materials/M_Master") {
    name
    path
  }
}
```

### 3. Delete an Asset

```graphql
mutation {
  deleteAsset(path: "/Game/Materials/M_Old")
}
```

### 4. Spawn an Actor

```graphql
mutation {
  spawnActor(input: {
    classPath: "StaticMeshActor"
    name: "Cube_001"
    transform: {
      location: { x: 0, y: 0, z: 100 }
      rotation: { pitch: 0, yaw: 0, roll: 0 }
      scale: { x: 1, y: 1, z: 1 }
    }
    tags: ["Spawned"]
  }) {
    name
    class
    location {
      x
      y
      z
    }
    tags
  }
}
```

### 5. Delete an Actor

```graphql
mutation {
  deleteActor(name: "Cube_001")
}
```

### 6. Set Actor Transform

```graphql
mutation {
  setActorTransform(
    name: "SpotLight_Main"
    transform: {
      location: { x: 500, y: 300, z: 200 }
      rotation: { pitch: 45, yaw: 90, roll: 0 }
      scale: { x: 1, y: 1, z: 1 }
    }
  ) {
    name
    location {
      x
      y
      z
    }
    rotation {
      pitch
      yaw
      roll
    }
  }
}
```

### 7. Create a Blueprint

```graphql
mutation {
  createBlueprint(input: {
    name: "BP_NewCharacter"
    blueprintType: "Actor"
    savePath: "/Game/Blueprints"
  }) {
    name
    path
    parentClass
  }
}
```

### 8. Add Variable to Blueprint

```graphql
mutation {
  addVariableToBlueprint(
    path: "/Game/Blueprints/BP_NewCharacter"
    input: {
      variableName: "Health"
      variableType: "Float"
      defaultValue: 100
    }
  ) {
    name
    variables {
      name
      type
      defaultValue
    }
  }
}
```

### 9. Add Function to Blueprint

```graphql
mutation {
  addFunctionToBlueprint(
    path: "/Game/Blueprints/BP_NewCharacter"
    input: {
      functionName: "TakeDamage"
      inputs: [
        { name: "Damage", type: "Float" }
        { name: "Source", type: "String" }
      ]
    }
  ) {
    name
    functions {
      name
      inputs {
        name
        type
      }
    }
  }
}
```

### 10. Load a Level

```graphql
mutation {
  loadLevel(path: "/Game/Maps/Level_01") {
    name
    path
    loaded
    actorCount
  }
}
```

### 11. Save Current Level

```graphql
mutation {
  saveLevel(path: "/Game/Maps/Level_01")
}
```

### 12. Create Material Instance

```graphql
mutation {
  createMaterialInstance(
    parentPath: "/Game/Materials/M_Master"
    name: "MI_Player"
    parameters: {
      BaseColor: { x: 1, y: 0, z: 0 }
      Metallic: 0.5
    }
  ) {
    name
    path
    class
  }
}
```

## Fragment Examples

Fragments allow you to reuse field selections:

```graphql
fragment AssetInfo on Asset {
  name
  path
  class
  packagePath
}

query {
  assets(filter: { class: "Material" }) {
    edges {
      node {
        ...AssetInfo
        dependencies {
          ...AssetInfo
        }
      }
    }
  }
}
```

## Variable Examples

Use variables for dynamic queries:

```graphql
query GetAsset($path: String!, $withDependencies: Boolean!) {
  asset(path: $path) {
    ...AssetInfo
    dependencies @include(if: $withDependencies) {
      ...AssetInfo
    }
  }
}

fragment AssetInfo on Asset {
  name
  path
  class
}
```

With these variables:
```json
{
  "path": "/Game/Materials/M_Master",
  "withDependencies": true
}
```

## Alias Examples

Use aliases to rename fields in results:

```graphql
query {
  materials: assets(filter: { class: "Material" }) {
    edges {
      node {
        materialName: name
        materialPath: path
        class
      }
    }
  }
}
```

## Introspection Query

Get the complete schema:

```graphql
query {
  __schema {
    types {
      name
      kind
      fields {
        name
        type {
          name
          kind
        }
      }
    }
  }
}
```

## Best Practices

### 1. Use Specific Field Selection
Always specify only the fields you need:

```graphql
# Good
query {
  assets {
    edges {
      node {
        name
        path
      }
    }
  }
}

# Bad (over-fetching)
query {
  assets {
    edges {
      node {
        _ # Get everything
      }
    }
  }
}
```

### 2. Use Fragments for Reuse
Create fragments for commonly used field sets:

```graphql
fragment ActorBasicInfo on Actor {
  name
  class
  location {
    x
    y
    z
  }
}
```

### 3. Use Pagination for Large Datasets
Always use pagination for queries that might return many results:

```graphql
query {
  assets(pagination: { offset: 0, limit: 50 }) {
    edges {
      node {
        name
        path
      }
    }
    pageInfo {
      hasNextPage
      hasPreviousPage
    }
  }
}
```

### 4. Use Filtering
Filter results on the server side to reduce data transfer:

```graphql
query {
  actors(filter: { class: "StaticMeshActor", tag: "Static" }) {
    edges {
      node {
        name
        class
      }
    }
  }
}
```

### 5. Use Mutations for Modifications
Use mutations instead of queries for operations that modify data:

```graphql
mutation {
  spawnActor(input: { ... }) {
    name
    location {
      x
      y
      z
    }
  }
}
```

## Limitations

1. **Unreal Connection Required**: GraphQL resolvers require an active connection to Unreal Editor
2. **Plugin Support**: Some queries require specific Unreal plugins to be enabled
3. **Performance**: Complex nested queries may be slower than focused MCP tool calls
4. **Not a Replacement**: GraphQL complements but doesn't replace the MCP tool system

## Troubleshooting

### GraphQL Server Not Starting

Check logs for errors:
```bash
# Check if port is already in use
netstat -an | grep 4000

# Verify environment variables
echo $GRAPHQL_ENABLED
echo $GRAPHQL_PORT
```

### Queries Timing Out

Increase timeout in your GraphQL client, or break complex queries into smaller parts.

### Authentication

Currently, GraphQL API has no authentication. Access is restricted to localhost by default. For production, consider:
- Running behind a reverse proxy with authentication
- Implementing API keys
- Using VPN or firewall rules

## Migration from MCP Tools

### MCP Tool → GraphQL Equivalent

| MCP Tool | GraphQL Query | Notes |
|----------|---------------|-------|
| `manage_asset` (list) | `assets` query | Use filter and pagination |
| `manage_asset` (create) | `createMaterialInstance` mutation | Asset creation varies |
| `control_actor` (spawn) | `spawnActor` mutation | Transform input format |
| `control_actor` (delete) | `deleteActor` mutation | Simpler API |
| `manage_blueprint` (get) | `blueprint` query | More detailed response |
| `manage_level` (load) | `loadLevel` mutation | Direct mapping |

### Example Migration

**Before (MCP Tool):**
```json
{
  "tool": "manage_asset",
  "arguments": {
    "action": "list",
    "directory": "/Game"
  }
}
```

**After (GraphQL):**
```graphql
query {
  assets(filter: { pathStartsWith: "/Game" }, pagination: { limit: 100 }) {
    edges {
      node {
        name
        path
        class
      }
    }
    totalCount
  }
}
```

## Additional Resources

### GraphQL Clients
- [GraphiQL](https://github.com/graphql/graphiql) - In-browser GraphQL IDE
- [Apollo Studio](https://studio.apollographql.com) - Powerful visual editor
- [Insomnia](https://insomnia.rest/) - REST client with GraphQL support
- [Postman](https://www.postman.com/) - API platform with GraphQL support

### Learning Resources
- [Introduction to GraphQL](https://graphql.org/learn/)
- [Queries and Mutations](https://graphql.org/learn/queries/)
- [Apollo Client React](https://www.apollographql.com/docs/react/)
- [GraphQL Best Practices](https://graphql.org/learn/best-practices/)

## Next Steps

1. Start the MCP server with `GRAPHQL_ENABLED=true`
2. Use a GraphQL client (GraphiQL, Apollo Studio, Insomnia, or Postman) to explore the schema
3. Try the query examples above
4. Build your own queries tailored to your needs
5. Consider using fragments and variables for complex workflows
