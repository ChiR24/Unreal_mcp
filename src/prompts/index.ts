export interface PromptArgument {
  type: string;
  description?: string;
  enum?: string[];
  default?: any;
  required?: boolean;
}

export interface Prompt {
  name: string;
  description: string;
  arguments?: Record<string, PromptArgument>;
}

export const prompts: Prompt[] = [
  {
    name: 'setup_three_point_lighting',
    description: 'Set up a basic three-point lighting rig around the current camera focus',
    arguments: {
      intensity: { 
        type: 'string', 
        enum: ['low', 'medium', 'high'], 
        default: 'medium',
        description: 'Light intensity level'
      }
    }
  },
  {
    name: 'create_fps_controller',
    description: 'Create a first-person shooter character controller',
    arguments: {
      spawnLocation: {
        type: 'object',
        description: 'Location to spawn the controller',
        required: false
      }
    }
  },
  {
    name: 'setup_post_processing',
    description: 'Configure post-processing volume with cinematic settings',
    arguments: {
      style: {
        type: 'string',
        enum: ['cinematic', 'realistic', 'stylized', 'noir'],
        default: 'cinematic',
        description: 'Visual style preset'
      }
    }
  }
];
