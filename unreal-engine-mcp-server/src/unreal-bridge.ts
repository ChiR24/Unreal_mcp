import WebSocket from 'ws';
import { createHttpClient } from './utils/http.js';
import { Logger } from './utils/logger.js';
import { loadEnv } from './types/env.js';

interface RcMessage {
  MessageName: string;
  Parameters?: any;
}

interface RcCallBody {
  objectPath: string; // e.g. "/Script/UnrealEd.Default__EditorAssetLibrary"
  functionName: string; // e.g. "ListAssets"
  parameters?: Record<string, any>;
}

export class UnrealBridge {
  private ws?: WebSocket;
  private http = createHttpClient('');
  private env = loadEnv();
  private log = new Logger('UnrealBridge');
  private connected = false;
  private seq = 0;
  private pending = new Map<number, (data: any) => void>();

  get isConnected() { return this.connected; }

  async connect(): Promise<void> {
    const wsUrl = `ws://${this.env.UE_HOST}:${this.env.UE_RC_WS_PORT}`;
    const httpBase = `http://${this.env.UE_HOST}:${this.env.UE_RC_HTTP_PORT}`;
    this.http = createHttpClient(httpBase);

    this.log.info(`Connecting to UE Remote Control: ${wsUrl}`);
    this.ws = new WebSocket(wsUrl);

    await new Promise<void>((resolve, reject) => {
      if (!this.ws) return reject(new Error('WS not created'));
      this.ws.on('open', () => {
        this.connected = true;
        this.log.info('Connected to Unreal Remote Control');
        resolve();
      });
      this.ws.on('error', (err) => {
        this.log.error('WebSocket error', err);
      });
      this.ws.on('close', () => {
        this.connected = false;
        this.log.warn('WebSocket closed');
      });
      this.ws.on('message', (raw: WebSocket.RawData) => {
        try {
          const msg = JSON.parse(String(raw));
          // Route reply messages with RequestId or internal mapping
          if (msg?.RequestId && this.pending.has(msg.RequestId)) {
            const cb = this.pending.get(msg.RequestId)!;
            this.pending.delete(msg.RequestId);
            cb(msg);
          } else {
            this.log.debug('WS message', msg);
          }
        } catch (e) {
          this.log.error('Failed parsing WS message', e);
        }
      });
    });
  }

  private sendWs<T = any>(message: RcMessage & { RequestId?: number }): Promise<T> {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      throw new Error('WebSocket not connected');
    }
    const id = ++this.seq;
    const msg = { ...message, RequestId: id };
    return new Promise<T>((resolve, reject) => {
      this.pending.set(id, (resp) => {
        if (resp?.Error) return reject(new Error(resp.Error));
        resolve(resp as T);
      });
      this.ws!.send(JSON.stringify(msg), (err) => {
        if (err) reject(err);
      });
    });
  }

  async httpCall<T = any>(path: string, method: 'GET' | 'POST' | 'PUT' = 'POST', body?: any): Promise<T> {
    const url = path.startsWith('/') ? path : `/${path}`;
    const resp = await this.http.request<T>({ url, method, data: body });
    return resp.data;
  }

  // Generic function call via Remote Control HTTP API
  async call(body: RcCallBody): Promise<any> {
    // Using HTTP endpoint /remote/object/call
    const result = await this.httpCall<any>('/remote/object/call', 'PUT', body);
    return result;
  }

  async getExposed(): Promise<any> {
    return this.httpCall('/remote/preset', 'GET');
  }
}
