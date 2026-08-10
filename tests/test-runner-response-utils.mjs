function getValueAtPath(source, pathExpression) {
  if (!pathExpression || typeof pathExpression !== 'string') return undefined;
  return pathExpression.split('.').reduce((current, part) => {
    if (current === undefined || current === null) return undefined;
    if (Array.isArray(current)) {
      const index = Number.parseInt(part, 10);
      return Number.isInteger(index) ? current[index] : undefined;
    }
    if (typeof current === 'object') {
      return Object.prototype.hasOwnProperty.call(current, part) ? current[part] : undefined;
    }
    return undefined;
  }, source);
}

function matchesObjectSubset(candidate, expectedSubset) {
  if (!candidate || typeof candidate !== 'object' || !expectedSubset || typeof expectedSubset !== 'object') return false;
  return Object.entries(expectedSubset).every(([key, expectedValue]) => {
    if (!Object.prototype.hasOwnProperty.call(candidate, key)) return false;
    const actualValue = candidate[key];
    if (expectedValue && typeof expectedValue === 'object' && !Array.isArray(expectedValue)) {
      if ('length' in expectedValue && Array.isArray(actualValue)) return actualValue.length === expectedValue.length;
      return matchesObjectSubset(actualValue, expectedValue);
    }
    return actualValue === expectedValue;
  });
}

export function evaluateAssertions(testCase, response) {
  if (!Array.isArray(testCase.assertions) || testCase.assertions.length === 0) return { passed: true };

  for (const assertion of testCase.assertions) {
    const label = assertion.label || assertion.path || 'assertion';
    const actual = getValueAtPath(response, assertion.path);

    if (Object.prototype.hasOwnProperty.call(assertion, 'equals') && actual !== assertion.equals) {
      return { passed: false, reason: `${label}: expected ${JSON.stringify(assertion.equals)}, got ${JSON.stringify(actual)}` };
    }

    if (Object.prototype.hasOwnProperty.call(assertion, 'approximately')) {
      const expected = assertion.approximately;
      const tolerance = assertion.tolerance ?? 1e-6;
      const validNumbers =
        typeof actual === 'number' &&
        Number.isFinite(actual) &&
        typeof expected === 'number' &&
        Number.isFinite(expected) &&
        typeof tolerance === 'number' &&
        Number.isFinite(tolerance) &&
        tolerance >= 0;
      if (!validNumbers || Math.abs(actual - expected) > tolerance) {
        return {
          passed: false,
          reason: `${label}: expected ${JSON.stringify(expected)} within ${JSON.stringify(tolerance)}, got ${JSON.stringify(actual)}`
        };
      }
    }

    if (Object.prototype.hasOwnProperty.call(assertion, 'includes')) {
      const expected = assertion.includes;
      if (
        typeof actual !== 'string' ||
        typeof expected !== 'string' ||
        !actual.includes(expected)
      ) {
        return {
          passed: false,
          reason: `${label}: expected ${JSON.stringify(actual)} to include ${JSON.stringify(expected)}`
        };
      }
    }

    if (Object.prototype.hasOwnProperty.call(assertion, 'notIncludes')) {
      const expected = assertion.notIncludes;
      const containsExpected =
        typeof actual === 'string' && typeof expected === 'string'
          ? actual.includes(expected)
          : Array.isArray(actual) && typeof expected === 'string'
            ? actual.includes(expected)
            : undefined;

      if (containsExpected === undefined) {
        return {
          passed: false,
          reason: `${label}: expected an array or string and string needle, got ${typeof actual} and ${typeof expected}`
        };
      }

      if (containsExpected) {
        return {
          passed: false,
          reason: `${label}: expected ${JSON.stringify(actual)} not to include ${JSON.stringify(expected)}`
        };
      }
    }

    if (Object.prototype.hasOwnProperty.call(assertion, 'length') && (!Array.isArray(actual) || actual.length !== assertion.length)) {
      return { passed: false, reason: `${label}: expected array length ${assertion.length}, got ${Array.isArray(actual) ? actual.length : typeof actual}` };
    }

    if (Object.prototype.hasOwnProperty.call(assertion, 'minLength') && (!Array.isArray(actual) || actual.length < assertion.minLength)) {
      return { passed: false, reason: `${label}: expected array length at least ${assertion.minLength}, got ${Array.isArray(actual) ? actual.length : typeof actual}` };
    }

    if (assertion.includesObject) {
      if (!Array.isArray(actual) || !actual.some((entry) => matchesObjectSubset(entry, assertion.includesObject))) {
        return { passed: false, reason: `${label}: no array item matched ${JSON.stringify(assertion.includesObject)}` };
      }
    }
  }

  return { passed: true };
}

export function selectCaptureValue(structuredContent, captureResult) {
  const { fromField, selectField, where } = captureResult ?? {};
  if (!fromField) return undefined;
  let value = fromField.includes('.')
    ? getValueAtPath(structuredContent, fromField)
    : structuredContent?.[fromField];

  if (where) {
    if (!Array.isArray(value) || typeof where.path !== 'string') return undefined;
    value = value.find((entry) => {
      const candidate = getValueAtPath(entry, where.path);
      if (Object.prototype.hasOwnProperty.call(where, 'equals')) {
        return candidate === where.equals;
      }
      return typeof candidate === 'string' &&
        typeof where.includes === 'string' &&
        candidate.includes(where.includes);
    });
  }

  return selectField ? getValueAtPath(value, selectField) : value;
}

export function selectCaptureValues(structuredContent, captureResult) {
  const captures = Array.isArray(captureResult)
    ? captureResult
    : [captureResult];
  const values = [];

  for (const capture of captures) {
    const { key, fromField } = capture ?? {};
    if (!key || !fromField) continue;
    const value = selectCaptureValue(structuredContent, capture);
    if (value !== undefined) {
      values.push({ key, value });
    }
  }

  return values;
}

export function withServerTimeout(callOptions, serverTimeoutMs) {
  const args = { ...(callOptions.arguments ?? {}) };
  if (callOptions.name === 'unreal') {
    if (args.operation !== 'execute') return { ...callOptions, arguments: args };

    const options = args.options !== null
      && typeof args.options === 'object'
      && !Array.isArray(args.options)
      ? { ...args.options }
      : {};
    if (!Object.prototype.hasOwnProperty.call(options, 'timeoutMs')) {
      options.timeoutMs = serverTimeoutMs;
    }
    args.options = options;
    return { ...callOptions, arguments: args };
  }

  if (!Object.prototype.hasOwnProperty.call(args, 'timeoutMs')) {
    args.timeoutMs = serverTimeoutMs;
  }
  return { ...callOptions, arguments: args };
}
