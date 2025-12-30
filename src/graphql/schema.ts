import { makeExecutableSchema } from '@graphql-tools/schema';
import { resolvers as baseResolvers } from './resolvers.js';
import { scalarResolvers } from './resolvers.js';
import type { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';

export const typeDefs = /* GraphQL */ `
  # Custom types for Unreal math types
  type Vector {
  x: Float!
  y: Float!
  z: Float!
}

type Rotator {
  pitch: Float!
  yaw: Float!
  roll: Float!
}

type Transform {
  location: Vector!
  rotation: Rotator!
  scale: Vector!
}

  # Asset - related types
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

  scalar JSON

  # Actor - related types
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

type Component {
  name: String!
  type: String!
  properties: JSON
}

  # Blueprint - specific types
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

type Variable {
  name: String!
  type: String!
  defaultValue: JSON
  metadata: JSON
}

type Function {
  name: String!
  inputs: [Parameter!]!
  outputs: [Parameter!]!
}

type Parameter {
  name: String!
  type: String!
}

type Event {
  name: String!
  type: String!
}

  # Level - related types
type Level {
  name: String!
  path: String!
  actors: [Actor!]!
  streamingLevels: [String!]!
  lightingQuality: String
  loaded: Boolean!
}

  # Material types
type Material {
  name: String!
  path: String!
  parameters: [MaterialParameter!]!
  usedInstances: [Asset!]!
}

type MaterialParameter {
  name: String!
  type: String!
  value: JSON
}

  # Sequence types
type Sequence {
  name: String!
  path: String!
  duration: Float
  tracks: [SequenceTrack!]!
}

type SequenceTrack {
  name: String!
  type: String!
  clips: [SequenceClip!]!
}

type SequenceClip {
  name: String!
  startTime: Float!
  endTime: Float!
}

  # World Partition types
type WorldPartitionCell {
  name: String!
  bounds: Box!
  isLoaded: Boolean!
  dataLayers: [String!]
}

type Box {
  min: Vector!
  max: Vector!
}

  # Niagara types
type NiagaraSystem {
  name: String!
  path: String!
  emitters: [NiagaraEmitter!]!
  parameters: [NiagaraParameter!]!
}

type NiagaraEmitter {
  name: String!
  isValid: Boolean!
  enabled: Boolean!
}

type NiagaraParameter {
  name: String!
  type: String!
  value: JSON
}

  # Query operations
type Query {
    # Asset queries
assets(filter: AssetFilter, pagination: PaginationInput): AssetConnection!
asset(path: String!): Asset

    # Actor queries
actors(filter: ActorFilter, pagination: PaginationInput): ActorConnection!
actor(name: String!): Actor

    # Blueprint queries
blueprints(filter: BlueprintFilter, pagination: PaginationInput): BlueprintConnection!
blueprint(path: String!): Blueprint

    # Level queries
levels: [Level!]!
currentLevel: Level

    # Material queries
materials(filter: MaterialFilter, pagination: PaginationInput): MaterialConnection!

    # Sequence queries
sequences(filter: SequenceFilter, pagination: PaginationInput): SequenceConnection!

    # World Partition queries
worldPartitionCells: [WorldPartitionCell!]!

    # Niagara queries
niagaraSystems(filter: NiagaraFilter, pagination: PaginationInput): NiagaraSystemConnection!
niagaraSystem(path: String!): NiagaraSystem

    # Search across all types
search(query: String!, type: SearchType): [SearchResult!]!
  }

  # Mutation operations
type Mutation {
    # Asset mutations
duplicateAsset(path: String!, newName: String!): Asset!
moveAsset(path: String!, newPath: String!): Asset!
deleteAsset(path: String!): Boolean!

    # Actor mutations
spawnActor(input: SpawnActorInput!): Actor!
deleteActor(name: String!): Boolean!
setActorTransform(name: String!, transform: TransformInput!): Actor!

    # Blueprint mutations
createBlueprint(input: CreateBlueprintInput!): Blueprint!
addVariableToBlueprint(path: String!, input: AddVariableInput!): Blueprint!
addFunctionToBlueprint(path: String!, input: AddFunctionInput!): Blueprint!

    # Level mutations
loadLevel(path: String!): Level!
saveLevel(path: String): Boolean!

    # Material mutations
createMaterialInstance(parentPath: String!, name: String!, parameters: JSON): Asset!
  }

  # Input types
  input AssetFilter {
  class: String
  tag: String
  pathStartsWith: String
}

  input NiagaraFilter {
  class: String
  tag: String
  pathStartsWith: String
}

  input ActorFilter {
  class: String
  tag: String
  locationNear: VectorInput
  distance: Float
}

  input BlueprintFilter {
  parentClass: String
  hasVariable: String
  hasFunction: String
}

  input MaterialFilter {
  hasParameter: String
}

  input SequenceFilter {
  hasTrackType: String
}

  input PaginationInput {
  offset: Int
  limit: Int
}

  input VectorInput {
  x: Float!
  y: Float!
  z: Float!
}

  input RotatorInput {
  pitch: Float!
  yaw: Float!
  roll: Float!
}

  input TransformInput {
  location: VectorInput!
  rotation: RotatorInput!
  scale: VectorInput!
}

  input SpawnActorInput {
  classPath: String!
  name: String
  transform: TransformInput!
  tags: [String!]
}

  input CreateBlueprintInput {
  name: String!
  blueprintType: String!
  savePath: String!
}

  input AddVariableInput {
  variableName: String!
  variableType: String!
  defaultValue: JSON
}

  input AddFunctionInput {
  functionName: String!
  inputs: [ParameterInput!]
}

  input ParameterInput {
  name: String!
  type: String!
}

  # Connection types for pagination
  type AssetConnection {
  edges: [AssetEdge!]!
  pageInfo: PageInfo!
  totalCount: Int!
}

type AssetEdge {
  node: Asset!
  cursor: String!
}

type ActorConnection {
  edges: [ActorEdge!]!
  pageInfo: PageInfo!
  totalCount: Int!
}

type ActorEdge {
  node: Actor!
  cursor: String!
}

type BlueprintConnection {
  edges: [BlueprintEdge!]!
  pageInfo: PageInfo!
  totalCount: Int!
}

type BlueprintEdge {
  node: Blueprint!
  cursor: String!
}

type MaterialConnection {
  edges: [MaterialEdge!]!
  pageInfo: PageInfo!
  totalCount: Int!
}

type MaterialEdge {
  node: Material!
  cursor: String!
}

type SequenceConnection {
  edges: [SequenceEdge!]!
  pageInfo: PageInfo!
  totalCount: Int!
}

type SequenceEdge {
  node: Sequence!
  cursor: String!
}

type NiagaraSystemConnection {
  edges: [NiagaraSystemEdge!]!
  pageInfo: PageInfo!
  totalCount: Int!
}

type NiagaraSystemEdge {
  node: NiagaraSystem!
  cursor: String!
}

type PageInfo {
  hasNextPage: Boolean!
  hasPreviousPage: Boolean!
  startCursor: String
  endCursor: String
}

  # Search functionality
  union SearchResult = Asset | Actor | Blueprint | Material | Sequence | NiagaraSystem

enum SearchType {
  ALL
    ASSETS
    ACTORS
    BLUEPRINTS
    MATERIALS
    SEQUENCES
    NIAGARA
}
`;



/**
 * Create the GraphQL schema with resolvers
 */
export function createGraphQLSchema(
  _bridge: UnrealBridge,
  _automationBridge: AutomationBridge
) {
  // Union type resolver
  const SearchResult = {
    __resolveType(obj: Record<string, unknown>) {
      if (obj.name && obj.path && obj.class) {
        return 'Asset';
      }
      if (obj.name && obj.class && !obj.path) {
        return 'Actor';
      }
      if (obj.name && obj.path && obj.variables) {
        return 'Blueprint';
      }
      if (obj.name && obj.path && obj.parameters) {
        // Naive check: Materials and Niagara both have parameters.
        // Differentiate by checking for 'usedInstances' (Material) vs 'emitters' (Niagara)
        if (obj.usedInstances) return 'Material';
        if (obj.emitters) return 'NiagaraSystem';
        return 'Material'; // Fallback
      }
      if (obj.name && obj.path && obj.tracks) {
        return 'Sequence';
      }
      return null;
    }
  };

  // Merge scalar resolvers with base resolvers
  const mergedResolvers = {
    ...baseResolvers,
    JSON: scalarResolvers.JSON,
    SearchResult
  };

  return makeExecutableSchema({
    typeDefs,
    resolvers: mergedResolvers
  });
}
