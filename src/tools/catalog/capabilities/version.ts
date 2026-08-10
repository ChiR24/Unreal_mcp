import type { UnrealVersion } from './identifiers.js';

const CHANNEL_RANK = { preview: 0, stable: 1 } as const;

export function compareUnrealVersion(a: UnrealVersion, b: UnrealVersion): number {
  if (a.major !== b.major) return a.major - b.major;
  if (a.minor !== b.minor) return a.minor - b.minor;
  if (a.patch !== b.patch) return a.patch - b.patch;
  const rankA = CHANNEL_RANK[a.channel];
  const rankB = CHANNEL_RANK[b.channel];
  if (rankA !== rankB) return rankA - rankB;
  if (a.channel === 'preview' && b.channel === 'preview') {
    return (a.preview ?? 0) - (b.preview ?? 0);
  }
  return 0;
}
