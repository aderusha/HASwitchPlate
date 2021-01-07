#!/usr/bin/env bash
###############################################################################
# deployhasp.sh - Configure Home Assistant for HASP integration, then download
# the latest HASP automation package and modify for the provided device name
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

# Check for write access to configuration.yaml
if [ ! -w configuration.yaml ]
then
  echo "ERROR: Cannot write to 'configuration.yaml'.  Exiting."
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
if [ "$hasp_input_name" != "$hasp_device" ]
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
    echo "         Please add the following lines to your configuration:"
    echo ""
    echo "homeassistant:"
    echo "  packages: !include_dir_named packages"
    echo "==========================================================================="
  else
    if grep "^homeassistant:" configuration.yaml > /dev/null
    then
      sed -i 's/^homeassistant:.*/homeassistant:\n  packages: !include_dir_named packages/' configuration.yaml
    elif grep "^default_config:" configuration.yaml > /dev/null
    then
      sed -i 's/^default_config:.*/default_config:\nhomeassistant:\n  packages: !include_dir_named packages/' configuration.yaml
    else
      echo "==========================================================================="
      echo "WARNING: Could not add package declaration to 'configuration.yaml'."
      echo "         Please add the following lines to your configuration:"
      echo ""
      echo "default_config:"
      echo "homeassistant:"
      echo "  packages: !include_dir_named packages"
      echo "==========================================================================="
    fi
  fi
fi

# Enable recorder if not enabled to persist relevant values
if ! grep "^recorder:" configuration.yaml > /dev/null
then
  echo >> configuration.yaml
  echo "\nrecorder:" >> configuration.yaml
fi

# Check if we have a scenes.yaml and create some simple examples if the file is empty
if [ -f scenes.yaml ]
then
  if [[ $(< scenes.yaml) == "[]" ]]
  then
    echo "INFO: empty scenes.yaml found, creating demonstration scenes"
    echo "- id: '"$(( ${EPOCHREALTIME/./} / 1000 ))"'" > scenes.yaml
    echo "  name: ${hasp_device} Backlight 100%" >> scenes.yaml
    echo "  entities:" >> scenes.yaml
    echo "    light.${hasp_device}_backlight:" >> scenes.yaml
    echo "      state: 'on'" >> scenes.yaml
    echo "      brightness: 255" >> scenes.yaml
    sleep 0.01
    echo "- id: '"$(( ${EPOCHREALTIME/./} / 1000 ))"'" >> scenes.yaml
    echo "  name: ${hasp_device} Backlight 75%" >> scenes.yaml
    echo "  entities:" >> scenes.yaml
    echo "    light.${hasp_device}_backlight:" >> scenes.yaml
    echo "      state: 'on'" >> scenes.yaml
    echo "      brightness: 191" >> scenes.yaml
    sleep 0.01
    echo "- id: '"$(( ${EPOCHREALTIME/./} / 1000 ))"'" >> scenes.yaml
    echo "  name: ${hasp_device} Backlight 50%" >> scenes.yaml
    echo "  entities:" >> scenes.yaml
    echo "    light.${hasp_device}_backlight:" >> scenes.yaml
    echo "      state: 'on'" >> scenes.yaml
    echo "      brightness: 128" >> scenes.yaml
    sleep 0.01
    echo "- id: '"$(( ${EPOCHREALTIME/./} / 1000 ))"'" >> scenes.yaml
    echo "  name: ${hasp_device} Backlight 25%" >> scenes.yaml
    echo "  entities:" >> scenes.yaml
    echo "    light.${hasp_device}_backlight:" >> scenes.yaml
    echo "      state: 'on'" >> scenes.yaml
    echo "      brightness: 64" >> scenes.yaml
  fi
fi

# Create a temp dir
hasp_temp_dir=`mktemp -d`

# Download latest packages
wget -q -P $hasp_temp_dir https://github.com/aderusha/HASwitchPlate/raw/dev/Home_Assistant/hasppackages.tar.gz
tar -zxf $hasp_temp_dir/hasppackages.tar.gz -C $hasp_temp_dir
rm $hasp_temp_dir/hasppackages.tar.gz

# Rename things if we are calling it something other than plate01
if [[ "$hasp_input_name" != "plate01" ]]
then
  # rename text in contents of files
  sed -i -- 's/plate01/'"$hasp_device"'/g' $hasp_temp_dir/packages/plate01/hasp_plate01_*.yaml
  sed -i -- 's/plate01/'"$hasp_device"'/g' $hasp_temp_dir/hasp-examples/plate01/hasp_plate01_*.yaml
  sed -i -- 's/plate01/'"$hasp_device"'/g' $hasp_temp_dir/packages/hasp_plate01_lovelace.txt

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
  # rename the lovelace UI file
  mv $hasp_temp_dir/packages/hasp_plate01_lovelace.txt $hasp_temp_dir/packages/hasp_${hasp_device}_lovelace.txt
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
    rm ./packages/hasp_${hasp_device}_lovelace.txt
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
echo "Check the file packages/hasp_${hasp_device}_lovelace.txt for a set of"
echo "basic Lovelace UI elements you can include in your configuration"
echo "to manage the new device."
echo ""
echo "Here are the contents of that file to paste into your Lovelace config."
echo "Hold 'shift' while selecting this text to copy to your clipboard:"
echo "==========================================================================="
echo ""

cat packages/hasp_${hasp_device}_lovelace.txt