import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageGameplayTagsToolDefinition: ToolDefinition = {
    name: 'manage_gameplay_tags',
    category: 'core',
    description: 'Manage gameplay tags: project dictionary and actor assignments. Actions: add_tag, remove_tag, list_tags, get_tag_children, has_tag, add_tag_to_actor, remove_tag_from_actor, get_actor_tags, get_tag_hierarchy.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'add_tag', 'remove_tag', 'list_tags', 'get_tag_children', 'has_tag',
            'add_tag_to_actor', 'remove_tag_from_actor', 'get_actor_tags',
            'get_tag_hierarchy'
          ],
          description: 'Gameplay tag action to perform.'
        },
        tag: { type: 'string', description: 'Gameplay tag string (e.g. "Character.State.Dead"). Dot-separated hierarchy.' },
        prefix: { type: 'string', description: 'Filter prefix for list_tags (e.g. "Character" lists all Character.* tags).' },
        actorName: commonSchemas.actorName,
        description: { type: 'string', description: 'Description for add_tag.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        tags: { type: 'array', items: { type: 'string' }, description: 'List of tag names.' },
        exists: { type: 'boolean', description: 'Whether the tag exists (has_tag).' },
        hierarchy: { type: 'object', description: 'Nested tag hierarchy (get_tag_hierarchy).' }
      }
    }
};
