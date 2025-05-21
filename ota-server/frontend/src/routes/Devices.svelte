<script>
  import { onMount } from 'svelte';
  import { push, location } from 'svelte-spa-router';
  import Card from '../lib/Card.svelte';
  import Button from '../lib/Button.svelte';
  import { fetchDevices, devices, loading, error, deleteDevice } from '../stores/devices';
  import { apiKey } from '../stores/auth';
  
  let confirmingDelete = $state(null);
  let searchTerm = $state('');
  let filteredDevices = $state([]);
  
  $effect(() => {
    if (searchTerm) {
      filteredDevices = $devices.filter(device => 
        device.mac.toLowerCase().includes(searchTerm.toLowerCase()) ||
        (device.device_id && device.device_id.toLowerCase().includes(searchTerm.toLowerCase()))
      );
    } else {
      filteredDevices = [...$devices];
    }
  });
  
  onMount(() => {
    if ($apiKey) {
      fetchDevices();
    }
  });
  
  function confirmDelete(mac) {
    confirmingDelete = mac;
  }
  
  async function performDelete(mac) {
    try {
      await deleteDevice(mac);
      confirmingDelete = null;
    } catch (error) {
      console.error('Error deleting device:', error);
    }
  }
  
  function cancelDelete() {
    confirmingDelete = null;
  }
  
  function editDevice(mac) {
    push(`/devices/edit/${mac}`);
  }
</script>

<div class="mt-6">
  <div class="flex items-center justify-between mb-6">
    <h2 class="text-2xl font-semibold text-gray-700 dark:text-gray-200">
      Devices
    </h2>
    <Button color="primary" icon="fas fa-plus" on:click={() => push('/devices/new')}>
      Add Device
    </Button>
  </div>
  
  {#if !$apiKey}
    <Card>
      <div class="text-center py-4">
        <i class="fas fa-lock text-4xl text-gray-400 mb-3"></i>
        <p class="text-gray-600 dark:text-gray-400 mb-4">
          Please enter your API key in the settings to manage devices.
        </p>
        <Button color="primary" icon="fas fa-cog" on:click={() => push('/settings')}>
          Go to Settings
        </Button>
      </div>
    </Card>
  {:else}
    {#if $error}
      <div class="bg-red-100 border-l-4 border-red-500 text-red-700 p-4 mb-6" role="alert">
        <p class="font-bold">Error</p>
        <p>{$error}</p>
      </div>
    {/if}
    
    <Card>
      <div class="mb-4">
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="search">
          Search Devices
        </label>
        <input
          id="search"
          type="text"
          class="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-purple-500 focus:border-purple-500 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
          placeholder="Search by MAC address or device ID"
          bind:value={searchTerm}
        />
      </div>
      
      {#if $loading}
        <div class="text-center py-8">
          <div class="animate-spin inline-block w-6 h-6 border-2 border-current border-t-transparent text-purple-600 rounded-full" role="status">
            <span class="sr-only">Loading...</span>
          </div>
          <p class="mt-2 text-gray-600 dark:text-gray-400">Loading devices...</p>
        </div>
      {:else if filteredDevices.length === 0}
        <div class="text-center py-8">
          <i class="fas fa-microchip text-4xl text-gray-400 mb-3"></i>
          <p class="text-gray-600 dark:text-gray-400 mb-4">
            {searchTerm ? 'No devices found matching your search.' : 'No devices registered yet.'}
          </p>
          {#if !searchTerm}
            <Button color="primary" icon="fas fa-plus" on:click={() => push('/devices/new')}>
              Add Device
            </Button>
          {/if}
        </div>
      {:else}
        <div class="overflow-x-auto">
          <table class="w-full whitespace-no-wrap">
            <thead>
              <tr class="text-xs font-semibold tracking-wide text-left text-gray-500 uppercase border-b dark:border-gray-700 bg-gray-50 dark:text-gray-400 dark:bg-gray-800">
                <th class="px-4 py-3">Device</th>
                <th class="px-4 py-3">MAC Address</th>
                <th class="px-4 py-3">Current</th>
                <th class="px-4 py-3">Target</th>
                <th class="px-4 py-3">Last Check</th>
                <th class="px-4 py-3">Actions</th>
              </tr>
            </thead>
            <tbody class="bg-white divide-y dark:divide-gray-700 dark:bg-gray-800">
              {#each filteredDevices as device}
                <tr class="text-gray-700 dark:text-gray-400">
                  <td class="px-4 py-3">
                    <div class="flex items-center text-sm">
                      <div>
                        <p class="font-semibold">{device.device_id || 'Unnamed Device'}</p>
                        <p class="text-xs text-gray-600 dark:text-gray-400">
                          {device.hardware_version || 'Unknown hardware'}
                        </p>
                      </div>
                    </div>
                  </td>
                  <td class="px-4 py-3 text-sm font-mono">{device.mac}</td>
                  <td class="px-4 py-3 text-sm">{device.current_version || 'Unknown'}</td>
                  <td class="px-4 py-3 text-sm">{device.target_version}</td>
                  <td class="px-4 py-3 text-sm">
                    {device.last_check ? new Date(device.last_check).toLocaleString() : 'Never'}
                  </td>
                  <td class="px-4 py-3 text-sm">
                    <div class="flex items-center space-x-2">
                      <a 
                        href={`/firmware/${firmware.filename}`} 
                        target="_blank" 
                        rel="noopener noreferrer"
                        class="flex items-center justify-between px-2 py-2 text-sm font-medium leading-5 text-purple-600 rounded-lg dark:text-gray-400 focus:outline-none focus:shadow-outline-gray"
                        aria-label="Download"
                      >
                        <i class="fas fa-download"></i>
                      </a>
                      
                      {#if confirmingDelete === firmware.filename}
                        <div class="flex items-center space-x-1">
                          <button
                            class="flex items-center justify-between px-2 py-2 text-sm font-medium leading-5 text-red-600 rounded-lg dark:text-red-400 focus:outline-none focus:shadow-outline-gray"
                            aria-label="Confirm Delete"
                            on:click={() => performDelete(firmware.filename)}
                          >
                            <i class="fas fa-check"></i>
                          </button>
                          <button
                            class="flex items-center justify-between px-2 py-2 text-sm font-medium leading-5 text-gray-600 rounded-lg dark:text-gray-400 focus:outline-none focus:shadow-outline-gray"
                            aria-label="Cancel Delete"
                            on:click={cancelDelete}
                          >
                            <i class="fas fa-times"></i>
                          </button>
                        </div>
                      {:else}
                        <button
                          class="flex items-center justify-between px-2 py-2 text-sm font-medium leading-5 text-red-600 rounded-lg dark:text-gray-400 focus:outline-none focus:shadow-outline-gray"
                          aria-label="Delete"
                          on:click={() => confirmDelete(firmware.filename)}
                        >
                          <i class="fas fa-trash"></i>
                        </button>
                      {/if}
                    </div>
                  </td>
                </tr>
              {/each}
            </tbody>
          </table>
        </div>
      {/if}
    </Card>
    
    <Card title="Usage Information">
      <div class="space-y-4">
        <div>
          <h4 class="text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
            How to use firmware files
          </h4>
          <p class="text-sm text-gray-600 dark:text-gray-400">
            After uploading firmware files, you can associate them with devices. When you upload a firmware file, it will be available in the device form dropdown.
          </p>
        </div>
        
        <div>
          <h4 class="text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
            Firmware URL Format
          </h4>
          <p class="text-sm text-gray-600 dark:text-gray-400">
            The firmware URL for devices should follow this format:
          </p>
          <div class="bg-gray-50 dark:bg-gray-700 p-3 rounded-md mt-2">
            <code class="text-sm font-mono">
              http://[server-address]/firmware/[filename]
            </code>
          </div>
        </div>
        
        <div>
          <h4 class="text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
            File Naming Recommendation
          </h4>
          <p class="text-sm text-gray-600 dark:text-gray-400">
            We recommend using a naming convention that includes the version number, like <code>firmware_v1.2.3.bin</code>. 
            This allows the system to automatically extract the version number when you select the file.
          </p>
        </div>
      </div>
    </Card>
  {/if}
</div>