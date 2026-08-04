from USBADC import USBADC

usbadc = USBADC()
usbadc.write_pin("GPIOB", 0, False)
usbadc.reboot()

usbadc = USBADC()
usbadc.write_pin("GPIOB", 0, False)
usbadc.disconnect()

while True:
    data = usbadc.connection.read_until(b"\r\n")
    print(data)
