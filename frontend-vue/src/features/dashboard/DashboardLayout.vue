<script setup lang="ts">
import { onMounted, onUnmounted } from "vue";
import { RouterLink } from "vue-router";
import { useDashboardStore } from "./dashboard-store";
import { useSession } from "../../services/auth/session";
const dashboard = useDashboardStore();
const session = useSession();
onMounted(dashboard.startPolling);
onUnmounted(dashboard.stopPolling);
async function logout(): Promise<void> { await session.signOut(); }
</script>
<template>
  <div class="app-shell">
    <header class="topbar">
      <RouterLink to="/dashboard" class="brand">
        <span class="brand-mark">B</span>
        <span><strong>BATTERY SYSTEM</strong><small>MANAGEMENT CONSOLE</small></span>
      </RouterLink>
      <nav class="main-nav" aria-label="Main navigation">
        <RouterLink to="/dashboard">Overview</RouterLink>
        <RouterLink to="/diagnostics">Diagnostics</RouterLink>
        <RouterLink to="/cycles">Cycle counter</RouterLink>
      </nav>
      <div class="topbar-actions">
        <span class="user-label">{{ session.state.username }}</span>
        <button class="button button-quiet" type="button" @click="logout">Sign out</button>
      </div>
    </header>
    <main class="page-content"><RouterView /></main>
    <footer class="footer">
      <span>BMS · C backend</span>
      <span v-if="dashboard.state.lastUpdated">Updated {{ dashboard.state.lastUpdated.toLocaleTimeString() }}</span>
    </footer>
  </div>
</template>
