<script>
  import { onMount } from 'svelte';
  import Card from '../lib/Card.svelte';
  import Button from '../lib/Button.svelte';
  import { fetchDevices, devices, loading as devicesLoading } from '../stores/devices';
  import { fetchFirmwareFiles, firmwareFiles, loading as firmwareLoading } from '../stores/firmware';
  import { apiKey } from '../stores/auth';
  
  let serverStatus = $state({
    status: 'Loading...',
    version: '',
    timestamp: ''
  });
  
  let statusLoading = $state(true);
  
  async function checkServerStatus() {
    try {
      statusLoading = true;
      const response = await fetch('/status');
      const data = await response.json();
      
      serverStatus = {
        status: data.status === 'ok' ? 'Online' : 'Error',
        version: data.version || 'Unknown',
        timestamp: data.timestamp || new Date().toISOString()
      };
    } catch (err) {
      console.error('Error checking server status:', err);
      serverStatus = {
        status: 'Offline',
        version: 'Unknown',
        timestamp: new Date().toISOString()
      };
    } finally {
      statusLoading = false;
    }
  }
  
  onMount(async () => {
    await checkServerStatus();
    
    if ($apiKey) {
      await fetchDevices();
      await fetchFirmwareFiles();
    }
  });
  
  // Reactive counts
  $effect(() => {
    devicesCount = $devices.length;
    firmwareCount = $firmwareFiles.length;
  });
  
  let devicesCount = $state(0);
  let firmwareCount = $state(0);
</script>

<div class="mt-6">
  <h2 class="my-6 text-2xl font-semibold text-gray-700 dark:text-gray-200">
    Dashboard
  </h2>
  
  {#if !$apiKey}
    <Card title="Authentication Required">
      <div class="text-center py-4">
        <i class="fas fa-lock text-4xl text-gray-400 mb-3"></i>
        <p class="text-gray-600 dark:text-gray-400 mb-4">
          Please enter your API key in the settings to access the dashboard.
        </p>
        <Button color="primary" icon="fas fa-cog" on:click={() => window.location.href = '#/settings'}>
          Go to Settings
        </Button>
      </div>
    </Card>
  {:else}
    <div class="grid gap-6 mb-8 md:grid-cols-2 xl:grid-cols-4">
      <!-- Server Status Card -->
      <div class="flex items-center p-4 bg-white rounded-lg shadow-xs dark:bg-gray-800">
        <div class="p-3 mr-4 text-green-500 bg-green-100 rounded-full dark:text-green-100 dark:bg-green-500">
          <i class="fas fa-server"></i>
        </div>
        <div>
          <p class="mb-2 text-sm font-medium text-gray-600 dark:text-gray-400">
            Server Status
          </p>
          <p class="text-lg font-semibold text-gray-700 dark:text-gray-200">
            {statusLoading ? 'Loading...' : serverStatus.status}
          </p>
          <p class="text-xs text-gray-500 dark:text-gray-400">
            Version: {serverStatus.version}
          </p>
        </div>
      </div>
      
      <!-- Devices Card -->
      <div class="flex items-center p-4 bg-white rounded-lg shadow-xs dark:bg-gray-800">
        <div class="p-3 mr-4 text-blue-500 bg-blue-100 rounded-full dark:text-blue-100 dark:bg-blue-500">
          <i class="fas fa-microchip"></i>
        </div>
        <div>
          <p class="mb-2 text-sm font-medium text-gray-600 dark:text-gray-400">
            Registered Devices
          </p>
          <p class="text-lg font-semibold text-gray-700 dark:text-gray-200">
            {$devicesLoading ? '...' : devicesCount}
          </p>
          <p class="text-xs text-gray-500 dark:text-gray-400">
            <a href="#/devices" class="hover:underline">Manage devices</a>
          </p>
        </div>
      </div>
      
      <!-- Firmware Card -->
      <div class="flex items-center p-4 bg-white rounded-lg shadow-xs dark:bg-gray-800">
        <div class="p-3 mr-4 text-purple-500 bg-purple-100 rounded-full dark:text-purple-100 dark:bg-purple-500">
          <i class="fas fa-code"></i>
        </div>
        <div>
          <p class="mb-2 text-sm font-medium text-gray-600 dark:text-gray-400">
            Firmware Files
          </p>
          <p class="text-lg font-semibold text-gray-700 dark:text-gray-200">
            {$firmwareLoading ? '...' : firmwareCount}
          </p>
          <p class="text-xs text-gray-500 dark:text-gray-400">
            <a href="#/firmware" class="hover:underline">Manage firmware</a>
          </p>
        </div>
      </div>
      
      <!-- Actions Card -->
      <div class="flex items-center p-4 bg-white rounded-lg shadow-xs dark:bg-gray-800">
        <div class="p-3 mr-4 text-orange-500 bg-orange-100 rounded-full dark:text-orange-100 dark:bg-orange-500">
          <i class="fas fa-bolt"></i>
        </div>
        <div>
          <p class="mb-2 text-sm font-medium text-gray-600 dark:text-gray-400">
            Quick Actions
          </p>
          <div class="flex space-x-2">
            <Button color="primary" size="sm" icon="fas fa-plus" on:click={() => window.location.href = '#/devices/new'}>
              New Device
            </Button>
            <Button color="success" size="sm" icon="fas fa-upload" on:click={() => window.location.href = '#/firmware'}>
              Upload
            </Button>
          </div>
        </div>
      </div>
    </div>
    
    <!-- Recent Devices -->
    <Card title="Recent Devices">
      {#if $devicesLoading}
        <div class="text-center py-4">
          <div class="animate-spin inline-block w-6 h-6 border-2 border-current border-t-transparent text-purple-600 rounded-full" role="status">
            <span class="sr-only">Loading...</span>
          </div>
          <p class="mt-2 text-gray-600 dark:text-gray-400">Loading devices...</p>
        </div>
      {:else if $devices.length === 0}
        <div class="text-center py-4">
          <i class="fas fa-microchip text-4xl text-gray-400 mb-3"></i>
          <p class="text-gray-600 dark:text-gray-400 mb-4">
            No devices registered yet.
          </p>
          <Button color="primary" icon="fas fa-plus" on:click={() => window.location.href = '#/devices/new'}>
            Add Device
          </Button>
        </div>
      {:else}
        <div class="overflow-x-auto">
          <table class="w-full whitespace-no-wrap">
            <thead>
              <tr class="text-xs font-semibold tracking-wide text-left text-gray-500 uppercase border-b dark:border-gray-700 bg-gray-50 dark:text-gray-400 dark:bg-gray-800">
                <th class="px-4 py-3">Device</th>
                <th class="px-4 py-3">MAC Address</th>
                <th class="px-4 py-3">Current Version</th>
                <th class="px-4 py-3">Target Version</th>
                <th class="px-4 py-3">Last Check</th>
              </tr>
            </thead>
            <tbody class="bg-white divide-y dark:divide-gray-700 dark:bg-gray-800">
              {#each $devices.slice(0, 5) as device}
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
                  <td class="px-4 py-3 text-sm">{device.mac}</td>
                  <td class="px-4 py-3 text-sm">{device.current_version || 'Unknown'}</td>
                  <td class="px-4 py-3 text-sm">{device.target_version}</td>
                  <td class="px-4 py-3 text-sm">
                    {device.last_check ? new Date(device.last_check).toLocaleString() : 'Never'}
                  </td>
                </tr>
              {/each}
            </tbody>
          </table>
        </div>
        <div class="px-4 py-3 border-t dark:border-gray-700">
          <Button color="neutral" size="sm" icon="fas fa-arrow-right" on:click={() => window.location.href = '#/devices'}>
            View All Devices
          </Button>
        </div>
      {/if}
    </Card>
    
    <!-- Recent Firmware -->
    <Card title="Firmware Files">
      {#if $firmwareLoading}
        <div class="text-center py-4">
          <div class="animate-spin inline-block w-6 h-6 border-2 border-current border-t-transparent text-purple-600 rounded-full" role="status">
            <span class="sr-only">Loading...</span>
          </div>
          <p class="mt-2 text-gray-600 dark:text-gray-400">Loading firmware files...</p>
        </div>
      {:else if $firmwareFiles.length === 0}
        <div class="text-center py-4">
          <i class="fas fa-code text-4xl text-gray-400 mb-3"></i>
          <p class="text-gray-600 dark:text-gray-400 mb-4">
            No firmware files uploaded yet.
          </p>
          <Button color="primary" icon="fas fa-upload" on:click={() => window.location.href = '#/firmware'}>
            Upload Firmware
          </Button>
        </div>
      {:else}
        <div class="overflow-x-auto">
          <table class="w-full whitespace-no-wrap">
            <thead>
              <tr class="text-xs font-semibold tracking-wide text-left text-gray-500 uppercase border-b dark:border-gray-700 bg-gray-50 dark:text-gray-400 dark:bg-gray-800">
                <th class="px-4 py-3">File</th>
                <th class="px-4 py-3">Checksum</th>
                <th class="px-4 py-3">Size</th>
              </tr>
            </thead>
            <tbody class="bg-white divide-y dark:divide-gray-700 dark:bg-gray-800">
              {#each $firmwareFiles.slice(0, 5) as firmware}
                <tr class="text-gray-700 dark:text-gray-400">
                  <td class="px-4 py-3 text-sm font-semibold">{firmware.filename}</td>
                  <td class="px-4 py-3 text-sm font-mono text-xs">{firmware.checksum}</td>
                  <td class="px-4 py-3 text-sm">
                    {firmware.size ? Math.round(firmware.size / 1024) + ' KB' : 'Unknown'}
                  </td>
                </tr>
              {/each}
            </tbody>
          </table>
        </div>
        <div class="px-4 py-3 border-t dark:border-gray-700">
          <Button color="neutral" size="sm" icon="fas fa-arrow-right" on:click={() => window.location.href = '#/firmware'}>
            Manage Firmware
          </Button>
        </div>
      {/if}
    </Card>
  {/if}
</div>