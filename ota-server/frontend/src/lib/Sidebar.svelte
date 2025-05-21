<script>
  export let sidebarOpen;
  
  import { location } from 'svelte-spa-router';
</script>

<aside
  class="z-20 w-64 overflow-y-auto bg-white dark:bg-gray-800 md:block flex-shrink-0 transition-all"
  class:hidden={!sidebarOpen}
>
  <div class="py-4 text-gray-500 dark:text-gray-400">
    <div class="ml-6 text-lg font-bold text-gray-800 dark:text-gray-200 flex items-center">
      <i class="fas fa-microchip mr-2"></i>
      OTA Server
    </div>
    <ul class="mt-6">
      <li class="relative px-6 py-3">
        {#if $location === '/'}
          <span class="absolute inset-y-0 left-0 w-1 bg-purple-600 rounded-tr-lg rounded-br-lg" aria-hidden="true"></span>
        {/if}
        <a
          class="inline-flex items-center w-full text-sm font-semibold transition-colors duration-150 hover:text-gray-800 dark:hover:text-gray-200"
          class:text-gray-800={$location === '/'}
          class:dark:text-gray-100={$location === '/'}
          href="#/"
        >
          <i class="fas fa-tachometer-alt w-5 h-5"></i>
          <span class="ml-4">Dashboard</span>
        </a>
      </li>
      <li class="relative px-6 py-3">
        {#if $location === '/devices'}
          <span class="absolute inset-y-0 left-0 w-1 bg-purple-600 rounded-tr-lg rounded-br-lg" aria-hidden="true"></span>
        {/if}
        <a
          class="inline-flex items-center w-full text-sm font-semibold transition-colors duration-150 hover:text-gray-800 dark:hover:text-gray-200"
          class:text-gray-800={$location === '/devices'}
          class:dark:text-gray-100={$location === '/devices'}
          href="#/devices"
        >
          <i class="fas fa-microchip w-5 h-5"></i>
          <span class="ml-4">Devices</span>
        </a>
      </li>
      <li class="relative px-6 py-3">
        {#if $location === '/firmware'}
          <span class="absolute inset-y-0 left-0 w-1 bg-purple-600 rounded-tr-lg rounded-br-lg" aria-hidden="true"></span>
        {/if}
        <a
          class="inline-flex items-center w-full text-sm font-semibold transition-colors duration-150 hover:text-gray-800 dark:hover:text-gray-200"
          class:text-gray-800={$location === '/firmware'}
          class:dark:text-gray-100={$location === '/firmware'}
          href="#/firmware"
        >
          <i class="fas fa-code w-5 h-5"></i>
          <span class="ml-4">Firmware</span>
        </a>
      </li>
      <li class="relative px-6 py-3">
        {#if $location === '/settings'}
          <span class="absolute inset-y-0 left-0 w-1 bg-purple-600 rounded-tr-lg rounded-br-lg" aria-hidden="true"></span>
        {/if}
        <a
          class="inline-flex items-center w-full text-sm font-semibold transition-colors duration-150 hover:text-gray-800 dark:hover:text-gray-200"
          class:text-gray-800={$location === '/settings'}
          class:dark:text-gray-100={$location === '/settings'}
          href="#/settings"
        >
          <i class="fas fa-cog w-5 h-5"></i>
          <span class="ml-4">Settings</span>
        </a>
      </li>
    </ul>
    <div class="px-6 my-6">
      <a 
        href="#/devices/new" 
        class="flex items-center justify-between w-full px-4 py-2 text-sm font-medium leading-5 text-white transition-colors duration-150 bg-purple-600 border border-transparent rounded-lg active:bg-purple-600 hover:bg-purple-700 focus:outline-none focus:shadow-outline-purple"
      >
        New Device
        <i class="fas fa-plus ml-2"></i>
      </a>
    </div>
  </div>
</aside>