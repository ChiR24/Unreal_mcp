import type { ResourceRevision, RevisionProvider, RevisionedUri } from '../server/mcp-primitives/resource-revision.js';
import type { AutomationRequestBridge } from '../types/tools/tool-interfaces.js';

/** A revision provider pinned per URI; unnamed URIs read revision 1. */
export function revisionsAt(revisions: Partial<Record<RevisionedUri, number>> = {}): RevisionProvider {
  return { currentRevision: (uri) => (revisions[uri] ?? 1) as ResourceRevision };
}

/** A connected bridge whose manage_asset `exists` answers `exists`. */
export function assetExistsBridge(exists: boolean): AutomationRequestBridge {
  return { isConnected: () => true, sendAutomationRequest: async () => ({ success: true, result: { exists } }) };
}
