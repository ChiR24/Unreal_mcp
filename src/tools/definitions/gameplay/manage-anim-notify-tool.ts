import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageAnimNotifyToolDefinition: ToolDefinition = {
    name: 'manage_anim_notify',
    category: 'gameplay',
    description: 'Manage animation notifies on UAnimSequence/UAnimMontage. Actions: add_notify, add_notify_state, remove_notify, list_notifies, set_notify_properties, list_notify_classes.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'add_notify', 'add_notify_state', 'remove_notify',
            'list_notifies', 'set_notify_properties', 'list_notify_classes'
          ],
          description: 'The animation notify action to perform.'
        },
        assetPath: { type: 'string', description: 'Path to the animation asset (AnimSequence or AnimMontage).' },
        notifyClass: { type: 'string', description: 'Class name of the notify (e.g., AnimNotify_PlaySound, AnimNotify_PlayParticleEffect).' },
        notifyName: { type: 'string', description: 'Display name for the notify.' },
        time: { type: 'number', description: 'Trigger time in seconds for add_notify.' },
        beginTime: { type: 'number', description: 'Start time for add_notify_state.' },
        endTime: { type: 'number', description: 'End time for add_notify_state.' },
        trackIndex: { type: 'integer', description: 'Notify track index (0-based).' },
        notifyIndex: { type: 'integer', description: 'Index of notify to remove/modify.' },
        properties: { type: 'object', description: 'Properties to set on the notify.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        notifies: { type: 'array', items: { type: 'object' }, description: 'List of notifies on the asset.' },
        classes: { type: 'array', items: { type: 'string' }, description: 'Available notify classes.' },
        notifyIndex: { type: 'integer', description: 'Index of added/modified notify.' }
      }
    }
};
