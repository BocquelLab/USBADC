from typing import List, Union
from zlib import crc32
import random
from threading import Thread, Lock
from concurrent.futures import CancelledError, Future
import platform
from queue import Queue
from .packets import Packet, RequestPing, RequestReadADC, RequestReboot, RequestVersion, RequestWritePin, ResponsePong, ResponseReadADC, ResponseStatus, ResponseVersion, ADCChannel, magic_bytes, minimum_packet_length, packet_class_from_message_type

import serial
import serial.tools.list_ports

class USBADC:
    USB_VID = 0xFFFF
    USB_PID = 0x0001

    def __init__(self):
        self.connection = USBADC._get_connection()
        self.next_message_id = 0

        self.futures = {}
        self.futures_lock = Lock()

        self.read_loop_thread = Thread(target=self.read_loop, daemon=True)
        self.read_loop_thread.start()

        self.write_queue = Queue()
        self.write_thread = Thread(target=self.write_loop, daemon=True)
        self.write_thread.start()


    def write_loop(self):
        all_bytes = b""
        while True:
            while len(all_bytes) == 0 or not self.write_queue.empty():
                all_bytes += self.write_queue.get()


            amount_written = 0
            while amount_written != len(all_bytes):
                amount_written += self.connection.write(all_bytes[amount_written:]) or 0
            self.connection.flush()
            # print(f"Send: {all_bytes}")
            all_bytes = b""


    def read_loop(self):
        all_bytes = b""
        while True:
            # TODO: There might be a way to read in bulk, maybe the Serial abstraction
            # doesn't expose it though.
            all_bytes += self.connection.read(1)

            # print(f"Received: {all_bytes}")

            # TODO: Avoid doing this at each byte read because it's expensive
            if magic_bytes not in all_bytes:
                # all_bytes = all_bytes[-4:]
                continue

            # Skip the bytes that aren't part of the magic bytes
            discarded_bytes = all_bytes[:all_bytes.index(magic_bytes)]
            if discarded_bytes:
                # print(discarded_bytes)
                pass

            all_bytes = all_bytes[all_bytes.index(magic_bytes):]

            # Ensure the remaining of the packet is long enough
            if len(all_bytes) < minimum_packet_length:
                continue


            message_id = (all_bytes[4] << 8) + (all_bytes[5])
            message_type = all_bytes[6]
            data_length = all_bytes[7]

            if len(all_bytes) < minimum_packet_length + data_length:
                continue

            data = all_bytes[8:8 + data_length]

            computed_checksum = crc32(all_bytes[: 8 + data_length]).to_bytes(4, "big") # All bytes up to the checksum
            expected_checksum = all_bytes[8 + data_length : 8 + 4 + data_length]

            # Chop the read bytes for the rest all bytes
            # print(all_bytes)
            all_bytes = all_bytes[4:]

            packet = packet_class_from_message_type(message_type).deserialize(data)

            with self.futures_lock:
                if message_id not in self.futures:
                    return

                future: Future = self.futures.pop(message_id)
                if computed_checksum != expected_checksum:
                    future.set_exception(ValueError("Received and computed checksums are different"))
                else:
                    future.set_result(packet)


    @staticmethod
    def _get_connection() -> serial.Serial:
        match platform.system():
            # Windows requires all devices to have a *.INF file describing them.
            # Since we do not have one, Windows replaces the manufacturer and product strings
            # with it's own and we can't be sure what device is behind it.
            # TODO: Initiate a ping and check if it responds with a pong.
            case "Windows":
                for port in serial.tools.list_ports.comports():
                    if port.vid != USBADC.USB_VID or port.pid != USBADC.USB_PID:
                        continue
                    return serial.Serial(port.device, baudrate=115200)

            case _:
                for port in serial.tools.list_ports.grep("USBADC"):
                    return serial.Serial(port.device, baudrate=115200)

        raise FileNotFoundError("Aucun USBADC n'est branché")


    def _get_message_id(self) -> int:
        id = self.next_message_id

        with self.futures_lock:
            if self.next_message_id in self.futures:
                future = self.futures.pop(self.next_message_id)
                future.set_exception(CancelledError)

        self.next_message_id += 1
        self.next_message_id %= 1 << 16;

        return id


    def _send_data(self, data: bytes):
        self.write_queue.put(data)


    # TODO: Change the name to indicate it also returns a response?
    def _send_command(self, command: Packet) -> Packet:
        future = Future()

        message_id = self._get_message_id()
        with self.futures_lock:
            self.futures[message_id] = future

        self._send_data(command.serialize(message_id))

        return future.result(timeout=1)


    def disconnect(self):
        self.connection.close()


    def ping(self):
        random_payload = random.randbytes(4)
        result = self._send_command(RequestPing(random_payload))

        if isinstance(result, ResponsePong):
            if result.data != random_payload:
                raise ValueError("PONG payload doesn't match expected payload")
        else:
            raise ValueError("Expected a PONG response from client")


    def read_adc(self, channel: ADCChannel) -> float:
        """
        Returns the voltage on the given ADC channel in volts
        """
        result = self._send_command(RequestReadADC(channel))

        if isinstance(result, ResponseReadADC):
            return result.voltage_mv / 1000
        elif isinstance(result, ResponseStatus):
            raise ValueError(f"Returned status: {result.status}")

        raise ValueError("Unexpected response")


    def write_pin(self, port: Union[str, int], pin: Union[List[int], int], state: bool):
        """
        Sets the specified pin on the specified gpio port to the specified state
        If state is true, it puts the corresponding GPIO into high mode
        """
        if isinstance(pin, int):
            pin = [pin]

        result = self._send_command(RequestWritePin(pin, port, state))
        if isinstance(result, ResponseStatus):
            if not result.is_ok():
                raise RuntimeError("Couldn't set the pins to the desired state")
        else:
            raise ValueError("Unexpected response")


    def reboot(self):
        """
        This also disconnects the device, if a way is found to reinitialize the USB connection, this could function could make it seem like the connection was no reset.
        """
        self._send_command(RequestReboot())
        self.disconnect()

    def get_firmware_version(self) -> ResponseVersion:
        result = self._send_command(RequestVersion())
        if isinstance(result, ResponseVersion):
            return result;
        else:
            raise ValueError("Unexpected response")
