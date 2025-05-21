import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

export default defineConfig({
  plugins: [svelte()],
  server: {
    proxy: {
      '/api': {
        target: 'http://localhost:5000',
        changeOrigin: true
      },
      '/admin/api': {
        target: 'http://localhost:5000',
        changeOrigin: true
      },
      '/firmware': {
        target: 'http://localhost:5000',
        changeOrigin: true
      }
    }
  }
});