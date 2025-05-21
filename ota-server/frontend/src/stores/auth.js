import { writable } from 'svelte/store';

// Get API key from localStorage if available
const storedApiKey = localStorage.getItem('ota_api_key') || '';

// Create writable store
export const apiKey = writable(storedApiKey);

// Subscribe to changes and save to localStorage
apiKey.subscribe(value => {
  localStorage.setItem('ota_api_key', value);
});

// Helper function to get API headers
export function getAuthHeaders() {
  return {
    'X-Admin-API-Key': localStorage.getItem('ota_api_key') || ''
  };
}