import { mcpClients } from 'mcp-client-capabilities';
import { config } from '../config.js';
import { parseClientCapabilityProfile, type ClientCapabilityProfile } from './mcp-primitives/session-capability-profile.js';

const KNOWN_DYNAMIC_CLIENT_NAMES = ['cursor', 'cline', 'windsurf', 'kilo', 'opencode', 'vscode', 'visual studio code'];

export function parseDefaultCategories(): string[] {
    const raw = config.MCP_DEFAULT_CATEGORIES || 'all';
    const cats = raw.split(',').map(c => c.trim().toLowerCase()).filter(c => c.length > 0);
    return cats.length > 0 ? cats : ['all'];
}

export function clientSupportsListChanged(clientName: string | undefined): boolean {
    if (!clientName) return false;

    const normalizedName = clientName.toLowerCase().trim();

    for (const [key, clientInfo] of Object.entries(mcpClients)) {
        if (key.toLowerCase() === normalizedName ||
            (clientInfo.title && clientInfo.title.toLowerCase() === normalizedName)) {
            const tools = clientInfo.tools as { listChanged?: boolean } | undefined;
            return Boolean(tools?.listChanged);
        }
    }

    for (const known of KNOWN_DYNAMIC_CLIENT_NAMES) {
        if (normalizedName.includes(known)) return true;
    }

    return false;
}

export function getEffectiveCategories(supportsListChanged: boolean, currentCategories: string[]): string[] {
    return (!supportsListChanged || currentCategories.includes('all'))
        ? ['all']
        : currentCategories;
}

// Task 35 — derive the per-session client profile STRUCTURALLY from the declared
// MCP capabilities. Unlike clientSupportsListChanged above (a legacy name-based
// heuristic that no longer steers the permanent single-tool listing), this never
// inspects the client name or version, so two clients that declare identical
// capabilities behave identically regardless of brand. Task 37 calls it at
// initialize with server.getClientCapabilities().
export function deriveClientCapabilityProfile(declaredCapabilities: unknown): ClientCapabilityProfile {
    return parseClientCapabilityProfile(declaredCapabilities);
}
