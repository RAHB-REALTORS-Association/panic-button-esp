import { writable } from 'svelte/store';
import axios from 'axios';
import { getAuthHeaders } from './auth';

export const firmwareFiles = writable([]);
export const loading = writable(false);
export const error = writable(null);

export async function fetchFirmwareFiles() {
  loading.set(true);
  error.set(null);
  
  try {
    const response = await axios.get('/admin/api/firmware', {
      headers: getAuthHeaders()
    });
    
    firmwareFiles.set(response.data);
  } catch (err) {
    console.error('Error fetching firmware files:', err);
    error.set(err.response?.data?.error || 'Failed to fetch firmware files');
  } finally {
    loading.set(false);
  }
}

export async function uploadFirmware(file) {
  loading.set(true);
  error.set(null);
  
  try {
    const formData = new FormData();
    formData.append('file', file);
    
    const response = await axios.post('/admin/api/firmware', formData, {
      headers: {
        ...getAuthHeaders(),
        'Content-Type': 'multipart/form-data'
      }
    });
    
    // Refresh firmware files list
    await fetchFirmwareFiles();
    return response.data;
  } catch (err) {
    console.error('Error uploading firmware:', err);
    error.set(err.response?.data?.error || 'Failed to upload firmware');
    throw err;
  } finally {
    loading.set(false);
  }
}

export async function deleteFirmware(filename) {
  loading.set(true);
  error.set(null);
  
  try {
    const response = await axios.delete(`/admin/api/firmware/${filename}`, {
      headers: getAuthHeaders()
    });
    
    // Refresh firmware files list
    await fetchFirmwareFiles();
    return response.data;
  } catch (err) {
    console.error('Error deleting firmware:', err);
    error.set(err.response?.data?.error || 'Failed to delete firmware');
    throw err;
  } finally {
    loading.set(false);
  }
}