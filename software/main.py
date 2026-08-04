import time

from USBADC import USBADC


usbadc = USBADC()
usbadc.write_pin("GPIOB", 0, False)
usbadc.write_pin("GPIOB", 0, False)
usbadc.write_pin("GPIOB", 0, False)
usbadc.write_pin("GPIOB", 0, False)
usbadc.read_adc(USBADC.ADC_Channel.USB_SENSE)

time.sleep(10)
