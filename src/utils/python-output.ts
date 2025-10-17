// DEPRECATED: Python support has been removed
// This file exists only for backwards compatibility during migration

export function parseStandardResult(_output: any): { success: boolean; data?: any; text?: string; raw?: any } {
  // Return empty result for compatibility
  // Python code paths will fail elsewhere since Python execution is disabled
  return { success: false, data: {}, text: '', raw: _output };
}

export function parsePythonOutput(_output: any): any {
  // Return empty result for compatibility
  return {};
}
