#!/usr/bin/env python3
"""
Admin tools for OTA Update Server
"""
import os
import argparse
from typing import Dict, Any, Optional

import requests

# Import utils from the main application
from utils import calculate_file_md5, format_mac_address
from config import load_config, get_config

def get_admin_api_key() -> str:
    """Get the admin API key from config or environment"""
    config = get_config()
    return config.get("admin_api_key", "")

def get_server_url() -> str:
    """Get the server URL from config"""
    config = get_config()
    host = config.get("server_host", "0.0.0.0")
    if host == "0.0.0.0":
        host = "localhost"  # For local connections
    port = config.get("server_port", 5000)
    return f"http://{host}:{port}"

def make_admin_request(
    endpoint: str, 
    method: str = "GET", 
    data: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """
    Make an authenticated request to the admin API
    
    Args:
        endpoint: API endpoint (e.g., "/admin/devices")
        method: HTTP method (GET, POST, PUT, DELETE)
        data: Request body data for POST/PUT requests
        
    Returns:
        Response data as dict
    """
    url = f"{get_server_url()}{endpoint}"
    api_key = get_admin_api_key()
    
    headers = {
        "X-Admin-API-Key": api_key,
        "Content-Type": "application/json"
    }
    
    try:
        if method == "GET":
            response = requests.get(url, headers=headers)
        elif method == "POST":
            response = requests.post(url, headers=headers, json=data)
        elif method == "PUT":
            response = requests.put(url, headers=headers, json=data)
        elif method == "DELETE":
            response = requests.delete(url, headers=headers)
        else:
            print(f"Error: Unknown method {method}")
            return {"error": f"Unknown method {method}"}
        
        # Parse JSON response
        try:
            result = response.json()
        except ValueError:
            result = {"error": "Invalid JSON response", "status_code": response.status_code}
        
        # Check for error status codes
        if response.status_code >= 400:
            print(f"Error {response.status_code}: {result.get('error', 'Unknown error')}")
        
        return result
    except requests.exceptions.RequestException as e:
        print(f"Request error: {e}")
        return {"error": str(e)}

def list_devices_cmd(_args):
    """Command to list all devices"""
    print("Fetching device list...")
    devices = make_admin_request("/admin/devices")
    
    if "error" in devices:
        return
        
    if not devices:
        print("No devices registered.")
        return
        
    print("\nRegistered Devices:")
    print("-" * 80)
    print(f"{'MAC Address':<18} {'Device ID':<15} {'Hardware':<10} {'Current':<10} {'Target':<10}")
    print("-" * 80)
    
    for mac, info in devices.items():
        print(f"{mac:<18} {info.get('device_id', 'N/A'):<15} "
              f"{info.get('hardware_version', 'N/A'):<10} "
              f"{info.get('current_version', 'N/A'):<10} "
              f"{info.get('target_version', 'N/A'):<10}")

def get_device_cmd(args):
    """Command to get device details"""
    mac = format_mac_address(args.mac)
    if not mac:
        print(f"Error: Invalid MAC address format: {args.mac}")
        return
        
    print(f"Fetching details for device {mac}...")
    device = make_admin_request(f"/admin/devices/{mac}")
    
    if "error" in device:
        return
        
    print("\nDevice Details:")
    print("-" * 50)
    for key, value in device.items():
        print(f"{key}: {value}")

def add_device_cmd(args):
    """Command to add a new device"""
    mac = format_mac_address(args.mac)
    if not mac:
        print(f"Error: Invalid MAC address format: {args.mac}")
        return

    # Check if firmware file exists
    if args.firmware_file and not os.path.exists(args.firmware_file):
        print(f"Error: Firmware file not found: {args.firmware_file}")
        return
        
    # Calculate checksum if firmware file provided
    checksum = ""
    if args.firmware_file:
        checksum = calculate_file_md5(args.firmware_file)
        print(f"Calculated MD5 checksum: {checksum}")
        
        # Copy firmware file to the firmware directory if needed
        config = get_config()
        firmware_dir = config.get("firmware_directory", "firmware")
        if not os.path.exists(firmware_dir):
            os.makedirs(firmware_dir)
            
        firmware_filename = os.path.basename(args.firmware_file)
        target_path = os.path.join(firmware_dir, firmware_filename)
        
        # Copy only if destination doesn't exist or is different
        if not os.path.exists(target_path) or calculate_file_md5(target_path) != checksum:
            import shutil
            shutil.copy2(args.firmware_file, target_path)
            print(f"Copied firmware file to {target_path}")
        
    # Prepare device data
    server_url = get_server_url()
    firmware_url = args.firmware_url
    
    # If no firmware URL but we have a file, construct the URL
    if not firmware_url and args.firmware_file:
        firmware_filename = os.path.basename(args.firmware_file)
        firmware_url = f"{server_url}/firmware/{firmware_filename}"
        
    device_data = {
        "mac_address": mac,
        "device_id": args.device_id or f"device_{mac.replace(':', '')}",
        "hardware_version": args.hardware or "1.0",
        "target_version": args.version,
        "firmware_url": firmware_url,
        "checksum": args.checksum or checksum,
        "last_check": None,
        "last_update": None
    }
    
    # Send the request
    print(f"Adding device {mac}...")
    result = make_admin_request("/admin/devices", method="POST", data=device_data)
    
    if "success" in result:
        print(f"Device {mac} added successfully.")
    
def update_device_cmd(args):
    """Command to update a device"""
    mac = format_mac_address(args.mac)
    if not mac:
        print(f"Error: Invalid MAC address format: {args.mac}")
        return
        
    # First get existing device data
    device = make_admin_request(f"/admin/devices/{mac}")
    if "error" in device:
        print(f"Device {mac} not found. Use 'add' command first.")
        return
        
    # Calculate checksum if firmware file provided
    checksum = ""
    if args.firmware_file:
        checksum = calculate_file_md5(args.firmware_file)
        print(f"Calculated MD5 checksum: {checksum}")
        
        # Copy firmware file to the firmware directory if needed
        config = get_config()
        firmware_dir = config.get("firmware_directory", "firmware")
        if not os.path.exists(firmware_dir):
            os.makedirs(firmware_dir)
            
        firmware_filename = os.path.basename(args.firmware_file)
        target_path = os.path.join(firmware_dir, firmware_filename)
        
        # Copy only if destination doesn't exist or is different
        if not os.path.exists(target_path) or calculate_file_md5(target_path) != checksum:
            import shutil
            shutil.copy2(args.firmware_file, target_path)
            print(f"Copied firmware file to {target_path}")
    
    # Update device data with any provided fields
    server_url = get_server_url()
    
    # If no firmware URL but we have a file, construct the URL
    if args.firmware_file and not args.firmware_url:
        firmware_filename = os.path.basename(args.firmware_file)
        args.firmware_url = f"{server_url}/firmware/{firmware_filename}"
    
    # Update values if provided
    if args.device_id:
        device["device_id"] = args.device_id
    if args.hardware:
        device["hardware_version"] = args.hardware
    if args.version:
        device["target_version"] = args.version
    if args.firmware_url:
        device["firmware_url"] = args.firmware_url
    if args.checksum or checksum:
        device["checksum"] = args.checksum or checksum
    
    # Send the update request
    print(f"Updating device {mac}...")
    result = make_admin_request(f"/admin/devices/{mac}", method="PUT", data=device)
    
    if "success" in result:
        print(f"Device {mac} updated successfully.")

def delete_device_cmd(args):
    """Command to delete a device"""
    mac = format_mac_address(args.mac)
    if not mac:
        print(f"Error: Invalid MAC address format: {args.mac}")
        return
        
    print(f"Deleting device {mac}...")
    result = make_admin_request(f"/admin/devices/{mac}", method="DELETE")
    
    if "success" in result:
        print(f"Device {mac} deleted successfully.")

def calc_checksum_cmd(args):
    """Command to calculate MD5 checksum for a firmware file"""
    if not os.path.exists(args.file):
        print(f"Error: File not found: {args.file}")
        return
        
    checksum = calculate_file_md5(args.file)
    print(f"File: {args.file}")
    print(f"MD5 Checksum: {checksum}")

def main():
    """Main function"""
    # Load configuration
    load_config()
    
    # Create argument parser
    parser = argparse.ArgumentParser(description='OTA Update Server Admin Tools')
    subparsers = parser.add_subparsers(dest='command', help='Command')
    
    # List devices command
    list_parser = subparsers.add_parser('list', help='List all devices')
    list_parser.set_defaults(func=list_devices_cmd)
    
    # Get device command
    get_parser = subparsers.add_parser('get', help='Get device details')
    get_parser.add_argument('mac', help='Device MAC address')
    get_parser.set_defaults(func=get_device_cmd)
    
    # Add device command
    add_parser = subparsers.add_parser('add', help='Add a new device')
    add_parser.add_argument('mac', help='Device MAC address')
    add_parser.add_argument('--device-id', help='Device ID')
    add_parser.add_argument('--hardware', help='Hardware version')
    add_parser.add_argument('--version', required=True, help='Target firmware version')
    add_parser.add_argument('--firmware-url', help='URL to the firmware binary')
    add_parser.add_argument('--firmware-file', help='Path to the firmware binary file')
    add_parser.add_argument('--checksum', help='MD5 checksum of the firmware')
    add_parser.set_defaults(func=add_device_cmd)
    
    # Update device command
    update_parser = subparsers.add_parser('update', help='Update device information')
    update_parser.add_argument('mac', help='Device MAC address')
    update_parser.add_argument('--device-id', help='Device ID')
    update_parser.add_argument('--hardware', help='Hardware version')
    update_parser.add_argument('--version', help='Target firmware version')
    update_parser.add_argument('--firmware-url', help='URL to the firmware binary')
    update_parser.add_argument('--firmware-file', help='Path to the firmware binary file')
    update_parser.add_argument('--checksum', help='MD5 checksum of the firmware')
    update_parser.set_defaults(func=update_device_cmd)
    
    # Delete device command
    delete_parser = subparsers.add_parser('delete', help='Delete a device')
    delete_parser.add_argument('mac', help='Device MAC address')
    delete_parser.set_defaults(func=delete_device_cmd)
    
    # Calculate checksum command
    checksum_parser = subparsers.add_parser('checksum', help='Calculate MD5 checksum for a file')
    checksum_parser.add_argument('file', help='Path to the file')
    checksum_parser.set_defaults(func=calc_checksum_cmd)
    
    # Parse arguments
    args = parser.parse_args()
    
    # Execute command
    if hasattr(args, 'func'):
        args.func(args)
    else:
        parser.print_help()

if __name__ == '__main__':
    main()