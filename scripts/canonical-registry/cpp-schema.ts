// scripts/canonical-registry/cpp-schema.ts
//
// Deterministic JSON-schema -> FMcpSchemaBuilder C++ emission.
//
// The generated BuildInputSchema() body uses the fluent FMcpSchemaBuilder.
// Nested object/array callbacks receive the SUB-builder as their parameter
// `S` and MUST emit onto `S` (never the outer `Schema`) -- FMcpSchemaBuilder
// constructs a fresh sub-builder and attaches its accumulated properties, so
// writing to `Schema` inside the lambda would be an implicit capture error.
//
// Union/any shapes (oneOf/anyOf, array-valued `type`, or a property with no
// `type`) are expressed by first-class FMcpSchemaBuilder methods
// TypeUnion(...) and AnyValue(...), so every node -- including nested unions
// inside object/array sub-builders -- always emits onto its receiving builder.
//
// FIDELITY RULE: a union is emitted as TypeUnion ONLY when every branch is a
// pure scalar type constraint (string/number/integer/boolean with no
// properties/items/structural sub-union). Any structural or multi-branch
// oneOf/anyOf -- whose properties/items C++ cannot faithfully represent -- is
// emitted as the honest permissive AnyValue instead of a lossy type list.

import { cppStringLiteral, pascalCase, type JsonSchemaNode } from './types.js';

const T = (s: string): string => `TEXT("${cppStringLiteral(s)}")`;

const TAB = '\t';

const unionBranches = (node: JsonSchemaNode): JsonSchemaNode[] => [
  ...(Array.isArray(node.oneOf) ? (node.oneOf as JsonSchemaNode[]) : []),
  ...(Array.isArray(node.anyOf) ? (node.anyOf as JsonSchemaNode[]) : []),
];

// A branch may itself carry an array-valued `type`; every member is part of the
// union, so dropping it would emit a narrower TypeUnion than the schema accepts.
const unionTypes = (node: JsonSchemaNode): string[] => {
  const types = new Set<string>();
  const addType = (t: unknown): void => {
    if (typeof t === 'string') types.add(t);
    else if (Array.isArray(t)) for (const sub of t) types.add(String(sub));
  };
  for (const branch of unionBranches(node)) addType(branch.type);
  addType(node.type);
  return [...types];
};

const cppActionEnumInline = (values: readonly string[]): string =>
  `{ ${values.map((v) => T(v)).join(', ')} }`;

const requiredNames = (node: JsonSchemaNode): string[] =>
  Array.isArray(node.required) ? (node.required as string[]) : [];

// FMcpSchemaBuilder attaches RequiredFields to whichever builder receives them,
// so a nested `required` must be emitted onto the sub-builder `S`; emitting it
// onto the outer `Schema` would attach the constraint at the wrong level.
const emitRequired = (
  names: readonly string[],
  indent: string,
  out: string[],
  builder: string,
): void => {
  if (names.length === 0) return;
  out.push(`${indent}${builder}.Required(${cppActionEnumInline(names)});`);
};

const isScalarType = (t: unknown): boolean =>
  t === 'string' || t === 'number' || t === 'integer' || t === 'boolean' || t === 'null';

// A union node can be emitted honestly as TypeUnion only when every branch is a
// pure scalar type constraint. A branch carrying properties/items or a nested
// sub-union is structural and cannot be flattened to a type list without
// dropping information, so such a union must fall back to AnyValue.
const isHonestScalarUnion = (node: JsonSchemaNode): boolean => {
  const type = node.type;
  if (Array.isArray(type)) {
    return type.every(isScalarType);
  }
  const branches: JsonSchemaNode[] = [
    ...(Array.isArray(node.oneOf) ? (node.oneOf as JsonSchemaNode[]) : []),
    ...(Array.isArray(node.anyOf) ? (node.anyOf as JsonSchemaNode[]) : []),
  ];
  if (branches.length === 0) return false;
  return branches.every((b) => {
    const bNode = b as JsonSchemaNode;
    const t = bNode.type;
    if (Array.isArray(t)) return t.every(isScalarType);
    if (typeof t !== 'string' || !isScalarType(t)) return false;
    if (bNode.properties !== undefined) return false;
    if (bNode.items !== undefined) return false;
    if (Array.isArray(bNode.oneOf) || Array.isArray(bNode.anyOf)) return false;
    return true;
  });
};

const emitNode = (
  name: string,
  node: JsonSchemaNode,
  indent: string,
  out: string[],
  builder: string = 'Schema',
): void => {
  const desc = typeof node.description === 'string' ? node.description : '';
  const lit = T(name);
  const descLit = T(desc);

  if (Array.isArray(node.enum)) {
    const enumVals = node.enum as string[];
    out.push(`${indent}${builder}.StringEnum(${lit}, ${cppActionEnumInline(enumVals)}, ${descLit});`);
    return;
  }

  const type = node.type;

  if (type === 'string') {
    out.push(`${indent}${builder}.String(${lit}, ${descLit});`);
    return;
  }
  if (type === 'number') {
    out.push(`${indent}${builder}.Number(${lit}, ${descLit});`);
    return;
  }
  if (type === 'integer') {
    out.push(`${indent}${builder}.Integer(${lit}, ${descLit});`);
    return;
  }
  if (type === 'boolean') {
    out.push(`${indent}${builder}.Bool(${lit}, ${descLit});`);
    return;
  }
  if (type === 'object') {
    emitObjectNode(name, node, indent, out, builder);
    return;
  }
  if (type === 'array') {
    emitArrayNode(name, node, indent, out, builder);
    return;
  }
  if (Array.isArray(node.oneOf) || Array.isArray(node.anyOf)) {
    emitUnionNode(name, node, indent, out, builder);
    return;
  }
  if (Array.isArray(type)) {
    emitUnionNode(name, node, indent, out, builder);
    return;
  }
  // No `type` at all -> unconstrained "any" value.
  out.push(`${indent}${builder}.AnyValue(${lit}, ${descLit});`);
};

const emitObjectNode = (
  name: string,
  node: JsonSchemaNode,
  indent: string,
  out: string[],
  builder: string,
): void => {
  const desc = typeof node.description === 'string' ? node.description : '';
  const lit = T(name);
  const descLit = T(desc);
  const subProps = (node.properties ?? {}) as Record<string, JsonSchemaNode>;
  const freeform = node.additionalProperties === true && Object.keys(subProps).length === 0;
  if (freeform) {
    out.push(`${indent}${builder}.FreeformObject(${lit}, ${descLit});`);
    return;
  }
  const subRequired = requiredNames(node);
  if (Object.keys(subProps).length === 0 && subRequired.length === 0) {
    out.push(`${indent}${builder}.Object(${lit}, ${descLit});`);
    return;
  }
  out.push(`${indent}${builder}.Object(${lit}, ${descLit}, [](FMcpSchemaBuilder& S) {`);
  const subIndent = `${indent}${TAB}`;
  for (const [subName, subNode] of Object.entries(subProps)) {
    emitNode(subName, subNode, `${subIndent}  `, out, 'S');
  }
  emitRequired(subRequired, `${subIndent}  `, out, 'S');
  out.push(`${indent}});`);
};

// FIDELITY RULE: `items: {}` accepts any element, so defaulting it to `string`
// would make the native schema reject payloads the contract accepts.
const isUnconstrainedItems = (items: JsonSchemaNode): boolean =>
  items.type === undefined
  && items.enum === undefined
  && items.oneOf === undefined
  && items.anyOf === undefined
  && items.properties === undefined
  && items.items === undefined;

const emitArrayNode = (
  name: string,
  node: JsonSchemaNode,
  indent: string,
  out: string[],
  builder: string,
): void => {
  const desc = typeof node.description === 'string' ? node.description : '';
  const lit = T(name);
  const descLit = T(desc);
  const items = (node.items ?? {}) as JsonSchemaNode;
  const itemType = items.type;
  if (itemType === 'object') {
    const subProps = (items.properties ?? {}) as Record<string, JsonSchemaNode>;
    const itemRequired = requiredNames(items);
    if (Object.keys(subProps).length === 0 && itemRequired.length === 0) {
      out.push(`${indent}${builder}.ArrayOfObjects(${lit}, ${descLit});`);
      return;
    }
    out.push(`${indent}${builder}.ArrayOfObjects(${lit}, ${descLit}, [](FMcpSchemaBuilder& S) {`);
    const subIndent = `${indent}${TAB}`;
    for (const [subName, subNode] of Object.entries(subProps)) {
      emitNode(subName, subNode, `${subIndent}  `, out, 'S');
    }
    emitRequired(itemRequired, `${subIndent}  `, out, 'S');
    out.push(`${indent}});`);
    return;
  }
  if (isUnconstrainedItems(items)) {
    out.push(`${indent}${builder}.ArrayOfAny(${lit}, ${descLit});`);
    return;
  }
  const cppItem = itemType === 'number' ? 'number'
    : itemType === 'integer' ? 'integer'
    : itemType === 'boolean' ? 'boolean'
    : 'string';
  out.push(`${indent}${builder}.Array(${lit}, ${descLit}, TEXT("${cppItem}"));`);
};

const emitUnionNode = (
  name: string,
  node: JsonSchemaNode,
  indent: string,
  out: string[],
  builder: string = 'Schema',
): void => {
  const desc = typeof node.description === 'string' ? node.description : '';
  const lit = T(name);
  const descLit = T(desc);
  // A flat/scalar union keeps its exact type list; any structural or
  // multi-branch union degrades to the honest permissive AnyValue so C++ does
  // not silently lose properties/items that TypeUnion cannot encode.
  if (isHonestScalarUnion(node)) {
    const types = unionTypes(node);
    const typeList = `{ ${types.map((t) => T(t)).join(', ')} }`;
    out.push(`${indent}${builder}.TypeUnion(${lit}, ${typeList}, ${descLit});`);
    return;
  }
  out.push(`${indent}${builder}.AnyValue(${lit}, ${descLit});`);
};

// Build the BuildInputSchema() schema-body lines (3-tab indent).
export interface SchemaEmitResult {
  readonly lines: string[];
}

export const jsonSchemaToCppCalls = (schema: JsonSchemaNode): SchemaEmitResult => {
  const out: string[] = [];
  const props = (schema.properties ?? {}) as Record<string, JsonSchemaNode>;
  const topIndent = `${TAB}${TAB}${TAB}`;

  for (const [name, node] of Object.entries(props)) {
    emitNode(name, node, topIndent, out);
  }
  emitRequired(requiredNames(schema), topIndent, out, 'Schema');
  return { lines: out };
};

export { pascalCase };
