# BMS frontend deployment on Render

The frontend is a static HTML application hosted as a Render Static Site.

- **Render service:** `bms-dashboard`
- **Repository:** `DARTVADER1-DROID/BMS`
- **Branch:** `rewrite/c-backend`
- **Publish directory:** `frontend`
- **Build command:** `echo 'Publishing static BMS dashboards'`
- **Public URL:** `https://bms-dashboard-xvti.onrender.com`

## Dashboards

- `/` redirects to VoltTrack.
- `/volttrack/` — VoltTrack.
- `/rul/` — Remaining useful life.
- `/cycles/` — Cycle analysis.

## API and authentication

The browser calls the C API at `https://bms-c-api.onrender.com`. The backend must have `BMS_CORS_ORIGIN=https://bms-dashboard-xvti.onrender.com`.

The API uses credentialed CORS and Secure, HttpOnly session cookies with `SameSite=None`. The login response returns a CSRF token which the frontend retains for authenticated state-changing requests. Never use a wildcard CORS origin with credentialed requests.

## Verify after deploy

1. Open the public site and confirm the dashboard renders.
2. Confirm `/auth.js` loads.
3. Confirm the browser displays the sign-in form when logged out.
4. Sign in and verify telemetry requests succeed.
5. Sign out and confirm the next protected request requires login.

Vercel deployment configuration has been removed from this repository.
