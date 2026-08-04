import { createUnknownActionResponse } from '../foundation/dispatch/handler-error-context.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { PipelineArgs } from '../../../types/handlers/handler-types.js';
import { handleRunUbt } from './pipeline-ubt-runner.js';

export async function handlePipelineTools(action: string, args: PipelineArgs, tools: ITools) {
  switch (action) {
    case 'run_ubt':
      return await handleRunUbt(args, tools);

    default:
      return createUnknownActionResponse(action, `Unknown system_control pipeline action: ${action}`);
  }
}
