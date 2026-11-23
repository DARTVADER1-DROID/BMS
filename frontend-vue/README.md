# BMS Vue frontend

The production frontend is a Vue 3 + TypeScript + Vite single-page application. The C backend remains separate and its HTTP API is unchanged.

## Local development

Requires Node.js 20 or newer.

```sh
cd frontend-vue
npm install
cp .env.example .env
npm run dev
```

Vite proxies requests from `/api/*` to the C API origin and strips the `/api` prefix. Override `VITE_API_PROXY_TARGET` for another backend.

## Quality checks

```sh
npm run typecheck
npm test
npm run build
npm run format:write
```

## Render Static Site

Update the existing Static Site service:

- Repository: `DARTVADER1-DROID/BMS`
- Branch: `rewrite/c-backend`
- Root directory: `frontend-vue`
- Build command: `npm install && npm run build`
- Publish directory: `dist`
- Environment: `VITE_API_BASE_URL=https://bms-c-api.onrender.com`

Add an SPA rewrite: source `/*`, destination `/index.html`, action `Rewrite`.

Set `BMS_CORS_ORIGIN` on the C backend to the exact deployed dashboard origin. Credentialed CORS must never use a wildcard origin. The current C backend already issues Secure session and CSRF cookies; the frontend sends credentials and the CSRF header for state-changing requests.

The previous standalone HTML dashboards remain in `frontend/` for rollback. Do not delete them until the new Render deployment and its workflows are verified.

## API compatibility notes

The app uses the existing endpoints: `/health`, `/auth/login`, `/auth/me`, `/auth/logout`, `/data`, `/state`, `/hardware_connection`, `/s_o_c`, `/s_o_h`, `/r_u_l`, `/charge`, `/discharge`, `/stop` and `/cycles?count=N`.

The backend exposes a cycle-count write endpoint but no read endpoint. The cycle screen therefore labels the displayed number as the last value submitted in the current browser session, not as a persisted server value.
