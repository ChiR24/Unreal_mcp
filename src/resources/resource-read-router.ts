// src/resources/resource-read-router.ts
// Task 31: the single seam the ResourceHandler delegates every non-legacy URI
// to. It parses the URI (exact static resources first, then templates), routes
// to the matching bounded provider, enforces the byte budget on the serialized
// payload, and returns MCP `contents`. Every failure surfaces as a typed
// `ResourceError` thrown by a provider or by this router; a failed read never
// returns a success-shaped payload. The emitted text always carries the URI and
// its revision so the revision is visible to the client.

import type { RevisionedResource } from '../server/mcp-primitives/resource-revision.js';
import { RESOURCE_ERROR_CODES, ResourceError, enforceByteBudget } from './resource-errors.js';
import type { CapabilityResources } from './capability-resources.js';
import type { EditorStateResources } from './editor-state-resources.js';
import type { KnowledgeResources } from './knowledge-resources.js';

export interface ResourceReadResult {
  contents: Array<{ uri: string; mimeType: string; text: string }>;
}

/** The delegation seam consumed by the ResourceHandler for non-legacy URIs. */
export interface ExtendedResourceReader {
  read(uri: string): Promise<ResourceReadResult>;
}

const CAPABILITY_PREFIX = 'ue://capability/';
const KNOWLEDGE_PREFIX = 'ue://knowledge/';
const OBJECT_PREFIX = 'ue://object/';
const ASSET_PREFIX = 'ue://asset/';

function matchPrefix(uri: string, prefix: string): string | undefined {
  return uri.startsWith(prefix) ? uri.slice(prefix.length) : undefined;
}

export class ResourceReadRouter implements ExtendedResourceReader {
  constructor(
    private readonly capability: CapabilityResources,
    private readonly editorState: EditorStateResources,
    private readonly knowledge: KnowledgeResources,
  ) {}

  async read(uri: string): Promise<ResourceReadResult> {
    const revisioned = await this.route(uri);
    return toContents(revisioned);
  }

  private async route(uri: string): Promise<RevisionedResource<unknown>> {
    switch (uri) {
      case 'ue://capability/catalog':
        return this.capability.readCatalog();
      case 'ue://project':
        return this.editorState.readProject();
      case 'ue://editor':
        return this.editorState.readEditor();
      case 'ue://selection':
        return this.editorState.readSelection();
      default:
        break;
    }

    const capabilityId = matchPrefix(uri, CAPABILITY_PREFIX);
    if (capabilityId !== undefined) {
      return this.capability.readRecord(uri, capabilityId);
    }

    const knowledgePath = matchPrefix(uri, KNOWLEDGE_PREFIX);
    if (knowledgePath !== undefined) {
      const slash = knowledgePath.indexOf('/');
      if (slash <= 0 || slash === knowledgePath.length - 1) {
        throw new ResourceError(
          RESOURCE_ERROR_CODES.INVALID_URI,
          uri,
          'Knowledge template requires ue://knowledge/{engineVersion}/{topic}',
        );
      }
      const engineVersion = knowledgePath.slice(0, slash);
      const topic = knowledgePath.slice(slash + 1);
      return this.knowledge.readKnowledge(uri, engineVersion, topic);
    }

    const objectPath = matchPrefix(uri, OBJECT_PREFIX);
    if (objectPath !== undefined) {
      return this.knowledge.readObject(uri, objectPath);
    }

    const assetPath = matchPrefix(uri, ASSET_PREFIX);
    if (assetPath !== undefined) {
      return this.knowledge.readAsset(uri, assetPath);
    }

    throw new ResourceError(RESOURCE_ERROR_CODES.NOT_FOUND, uri, `Unknown resource: ${uri}`);
  }
}

function toContents(revisioned: RevisionedResource<unknown>): ResourceReadResult {
  const text = JSON.stringify(
    { uri: revisioned.uri, revision: revisioned.revision, data: revisioned.data },
    null,
    2,
  );
  enforceByteBudget(revisioned.uri, text);
  return { contents: [{ uri: revisioned.uri, mimeType: 'application/json', text }] };
}
