/**
 * Effect (C18 / O21) route dispositions.
 * Every row cites a concrete .cpp file (v2 source-derived).
 */
import { type RawRouteDisposition, ROUTE_EVIDENCE_PATHS } from './routedispositions-paths.js';

const { EFFECT_NIAGARA_SPAWN, EFFECT_TOOL, EFFECT_MODULE_ROUTING } = ROUTE_EVIDENCE_PATHS;

export const EFFECT_ROUTE_DISPOSITIONS: readonly RawRouteDisposition[] = [
  {
    key: 'route:effect:niagara',
    route: 'niagara',
    domain: 'effect',
    status: 'hidden',
    owner: 'Effect',
    evidenceSource: EFFECT_NIAGARA_SPAWN,
    evidenceSymbol: 'LowerSubAction == TEXT("niagara")',
    evidenceTool: 'manage_effect',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_effect:spawn_niagara',
    rationale: 'C18/O21: niagara shares the TS route with spawn_niagara; map to the canonical id.',
  },
  {
    key: 'route:effect:activate',
    route: 'activate',
    domain: 'effect',
    status: 'hidden',
    owner: 'Effect',
    evidenceSource: EFFECT_TOOL,
    evidenceSymbol: 'TEXT("activate")',
    evidenceTool: 'manage_effect',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_effect:activate_effect',
    rationale: 'C18/O21: activate shares the TS route with activate_effect; map to the canonical id.',
  },
  {
    key: 'route:effect:shadowed_stubs',
    route: 'shadowed_effect_module_stubs',
    domain: 'effect',
    status: 'hidden',
    owner: 'Effect',
    evidenceSource: EFFECT_MODULE_ROUTING,
    evidenceSymbol:
      'HandleNiagaraSpawnModules; HandleNiagaraBehaviorModules; HandleNiagaraRenderModules; HandleNiagaraDataEventModules; HandleNiagaraParameterModules',
    evidenceTool: 'manage_effect',
    disposition: 'remove',
    removalGuidance:
      'C18/O21: five false-return module helpers are shadowed for canonical module actions but still run on fallback paths; remove the shadowed stubs.',
    rationale: 'C18/O21: shadowed Effect-domain stubs with false returns.',
  },
];
