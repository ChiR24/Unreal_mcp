/**
 * MCP Resource Templates for Unreal Engine MCP Server
 * Phase E2: Add parameterized resource templates
 * 
 * Resource templates allow clients to construct URIs dynamically
 * for accessing specific assets or actors by path/name.
 */

import { sanitizePathSafe, SanitizePathResult } from '../utils/validation.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('ResourceTemplates');

/**
 * Resource template definition matching MCP SDK schema
 */
export interface ResourceTemplate {
  uriTemplate: string;
  name: string;
  description: string;
  mimeType: string;
}

/**
 * All available resource templates
 */
export const resourceTemplates: ResourceTemplate[] = [
  {
    uriTemplate: 'ue://asset/{path}',
    name: 'Asset by Path',
    description: 'Get metadata for a specific Unreal Engine asset by its path (e.g., ue://asset/Game/Meshes/Cube)',
    mimeType: 'application/json'
  },
  {
    uriTemplate: 'ue://actor/{name}',
    name: 'Actor by Name',
    description: 'Get information about a specific actor in the current level by name or label',
    mimeType: 'application/json'
  }
];

/**
 * Result of matching a URI against templates
 */
export interface TemplateMatch {
  template: ResourceTemplate;
  params: Record<string, string>;
}

/**
 * Match a URI against all registered templates
 * @param uri The URI to match
 * @returns TemplateMatch if matched, null otherwise
 */
export function matchTemplate(uri: string): TemplateMatch | null {
  for (const template of resourceTemplates) {
    // Convert template to regex: {param} -> named capture group
    const regexPattern = template.uriTemplate
      .replace(/[.*+?^${}()|[\]\\]/g, '\\$&')  // Escape special regex chars
      .replace(/\\\{(\w+)\\\}/g, '(?<$1>[^/]+)');  // Convert {param} to named group
    
    const regex = new RegExp(`^${regexPattern}$`);
    const match = uri.match(regex);
    
    if (match?.groups) {
      log.debug(`URI "${uri}" matched template "${template.uriTemplate}"`, match.groups);
      return { template, params: match.groups };
    }
  }
  
  log.debug(`URI "${uri}" did not match any template`);
  return null;
}

/**
 * Validation result for template parameters
 */
export interface ValidationResult {
  valid: boolean;
  error?: string;
  sanitizedParams?: Record<string, string>;
}

/**
 * Validate and sanitize template parameters
 * @param params Raw parameters from URI match
 * @returns Validation result with sanitized parameters
 */
export function validateTemplateParams(params: Record<string, string>): ValidationResult {
  const sanitizedParams: Record<string, string> = {};
  
  // Validate and sanitize 'path' parameter
  if (params.path) {
    // Ensure path starts with / for consistency
    let pathValue = params.path;
    if (!pathValue.startsWith('/')) {
      pathValue = '/' + pathValue;
    }
    
    const result: SanitizePathResult = sanitizePathSafe(pathValue);
    if (!result.success) {
      log.warn(`Invalid path parameter: ${result.error}`);
      return { valid: false, error: `Invalid path: ${result.error}` };
    }
    sanitizedParams.path = result.path;
  }
  
  // Validate 'name' parameter (actor name)
  if (params.name) {
    const name = params.name.trim();
    
    if (name.length === 0) {
      return { valid: false, error: 'Actor name cannot be empty' };
    }
    
    if (name.length > 256) {
      return { valid: false, error: 'Actor name exceeds maximum length (256)' };
    }
    
    // Check for dangerous characters that could be used for injection
    // eslint-disable-next-line no-control-regex
    const dangerousChars = /[<>:"|?*\x00-\x1f]/;
    if (dangerousChars.test(name)) {
      log.warn(`Actor name contains invalid characters: ${name}`);
      return { valid: false, error: 'Actor name contains invalid characters' };
    }
    
    sanitizedParams.name = name;
  }
  
  // Copy through any other parameters (already validated by regex match)
  for (const key of Object.keys(params)) {
    if (!(key in sanitizedParams)) {
      const value = params[key];
      if (value !== undefined) {
        sanitizedParams[key] = value;
      }
    }
  }
  
  return { valid: true, sanitizedParams };
}

/**
 * Check if a URI matches the resource template pattern
 * @param uri URI to check
 * @returns true if URI matches any template
 */
export function isTemplateUri(uri: string): boolean {
  return matchTemplate(uri) !== null;
}

/**
 * Extract template name from matched URI
 * @param uri URI that matched a template
 * @returns Template name or null if no match
 */
export function getTemplateName(uri: string): string | null {
  const match = matchTemplate(uri);
  if (!match) return null;
  return match.template.name;
}
