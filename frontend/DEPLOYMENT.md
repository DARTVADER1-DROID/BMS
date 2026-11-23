# BMS frontend deployment on Render

The replacement frontend is the Vue 3 + TypeScript + Vite application in `frontend-vue/`. The old static HTML dashboards remain in `frontend/` as a rollback copy and are no longer the intended production build.

## Update the existing Render Static Site

- Repository: `DARTVADER1-DROID/BMS`
- Branch: `rewrite/c-backend`
- Root directory: `frontend-vue`
- Build command: `npm install && npm run build`
- Publish directory: `dist`
- Environment variable: `VITE_API_BASE_URL=https://bms-c-api.onrender.com`

Add a Render rewrite rule for client-side routing:

- Source: `/*`
- Destination: `/index.html`
- Action: `Rewrite`

## API and authentication

The browser calls the C API at `https://bms-c-api.onrender.com`. Set `BMS_CORS_ORIGIN` on the backend to the exact deployed dashboard origin, for example `https://bms-dashboard-xvti.onrender.com`.

The API uses credentialed CORS and Secure session/CSRF cookies. The Vue client sends credentials with API calls and reads the CSRF cookie to add the required header to state-changing requests. Never use a wildcard CORS origin with credentialed requests.

## Post-deployment verification

1. Open the Static Site root and confirm the Vue application loads.
2. Confirm unauthenticated users are presented with the sign-in screen.
3. Sign in and verify telemetry, hardware status and estimates load.
4. Verify the charge/discharge/stop controls remain disabled while the backend reports control as uncommissioned.
5. Verify cycle count updates and sign out.
6. Open `/diagnostics` and `/cycles` directly to confirm SPA rewrites work.

The C backend and its API paths are unchanged. See `frontend-vue/README.md` for local development and test commands.
