#!/usr/bin/env bash
###############################################################################
# migrate-hasp-107.sh - Modify an existing Home Assistant configuration
# with HASP deployed to accomodate breaking changes in v107
###############################################################################

# First order of business is to find 'configuration.yaml' in some likely places
if [ ! -f configuration.yaml ] # check current directory first
then
  if [ -f /config/configuration.yaml ] # next check container config dir
  then
    cd /config
  elif [ -f ~/.homeassistant/configuration.yaml ]
  then
    cd ~/.homeassistant
  else
    echo "WARNING: 'configuration.yaml' not found in current directory."
    echo "         Searching for 'configuration.yaml'..."
    foundConfigFiles=$(find / -name configuration.yaml 2>/dev/null)
    foundConfigCount=$(echo "$foundConfigFiles" | wc -l)
    if [ $foundConfigCount == 1 ]
    then
      foundConfigDir=$(dirname "${foundConfigFiles}")
      cd $foundConfigDir
      echo "INFO: configuration.yaml found under: $foundConfigDir"
    else
      echo "ERROR: Failed to locate the active 'configuration.yaml'"
      echo "       Please run this script from the homeassistant"
      echo "       configuration folder for your environment."
      exit 1
    fi
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