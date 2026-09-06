export interface BaseToolResponse {
  success: boolean;
  message?: string;
  error?: string;
  warning?: string;
  retriable?: boolean;
  scope?: string;
}
