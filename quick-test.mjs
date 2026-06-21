import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = __dirname;

async function runQuickTest() {
  const transport = new StdioClientTransport({
    command: 'node',
    args: [path.join(repoRoot, 'dist', 'cli.js')],
    env: { ...process.env, NODE_ENV: 'development' }
  });

  const client = new Client(
    { name: 'quick-test', version: '1.0.0' },
    { capabilities: {} }
  );

  console.log('Connecting to MCP server...');
  await client.connect(transport);
  console.log('Connected!');

  try {
    const uiJson = {
      widget_name: 'RootCanvas',
      widget_class: '/Script/UMG.CanvasPanel',
      properties: {
        bIsVariable: true
      },
      children: [
        {
          widget_name: 'TitleText',
          widget_class: '/Script/UMG.TextBlock',
          properties: {
            Text: 'Hola desde WebUMG!',
            ColorAndOpacity: {
              SpecifiedColor: { R: 0, G: 1, B: 0, A: 1 }
            },
            Font: { Size: 48 }
          },
          slot_properties: {
            LayoutData: {
              Offsets: { Left: 50, Top: 50, Right: 500, Bottom: 100 }
            }
          }
        },
        {
          widget_name: 'TestButton',
          widget_class: '/Script/UMG.Button',
          properties: {
            BackgroundColor: { R: 1, G: 0, B: 0, A: 1 }
          },
          slot_properties: {
            LayoutData: {
              Offsets: { Left: 50, Top: 200, Right: 250, Bottom: 60 }
            }
          },
          children: [
            {
              widget_name: 'ButtonText',
              widget_class: '/Script/UMG.TextBlock',
              properties: {
                Text: 'Haz Clic',
                ColorAndOpacity: {
                  SpecifiedColor: { R: 1, G: 1, B: 1, A: 1 }
                }
              }
            }
          ]
        }
      ]
    };

    console.log('Calling apply_widget_tree...');
    const result = await client.callTool({
      name: 'manage_widget_authoring',
      arguments: {
        action: 'apply_widget_tree',
        widgetPath: '/Game/WBP_TestUI',
        widgetTreeJson: JSON.stringify(uiJson)
      }
    });
    console.log('Result:', JSON.stringify(result, null, 2));

  } catch (err) {
    console.error('Error calling tool:', err);
  } finally {
    await transport.close();
  }
}

runQuickTest().catch(console.error);
