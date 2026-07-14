import { manageAiBehaviorProperties } from './manage-ai-behavior-properties.js';
import { manageAiNavigationProperties } from './manage-ai-navigation-properties.js';
import { manageAiRuntimeProperties } from './manage-ai-runtime-properties.js';
import { manageAiNpcProperties } from './manage-ai-npc-properties.js';

export const manageAiInputSchema = {
      type: 'object',
      properties: {
        ...manageAiBehaviorProperties,
        ...manageAiNavigationProperties,
        ...manageAiRuntimeProperties,
        ...manageAiNpcProperties
      },
      required: ['action']
    };

