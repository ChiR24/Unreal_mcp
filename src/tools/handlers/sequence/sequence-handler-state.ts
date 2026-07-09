import type { StandardActionResponse } from '../../../types/tools/tool-interfaces.js';

export interface SequenceActionResponse extends StandardActionResponse {
  result?: {
    sequencePath?: string;
    results?: Array<{ success?: boolean; error?: string }>;
    [key: string]: unknown;
  };
  bindings?: Array<{ name?: string; [key: string]: unknown }>;
  message?: string;
}

export function getErrorString(res: SequenceActionResponse | null | undefined): string {
  if (!res) return '';
  return typeof res.error === 'string' ? res.error : '';
}

export function getMessageString(res: SequenceActionResponse | null | undefined): string {
  if (!res) return '';
  return typeof res.message === 'string' ? res.message : '';
}
