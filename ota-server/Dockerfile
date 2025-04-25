FROM python:3.9-slim

WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application code
COPY . .

# Create firmware directory if it doesn't exist
RUN mkdir -p firmware

# Expose the port the app runs on
EXPOSE 5000

# Command to run the application
CMD ["python", "app.py"]