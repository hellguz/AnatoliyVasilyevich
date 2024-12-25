# Use the official Python image from the Docker Hub
FROM python:3.10-slim

# Set environment variables
ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1

# Set the working directory in the container
WORKDIR /app

# Copy the requirements file into the container
COPY requirements.txt .

# Install the dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of the application code into the container
COPY . .

# Expose the port that the app runs on
EXPOSE 7462

# Command to run the application
#CMD ["gunicorn", "-k", "uvicorn.workers.UvicornWorker", "anatoliy:app", "--bind", "0.0.0.0:7462"]
CMD ["uvicorn", "anatoliy:app", "--host", "0.0.0.0", "--port", "7462", "--reload"]

