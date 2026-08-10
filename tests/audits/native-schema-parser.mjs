import { maskCppLiteralsAndComments } from '../native-mcp-source-parser.mjs';
import { reachableNativeSchemaSources } from './native-schema-sources.mjs';

const METHOD_TYPES = {
  StringEnum: 'string',
  String: 'string',
  Array: 'array',
  ArrayOfObjects: 'array',
  ArrayOfAny: 'array',
  Number: 'number',
  Integer: 'integer',
  Bool: 'boolean',
  Object: 'object',
  FreeformObject: 'object',
  AnyValue: 'any',
  TypeUnion: 'union'
};

function textValues(source) {
  return [...source.matchAll(/TEXT\(\s*"([^"]+)"\s*\)/g)].map((match) => match[1]);
}

function closingIndex(maskedSource, openIndex) {
  let depth = 0;
  for (let index = openIndex; index < maskedSource.length; index += 1) {
    if (maskedSource[index] === '(') depth += 1;
    if (maskedSource[index] === ')') {
      depth -= 1;
      if (depth === 0) return index;
    }
  }
  return undefined;
}

function splitArguments(source) {
  const maskedSource = maskCppLiteralsAndComments(source);
  const argumentsList = [];
  let start = 0;
  let roundDepth = 0;
  let braceDepth = 0;
  let bracketDepth = 0;
  for (let index = 0; index < maskedSource.length; index += 1) {
    const character = maskedSource[index];
    if (character === '(') roundDepth += 1;
    if (character === ')') roundDepth -= 1;
    if (character === '{') braceDepth += 1;
    if (character === '}') braceDepth -= 1;
    if (character === '[') bracketDepth += 1;
    if (character === ']') bracketDepth -= 1;
    if (
      character === ','
      && roundDepth === 0
      && braceDepth === 0
      && bracketDepth === 0
    ) {
      argumentsList.push(source.slice(start, index).trim());
      start = index + 1;
    }
  }
  argumentsList.push(source.slice(start).trim());
  return argumentsList;
}

function lambdaBody(source) {
  const maskedSource = maskCppLiteralsAndComments(source);
  const openIndex = maskedSource.indexOf('{');
  if (openIndex < 0) return undefined;
  let depth = 0;
  for (let index = openIndex; index < maskedSource.length; index += 1) {
    if (maskedSource[index] === '{') depth += 1;
    if (maskedSource[index] === '}') {
      depth -= 1;
      if (depth === 0) return source.slice(openIndex + 1, index);
    }
  }
  return undefined;
}

function propertySchema(method, argumentsList) {
  const type = METHOD_TYPES[method];
  if (!type) return undefined;
  if (method === 'StringEnum') {
    return { type, enum: [...new Set(textValues(argumentsList[1] ?? ''))].sort() };
  }
  if (method === 'Array') {
    const itemType = textValues(argumentsList[2] ?? '')[0] ?? 'string';
    return { type, items: { type: itemType } };
  }
  if (method === 'ArrayOfAny') {
    return { type, items: {} };
  }
  if (method === 'ArrayOfObjects') {
    const body = lambdaBody(argumentsList[2] ?? '');
    const itemSchema = body ? parseBuilderSchema(body) : { properties: {}, required: [] };
    return {
      type,
      items: {
        type: 'object',
        ...(Object.keys(itemSchema.properties).length > 0
          ? { properties: itemSchema.properties, required: itemSchema.required }
          : {})
      }
    };
  }
  if (method === 'Object') {
    const body = lambdaBody(argumentsList[2] ?? '');
    const nested = body ? parseBuilderSchema(body) : undefined;
    return {
      type,
      ...(nested && Object.keys(nested.properties).length > 0
        ? { properties: nested.properties, required: nested.required }
        : {})
    };
  }
  if (method === 'AnyValue') {
    return {};
  }
  if (method === 'TypeUnion') {
    const typeList = textValues(argumentsList[1] ?? '');
    return { type: [...new Set(typeList)].sort() };
  }
  return { type };
}

export function parseBuilderSchema(source) {
  const maskedSource = maskCppLiteralsAndComments(source);
  const properties = {};
  const required = [];
  const methodPattern = /\.(StringEnum|String|ArrayOfObjects|ArrayOfAny|Array|Number|Integer|Bool|Object|FreeformObject|AnyValue|TypeUnion|Required)\s*\(/g;
  let match;
  while ((match = methodPattern.exec(maskedSource)) !== null) {
    const openIndex = match.index + match[0].lastIndexOf('(');
    const closeIndex = closingIndex(maskedSource, openIndex);
    if (closeIndex === undefined) break;
    const argumentsList = splitArguments(source.slice(openIndex + 1, closeIndex));
    if (match[1] === 'Required') {
      required.push(...textValues(argumentsList[0] ?? ''));
    } else {
      const name = textValues(argumentsList[0] ?? '')[0];
      const schema = propertySchema(match[1], argumentsList);
      if (name && schema) properties[name] = schema;
    }
    methodPattern.lastIndex = closeIndex + 1;
  }
  return {
    type: 'object',
    properties,
    required: [...new Set(required)].sort()
  };
}

function parseRawSchemaHelpers(source) {
  const maskedSource = maskCppLiteralsAndComments(source);
  const properties = {};
  for (const match of source.matchAll(
    /\bAddAnyValue\s*\(\s*\w+\s*,\s*TEXT\(\s*"([^"]+)"\s*\)/g
  )) {
    if (
      match.index !== undefined
      && maskedSource.slice(match.index, match.index + 'AddAnyValue'.length) === 'AddAnyValue'
    ) {
      properties[match[1]] = {};
    }
  }
  for (const match of source.matchAll(
    /\bAddTypeUnion\s*\(\s*\w+\s*,\s*TEXT\(\s*"([^"]+)"\s*\)\s*,\s*\{([\s\S]*?)\}\s*,/g
  )) {
    if (
      match.index !== undefined
      && maskedSource.slice(match.index, match.index + 'AddTypeUnion'.length) === 'AddTypeUnion'
    ) {
      properties[match[1]] = { type: [...new Set(textValues(match[2]))].sort() };
    }
  }
  return properties;
}

export function extractNativeToolSchema(config) {
  const sources = reachableNativeSchemaSources(config);
  const schema = { type: 'object', properties: {}, required: [] };
  for (const source of sources) {
    const parsed = parseBuilderSchema(source);
    Object.assign(schema.properties, parsed.properties);
    Object.assign(schema.properties, parseRawSchemaHelpers(source));
    schema.required.push(...parsed.required);
  }
  schema.required = [...new Set(schema.required)].sort();
  return schema;
}
