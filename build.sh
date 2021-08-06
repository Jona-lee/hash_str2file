#!/bin/bash

# github action secrets test
echo "SECRETS_SECURE_BOOT_PASS:${SECRETS_SECURE_BOOT_PASS}" >build.log

make |tee -a build.log
