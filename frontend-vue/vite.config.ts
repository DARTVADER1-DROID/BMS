import { defineConfig } from "vitest/config";
import { loadEnv } from "vite";
import vue from "@vitejs/plugin-vue";

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), "");
  const apiTarget = env.VITE_API_PROXY_TARGET || "https://bms-c-api.onrender.com";
  return {
    plugins: [vue()],
    server: {
      proxy: {
        "/api": {
          target: apiTarget,
          changeOrigin: true,
          secure: true,
          rewrite: (path) => path.replace(/^\/api/, ""),
        },
      },
    },
    build: { target: "es2020", sourcemap: true, chunkSizeWarningLimit: 500 },
    test: { environment: "node", include: ["tests/**/*.test.ts"] },
  };
});
