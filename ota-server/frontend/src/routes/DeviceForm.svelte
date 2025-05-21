<script>
  import { onMount } from 'svelte';
  import { push, location } from 'svelte-spa-router';
  import Card from '../lib/Card.svelte';
  import Button from '../lib/Button.svelte';
  import { fetchDevices, devices, addDevice, updateDevice, loading, error } from '../stores/devices';
  import { fetchFirmwareFiles, firmwareFiles } from '../stores/firmware';
  import { apiKey } from '../stores/auth';
  
  // Props
  export let params = {};
  
  let isEditing = $state(false);
  let deviceData = $state({
    mac_address: '',
    device_id: '',
    hardware_version: '1.0',
    target_version: '',
    firmware_url: '',
    checksum: ''
  });
  let formError = $state('');
  let successMessage = $state('');
  
  onMount(async () => {
    if (!$apiKey) {
      push('/settings');
      return;
    }
    
    // Check if we're editing an existing device
    if (params && params.mac) {
      isEditing = true;
      
      // Fetch devices if not already loaded
      if ($devices.length === 0) {
        await fetchDevices();
      }
      
      // Find the device
      const device = $devices.find(d => d.mac === params.mac);
      if (device) {
        deviceData = {
          mac_address: device.mac,
          device_id: device.device_id || '',
          hardware_version: device.hardware_version || '1.0',
          target_version: device.target_version || '',
          firmware_url: device.firmware_url || '',
          checksum: device.checksum || ''
        };
      } else {
        formError = 'Device not found';
      }
    }
    
    // Fetch firmware files for the dropdown
    await fetchFirmwareFiles();
  });
  
  // Handle firmware selection
  function handleFirmwareSelect(event) {
    const selectedFile = $firmwareFiles.find(f => f.filename === event.target.value);
    if (selectedFile) {
      // Generate firmware URL
      const urlBase = window.location.origin;
      deviceData.firmware_url = `${urlBase}/firmware/${selectedFile.filename}`;
      deviceData.checksum = selectedFile.checksum;
      
      // Try to extract version from filename (common pattern: name_v1.2.3.bin)
      const versionMatch = selectedFile.filename.match(/_v(\d+\.\d+\.\d+)/);
      if (versionMatch && versionMatch[1]) {
        deviceData.target_version = versionMatch[1];
      }
    }
  }
  
  // Validate form before submission
  function validateForm() {
    formError = '';
    
    // Validate MAC address (basic check)
    if (!deviceData.mac_address) {
      formError = 'MAC address is required';
      return false;
    }
    
    // Simple regex for MAC address format (with or without colons)
    const macPattern = /^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$|^([0-9A-Fa-f]{12})$/;
    if (!macPattern.test(deviceData.mac_address)) {
      formError = 'Invalid MAC address format (e.g. AA:BB:CC:DD:EE:FF)';
      return false;
    }
    
    // Validate required fields
    if (!deviceData.device_id) {
      formError = 'Device ID is required';
      return false;
    }
    
    if (!deviceData.target_version) {
      formError = 'Target version is required';
      return false;
    }
    
    // Validate version format
    const versionPattern = /^\d+(\.\d+){0,2}$/;
    if (!versionPattern.test(deviceData.target_version)) {
      formError = 'Invalid version format (e.g. 1.2.3)';
      return false;
    }
    
    if (!deviceData.firmware_url) {
      formError = 'Firmware URL is required';
      return false;
    }
    
    if (!deviceData.checksum) {
      formError = 'Checksum is required';
      return false;
    }
    
    return true;
  }
  
  // Submit form
  async function submitForm() {
    if (!validateForm()) return;
    
    try {
      if (isEditing) {
        // Remove mac_address from payload for update
        const updatePayload = { ...deviceData };
        delete updatePayload.mac_address;
        
        await updateDevice(deviceData.mac_address, updatePayload);
        successMessage = 'Device updated successfully';
      } else {
        await addDevice(deviceData);
        successMessage = 'Device added successfully';
        
        // Reset form after successful addition
        deviceData = {
          mac_address: '',
          device_id: '',
          hardware_version: '1.0',
          target_version: '',
          firmware_url: '',
          checksum: ''
        };
      }
      
      // Clear success message after 3 seconds
      setTimeout(() => {
        successMessage = '';
      }, 3000);
    } catch (err) {
      formError = err.message || 'An error occurred';
    }
  }
</script>

<div class="mt-6">
  <div class="flex items-center justify-between mb-6">
    <h2 class="text-2xl font-semibold text-gray-700 dark:text-gray-200">
      {isEditing ? 'Edit Device' : 'Add New Device'}
    </h2>
    <Button color="neutral" icon="fas fa-arrow-left" on:click={() => push('/devices')}>
      Back to Devices
    </Button>
  </div>
  
  <Card>
    {#if formError}
      <div class="bg-red-100 border-l-4 border-red-500 text-red-700 p-4 mb-6" role="alert">
        <p class="font-bold">Error</p>
        <p>{formError}</p>
      </div>
    {/if}
    
    {#if successMessage}
      <div class="bg-green-100 border-l-4 border-green-500 text-green-700 p-4 mb-6" role="alert">
        <p class="font-bold">Success</p>
        <p>{successMessage}</p>
      </div>
    {/if}
    
    <form class="space-y-6" on:submit|preventDefault={submitForm}>
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="mac_address">
          MAC Address *
        </label>
        <input
          id="mac_address"
          type="text"
          class="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-purple-500 focus:border-purple-500 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
          placeholder="e.g. AA:BB:CC:DD:EE:FF"
          bind:value={deviceData.mac_address}
          disabled={isEditing}
          required
        />
        <p class="mt-1 text-sm text-gray-500 dark:text-gray-400">
          Physical MAC address of the device (format: AA:BB:CC:DD:EE:FF)
        </p>
      </div>
      
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="device_id">
          Device ID *
        </label>
        <input
          id="device_id"
          type="text"
          class="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-purple-500 focus:border-purple-500 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
          placeholder="e.g. panic_button_01"
          bind:value={deviceData.device_id}
          required
        />
        <p class="mt-1 text-sm text-gray-500 dark:text-gray-400">
          Unique identifier for the device
        </p>
      </div>
      
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="hardware_version">
          Hardware Version
        </label>
        <input
          id="hardware_version"
          type="text"
          class="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-purple-500 focus:border-purple-500 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
          placeholder="e.g. 1.0"
          bind:value={deviceData.hardware_version}
        />
        <p class="mt-1 text-sm text-gray-500 dark:text-gray-400">
          Hardware version of the device
        </p>
      </div>
      
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="firmware_select">
          Firmware File
        </label>
        <select
          id="firmware_select"
          class="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-purple-500 focus:border-purple-500 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
          on:change={handleFirmwareSelect}
        >
          <option value="">Select firmware file</option>
          {#each $firmwareFiles as firmware}
            <option value={firmware.filename}>{firmware.filename}</option>
          {/each}
        </select>
        <p class="mt-1 text-sm text-gray-500 dark:text-gray-400">
          Selecting a firmware file will auto-fill the firmware URL, checksum, and version (if available in filename)
        </p>
      </div>
      
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="target_version">
          Target Version *
        </label>
        <input
          id="target_version"
          type="text"
          class="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-purple-500 focus:border-purple-500 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
          placeholder="e.g. 1.2.3"
          bind:value={deviceData.target_version}
          required
        />
        <p class="mt-1 text-sm text-gray-500 dark:text-gray-400">
          Target firmware version (semantic versioning: x.y.z)
        </p>
      </div>
      
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="firmware_url">
          Firmware URL *
        </label>
        <input
          id="firmware_url"
          type="text"
          class="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-purple-500 focus:border-purple-500 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
          placeholder="e.g. http://example.com/firmware/file.bin"
          bind:value={deviceData.firmware_url}
          required
        />
        <p class="mt-1 text-sm text-gray-500 dark:text-gray-400">
          URL where the firmware binary can be downloaded
        </p>
      </div>
      
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="checksum">
          MD5 Checksum *
        </label>
        <input
          id="checksum"
          type="text"
          class="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-purple-500 focus:border-purple-500 dark:bg-gray-700 dark:border-gray-600 dark:text-white"
          placeholder="e.g. a1b2c3d4e5f6..."
          bind:value={deviceData.checksum}
          required
        />
        <p class="mt-1 text-sm text-gray-500 dark:text-gray-400">
          MD5 checksum of the firmware file for verification
        </p>
      </div>
      
      <div class="flex items-center space-x-4">
        <Button 
          type="submit" 
          color="primary" 
          icon={isEditing ? "fas fa-save" : "fas fa-plus"}
          loading={$loading}
        >
          {isEditing ? 'Update Device' : 'Add Device'}
        </Button>
        <Button 
          type="button" 
          color="neutral" 
          on:click={() => push('/devices')}
        >
          Cancel
        </Button>
      </div>
    </form>
  </Card>
</div>