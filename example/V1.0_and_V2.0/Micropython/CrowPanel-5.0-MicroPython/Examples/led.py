#Make by Elecrow
#Web：www.elecrow.com

import time
from machine import Pin
pin38 = Pin(38, Pin.OUT)
while True:

    pin38.value(1)
    time.sleep(0.5)
    pin38.value(0)
    time.sleep(0.5)