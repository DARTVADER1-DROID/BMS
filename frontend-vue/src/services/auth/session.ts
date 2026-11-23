import { reactive, readonly } from "vue";
import { ApiError, api } from "../api/client";

interface SessionState {
  checking: boolean;
  authenticated: boolean;
  username: string;
  error: string;
}
const state = reactive<SessionState>({
  checking: true, authenticated: false, username: "", error: "",
});

if (typeof window !== "undefined") {
  window.addEventListener("bms:unauthorized", () => {
    state.authenticated = false;
    state.username = "";
    state.error = "Your session expired. Please sign in again.";
  });
}

async function checkSession(): Promise<void> {
  state.checking = true;
  state.error = "";
  try {
    const session = await api.session();
    state.authenticated = true;
    state.username = session.username;
  } catch (error) {
    state.authenticated = false;
    state.username = "";
    if (!(error instanceof ApiError && error.status === 401)) {
      state.error = "Authentication service is unavailable. Check the API connection.";
    }
  } finally {
    state.checking = false;
  }
}

async function signIn(username: string, password: string): Promise<void> {
  state.error = "";
  const response = await api.login(username.trim(), password);
  state.authenticated = response.authenticated;
  state.username = response.username;
}

async function signOut(): Promise<void> {
  try { await api.logout(); }
  finally {
    state.authenticated = false;
    state.username = "";
  }
}

export function useSession() {
  return { state: readonly(state), checkSession, signIn, signOut };
}
