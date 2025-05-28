// frontend/src/routes.js
import Dashboard from './routes/Dashboard.svelte';
import Devices from './routes/Devices.svelte';
import DeviceForm from './routes/DeviceForm.svelte';
import Firmware from './routes/Firmware.svelte';
import Settings from './routes/Settings.svelte';
import NotFound from './routes/NotFound.svelte';

export default {
  // Home page
  '/': Dashboard,
  
  // Devices
  '/devices': Devices,
  '/devices/new': DeviceForm,
  '/devices/edit/:mac': DeviceForm,
  
  // Firmware
  '/firmware': Firmware,
  
  // Settings
  '/settings': Settings,
  
  // Catch-all route for 404
  '*': NotFound
};