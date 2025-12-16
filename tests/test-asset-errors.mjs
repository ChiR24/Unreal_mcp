import { UnrealAutomationClient } from '../src/unreal-client.js';
import { runUnrealTests, assert } from './run-unreal-tool-tests.mjs';

runUnrealTests('asset_error_messages', [
    {
        name: 'Import Non-Existent File',
        action: async (client) => {
            try {
                await client.sendRequest('import', {
                    sourcePath: 'C:/Non/Existent/File.fbx',
                    destinationPath: '/Game/Tests/ImportFail'
                });
                throw new Error('Should have failed');
            } catch (error) {
                assert(error.code === 'SOURCE_NOT_FOUND', `Unexpected error code: ${error.code}`);
                assert(error.message.includes('Source file not found'), 'Unexpected error message');
            }
        }
    },
    {
        name: 'Rename Non-Existent Asset',
        action: async (client) => {
            try {
                await client.sendRequest('rename', {
                    sourcePath: '/Game/NonExistentAsset',
                    destinationPath: '/Game/NewLocation'
                });
                throw new Error('Should have failed');
            } catch (error) {
                assert(error.code === 'ASSET_NOT_FOUND', `Unexpected error code: ${error.code}`);
                assert(error.message.includes('Source asset not found'), 'Unexpected error message');
            }
        }
    }
]);
