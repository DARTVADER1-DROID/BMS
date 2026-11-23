import { createRouter, createWebHistory } from "vue-router";
import DashboardLayout from "../features/dashboard/DashboardLayout.vue";
import DashboardView from "../features/dashboard/DashboardView.vue";
import DiagnosticsView from "../features/diagnostics/DiagnosticsView.vue";
import CyclesView from "../features/cycles/CyclesView.vue";

export const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: "/",
      component: DashboardLayout,
      children: [
        { path: "", redirect: "/dashboard" },
        { path: "dashboard", name: "dashboard", component: DashboardView },
        { path: "diagnostics", name: "diagnostics", component: DiagnosticsView },
        { path: "cycles", name: "cycles", component: CyclesView },
      ],
    },
    { path: "/:pathMatch(.*)*", redirect: "/dashboard" },
  ],
});
