from flask import Flask, abort, render_template, jsonify
import random
from serial.serialutil import SerialException
import time

from usbadc.USBADC import USBADC
from usbadc.packets import ADCChannel

app = Flask(__name__)
usbadc = USBADC()
def connect_usbadc():
    global usbadc
    usbadc = USBADC()


@app.route("/")
def index():
    return render_template("index.html")

@app.route("/data/power_meter")
def data_power_meter():
    while True:
        try:
            return jsonify({
                "timestamp": time.time(),
                "value": usbadc.read_adc(ADCChannel.POWER_METER),
            })
        except TimeoutError:
            pass

@app.route("/data/temperature")
def data_temperature():
    while True:
        try:
            return jsonify({
                "timestamp": time.time(),
                "value": usbadc.read_adc(ADCChannel.TEMPERATURE),
            })
        except TimeoutError:
            pass
        except SerialException:
            connect_usbadc()
        except OSError:
            connect_usbadc()

@app.route("/data/pin_a3")
def data_pin_a3():
    while True:
        try:
            return jsonify({
                "timestamp": time.time(),
                "value": usbadc.read_adc(ADCChannel.PIN_A3),
            })
        except TimeoutError:
            pass

@app.route("/set_pin/<pin_str>/<state_str>")
def set_pin(pin_str: str, state_str: str):
    if len(pin_str) <= 2 or pin_str[0] != "P" or pin_str[1] not in "AB" or not 0 <= int(pin_str[2:]) < 16:
        abort(400, description="invalid pin")

    port = "GPIO" + pin_str[1]
    pin = int(pin_str[2:])

    state: bool
    if state_str.lower() in ["true", "1", "on"]:
        state = True
    elif state_str.lower() in ["false", "0", "off"]:
        state = False
    else:
        abort(400, description="invalid state")

    while True:
        try:
            usbadc.write_pin(port, pin, state)
            return {}
        except TimeoutError:
            pass


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
