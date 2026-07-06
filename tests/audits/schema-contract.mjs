function sortedUnique(values) {
  return [...new Set(values ?? [])].sort();
}

function normalizedType(value) {
  return Array.isArray(value) ? sortedUnique(value) : value;
}

function sameValue(left, right) {
  return JSON.stringify(left) === JSON.stringify(right);
}

function recordMismatch(mismatches, path, typeScript, native) {
  if (sameValue(typeScript, native)) return;
  mismatches.push({ path, typeScript, native });
}

function compareRequired(path, typeScript, native, mismatches) {
  const typeScriptRequired = sortedUnique(typeScript.required);
  const nativeRequired = sortedUnique(native.required);
  recordMismatch(
    mismatches,
    path ? `${path}.required` : 'required',
    typeScriptRequired,
    nativeRequired
  );
}

function compareProperties(path, typeScript, native, mismatches) {
  const typeScriptProperties = typeScript.properties;
  const nativeProperties = native.properties;
  if (!typeScriptProperties && !nativeProperties) return;
  if (!typeScriptProperties || !nativeProperties) {
    recordMismatch(
      mismatches,
      `${path}.properties.present`,
      Boolean(typeScriptProperties),
      Boolean(nativeProperties)
    );
    return;
  }

  const propertyNames = sortedUnique([
    ...Object.keys(typeScriptProperties),
    ...Object.keys(nativeProperties)
  ]);
  for (const propertyName of propertyNames) {
    const propertyPath = path ? `${path}.${propertyName}` : propertyName;
    const typeScriptProperty = typeScriptProperties[propertyName];
    const nativeProperty = nativeProperties[propertyName];
    if (!typeScriptProperty || !nativeProperty) {
      recordMismatch(
        mismatches,
        `${propertyPath}.present`,
        Boolean(typeScriptProperty),
        Boolean(nativeProperty)
      );
      continue;
    }
    compareSchemaNode(propertyPath, typeScriptProperty, nativeProperty, mismatches);
  }
  compareRequired(path, typeScript, native, mismatches);
}

function compareSchemaNode(path, typeScript, native, mismatches) {
  recordMismatch(
    mismatches,
    `${path}.type`,
    normalizedType(typeScript.type),
    normalizedType(native.type)
  );

  if (typeScript.enum !== undefined || native.enum !== undefined) {
    recordMismatch(
      mismatches,
      `${path}.enum`,
      sortedUnique(typeScript.enum),
      sortedUnique(native.enum)
    );
  }

  if (typeScript.type === 'array' && native.type === 'array') {
    const typeScriptItems = typeScript.items ?? {};
    const nativeItems = native.items ?? {};
    compareSchemaNode(`${path}.items`, typeScriptItems, nativeItems, mismatches);
  }

  if (typeScript.type === 'object' && native.type === 'object') {
    compareProperties(path, typeScript, native, mismatches);
  }
}

function difference(left, right) {
  const rightSet = new Set(right);
  return left.filter((value) => !rightSet.has(value)).sort();
}

export function compareToolSchemas(tool, typeScriptSchema, nativeSchema) {
  const typeScriptProperties = typeScriptSchema?.properties ?? {};
  const nativeProperties = nativeSchema?.properties ?? {};
  const typeScriptNames = Object.keys(typeScriptProperties).sort();
  const nativeNames = Object.keys(nativeProperties).sort();
  const missingNativeProperties = difference(typeScriptNames, nativeNames);
  const extraNativeProperties = difference(nativeNames, typeScriptNames);
  const schemaMismatches = [];

  for (const propertyName of typeScriptNames) {
    const typeScriptProperty = typeScriptProperties[propertyName];
    const nativeProperty = nativeProperties[propertyName];
    if (!typeScriptProperty || !nativeProperty) continue;
    compareSchemaNode(propertyName, typeScriptProperty, nativeProperty, schemaMismatches);
  }
  compareRequired('', typeScriptSchema ?? {}, nativeSchema ?? {}, schemaMismatches);
  schemaMismatches.sort((left, right) => left.path.localeCompare(right.path));

  if (
    missingNativeProperties.length === 0
    && extraNativeProperties.length === 0
    && schemaMismatches.length === 0
  ) {
    return undefined;
  }
  return {
    tool,
    missingNativeProperties,
    extraNativeProperties,
    schemaMismatches
  };
}
