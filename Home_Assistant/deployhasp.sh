#!/usr/bin/env bash
###############################################################################
# deployhasp.sh - Configure Home Assistnat for HASP integration, then download
# the latest HASP automation package and modify for the provided device name
###############################################################################

# Check that a new device name has been supplied and ask the user if we're missing
hasp_input_name="$@"

if [ "$hasp_input_name" == "" ]
then
  read -e -p "Enter the new HASP device name and press [RETURN]: " -i "plate01" hasp_input_name
fi

# If it's still empty just pout and quit
if [ "$hasp_input_name" == "" ]
then
  echo "ERROR: No device name provided"
  exit 1
fi

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

# Check to see if packages are being included
if ! grep "^  packages: \!include_dir_named packages" configuration.yaml > /dev/null
then
  if grep "^  packages: " configuration.yaml > /dev/null
  then
    echo "WARNING: Conflicting packages definition found in 'configuration.yaml'."
    echo "         Please add the following statement to your configuration:"
    echo ""
    echo "homeassistant:"
    echo "  packages: !include_dir_named packages"
    echo ""
  else
    sed -i 's/^homeassistant:.*/homeassistant:\n  packages: !include_dir_named packages/' configuration.yaml
  fi
fi

# Enable recorder if not enabled to persist input values
if ! grep "^recorder:" configuration.yaml > /dev/null
then
  echo "recorder:" >> configuration.yaml
  echo "  include:" >> configuration.yaml
  echo "    domains:" >> configuration.yaml
  echo "    - automation" >> configuration.yaml
  echo "    - input_boolean" >> configuration.yaml
  echo "    - input_number" >> configuration.yaml
  echo "    - input_select" >> configuration.yaml
  echo "    - input_datetime" >> configuration.yaml
  echo "    - input_text" >> configuration.yaml
fi

# Enable python_script if not enabled to simplify HASP automations
if ! grep "^python_script:" configuration.yaml > /dev/null
then
  echo "python_script:" >> configuration.yaml
fi

# Enable MQTT if not enabled
if ! grep "^mqtt:" configuration.yaml > /dev/null
then
  echo "mqtt:" >> configuration.yaml
  # Check to see if we're running hass.io
  if [ -f /etc/alpine-release ]
  then
    echo "  broker: core-mosquitto" >> configuration.yaml
  fi
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

# Santize the requested devicename to work with hass
hasp_device=`echo "$hasp_input_name" | tr '[:upper:]' '[:lower:]' | tr ' [:punct:]' '_'`

# Warn the user if we had rename anything
if [[ "$hasp_input_name" != "$hasp_device" ]]
then
  echo "WARNING: Sanitized device name to \"$hasp_device\""
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
fi

# Copy everything over and burn the evidence
cp -rf $hasp_temp_dir/* .
rm -rf $hasp_temp_dir

# Create the python_scripts folder if it doesn't already exist
if [ ! -d python_scripts ]
then
  mkdir python_scripts
fi
# Download the hasp_update_message python script if it doesn't already exist
if [ ! -f python_scripts/hasp_update_message.py ]
then
  wget -q -P python_scripts https://github.com/aderusha/HASwitchPlate/raw/master/Home_Assistant/python_scripts/hasp_update_message.py
fi
