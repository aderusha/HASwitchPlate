#!/usr/bin/env bash
# deployhasp.sh - download the latest HASP automation package and modify for the provided device name

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
    echo "ERROR: 'configuration.yaml' not found in current directory.  Please run this script from '.homeassistant'"
    exit 1
fi

# Santize the requested devicename to work with hass
hasp_device=`echo "$hasp_input_name" | tr '[:upper:]' '[:lower:]' | tr ' [:punct:]' '_'`

# Warn the user if we had rename anything
if [[ "$hasp_input_name" != "$hasp_device" ]]
then
  echo "WARNING: Sanitized device name to \"$hasp_device\""
fi

# create a temp dir
hasp_temp_dir=`mktemp -d`

# download latest packages
wget -q -P $hasp_temp_dir https://github.com/aderusha/HASwitchPlate/raw/master/Home_Assistant/hasppackages.tar.gz
tar -zxf hasppackages.tar.gz -C $hasp_temp_dir
rm $hasp_temp_dir/hasppackages.tar.gz

# rename things if we are calling it something other than plate01
if [[ "$hasp_input_name" != "plate01" ]]
then
  # rename text in contents of files
  sed -i -- 's/plate01/'"$hasp_device"'/g' $hasp_temp_dir/packages/plate01/hasp_plate01_*.yaml
  # rename files and folder
  prename 's/hasp_plate01_/hasp_'"$hasp_device"'_/' $hasp_temp_dir/packages/plate01/hasp_plate01_*.yaml
  mv $hasp_temp_dir/packages/plate01 $hasp_temp_dir/packages/$hasp_device
fi

# copy everything over and burn the evidence
cp -rf $hasp_temp_dir/* .
rm -rf $hasp_temp_dir