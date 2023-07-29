import os
import subprocess
from socket import create_connection

# List of API endpoints
api_endpoints = [('api.twilio.com', 'twilio.crt'), ('api.sendgrid.com', 'sendgrid.crt')]

# Define the path to the data directory in your Arduino project folder
data_dir = os.path.join(os.path.dirname(__file__), 'panic-button', 'data')

def get_certificate_chain(host, cert_file, port=443):
    # Call the OpenSSL command-line tool to get the certificate chain
    command = f"openssl s_client -showcerts -connect {host}:{port} < /dev/null 2> /dev/null | openssl x509 -outform PEM"
    cert_chain = subprocess.check_output(command, shell=True)

    # Save the root CA certificate to a file
    root_ca_filename = f"{os.path.join(data_dir, cert_file)}"
    with open(root_ca_filename, 'wb') as f:
        f.write(cert_chain)
    print(f"Saved root CA certificate for {host} to {root_ca_filename}")

# Create data directory if not exists
if not os.path.exists(data_dir):
    os.makedirs(data_dir)

# Use the function for each API endpoint
for endpoint, cert_file in api_endpoints:
    get_certificate_chain(endpoint, cert_file)
