import type {
  ControlResponse, HardwareResponse, HealthResponse, LoginResponse,
  RulResponse, SessionResponse, SocResponse, SohResponse, StateResponse,
  TelemetryResponse,
} from "./contracts";

export class ApiError extends Error {
  constructor(message: string, public readonly status: number) {
    super(message);
    this.name = "ApiError";
  }
}

export function readCsrfCookie(cookieString: string): string {
  const entry = cookieString.split(";").map((part) => part.trim())
    .find((part) => part.startsWith("__Host-bms_csrf="));
  if (!entry) return "";
  try {
    return decodeURIComponent(entry.slice("__Host-bms_csrf=".length));
  } catch {
    return "";
  }
}

export function createApiClient(baseUrl: string, fetchImplementation: typeof fetch = fetch) {
  const base = baseUrl.replace(/\/$/, "");

  async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
    const method = (options.method || "GET").toUpperCase();
    const headers = new Headers(options.headers);
    const isLogin = path === "/auth/login";
    if (options.body && !headers.has("Content-Type")) {
      headers.set("Content-Type", "application/json");
    }
    if (!["GET", "HEAD", "OPTIONS"].includes(method) && !isLogin) {
      const csrf = typeof document === "undefined" ? "" : readCsrfCookie(document.cookie);
      if (csrf) headers.set("X-CSRF-Token", csrf);
    }

    const response = await fetchImplementation(base + path, {
      ...options, method, headers, credentials: "include",
    });
    if (response.status === 401 && !isLogin && typeof window !== "undefined") {
      window.dispatchEvent(new Event("bms:unauthorized"));
    }
    if (!response.ok) {
      let message = `Request failed (HTTP ${response.status})`;
      try {
        const payload = await response.json() as { error?: string; message?: string };
        message = payload.message || payload.error || message;
      } catch { /* Non-JSON error response. */ }
      throw new ApiError(message, response.status);
    }
    if (response.status === 204) return undefined as T;
    return await response.json() as T;
  }

  return {
    health: () => request<HealthResponse>("/health"),
    login: (username: string, password: string) => request<LoginResponse>("/auth/login", {
      method: "POST", body: JSON.stringify({ username, password }),
    }),
    session: () => request<SessionResponse>("/auth/me"),
    logout: () => request<{ authenticated: false }>("/auth/logout", { method: "POST" }),
    telemetry: () => request<TelemetryResponse>("/data"),
    state: () => request<StateResponse>("/state"),
    hardware: () => request<HardwareResponse>("/hardware_connection"),
    soc: () => request<SocResponse>("/s_o_c"),
    soh: () => request<SohResponse>("/s_o_h"),
    rul: () => request<RulResponse>("/r_u_l"),
    command: (name: "charge" | "discharge" | "stop") =>
      request<ControlResponse>(`/${name}`, { method: "POST" }),
    updateCycles: (count: number) =>
      request<ControlResponse>(`/cycles?count=${count}`, { method: "POST" }),
  };
}

const configuredBaseUrl = import.meta.env.VITE_API_BASE_URL?.trim();
export const api = createApiClient(configuredBaseUrl || "/api");
