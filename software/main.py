import time

from USBADC import USBADC


usbadc = USBADC()
usbadc.ping()
usbadc.write_pin("GPIOB", 0, False)
usbadc.write_pin("GPIOB", 0, False)
usbadc.write_pin("GPIOB", 0, False)
usbadc.write_pin("GPIOB", 0, False)

while True:
    power_meter = usbadc.read_adc(USBADC.ADC_Channel.POWER_METER)
    usb_sense = usbadc.read_adc(USBADC.ADC_Channel.USB_SENSE)
    temperature = usbadc.read_adc(USBADC.ADC_Channel.TEMPERATURE)
    pin_a3 = usbadc.read_adc(USBADC.ADC_Channel.PIN_A3)

    print(f"Power meter: {power_meter}V")
    print(f"USB Sense: {usb_sense}V")
    print(f"Temperature: {temperature}V")
    print(f"Pin A3: {pin_a3}V")
    print()

    time.sleep(0.1)
