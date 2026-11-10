(() => {
  "use strict";

  const API_PREFIX = "/api";
  const originalFetch = window.fetch.bind(window);
  let overlay;
  let statusNode;
  let loginInProgress = false;

  function csrfToken() {
    const match = document.cookie.match(/(?:^|; )__Host-bms_csrf=([^;]+)/);
    return match ? decodeURIComponent(match[1]) : "";
  }

  function showLogin(message) {
    if (!overlay) return;
    overlay.hidden = false;
    if (statusNode) statusNode.textContent = message || "Sign in to access your battery dashboard.";
    const username = overlay.querySelector("#bms-auth-username");
    if (username) username.focus();
  }

  function hideLogin() {
    if (overlay) overlay.hidden = true;
  }

  function createInterface() {
    const style = document.createElement("style");
    style.textContent = `
      #bms-auth-overlay{position:fixed;inset:0;z-index:99999;display:flex;align-items:center;justify-content:center;padding:20px;background:rgba(0,0,0,.88);backdrop-filter:blur(14px)}
      #bms-auth-overlay[hidden]{display:none}
      #bms-auth-card{width:min(100%,390px);padding:28px;border:1px solid rgba(110,180,150,.35);border-radius:18px;background:#101a17;color:#e9fff5;font:14px system-ui,sans-serif;box-shadow:0 24px 90px #000}
      #bms-auth-card h2{margin:0 0 8px;font-size:22px}
      #bms-auth-card p{margin:0 0 18px;color:#a7c6b8;line-height:1.5}
      #bms-auth-card label{display:block;margin:12px 0 6px;color:#b9d8ca;font-size:12px}
      #bms-auth-card input{width:100%;padding:12px;border:1px solid #34594a;border-radius:9px;background:#07110d;color:#fff;font:inherit}
      #bms-auth-card button{width:100%;margin-top:18px;padding:12px;border:0;border-radius:9px;background:#3e9b72;color:#fff;font-weight:700;font-size:14px}
      #bms-auth-card button:disabled{opacity:.55}
      #bms-auth-status{min-height:20px;margin-top:12px;color:#f4c2a8;font-size:12px}
      #bms-auth-logout{position:fixed;right:14px;bottom:14px;z-index:9998;padding:9px 13px;border:1px solid #678;border-radius:999px;background:#101a17;color:#e9fff5;font:12px system-ui,sans-serif}
    `;
    document.head.appendChild(style);

    overlay = document.createElement("div");
    overlay.id = "bms-auth-overlay";
    overlay.hidden = true;
    overlay.innerHTML = `
      <form id="bms-auth-card" autocomplete="on">
        <h2>Battery system sign in</h2>
        <p>Authenticate to view and operate the battery dashboard.</p>
        <label for="bms-auth-username">Username</label>
        <input id="bms-auth-username" name="username" autocomplete="username" required maxlength="128"/>
        <label for="bms-auth-password">Password</label>
        <input id="bms-auth-password" name="password" type="password" autocomplete="current-password" required/>
        <button type="submit">Sign in</button>
        <div id="bms-auth-status" role="status"></div>
      </form>`;
    document.body.appendChild(overlay);
    statusNode = overlay.querySelector("#bms-auth-status");

    overlay.querySelector("form").addEventListener("submit", async event => {
      event.preventDefault();
      if (loginInProgress) return;
      loginInProgress = true;
      const form = event.currentTarget;
      const button = form.querySelector("button");
      button.disabled = true;
      statusNode.textContent = "Signing in…";
      try {
        const response = await originalFetch(API_PREFIX + "/auth/login", {
          method: "POST",
          credentials: "include",
          headers: {"Content-Type": "application/json"},
          body: JSON.stringify({
            username: form.elements.username.value,
            password: form.elements.password.value
          })
        });
        if (!response.ok) throw new Error("Invalid username or password.");
        form.elements.password.value = "";
        hideLogin();
        window.location.reload();
      } catch (error) {
        statusNode.textContent = error.message || "Unable to sign in.";
      } finally {
        loginInProgress = false;
        button.disabled = false;
      }
    });

    const logout = document.createElement("button");
    logout.id = "bms-auth-logout";
    logout.type = "button";
    logout.textContent = "Sign out";
    logout.addEventListener("click", async () => {
      try {
        await originalFetch(API_PREFIX + "/auth/logout", {
          method: "POST",
          credentials: "include",
          headers: {"X-CSRF-Token": csrfToken()}
        });
      } finally {
        showLogin("You have signed out.");
      }
    });
    document.body.appendChild(logout);
  }

  window.fetch = async (input, init = {}) => {
    const rawUrl = typeof input === "string" ? input : input.url;
    const url = new URL(rawUrl, window.location.href);
    if (url.origin !== window.location.origin || !url.pathname.startsWith(API_PREFIX + "/")) {
      return originalFetch(input, init);
    }

    const method = (init.method || (input instanceof Request ? input.method : "GET")).toUpperCase();
    const headers = new Headers(init.headers || (input instanceof Request ? input.headers : undefined));
    const options = {...init, method, headers, credentials: "include"};

    if (!["GET", "HEAD", "OPTIONS"].includes(method)
        && !url.pathname.endsWith("/auth/login")) {
      const token = csrfToken();
      if (token) headers.set("X-CSRF-Token", token);
    }

    const response = await originalFetch(input, options);
    if (response.status === 401 && !url.pathname.endsWith("/auth/login")) {
      showLogin("Your session has expired. Please sign in again.");
    }
    return response;
  };

  window.BMSAuth = {
    showLogin,
    async check() {
      try {
        const response = await originalFetch(API_PREFIX + "/auth/me", {credentials: "include"});
        if (!response.ok) showLogin();
      } catch {
        showLogin("Authentication service is unavailable. Try again shortly.");
      }
    }
  };

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", () => {
      createInterface();
      window.BMSAuth.check();
    }, {once: true});
  } else {
    createInterface();
    window.BMSAuth.check();
  }
})();
