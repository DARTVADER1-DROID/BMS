<script setup lang="ts">
import { onMounted } from "vue";
import LoginView from "../components/LoginView.vue";
import { useSession } from "../services/auth/session";
const { state, checkSession } = useSession();
onMounted(() => void checkSession());
</script>
<template>
  <div v-if="state.checking" class="loading-screen">
    <div><div class="loading-mark">B</div><p>Connecting to BMS…</p></div>
  </div>
  <LoginView v-else-if="!state.authenticated" />
  <RouterView v-else />
</template>
