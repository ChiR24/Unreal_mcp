import type { ITools } from '../../../types/tools/tool-interfaces.js';
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
    const requestedTimeoutMs = payload.timeoutMs;
    const validRequestedTimeout =
      typeof requestedTimeoutMs === 'number' &&
      Number.isFinite(requestedTimeoutMs) &&
      requestedTimeoutMs > 0 &&
      requestedTimeoutMs <= MAX_MRQ_TIMEOUT_MS
        ? requestedTimeoutMs
        : undefined;
    const renderTimeoutMs = validRequestedTimeout ?? DEFAULT_MRQ_TIMEOUT_MS;
    if (requestedTimeoutMs === undefined) {
      payload.timeoutMs = DEFAULT_MRQ_TIMEOUT_MS;
    }
    return await executeAutomationRequest(tools, 'manage_sequence', payload, undefined, {
      timeoutMs: renderTimeoutMs + MRQ_TRANSPORT_GRACE_MS,
      forwardTimeoutMsToUnreal: true
    });
  }
  return await executeAutomationRequest(tools, 'manage_sequence', payload);
}
