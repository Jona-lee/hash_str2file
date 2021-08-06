#!/bin/bash

# github action secrets test
echo "secrets.SECURE_BOOT_PASS:${secrets.SECURE_BOOT_PASS}" >build.log
echo "SECURE_BOOT_PASS:${SECURE_BOOT_PASS}" >>build.log

make |tee -a build.log
