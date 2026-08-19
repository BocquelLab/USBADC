from flask import Flask, render_template, jsonify, Response
import time
import threading
from bisect import bisect_right
from typing import List, Tuple

from usbadc.USBADC import USBADC
from usbadc.packets import ADCChannel

app = Flask(__name__)

usbadc = None
connect_lock = threading.Lock()


def interpolate(xy_values: List[Tuple[float, float]], value: float) -> float:
    if not xy_values:
        raise ValueError("xy_values cannot be empty")

    if len(xy_values) == 1:
        return xy_values[0][1]

    xs = [x for x, _ in xy_values]
    i = bisect_right(xs, value)

    if i == 0:
        return xy_values[0][1]
    if i == len(xy_values):
        return xy_values[-1][1]

    x0, y0 = xy_values[i - 1]
    x1, y1 = xy_values[i]

    return y0 + (value - x0) * (y1 - y0) / (x1 - x0)


def disconnect_usbadc():
    global usbadc

    old = usbadc
    usbadc = None

    if old is not None:
        try:
            old.disconnect()
        except Exception as e:
            print(f"Error closing USBADC: {e}")


def try_connecting_usbadc():
    global usbadc

    with connect_lock:
        if usbadc is not None:
            return True

        try:
            print("Trying to connect USBADC...")
            device = USBADC()
            usbadc = device
            print("USBADC connected")

            return True

        except Exception as e:
            print(f"USBADC connection failed: {type(e).__name__}: {e}")
            usbadc = None
            return False


def read_adc(channel):
    global usbadc

    # Connect if necessary
    if not try_connecting_usbadc():
        raise ConnectionError("USBADC not connected")

    try:
        return usbadc.read_adc(channel)

    except Exception as e:
        print(
            f"USBADC communication failed: "
            f"{type(e).__name__}: {e}"
        )

        # The existing USB connection is no longer usable.
        disconnect_usbadc()

        # Try creating a completely new USBADC object.
        if try_connecting_usbadc():
            try:
                return usbadc.read_adc(channel)
            except Exception as e2:
                print(
                    f"USBADC retry failed: "
                    f"{type(e2).__name__}: {e2}"
                )
                disconnect_usbadc()

        raise ConnectionError("USBADC communication failed")


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/data/power_meter")
def data_power_meter():
    try:
        value = read_adc(ADCChannel.POWER_METER)
        value = interpolate([
            (0.50,  20),
            (0.60,  15),
            (0.73,  10),
            (0.97,  5),
            (1.00,  0),
            (1.11, -5),
            (1.21, -10),
            (1.36, -15),
            (1.48, -20),
            (1.60, -25),
            (1.71, -30),
            (1.87, -35),
            (2.00, -40),
            (2.05, -45),
            (2.1,  -50),
        ], value)

        return jsonify({
            "timestamp": time.time(),
            "value": value,
        })

    except ConnectionError:
        return Response("No USBADC detected", status=503)

    except TimeoutError:
        return Response("USBADC timed out", status=504)

    except Exception as e:
        print(f"Power meter error: {type(e).__name__}: {e}")
        return Response("USBADC error", status=500)


@app.route("/data/temperature")
def data_temperature():
    try:
        value = read_adc(ADCChannel.TEMPERATURE)
        # Depuis la datasheet:
        # v =  0.55 + (0.045 / 20)t
        # t = (v - 0.55) / (0.045 / 20)
        print(value)
        value = (value - 0.55) / (0.045 / 20) 
        print(value)

        return jsonify({
            "timestamp": time.time(),
            "value": value,
        })

    except ConnectionError:
        return Response("No USBADC detected", status=503)

    except TimeoutError:
        return Response("USBADC timed out", status=504)

    except Exception as e:
        print(f"Temperature error: {type(e).__name__}: {e}")
        return Response("USBADC error", status=500)


@app.route("/data/pin_a3")
def data_pin_a3():
    try:
        value = read_adc(ADCChannel.PIN_A3)

        return jsonify({
            "timestamp": time.time(),
            "value": value,
        })

    except ConnectionError:
        return Response("No USBADC detected", status=503)

    except TimeoutError:
        return Response("USBADC timed out", status=504)

    except Exception as e:
        print(f"PIN A3 error: {type(e).__name__}: {e}")
        return Response("USBADC error", status=500)


@app.route("/set_pin/<pin_str>/<state_str>")
def set_pin(pin_str: str, state_str: str):
    if (
        len(pin_str) <= 2
        or pin_str[0] != "P"
        or pin_str[1] not in "AB"
    ):
        return Response("Invalid pin", status=400)

    try:
        pin = int(pin_str[2:])
    except ValueError:
        return Response("Invalid pin", status=400)

    if not 0 <= pin < 16:
        return Response("Invalid pin", status=400)

    if state_str.lower() in ("true", "1", "on"):
        state = True
    elif state_str.lower() in ("false", "0", "off"):
        state = False
    else:
        return Response("Invalid state", status=400)

    if not try_connecting_usbadc():
        return Response("No USBADC detected", status=503)

    # TODO: perform the actual GPIO operation here.

    return jsonify({
        "timestamp": time.time(),
        "pin": pin_str,
        "state": state,
    })


if __name__ == "__main__":
    try_connecting_usbadc()

    app.run(
        host="0.0.0.0",
        port=5000,
    )
