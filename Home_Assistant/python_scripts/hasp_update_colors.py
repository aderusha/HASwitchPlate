###############################################################################
#  @author       :  Rohan Kapoor
#  @date         :  08/26/2018
#  @name         :  hasp_update_colors.py
#  @description  :  Handles updating background/foreground colors for
#                   a HASP device. At least one of background/foreground
#                   must be provided
#  @params       :  All params are taken as strings from Home Assistant
#   nodename: the name of the hasp device, eg: plat01
#   object_id: the id of the object on the HASP you want to update, eg: p[2].b[5]
#   background: the color to display on the background on the HASP device
#   foreground: the color to display on the foreground on the HASP device
###############################################################################

nodename = data.get('nodename')
object_id = data.get('object_id')
background = data.get('background')
foreground = data.get('foreground')

base_topic = 'hasp/{}/command/{}'.format(nodename, object_id)

if background:
    background_payload = {
        'topic': '{}.bco'.format(base_topic),
        'payload': '{}'.format(background)
    }
    hass.services.call('mqtt', 'publish', background_payload)

if foreground:
    foreground_payload = {
        'topic': '{}.pco'.format(base_topic),
        'payload': '{}'.format(foreground)
    }
    hass.services.call('mqtt', 'publish', foreground_payload)
