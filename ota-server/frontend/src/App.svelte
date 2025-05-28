<script>
  import { Router, Route } from 'svelte-spa-router';
  import { wrap } from 'svelte-spa-router/wrap';
  
  // Import routes
  import Dashboard from './routes/Dashboard.svelte';
  import Devices from './routes/Devices.svelte';
  import Firmware from './routes/Firmware.svelte';
  import Settings from './routes/Settings.svelte';
  import NotFound from './routes/NotFound.svelte';
  
  // Import components
  import Navbar from './lib/Navbar.svelte';
  import Footer from './lib/Footer.svelte';
  import Sidebar from './lib/Sidebar.svelte';
  
  // State
  let sidebarOpen = $state(true);
  
  const toggleSidebar = () => {
    sidebarOpen = !sidebarOpen;
  };
  
  // Routes
  const routes = {
    '/': Dashboard,
    '/devices': Devices,
    '/firmware': Firmware,
    '/settings': Settings,
    
    // The wildcard route, which catches all routes not defined above
    '*': NotFound
  };
</script>

<main class="flex h-screen bg-gray-50 dark:bg-gray-900">
  <Sidebar {sidebarOpen} />
  
  <div class="flex flex-col flex-1 w-full">
    <Navbar {toggleSidebar} />
    
    <div class="h-full overflow-y-auto">
      <div class="container px-6 mx-auto grid">
        <Router {routes} />
      </div>
    </div>
    
    <Footer />
  </div>
</main>

<style>
  :global(body) {
    margin: 0;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, 
                 Oxygen-Sans, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;
  }
  
  :global(*, *::before, *::after) {
    box-sizing: border-box;
  }
  
  :global(#app) {
    width: 100%;
    height: 100vh;
  }
</style>