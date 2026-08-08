const ASSIGNMENT_PATTERN = /(^|[^=!<>])(?:\+=|-=|\*=|\/=|%=|&=|\|=|\^=|<<=|>>=|=(?!=))/;
const FUNCTION_CALL_PATTERN = /(?:^|[^\w])(?:[A-Za-z_]\w*(?:::\w+|\.\w+|->\w+)*)\s*\(/;
const MUTATION_PATTERN = /(?:\+\+|--|\b(?:delete|new|throw)\b)/;

export function expressionRequiresUnsafePermission(expression: string): boolean {
  return ASSIGNMENT_PATTERN.test(expression)
    || FUNCTION_CALL_PATTERN.test(expression)
    || MUTATION_PATTERN.test(expression);
}

export function unsafePermissionGranted(explicitUnsafe: unknown): boolean {
  return process.env.UE_MCP_DEBUG_ALLOW_UNSAFE === 'true' && explicitUnsafe === true;
}
