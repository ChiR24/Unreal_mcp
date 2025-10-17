/**
 * Python string escaping utilities
 * Used for AutomationBridge plugin communication only
 */

export function escapePythonString(str: string): string {
  // Properly escape strings for embedding in Python code
  // This is needed when passing context to AutomationBridge Python execution
  return str
    .replace(/\\/g, '\\\\')  // Backslash
    .replace(/"/g, '\\"')    // Double quote
    .replace(/\n/g, '\\n')    // Newline
    .replace(/\r/g, '\\r')    // Carriage return
    .replace(/\t/g, '\\t');   // Tab
}

export function executePython(_code: string): Promise<any> {
  throw new Error('Direct Python execution has been removed. Please use the automation bridge for all operations.');
}
