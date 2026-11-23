import { expect, test, type Page } from "@playwright/test";

const username = process.env.BMS_AUTH_USERNAME ?? "ci_test";
const password = process.env.BMS_AUTH_PASSWORD ?? "ci_test_password_123";
const deviceToken = process.env.BMS_DEVICE_TOKEN ?? "ci_test_device_token";

async function signIn(page: Page): Promise<void> {
  await page.goto("/");
  await expect(page.getByRole("heading", { name: "Welcome back" })).toBeVisible();
  await page.getByLabel("Username").fill(username);
  await page.getByLabel("Password").fill(password);
  await page.getByRole("button", { name: "Sign in" }).click();
  await expect(page.getByRole("heading", { name: "Battery overview" })).toBeVisible();
}

test.beforeEach(async ({ request }) => {
  const response = await request.post("/api/update", {
    headers: { Authorization: `Bearer ${deviceToken}` },
    data: {
      voltage: 3.7,
      current: 0.1,
      temperature: 25,
      time: Math.floor(Date.now() / 1000),
      hardware_connection: true,
    },
  });
  expect(response.status(), "test telemetry must be accepted by the real C backend").toBe(200);
});

test("protects dashboard routes and rejects invalid credentials", async ({ page }) => {
  await page.goto("/dashboard");
  await expect(page.getByRole("heading", { name: "Welcome back" })).toBeVisible();
  await page.getByLabel("Username").fill(username);
  await page.getByLabel("Password").fill("definitely-the-wrong-password");
  await page.getByRole("button", { name: "Sign in" }).click();
  await expect(page.getByRole("alert")).toContainText("invalid_credentials");
  await expect(page.getByRole("heading", { name: "Welcome back" })).toBeVisible();
});

test("authenticates and renders live telemetry from the C backend", async ({ page }) => {
  await signIn(page);
  await expect(page.getByText("API online")).toBeVisible();
  await expect(page.getByText("Hardware linked")).toBeVisible();
  await expect(page.getByText("3.700", { exact: true })).toBeVisible();
  await expect(page.getByText("0.100", { exact: true })).toBeVisible();
  await expect(page.getByText("25.00", { exact: true })).toBeVisible();
});

test("navigates between dashboard modules without losing the session", async ({ page }) => {
  await signIn(page);
  await page.getByRole("link", { name: "Diagnostics" }).click();
  await expect(page.getByRole("heading", { name: "Diagnostics" })).toBeVisible();
  await expect(page.getByText("Backend", { exact: true })).toBeVisible();

  await page.getByRole("link", { name: "Cycle counter" }).click();
  await expect(page.getByRole("heading", { name: "Cycle counter" })).toBeVisible();
  await page.getByLabel("New cycle count").fill("123");
  await page.getByRole("button", { name: "Update cycle count" }).click();
  await expect(page.getByRole("status")).toContainText("Cycle count updated successfully");
  await expect(page.locator(".cycle-number")).toHaveText("123");
});

test("keeps hardware control actions disabled until backend commissioning", async ({ page }) => {
  await signIn(page);
  await expect(page.getByText(/controls are disabled by the backend/i)).toBeVisible();
  await expect(page.getByRole("button", { name: "Start charging" })).toBeDisabled();
  await expect(page.getByRole("button", { name: "Start discharging" })).toBeDisabled();
  await expect(page.getByRole("button", { name: "Stop", exact: true })).toBeDisabled();
});

test("logout revokes the old session cookie and returns to the login screen", async ({ page, request }) => {
  await signIn(page);
  const cookies = await page.context().cookies();
  const sessionCookie = cookies.find((cookie) => cookie.name === "__Host-bms_session");
  const csrfCookie = cookies.find((cookie) => cookie.name === "__Host-bms_csrf");
  expect(sessionCookie?.value).toBeTruthy();
  expect(csrfCookie?.value).toBeTruthy();

  await page.getByRole("button", { name: "Sign out" }).click();
  await expect(page.getByRole("heading", { name: "Welcome back" })).toBeVisible();

  const replay = await request.get("/api/auth/me", {
    headers: {
      Cookie: `__Host-bms_session=${sessionCookie!.value}; __Host-bms_csrf=${csrfCookie!.value}`,
    },
  });
  expect(replay.status(), "a copied session must be rejected after logout").toBe(401);
});
