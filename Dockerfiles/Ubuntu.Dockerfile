# Use the official Ubuntu base image from Docker Hub
FROM ubuntu:latest

# Add a few required packages for building and developer tools
RUN apt-get update && apt-get install -y \
    vim \
    git \
    pipx \
    gcc-13\
    g++-13 \
    cmake

# Create a default working directory
WORKDIR /home/ubuntu/spectatord

# Copy all files & folders in the projects root directory 
# Exclude files listed in the dockerignore file
COPY ../ /home/ubuntu/spectatord

# Expose binaries installed by pipx to our path
ENV PATH="/root/.local/bin:$PATH"

# Install Conan and manage our virtual python environment through pipx
RUN pipx ensurepath && pipx install conan