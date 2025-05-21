<script>
  import { onMount } from 'svelte';
  import Card from '../lib/Card.svelte';
  import Button from '../lib/Button.svelte';
  import { fetchFirmwareFiles, firmwareFiles, loading, error, uploadFirmware, deleteFirmware } from '../stores/firmware';
  import { apiKey } from '../stores/auth';
  
  let fileInput;
  let selectedFile = $state(null);
  let uploadProgress = $state(0);
  let isUploading = $state(false);
  let successMessage = $state('');
  let confirmingDelete = $state(null);
  
  onMount(() => {
    if ($apiKey) {
      fetchFirmwareFiles();
    }
  });
  
  function handleFileSelect(event) {
    const files = event.target.files;
    if (files.length > 0) {
      selectedFile = files[0];
    }
  }
  
  async function uploadFile() {
    if (!selectedFile) return;
    
    try {
      isUploading = true;
      
      await uploadFirmware(selectedFile);
      
      // Reset state
      selectedFile = null;
      fileInput.value = '';
      successMessage = 'Firmware uploaded successfully';
      
      // Clear success message after 3 seconds
      setTimeout(() => {
        successMessage = '';
      }, 3000);
    } catch (err) {
      console.error('Error uploading file:', err);
    } finally {
      isUploading = false;
    }
  }
  
  function confirmDelete(filename) {
    confirmingDelete = filename;
  }
  
  async function performDelete(filename) {
    try {
      await deleteFirmware(filename);
      confirmingDelete = null;
    } catch (error) {
      console.error('Error deleting firmware:', error);
    }
  }
  
  function cancelDelete() {
    confirmingDelete = null;
  }
  
  function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
  }
</script>

<div class="mt-6">
  <h2 class="my-6 text-2xl font-semibold text-gray-700 dark:text-gray-200">
    Firmware Files
  </h2>
  
  {#if !$apiKey}
    <Card>
      <div class="text-center py-4">
        <i class="fas fa-lock text-4xl text-gray-400 mb-3"></i>
        <p class="text-gray-600 dark:text-gray-400 mb-4">
          Please enter your API key in the settings to manage firmware files.
        </p>
        <Button color="primary" icon="fas fa-cog" on:click={() => window.location.href = '#/settings'}>
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
    
    {#if successMessage}
      <div class="bg-green-100 border-l-4 border-green-500 text-green-700 p-4 mb-6" role="alert">
        <p class="font-bold">Success</p>
        <p>{successMessage}</p>
      </div>
    {/if}
    
    <Card title="Upload Firmware">
      <div class="space-y-4">
        <div>
          <label class="block text-sm font-medium text-gray-700 dark:text-gray-400 mb-2" for="firmware-file">
            Firmware Binary File
          </label>
          <input
            id="firmware-file"
            type="file"
            accept=".bin"
            class="block w-full text-sm text-gray-500 file:mr-4 file:py-2 file:px-4 file:border-0 file:text-sm file:font-semibold file:bg-purple-50 file:text-purple-700 hover:file:bg-purple-100 dark:text-gray-400 dark:file:bg-gray-700 dark:file:text-purple-300"
            on:change={handleFileSelect}
            bind:this={fileInput}
          />
          <p class="mt-1 text-sm text-gray-500 dark:text-gray-400">
            Select a firmware binary file to upload (.bin)
          </p>
        </div>
        
        {#if selectedFile}
          <div class="bg-gray-50 dark:bg-gray-700 p-3 rounded-md">
            <p class="text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
              Selected File:
            </p>
            <p class="text-sm text-gray-600 dark:text-gray-400">
              <span class="font-semibold">{selectedFile.name}</span> ({formatBytes(selectedFile.size)})
            </p>
          </div>
        {/if}
        
        <Button 
          type="button" 
          color="primary" 
          icon="fas fa-upload"
          loading={isUploading}
          disabled={!selectedFile || isUploading}
          on:click={uploadFile}
        >
          Upload Firmware
        </Button>
      </div>
    </Card>
    
    <Card title="Available Firmware Files">
      {#if $loading}
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
        </div>
      {:else}
        <div class="overflow-x-auto">
          <table class="w-full whitespace-no-wrap">
            <thead>
              <tr class="text-xs font-semibold tracking-wide text-left text-gray-500 uppercase border-b dark:border-gray-700 bg-gray-50 dark:text-gray-400 dark:bg-gray-800">
                <th class="px-4 py-3">Filename</th>
                <th class="px-4 py-3">Size</th>
                <th class="px-4 py-3">Checksum (MD5)</th>
                <th class="px-4 py-3">Actions</th>
              </tr>
            </thead>
            <tbody class="bg-white divide-y dark:divide-gray-700 dark:bg-gray-800">
              {#each $firmwareFiles as firmware}
                <tr class="text-gray-700 dark:text-gray-400">
                  <td class="px-4 py-3 text-sm">
                    <div class="flex items-center">
                      <i class="fas fa-file-code text-purple-500 mr-2"></i>
                      <span class="font-semibold">{firmware.filename}</span>
                    </div>
                  </td>
                  <td class="px-4 py-3 text-sm">
                    {firmware.size ? formatBytes(firmware.size) : 'Unknown'}
                  </td>
                  <td class="px-4 py-3 text-sm font-mono text-xs">
                    {firmware.checksum}
                  </td>
                  <td class="px-4 py-3 text-sm">
                    <div class="flex items-center space-x-2">
                      <a 
                        href={`/firmware/${firmware.filename}`} 
                        target="_blank" 
                        rel="noopener noreferrer"
                        class="flex items-center justify-between px-2 py-2 text-sm font-medium leading-5 text-purple-600 rounded-lg dark:text-gray-400 focus:outline-none focus:shadow-outline-gray"
                        aria-label="Download"
                        title="Download firmware"
                      >
                        <i class="fas fa-download"></i>
                      </a>
                      
                      {#if confirmingDelete === firmware.filename}
                        <div class="flex items-center space-x-1">
                          <button
                            class="flex items-center justify-between px-2 py-2 text-sm font-medium leading-5 text-red-600 rounded-lg dark:text-red-400 focus:outline-none focus:shadow-outline-gray"
                            aria-label="Confirm Delete"
                            title="Confirm delete"
                            on:click={() => performDelete(firmware.filename)}
                          >
                            <i class="fas fa-check"></i>
                          </button>
                          <button
                            class="flex items-center justify-between px-2 py-2 text-sm font-medium leading-5 text-gray-600 rounded-lg dark:text-gray-400 focus:outline-none focus:shadow-outline-gray"
                            aria-label="Cancel Delete"
                            title="Cancel"
                            on:click={cancelDelete}
                          >
                            <i class="fas fa-times"></i>
                          </button>
                        </div>
                      {:else}
                        <button
                          class="flex items-center justify-between px-2 py-2 text-sm font-medium leading-5 text-red-600 rounded-lg dark:text-gray-400 focus:outline-none focus:shadow-outline-gray"
                          aria-label="Delete"
                          title="Delete firmware"
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