# Frontend deployment (Vercel)

This is a static HTML frontend. The repository now supports either Vercel project root:

## Recommended: project root is `frontend`

- **Git repository:** `DARTVADER1-DROID/BMS`
- **Root Directory:** `frontend`
- **Framework Preset:** Other
- **Build Command:** leave empty / disabled
- **Output Directory:** `.`
- **Install Command:** leave empty
- **Production branch:** `rewrite/c-backend` while testing this backend/frontend integration

Vercel reads `frontend/vercel.json`. The dashboard HTML files and `auth.js` are at the project root.

## Alternative: project root is the repository root

If the Vercel project is configured with the repository root as its Root Directory, Vercel reads the top-level `vercel.json`. That configuration maps the same dashboard paths to files under `/frontend`, including an explicit `/auth.js` rewrite.

Both configurations use filesystem-aware rewrites, so static files are served normally and the API proxy is applied only to `/api/*`.

## Routes and authentication

Both configurations map:

- `/` and `/volttrack` to the VoltTrack dashboard
- `/rul` to the RUL dashboard
- `/cycles` to the cycle counter
- `/api/*` to `https://bms-c-api.onrender.com/*`

The dashboards load `/auth.js` from the same origin and call the backend through `/api`. Do not configure a separate frontend API origin: the proxy keeps session cookies first-party and avoids cross-origin cookie restrictions.

## Deployment verification

After Vercel reports the deployment as Ready, check:

1. `/` — renders VoltTrack, not a Vercel placeholder.
2. `/volttrack`, `/rul`, and `/cycles` — each renders its dashboard.
3. `/auth.js` — returns JavaScript, not an HTML 404 page.
4. `/api/health` — returns backend health JSON.
5. `/api/auth/me` — returns HTTP 401 before login and HTTP 200 after login.

The browser should show the sign-in overlay when logged out. After a successful login, the session cookie should be sent automatically to same-origin `/api/*` requests.
