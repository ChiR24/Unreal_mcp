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

// The MRQ custom playback range is END-EXCLUSIVE, so an equal pair renders no
// frames. Unreal now refuses it, but only after the job has been queued and the
// payload has crossed the bridge; refusing here keeps the typed error before
// dispatch. Mirrors the native gate in
// McpAutomationBridge_SequenceMovieRenderOutput.cpp.
function refuseEmptyRenderRange(payload: Record<string, unknown>): Record<string, unknown> | undefined {
  const start = payload.startFrame;
  const end = payload.endFrame;
  if (typeof start !== 'number' || typeof end !== 'number') return undefined;
  if (!Number.isFinite(start) || !Number.isFinite(end) || end > start) return undefined;
  return {
    success: false,
    error: 'INVALID_FRAME_RANGE',
    message: `endFrame (${end}) must be greater than startFrame (${start}). The MRQ playback range is end-exclusive, so an equal pair renders no frames.`,
    action: 'configure_output_settings'
  };
}

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
  if (seqAction === 'configure_output_settings') {
    const emptyRange = refuseEmptyRenderRange(payload);
    if (emptyRange !== undefined) {
      return emptyRange;
    }
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
