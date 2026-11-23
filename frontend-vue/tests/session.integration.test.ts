import { afterEach, describe, expect, it, vi } from "vitest";

const api = vi.hoisted(() => ({
  login: vi.fn(), session: vi.fn(), logout: vi.fn(),
}));

vi.mock("../src/services/api/client", () => ({
  api,
  ApiError: class ApiError extends Error {
    constructor(message: string, public readonly status: number) { super(message); }
  },
}));

async function loadSession() {
  vi.resetModules();
  const module = await import("../src/services/auth/session");
  return module.useSession();
}

describe("session service integration", () => {
  afterEach(() => {
    api.login.mockReset();
    api.session.mockReset();
    api.logout.mockReset();
    vi.unstubAllGlobals();
  });

  it("restores an existing session and exposes its username", async () => {
    api.session.mockResolvedValue({ username: "operator" });
    const session = await loadSession();

    await session.checkSession();

    expect(session.state.authenticated).toBe(true);
    expect(session.state.username).toBe("operator");
    expect(session.state.checking).toBe(false);
    expect(session.state.error).toBe("");
  });

  it("treats an unauthenticated session as signed out without a service error", async () => {
    const ApiError = (await import("../src/services/api/client")).ApiError;
    api.session.mockRejectedValue(new ApiError("unauthenticated", 401));
    const session = await loadSession();

    await session.checkSession();

    expect(session.state.authenticated).toBe(false);
    expect(session.state.username).toBe("");
    expect(session.state.error).toBe("");
  });

  it("reports service outages distinctly from invalid sessions", async () => {
    api.session.mockRejectedValue(new TypeError("network down"));
    const session = await loadSession();

    await session.checkSession();

    expect(session.state.authenticated).toBe(false);
    expect(session.state.error).toContain("Authentication service is unavailable");
  });

  it("trims usernames before login and stores the returned identity", async () => {
    api.login.mockResolvedValue({ authenticated: true, username: "operator" });
    const session = await loadSession();

    await session.signIn("  operator  ", "password");

    expect(api.login).toHaveBeenCalledWith("operator", "password");
    expect(session.state.authenticated).toBe(true);
    expect(session.state.username).toBe("operator");
  });

  it("does not mark a user authenticated when login fails", async () => {
    api.login.mockRejectedValue(new Error("invalid credentials"));
    const session = await loadSession();

    await expect(session.signIn("operator", "wrong")).rejects.toThrow("invalid credentials");
    expect(session.state.authenticated).toBe(false);
    expect(session.state.username).toBe("");
  });

  it("clears local identity even when the logout request fails", async () => {
    api.session.mockResolvedValue({ username: "operator" });
    const session = await loadSession();
    await session.checkSession();
    api.logout.mockRejectedValue(new Error("network down"));

    await expect(session.signOut()).rejects.toThrow("network down");
    expect(session.state.authenticated).toBe(false);
    expect(session.state.username).toBe("");
  });

  it("clears the session when the global unauthorized event is received", async () => {
    const listeners = new Map<string, (event: Event) => void>();
    vi.stubGlobal("window", {
      addEventListener: (name: string, callback: (event: Event) => void) => listeners.set(name, callback),
    });
    api.session.mockResolvedValue({ username: "operator" });
    const session = await loadSession();
    await session.checkSession();

    listeners.get("bms:unauthorized")?.(new Event("bms:unauthorized"));

    expect(session.state.authenticated).toBe(false);
    expect(session.state.username).toBe("");
    expect(session.state.error).toContain("session expired");
  });
});
