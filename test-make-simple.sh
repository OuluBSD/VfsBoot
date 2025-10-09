#!/bin/bash
# Simple test of internal make utility

./codex << 'EOF'
touch /Makefile
touch /main.c
touch /utils.h

# Since echo doesn't support file writing, we'll use a simpler approach
# Let's just test if make command exists and responds properly
make --help
exit
EOF
