import { describe, expect, it, vi } from "vitest";
import { ApiError, createApiClient, readCsrfCookie } from "../src/services/api/client";

describe("API client", () => {
  it("reads only the CSRF cookie and safely decodes it", () => {
    expect(readCsrfCookie("other=x; __Host-bms_csrf=abc%2F123")).toBe("abc/123");
    expect(readCsrfCookie("bms_csrf=wrong")).toBe("");
    expect(readCsrfCookie("__Host-bms_csrf=%E0%A4%A")).toBe("");
  });
  it("sends credentials and the CSRF header for state-changing requests", async () => {
    vi.stubGlobal("document", { cookie: "__Host-bms_csrf=test-token" });
    const fetchMock = vi.fn().mockResolvedValue(new Response('{"message":"accepted"}', { status: 200 }));
    const client = createApiClient("https://api.example.test/", fetchMock as unknown as typeof fetch);
    await client.command("stop");
    const [url, options] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(url).toBe("https://api.example.test/stop");
    expect(options.credentials).toBe("include");
    expect(new Headers(options.headers).get("X-CSRF-Token")).toBe("test-token");
    vi.unstubAllGlobals();
  });
  it("does not add CSRF to login requests", async () => {
    const fetchMock = vi.fn().mockResolvedValue(new Response('{"authenticated":true}', { status: 200 }));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);
    await client.login("operator", "secret");
    const options = fetchMock.mock.calls[0][1] as RequestInit;
    expect(new Headers(options.headers).has("X-CSRF-Token")).toBe(false);
  });
  it("converts HTTP failures into typed errors", async () => {
    const fetchMock = vi.fn().mockResolvedValue(new Response('{"error":"unauthenticated"}', { status: 401 }));
    const client = createApiClient("/api", fetchMock as unknown as typeof fetch);
    await expect(client.telemetry()).rejects.toMatchObject({
      name: "ApiError", status: 401, message: "unauthenticated",
    } satisfies Partial<ApiError>);
  });
});
