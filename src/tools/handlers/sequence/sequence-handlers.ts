import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { getGatewayTimeoutMs } from '../../../automation/gateway-timeout-context.js';
import { executeAutomationRequest, normalizePathFields } from '../foundation/dispatch/common-handlers.js';
import { handleSequenceAssetAction } from './sequence-asset-actions.js';
import { handleSequenceCoreAction } from './sequence-core-actions.js';
import { handleSequencePlaybackAction } from './sequence-playback-actions.js';
import { handleSequenceTrackAction } from './sequence-track-actions.js';

const DEFAULT_MRQ_TIMEOUT_MS = 300000;
const MAX_MRQ_TIMEOUT_MS = 3600000;
const MRQ_TRANSPORT_GRACE_MS = 35000;

type SequenceActionHandler = (
  action: string,
  args: Record<string, unknown>,
  tools: ITools
) => Promise<unknown | undefined>;

const sequenceActionHandlers: readonly SequenceActionHandler[] = [
  handleSequenceCoreAction,
  handleSequencePlaybackAction,
  handleSequenceAssetAction,
  handleSequenceTrackAction
];

export async function handleSequenceTools(action: string, args: Record<string, unknown>, tools: ITools): Promise<unknown> {
  const seqAction = String(action || '').trim();
  const normalizedArgs = normalizePathFields(args, [
    'path',
    'assetPath',
    'destinationPath',
    'sequencePath',
    'masterSequencePath',
    'subsequencePath',
    'shotSequencePath',
    'mapPath',
    'mediaPlayerPath',
    'mediaSourcePath',
    'playlistPath',
    'playerPath',
    'sourcePath',
    'defaultSourcePath',
    'sourcePaths',
    'platformSources',
    'animationPath',
    'animationSequencePath',
    'skeletalMeshPath',
    'materialPath',
    'particleSystemPath',
    'cameraShakePath',
    'cameraShakeClass',
    'takePresetPath',
    'recordingSequencePath',
    'takeSequencePath',
    'executorClass',
    'burnIn'
  ]);

  for (const handler of sequenceActionHandlers) {
    const result = await handler(seqAction, normalizedArgs, tools);
    if (result !== undefined) {
      return result;
    }
  }

  const payload = { ...normalizedArgs };
  if (payload.action && !payload.subAction) {
    payload.subAction = payload.action;
  }
  if (seqAction === 'start_render') {
    // start_render is the one action whose deadline Unreal itself enforces, so
    // the value has to reach the payload. The gateway refuses `timeoutMs` in
    // action params, which left `options.timeoutMs` as the only client route to
    // it; taking it here is what keeps a client-set render deadline reachable.
    const requestedTimeoutMs = payload.timeoutMs ?? getGatewayTimeoutMs();
    const validRequestedTimeout =
      typeof requestedTimeoutMs === 'number' &&
      Number.isFinite(requestedTimeoutMs) &&
      requestedTimeoutMs > 0 &&
      requestedTimeoutMs <= MAX_MRQ_TIMEOUT_MS
        ? requestedTimeoutMs
        : undefined;
    const renderTimeoutMs = validRequestedTimeout ?? DEFAULT_MRQ_TIMEOUT_MS;
    if (payload.timeoutMs === undefined) {
      payload.timeoutMs = renderTimeoutMs;
    }
    return await executeAutomationRequest(tools, 'manage_sequence', payload, undefined, {
      timeoutMs: renderTimeoutMs + MRQ_TRANSPORT_GRACE_MS,
      forwardTimeoutMsToUnreal: true
    });
  }
  return await executeAutomationRequest(tools, 'manage_sequence', payload);
}
