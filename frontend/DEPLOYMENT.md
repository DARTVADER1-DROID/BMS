# Frontend deployment (Vercel)

This is a static HTML frontend. Configure the Vercel project as follows:

- **Git repository:** `DARTVADER1-DROID/BMS`
- **Production branch:** `rewrite/c-backend` (until the frontend is intentionally promoted)
- **Root Directory:** `frontend`
- **Framework Preset:** Other
- **Build Command:** leave empty / disabled
- **Output Directory:** `.`
- **Install Command:** leave empty

The `vercel.json` in this directory assumes that `frontend` is the project root. It maps:

- `/` and `/volttrack` to the VoltTrack dashboard
- `/rul` to the RUL dashboard
- `/cycles` to the cycle counter
- `/api/*` to the C backend at `https://bms-c-api.onrender.com/*`

The dashboards load `/auth.js` from this same root and call the API through the same-origin `/api` prefix. Do not configure a separate frontend API origin: the proxy keeps the session cookies first-party and avoids cross-origin cookie restrictions.

## Deployment verification

After Vercel reports the deployment as Ready, check these URLs:

1. `/` — should render VoltTrack, not a Vercel placeholder.
2. `/volttrack`, `/rul`, and `/cycles` — each should render its own dashboard.
3. `/auth.js` — should return JavaScript, not an HTML 404 page.
4. `/api/health` — should return the backend health JSON.
5. `/api/auth/me` — should return HTTP 401 before login and HTTP 200 after login.

The browser should show the sign-in overlay when logged out. After a successful login, the session cookie should be sent automatically to same-origin `/api/*` requests.
