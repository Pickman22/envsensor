import paho.mqtt.client as mqtt
import json
import logging
import datetime
import copy
from influxdb import InfluxDBClient

global db_client

json_body = {
    'measurement': '',
    'tags': {'host': '', 'region': ''},
    'time': '',
    'fields': {}
}

logging.basicConfig(level=logging.DEBUG)
logging.getLogger().setLevel(logging.DEBUG)


def on_connect(client, userdata, rc):
    logging.debug('Connection succesful: {}'.format(rc))
    client.subscribe('feedback_systems_sensors')


def on_message(client, userdata, msg):
    logging.debug('{}: {}'.format(msg.topic, msg.payload))
    try:
        datastr = copy.deepcopy(json_body)
        data = json.loads(msg.payload)
        datastr['measurement'] = data['id']
        datastr['tags']['region'] = data['reg']
        datastr['tags']['host'] = 'apt202'
        datastr['fields'] = {
            'temperature': data['t'],
            'pressure': data['p'],
            'humidity': data['h'],
            'Battery': data['v']
        }
        datastr['time'] = str(datetime.datetime.now())
        logging.debug(datastr)
        db_client.write_points([datastr])
    except ValueError:
        logging.error('Ignoring invalid JSON data: {}'.format(msg.payload))


if __name__ == '__main__':
    mqtt_args = ('iot.eclipse.org', 1883, 60)
    db_args = ('localhost', 8086, 'root', 'root', 'atmospheric_data')

    db_client = InfluxDBClient(*db_args)
    logging.debug('Creating MQTT client...')
    client = mqtt.Client()
    logging.debug('Attaching callbacks...')
    client.on_connect = on_connect
    client.on_message = on_message
    logging.debug('Attempting connection...')
    client.connect(*mqtt_args)
    logging.debug('Connection established to {}'.format(mqtt_args[0]))
    client.loop_forever()
