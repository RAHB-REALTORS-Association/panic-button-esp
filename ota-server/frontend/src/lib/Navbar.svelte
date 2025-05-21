<script>
  export let toggleSidebar;
  
  import { apiKey } from '../stores/auth';
  
  let darkMode = $state(
    window.matchMedia('(prefers-color-scheme: dark)').matches
  );
  
  function toggleDarkMode() {
    darkMode = !darkMode;
    document.documentElement.classList.toggle('dark', darkMode);
  }
  
  $effect(() => {
    document.documentElement.classList.toggle('dark', darkMode);
  });
</script>

<header class="z-10 py-4 bg-white shadow-md dark:bg-gray-800">
  <div class="container flex items-center justify-between h-full px-6 mx-auto">
    <!-- Mobile hamburger -->
    <button 
      class="p-1 mr-5 -ml-1 rounded-md md:hidden focus:outline-none focus:shadow-outline-purple"
      on:click={toggleSidebar}
      aria-label="Menu"
    >
      <i class="fas fa-bars"></i>
    </button>
    
    <!-- Page title -->
    <h2 class="text-2xl font-semibold text-gray-700 dark:text-gray-200">
      OTA Update Server
    </h2>
    
    <div class="flex items-center space-x-4">
      <!-- Dark mode button -->
      <button
        class="p-2 rounded-md focus:outline-none focus:shadow-outline-purple"
        on:click={toggleDarkMode}
        aria-label="Toggle dark mode"
      >
        {#if darkMode}
          <i class="fas fa-sun text-gray-300"></i>
        {:else}
          <i class="fas fa-moon text-gray-700"></i>
        {/if}
      </button>
      
      <!-- Status badge -->
      {#if $apiKey}
        <div class="inline-flex items-center px-3 py-1 text-sm font-medium leading-5 text-white transition-colors duration-150 bg-green-500 border border-transparent rounded-full">
          <i class="fas fa-check-circle mr-1"></i> Connected
        </div>
      {:else}
        <div class="inline-flex items-center px-3 py-1 text-sm font-medium leading-5 text-white transition-colors duration-150 bg-red-500 border border-transparent rounded-full">
          <i class="fas fa-exclamation-circle mr-1"></i> Not Authenticated
        </div>
      {/if}
    </div>
  </div>
</header>