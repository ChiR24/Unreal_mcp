import { CallToolResult } from '@modelcontextprotocol/sdk/types.js';

export class ResponseFactory {
    static success(message: string, data?: unknown): CallToolResult {
        const content: any[] = [{ type: 'text', text: message }];

        if (data) {
            content.push({
                type: 'text',
                text: JSON.stringify(data, null, 2)
            });
        }

        return {
            content,
            isError: false
        };
    }

    static error(message: string, error?: unknown): CallToolResult {
        const errorText = error instanceof Error ? error.message : String(error);
        const fullMessage = error ? `${message}: ${errorText}` : message;

        return {
            content: [{ type: 'text', text: fullMessage }],
            isError: true
        };
    }

    static json(data: unknown): CallToolResult {
        return {
            content: [{
                type: 'text',
                text: JSON.stringify(data, null, 2)
            }],
            isError: false
        };
    }
}
