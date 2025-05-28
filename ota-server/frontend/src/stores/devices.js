import { writable } from 'svelte/store';
import axios from 'axios';
import { getAuthHeaders } from './auth';

export const devices = writable([]);
export const loading = writable(false);
export const error = writable(null);

export async function fetchDevices() {
  loading.set(true);
  error.set(null);
  
  try {
    const response = await axios.get('/admin/api/devices', {
      headers: getAuthHeaders()
    });
    
    // Transform object to array
    const devicesArray = Object.entries(response.data).map(([mac, info]) => ({
      mac,
      ...info
    }));
    
    devices.set(devicesArray);
  } catch (err) {
    console.error('Error fetching devices:', err);
    error.set(err.response?.data?.error || 'Failed to fetch devices');
  } finally {
    loading.set(false);
  }
}

export async function addDevice(deviceData) {
  loading.set(true);
  error.set(null);
  
  try {
    const response = await axios.post('/admin/api/devices', deviceData, {
      headers: getAuthHeaders()
    });
    
    // Refresh devices list
    await fetchDevices();
    return response.data;
  } catch (err) {
    console.error('Error adding device:', err);
    error.set(err.response?.data?.error || 'Failed to add device');
    throw err;
  } finally {
    loading.set(false);
  }
}

export async function updateDevice(mac, deviceData) {
  loading.set(true);
  error.set(null);
  
  try {
    const response = await axios.put(`/admin/api/devices/${mac}`, deviceData, {
      headers: getAuthHeaders()
    });
    
    // Refresh devices list
    await fetchDevices();
    return response.data;
  } catch (err) {
    console.error('Error updating device:', err);
    error.set(err.response?.data?.error || 'Failed to update device');
    throw err;
  } finally {
    loading.set(false);
  }
}

export async function deleteDevice(mac) {
  loading.set(true);
  error.set(null);
  
  try {
    const response = await axios.delete(`/admin/api/devices/${mac}`, {
      headers: getAuthHeaders()
    });
    
    // Refresh devices list
    await fetchDevices();
    return response.data;
  } catch (err) {
    console.error('Error deleting device:', err);
    error.set(err.response?.data?.error || 'Failed to delete device');
    throw err;
  } finally {
    loading.set(false);
  }
}