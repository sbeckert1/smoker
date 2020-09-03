import RPi.GPIO as GPIO
import paho.mqtt.client as mqtt

GPIO.setmode(GPIO.BOARD)
GPIO.setup(32,GPIO.OUT)
p = GPIO.PWM(32,50)
p.start(50)

def on_connect(client, userdata, flags, rc):
    print('on_connect')
    client.subscribe('smoker/fan/cmd')

def on_message(client, userdata, msg):
    print('msg:' + str(msg.payload))
    try:
        cmd = int(msg.payload)
        p.ChangeDutyCycle(cmd)
    except ValueError:
        print('cmd error')

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect('localhost')
client.loop_forever()
