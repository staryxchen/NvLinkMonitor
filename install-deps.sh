#!/bin/bash

# NvLink Monitor Dependencies Installation Script
# This script installs the required dependencies for building and running both nvlink_monitor and nvlink_bw_test

set -e  # Exit on any error

echo "Installing dependencies for NvLink Monitor and Bandwidth Test..."

# Detect the operating system
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
    VER=$VERSION_ID
elif type lsb_release >/dev/null 2>&1; then
    OS=$(lsb_release -si)
    VER=$(lsb_release -sr)
elif [ -f /etc/lsb-release ]; then
    . /etc/lsb-release
    OS=$DISTRIB_ID
    VER=$DISTRIB_RELEASE
elif [ -f /etc/debian_version ]; then
    OS=Debian
    VER=$(cat /etc/debian_version)
elif [ -f /etc/SuSe-release ]; then
    OS=openSUSE
elif [ -f /etc/redhat-release ]; then
    OS=RedHat
else
    OS=$(uname -s)
    VER=$(uname -r)
fi

echo "Detected OS: $OS $VER"

# Install dependencies based on OS
case $OS in
    *Ubuntu*|*Debian*|*"Linux Mint"*)
        echo "Installing dependencies for Ubuntu/Debian..."
        sudo apt-get update
        sudo apt-get install -y build-essential libnvidia-ml-dev
        
        # Install CUDA toolkit for nvlink_bw_test
        echo "Installing CUDA toolkit..."
        if ! command -v nvcc &> /dev/null; then
            echo "CUDA toolkit not found. Installing CUDA development packages..."
            sudo apt-get install -y nvidia-cuda-toolkit
        else
            echo "CUDA toolkit already installed."
        fi
        
        echo "Dependencies installed successfully!"
        ;;
    *CentOS*|*"Red Hat"*|*RHEL*|*Fedora*|*Rocky*|*AlmaLinux*)
        echo "Installing dependencies for CentOS/RHEL..."
        sudo yum groupinstall -y "Development Tools"
        sudo yum install -y nvidia-devel
        
        # Install CUDA toolkit for nvlink_bw_test
        echo "Installing CUDA toolkit..."
        if ! command -v nvcc &> /dev/null; then
            echo "CUDA toolkit not found. Installing CUDA development packages..."
            sudo yum install -y cuda-toolkit
        else
            echo "CUDA toolkit already installed."
        fi
        
        echo "Dependencies installed successfully!"
        ;;
    *openSUSE*|*SUSE*)
        echo "Installing dependencies for openSUSE..."
        sudo zypper install -y gcc-c++ make nvidia-devel
        
        # Install CUDA toolkit for nvlink_bw_test
        echo "Installing CUDA toolkit..."
        if ! command -v nvcc &> /dev/null; then
            echo "CUDA toolkit not found. Installing CUDA development packages..."
            sudo zypper install -y cuda-toolkit
        else
            echo "CUDA toolkit already installed."
        fi
        
        echo "Dependencies installed successfully!"
        ;;
    *Arch*|*Manjaro*)
        echo "Installing dependencies for Arch Linux..."
        sudo pacman -S --needed base-devel nvidia-utils
        
        # Install CUDA toolkit for nvlink_bw_test
        echo "Installing CUDA toolkit..."
        if ! command -v nvcc &> /dev/null; then
            echo "CUDA toolkit not found. Installing CUDA development packages..."
            sudo pacman -S --needed cuda
        else
            echo "CUDA toolkit already installed."
        fi
        
        echo "Dependencies installed successfully!"
        ;;
    *)
        echo "Unsupported operating system: $OS"
        echo "Please install the following packages manually:"
        echo "- gcc/g++ compiler"
        echo "- make"
        echo "- nvidia-ml development libraries"
        echo "- CUDA toolkit (for nvlink_bw_test)"
        exit 1
        ;;
esac

# Check if CUDA is properly installed
echo ""
echo "Checking CUDA installation..."
if command -v nvcc &> /dev/null; then
    echo "✓ CUDA toolkit found: $(nvcc --version | head -n1)"
else
    echo "⚠ Warning: CUDA toolkit not found. nvlink_bw_test may not build properly."
    echo "  Please install CUDA toolkit manually if you need the bandwidth test tool."
fi

# Check if NVML is available
echo "Checking NVML installation..."
if [ -f "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1" ] || [ -f "/usr/lib64/libnvidia-ml.so.1" ]; then
    echo "✓ NVML library found"
else
    echo "⚠ Warning: NVML library not found. nvlink_monitor may not build properly."
fi

echo ""
echo "Dependencies installation completed!"
echo "You can now build the project using: make"
echo ""
echo "Available build targets:"
echo "  make        - Build both nvlink_monitor and nvlink_bw_test"
echo "  make monitor - Build only nvlink_monitor"
echo "  make example - Build only nvlink_bw_test" 