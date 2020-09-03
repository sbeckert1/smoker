import paho.mqtt.client as mqtt
import time

def on_connect(client, userdata, flags, rc):
    client.subscribe('smoker/#')

def on_message(client, userdata, msg):
    print('|'.join([str(int(time.time())), str(msg.topic), str(msg.payload)]), flush=True)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect('192.168.1.104')
#client.connect('localhost')

client.loop_forever()
