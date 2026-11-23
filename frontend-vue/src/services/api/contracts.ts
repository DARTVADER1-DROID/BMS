export interface HealthResponse {
  message: string;
  implementation: string;
  control_enabled: boolean;
}
export interface TelemetryResponse {
  voltage: number;
  current: number;
  temperature: number;
  time: number;
  hardware_connection: boolean;
}
export interface StateResponse {
  state: string;
  warning_messages?: string | string[] | Record<string, string>;
}
export interface HardwareResponse { hardware_connection: boolean; }
export interface SocResponse { s_o_c: number | null; }
export interface SohResponse { s_o_h: number | null; }
export interface RulResponse { r_u_l: number | null; }
export interface ControlResponse { message?: string; state?: string; }
export interface LoginResponse {
  authenticated: true;
  username: string;
  csrf_token: string;
}
export interface SessionResponse { username: string; }
