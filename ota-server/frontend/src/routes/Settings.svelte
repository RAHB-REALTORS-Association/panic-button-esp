<script>
  import { onMount } from 'svelte';
  import Card from '../lib/Card.svelte';
  import Button from '../lib/Button.svelte';
  import { apiKey } from '../stores/auth';
  
  let currentApiKey = $state('');
  let saving = $state(false);
  let showApiKey = $state(false);
  
  onMount(() => {
    currentApiKey = $apiKey;
  });
  
  function saveApiKey() {
    saving = true;
    
    // Update the store
    apiKey.set(currentApiKey);
    
    // Simulate saving delay
    setTimeout(() => {
      saving = false;
    }, 500);
  }
</script>

<div class="mt-6">
  <h2 class="my-6 text-2xl font-semibold text-gray-700 dark:text-gray-200">
    Settings
  </h2>
  
  <Card title="API Authentication">
    <form class="space-y-4" on:submit|preventDefault={saveApiKey}>
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="api-key">
          Admin API Key
        </label>
        <div class="relative">
          <input
            id="api-key"
            type={showApiKey ? 'text' : 'password'}
            class="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-purple-500 focus:border-purple-500 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
            bind:value={currentApiKey}
            placeholder="Enter your admin API key"
          />
          <button 
            type="button"
            class="absolute inset-y-0 right-0 pr-3 flex items-center text-gray-500 dark:text-gray-400"
            on:click={() => showApiKey = !showApiKey}
          >
            <i class={`fas ${showApiKey ? 'fa-eye-slash' : 'fa-eye'}`}></i>
          </button>
        </div>
        <p class="mt-1 text-sm text-gray-500 dark:text-gray-400">
          The API key is used to authenticate with the OTA Update Server admin API.
        </p>
      </div>
      
      <div>
        <Button 
          type="submit" 
          color="primary" 
          icon="fas fa-save" 
          loading={saving}
          disabled={!currentApiKey}
        >
          Save API Key
        </Button>
      </div>
    </form>
  </Card>
  
  <Card title="Server Information">
    <div class="space-y-4">
      <div>
        <h4 class="text-sm font-medium text-gray-700 dark:text-gray-400 mb-2">
          API Endpoints
        </h4>
        <div class="bg-gray-100 dark:bg-gray-700 p-3 rounded-md">
          <code class="text-sm font-mono">
            GET /api/firmware - Check for firmware updates<br>
            GET /firmware/&lt;filename&gt; - Download firmware<br>
            GET /status - Server status<br>
            <br>
            Admin API (requires API key):<br>
            GET /admin/api/devices - List all devices<br>
            POST /admin/api/devices - Add a new device<br>
            GET /admin/api/devices/:mac - Get device info<br>
            PUT /admin/api/devices/:mac - Update device<br>
            DELETE /admin/api/devices/:mac - Delete device<br>
            <br>
            GET /admin/api/firmware - List firmware files<br>
            POST /admin/api/firmware - Upload firmware<br>
            DELETE /admin/api/firmware/:filename - Delete firmware
          </code>
        </div>
      </div>
      
      <div>
        <h4 class="text-sm font-medium text-gray-700 dark:text-gray-400 mb-2">
          Documentation
        </h4>
        <div class="flex space-x-2">
          <a 
            href="/admin/docs" 
            target="_blank" 
            rel="noopener noreferrer" 
            class="text-purple-600 hover:text-purple-800 dark:text-purple-400 dark:hover:text-purple-300"
          >
            <i class="fas fa-file-alt mr-1"></i>
            API Documentation
          </a>
          <span class="text-gray-500">|</span>
          <a 
            href="https://github.com/RAHB-REALTORS-Association/panic-button-esp/tree/master/ota-server" 
            target="_blank" 
            rel="noopener noreferrer" 
            class="text-purple-600 hover:text-purple-800 dark:text-purple-400 dark:hover:text-purple-300"
          >
            <i class="fab fa-github mr-1"></i>
            GitHub Repository
          </a>
        </div>
      </div>
    </div>
  </Card>
</div>