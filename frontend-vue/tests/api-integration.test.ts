import { afterEach, describe, expect, it, vi } from "vitest";
import { ApiError, createApiClient } from "../src/services/api/client";

function jsonResponse(payload: unknown, status = 200): Response {
  return new Response(JSON.stringify(payload), {
    status,
    headers: { "Content-Type": "application/json" },
  });
}

describe("API client integration contract", () => {
  afterEach(() => vi.unstubAllGlobals());

  it("maps every read operation to the expected backend route", async () => {
    const fetchMock = vi.fn().mockImplementation(() => Promise.resolve(jsonResponse({})));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await client.health();
    await client.session();
    await client.telemetry();
    await client.state();
    await client.hardware();
    await client.soc();
    await client.soh();
    await client.rul();

    expect(fetchMock.mock.calls.map(([url]) => url)).toEqual([
      "/api/health", "/api/auth/me", "/api/data", "/api/state",
      "/api/hardware_connection", "/api/s_o_c", "/api/s_o_h", "/api/r_u_l",
    ]);
    for (const [, options] of fetchMock.mock.calls) {
      expect((options as RequestInit).method).toBe("GET");
      expect((options as RequestInit).credentials).toBe("include");
    }
  });

  it.each([
    ["charge", "/api/charge"],
    ["discharge", "/api/discharge"],
    ["stop", "/api/stop"],
  ] as const)("sends %s as a POST command", async (command, expectedUrl) => {
    const fetchMock = vi.fn().mockResolvedValue(jsonResponse({ message: "accepted" }));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await client.command(command);

    const [url, options] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(url).toBe(expectedUrl);
    expect(options.method).toBe("POST");
    expect(options.body).toBeUndefined();
  });

  it("preserves zero when updating the cycle counter", async () => {
    const fetchMock = vi.fn().mockResolvedValue(jsonResponse({ message: "updated" }));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await client.updateCycles(0);

    expect(fetchMock.mock.calls[0][0]).toBe("/api/cycles?count=0");
    expect((fetchMock.mock.calls[0][1] as RequestInit).method).toBe("POST");
  });

  it("sends login credentials as JSON and always includes cookies", async () => {
    const fetchMock = vi.fn().mockResolvedValue(jsonResponse({
      authenticated: true, username: "operator", csrf_token: "csrf",
    }));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await client.login("operator", "correct horse");

    const [, options] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(options.method).toBe("POST");
    expect(options.credentials).toBe("include");
    expect(new Headers(options.headers).get("Content-Type")).toBe("application/json");
    expect(JSON.parse(String(options.body))).toEqual({
      username: "operator", password: "correct horse",
    });
  });

  it("sends logout as a credentialed state-changing request", async () => {
    vi.stubGlobal("document", { cookie: "__Host-bms_csrf=logout-token" });
    const fetchMock = vi.fn().mockResolvedValue(jsonResponse({ authenticated: false }));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await client.logout();

    const [, options] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(options.method).toBe("POST");
    expect(new Headers(options.headers).get("X-CSRF-Token")).toBe("logout-token");
  });

  it("dispatches the unauthorized event for expired protected sessions", async () => {
    const dispatchEvent = vi.fn();
    vi.stubGlobal("window", { dispatchEvent });
    const fetchMock = vi.fn().mockResolvedValue(jsonResponse({ error: "unauthenticated" }, 401));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await expect(client.telemetry()).rejects.toBeInstanceOf(ApiError);
    expect(dispatchEvent).toHaveBeenCalledTimes(1);
    expect(dispatchEvent.mock.calls[0][0].type).toBe("bms:unauthorized");
  });

  it("does not dispatch unauthorized for the login endpoint", async () => {
    const dispatchEvent = vi.fn();
    vi.stubGlobal("window", { dispatchEvent });
    const fetchMock = vi.fn().mockResolvedValue(jsonResponse({ error: "invalid credentials" }, 401));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await expect(client.login("bad", "bad")).rejects.toMatchObject({ status: 401 });
    expect(dispatchEvent).not.toHaveBeenCalled();
  });

  it("supports empty 204 responses without attempting JSON parsing", async () => {
    const fetchMock = vi.fn().mockResolvedValue(new Response(null, { status: 204 }));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await expect(client.logout()).resolves.toBeUndefined();
  });

  it("uses a safe fallback for non-JSON error bodies", async () => {
    const fetchMock = vi.fn().mockResolvedValue(new Response("proxy failure", { status: 502 }));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await expect(client.health()).rejects.toMatchObject({
      name: "ApiError", status: 502, message: "Request failed (HTTP 502)",
    });
  });

  it("propagates network failures without disguising them as HTTP errors", async () => {
    const fetchMock = vi.fn().mockRejectedValue(new TypeError("Failed to fetch"));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);

    await expect(client.health()).rejects.toThrow("Failed to fetch");
  });
});
