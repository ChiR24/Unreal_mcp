import axios, { AxiosInstance } from 'axios';

export function createHttpClient(baseURL: string): AxiosInstance {
  const client = axios.create({
    baseURL,
    headers: { 'Content-Type': 'application/json' },
    timeout: 15000,
  });
  return client;
}
