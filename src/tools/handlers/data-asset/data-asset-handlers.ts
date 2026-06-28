/**
 * Data Asset Handlers
 *
 * General-purpose UDataAsset / UPrimaryDataAsset management:
 * - create_data_asset: Create an instance of a data asset class
 * - create_data_asset_blueprint: Create a Blueprint subclass of UPrimaryDataAsset
 * - get_data_asset_properties: Read all UPROPERTY values from a data asset
 * - set_data_asset_properties: Write UPROPERTY values on a data asset
 * - list_data_assets: List data assets by class or path
 * - duplicate_data_asset: Duplicate an existing data asset
 *
 * @module data-asset-handlers
 */

import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

export async function handleDataAssetTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const payload = { ...argsRecord, subAction: action };

  const result = await executeAutomationRequest(
    tools,
    'manage_data_asset',
    payload as HandlerArgs,
    `Automation bridge not available for data asset action: ${action}`
  );
  return cleanObject(result) as Record<string, unknown>;
}
