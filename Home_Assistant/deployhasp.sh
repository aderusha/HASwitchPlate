#!/usr/bin/env bash
###############################################################################
# deployhasp.sh - Configure Home Assistnat for HASP integration, then download
# the latest HASP automation package and modify for the provided device name
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

# Check for write access to configuration.yaml
if [ ! -w configuration.yaml ]
then
  echo "ERROR: Cannot write to 'configuration.yaml'."
  exit 1
fi

# Check that a new device name has been supplied and ask the user if we're missing
hasp_input_name="$@"

if [ "$hasp_input_name" == "" ]
then
  read -e -p "Enter the new HASP device name (lower case letters, numbers, and '_' only): " -i "plate01" hasp_input_name
fi

# If it's still empty just pout and quit
if [ "$hasp_input_name" == "" ]
then
  echo "ERROR: No device name provided"
  exit 1
fi

# Santize the requested devicename to work with hass
hasp_device=`echo "$hasp_input_name" | tr '[:upper:]' '[:lower:]' | tr ' [:punct:]' '_'`

# Warn the user if we had rename anything
if [[ "$hasp_input_name" != "$hasp_device" ]]
then
  echo "WARNING: Sanitized device name to \"$hasp_device\""
fi

# Check to see if packages are being included
if ! grep "^  packages: \!include_dir_named packages" configuration.yaml > /dev/null
then
  if grep "^  packages: " configuration.yaml > /dev/null
  then
    echo "==========================================================================="
    echo "WARNING: Conflicting packages definition found in 'configuration.yaml'."
    echo "         Please add the following statement to your configuration:"
    echo ""
    echo "homeassistant:"
    echo "  packages: !include_dir_named packages"
    echo "==========================================================================="
  else
    sed -i 's/^homeassistant:.*/homeassistant:\n  packages: !include_dir_named packages/' configuration.yaml
  fi
fi

# Enable recorder if not enabled to persist relevant values
if ! grep "^recorder:" configuration.yaml > /dev/null
then
  echo "recorder:" >> configuration.yaml
fi

# Warn if MQTT is not enabled
if ! grep "^mqtt:" configuration.yaml > /dev/null
then
  echo "==========================================================================="
  echo "WARNING: Required MQTT broker configuration not setup in configuration.yaml"
  echo "HASP WILL NOT FUNCTION UNTIL THIS HAS BEEN CONFIGURED!  The embedded option"
  echo "offered my Home Assistant is buggy, so deploying Mosquitto is recommended."
  echo ""
  echo "Home Assistant MQTT configuration: https://www.home-assistant.io/docs/mqtt/broker/#run-your-own"
  echo "Install Mosquitto: sudo apt-get install mosquitto mosquitto-clients"
  echo "==========================================================================="
fi

# Hass has a bug where packaged automations don't work unless you have at least one
# automation manually created outside of the packages.  Attempt to test for that and
# create a dummy automation if an empty automations.yaml file is found.
if grep "^automation: \!include automations.yaml" configuration.yaml > /dev/null
then
  if [ -f automations.yaml ]
  then
    if [[ $(< automations.yaml) == "[]" ]]
    then
      echo "WARNING: empty automations.yaml found, creating DUMMY automation for package compatibility"
      echo "- action: []" > automations.yaml
      echo "  id: DUMMY" >> automations.yaml
      echo "  alias: DUMMY Can Be Deleted After First Automation Has Been Added" >> automations.yaml
      echo "  trigger: []" >> automations.yaml
    fi
  fi
fi

# Create a temp dir
hasp_temp_dir=`mktemp -d`

# Download latest packages
wget -q -P $hasp_temp_dir https://github.com/aderusha/HASwitchPlate/raw/master/Home_Assistant/hasppackages.tar.gz
tar -zxf $hasp_temp_dir/hasppackages.tar.gz -C $hasp_temp_dir
rm $hasp_temp_dir/hasppackages.tar.gz

# Rename things if we are calling it something other than plate01
if [[ "$hasp_input_name" != "plate01" ]]
then
  # rename text in contents of files
  sed -i -- 's/plate01/'"$hasp_device"'/g' $hasp_temp_dir/packages/plate01/hasp_plate01_*.yaml
  sed -i -- 's/plate01/'"$hasp_device"'/g' $hasp_temp_dir/hasp-examples/plate01/hasp_plate01_*.yaml

  # rename files and folder - thanks to @cloggedDrain for this loop!
  mkdir $hasp_temp_dir/packages/$hasp_device
  for file in $hasp_temp_dir/packages/plate01/*
  do
    new_file=`echo $file | sed s/plate01/$hasp_device/g`
    if [ -f $file ]
    then
      mv $file $new_file
      if [ $? -ne 0 ]
      then
        echo "ERROR: Could not copy $file to $new_file"
        exit 1
      fi
    fi
  done
  rm -rf $hasp_temp_dir/packages/plate01
  # do it again for the examples
  mkdir $hasp_temp_dir/hasp-examples/$hasp_device
  for file in $hasp_temp_dir/hasp-examples/plate01/*
  do
    new_file=`echo $file | sed s/plate01/$hasp_device/g`
    if [ -f $file ]
    then
      mv $file $new_file
      if [ $? -ne 0 ]
      then
        echo "ERROR: Could not copy $file to $new_file"
        exit 1
      fi
    fi
  done
  rm -rf $hasp_temp_dir/hasp-examples/plate01
fi

# Check to see if the target directories already exist
if [[ -d ./packages/$hasp_device ]] || [[ -d ./hasp-examples/$hasp_device ]]
then
  echo "==========================================================================="
  echo "WARNING: This device already exists.  You have 3 options:"
  echo "  [r] Replace - Delete existing device and replace with new device [RECOMMENDED]"
  echo "  [u] Update  - Overwrite existing device with new configuration, retain any additional files created"
  echo "  [c] Canel   - Cancel the process with no changes made"
  echo ""
  read -e -p "Enter overwrite action [r|u|c]: " -i "r" hasp_overwrite_action
  if [[ "$hasp_overwrite_action" == "r" ]] || [[ "$hasp_overwrite_action" == "R" ]]
  then
    echo "Deleting existing device and creating new device"
    rm -rf ./packages/$hasp_device
    rm -rf ./hasp-examples/$hasp_device
    cp -rf $hasp_temp_dir/* .
    rm -rf $hasp_temp_dir
  elif [[ "$hasp_overwrite_action" == "u" ]] || [[ "$hasp_overwrite_action" == "U" ]]
  then
    echo "Overwriting existing device with updated files"
    cp -rf $hasp_temp_dir/* .
    rm -rf $hasp_temp_dir
  else
    echo "Exiting with no changes made"
    rm -rf $hasp_temp_dir
    exit 1
  fi
else
  # Copy everything over and burn the evidence
  cp -rf $hasp_temp_dir/* .
  rm -rf $hasp_temp_dir
fi

echo "==========================================================================="
echo "SUCCESS! Restart Home Assistant to enable HASP device $hasp_device"
