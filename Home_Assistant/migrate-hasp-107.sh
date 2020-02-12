#!/usr/bin/env bash
###############################################################################
# migrate-hasp-107.sh - Modify an existing Home Assistant configuration
# with HASP deployed to accomodate breaking changes in v107
###############################################################################

# Confirm that we're working in the .homeassistant folder by checking for configuration.yaml
if [ ! -f configuration.yaml ]
then
  echo "WARNING: 'configuration.yaml' not found in current directory."
  echo "Searching for Home Assistant 'configuration.yaml'..."
  configfile=$(find / -name configuration.yaml 2>/dev/null)
  count=$(echo "$configfile" | wc -l)
  if [ $count == 1 ]
  then
    configdir=$(dirname "${configfile}")
    cd $configdir
    echo "INFO: configuration.yaml found under: $configdir"
  else
    echo "ERROR: Failed to locate the active 'configuration.yaml'"
    echo "       Please run this script from the homeassistant"
    echo "       configuration folder for your environment."
    exit 1
  fi
fi

# If the user has already deployed HASP devices...
if [ -d packages ]
then
  # Search through existing packages to see if we have any "view:" statements to remove as they have been deprecated
  find packages -name "hasp_*.yaml" -exec sed -i '/[[:blank:]]view\:/d' {} +
fi

if [ -d hasp-examples ]
then
  # Search through existing examples to see if we have any "view:" statements to remove as they have been deprecated
  find hasp-examples -name "hasp_*.yaml" -exec sed -i '/[[:blank:]]view\:/d' {} +
fi