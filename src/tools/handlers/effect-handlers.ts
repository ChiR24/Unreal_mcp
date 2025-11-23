import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleEffectTools(action: string, args: any, tools: ITools) {
  // Handle creation actions explicitly to use NiagaraTools helper
  if (action === 'create_niagara_system') {
    const res = await tools.niagaraTools.createSystem({
      name: args.name,
      savePath: args.savePath,
      template: args.template
    });
    return cleanObject(res);
  }
  if (action === 'create_niagara_emitter') {
    const res = await tools.niagaraTools.createEmitter({
      name: args.name,
      savePath: args.savePath,
      systemPath: args.systemPath,
      template: args.template
    });
    return cleanObject(res);
  }

  // Pre-process arguments for particle presets
  if (args.action === 'particle') {
    const presets: Record<string, string> = {
      'Default': '/StarterContent/Particles/P_Steam_Lit.P_Steam_Lit',
      'Smoke': '/StarterContent/Particles/P_Smoke.P_Smoke',
      'Fire': '/StarterContent/Particles/P_Fire.P_Fire',
      'Explosion': '/StarterContent/Particles/P_Explosion.P_Explosion',
    };
    // Check both preset and effectType fields
    const key = args.preset || args.effectType;
    if (key && presets[key]) {
      args.preset = presets[key];
    }
  }
  const res = await executeAutomationRequest(tools, 'create_effect', args, 'Automation bridge not available for effect creation operations');
  return cleanObject(res);
}
