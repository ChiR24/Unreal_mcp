export function escapePythonString(value: string): string {
  return value.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
}
