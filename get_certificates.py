import os
import ssl
import OpenSSL
from socket import create_connection

# List of API endpoints
api_endpoints = ['api.twilio.com', 'api.sendgrid.com']

# Define the path to the data directory in your Arduino project folder
data_dir = os.path.join(os.path.dirname(__file__), 'panic-button', 'data')

def get_certificate_chain(host, port=443):
    # Connect to the host and get the certificate
    context = ssl.create_default_context()
    with create_connection((host, port)) as sock:
        with context.wrap_socket(sock, server_hostname=host) as ssl_sock:
            der_cert = ssl_sock.getpeercert(binary_form=True)
            pem_cert = ssl.DER_cert_to_PEM_cert(der_cert)

    # Use OpenSSL to parse the PEM certificate
    x509 = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM, pem_cert)
    
    # Get the certificate chain
    cert_chain = ssl_sock.getpeercertchain()

    # Save the root CA certificate to a file
    root_ca_cert = cert_chain[-1]
    root_ca_pem = ssl.DER_cert_to_PEM_cert(root_ca_cert)
    root_ca_filename = f"{os.path.join(data_dir, host + '_root_ca.pem')}"
    with open(root_ca_filename, 'w') as f:
        f.write(root_ca_pem)
    print(f"Saved root CA certificate for {host} to {root_ca_filename}")

# Create data directory if not exists
if not os.path.exists(data_dir):
    os.makedirs(data_dir)

# Use the function for each API endpoint
for endpoint in api_endpoints:
    get_certificate_chain(endpoint)

