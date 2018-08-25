###############################################################################
#  @author       :  Rohan Kapoor
#  @date         :  08/24/2018
#  @name         :  hasp_update_message.py
#  @description  :  Handles publishing new text with the correct font size
#                   adjustments for a HASP device
#  @params       :  All params are taken as strings from Home Assistant
#   nodename: the name of the hasp device, eg: plat01
#   object_id: the id of the object on the HASP you want to update, eg: p[2].b[5]
#   text: the text that should be sent to the HASP device
#   update_font: Update the font when updating the text (defaults to true)
###############################################################################

def get_font_size(text):
    text_length = len(text)
    if text_length <= 6:
        return 3
    elif text_length <= 10:
        return 2
    elif text_length <= 15:
        return 1
    else:
        return 0

nodename = data.get('nodename')
object_id = data.get('object_id')
text = data.get('text')
update_font = data.get('update_font')

base_topic = 'hasp/{}/command/{}'.format(nodename, object_id)

text_payload = {
    'topic': '{}.txt'.format(base_topic),
    'payload': '\"{}\"'.format(text)
}
hass.services.call('mqtt', 'publish', text_payload)

if not update_font == 'false':
    font_payload = {
        'topic': '{}.font'.format(base_topic),
        'payload': get_font_size(text)
    }
    hass.services.call('mqtt', 'publish', font_payload)
