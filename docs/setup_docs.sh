#!/bin/bash
# Script to set up the documentation environment

# Create a Python virtual environment
python3 -m venv .venv

# Activate the virtual environment
source .venv/bin/activate

# Install required packages
pip install -r requirements.txt

# Create a message
echo "Documentation environment set up successfully."
echo "To activate the environment, run: source .venv/bin/activate"
echo "To build the documentation, run: make html"
echo "To view the documentation, open: build/html/index.html"
echo "To start the live documentation server, run: make livehtml" 