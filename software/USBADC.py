from typing import Union
from enum import Enum
from zlib import crc32
import struct
import random
from threading import Thread, Lock
from concurrent.futures import CancelledError, Future
from queue import Queue

import serial
import serial.tools.list_ports

class ChecksumDiffers(Exception):
    pass

class USBADC:
    magic_bytes = b"\xAA\x55\xAA\x55"
    minimum_packet_length = 4 + 2 + 1 + 1 + 4 # Magic bytes + message id + type + length + crc32
    class Command(Enum):
        # Server requests to client
        class Request(Enum):
            PING = 0x00
            READ_ADC = 0x01
            WRITE_PIN = 0x02
            REBOOT = 0x03
            VERSION = 0x04

        # Client responses to server
        class Response(Enum):
            PONG = 0x80
            READ_ADC = 0x81
            STATUS = 0x83
            VERSION = 0x84

    class ADC_Channel(Enum):
        POWER_METER = 0
        USB_SENSE = 1
        TEMPERATURE = 2
        PIN_A3 = 3

    class Status(Enum):
        OK = 0
        NOT_AVAILABLE = 1
        INVALID_DATA = 2
        TIMEOUT = 3
        UNKNOWN = 4


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
        all_data = b""
        while True:
            while len(all_data) == 0 or not self.write_queue.empty():
                all_data += self.write_queue.get()

            amount_written = 0
            while amount_written != len(all_data):
                amount_written += self.connection.write(all_data[amount_written:]) or 0
            all_data = b""


    def read_loop(self):
        all_bytes = b""
        while True:
            # TODO: There might be a way to read in bulk, maybe the Serial abstraction
            # doesn't expose it though.
            all_bytes += self.connection.read(1)

            # TODO: Avoid doing this at each byte read because it's expensive
            if self.magic_bytes not in all_bytes:
                continue

            # Skip the bytes that aren't part of the magic bytes
            discarded_bytes = all_bytes[:all_bytes.index(self.magic_bytes)]
            if discarded_bytes:
                # print(discarded_bytes)
                pass

            all_bytes = all_bytes[all_bytes.index(self.magic_bytes):]

            # Ensure the remaining of the packet is long enough
            if len(all_bytes) < self.minimum_packet_length:
                continue

            message_id = (all_bytes[4] << 8) + (all_bytes[5])
            try:
                message_type = self.Command.Response(all_bytes[6])
            except ValueError:
                all_bytes = all_bytes[self.minimum_packet_length:]
                continue

            data_length = all_bytes[7]
            if len(all_bytes) < self.minimum_packet_length + data_length:
                continue

            data = all_bytes[8:8 + data_length]

            bytes_to_checksum = all_bytes[: 8 + data_length] # All bytes up to the checksum
            checksum = all_bytes[8 + data_length : 8 + 4 + data_length]

            # Chop the read bytes for the rest all bytes
            all_bytes = all_bytes[8 + 4 + data_length:]

            with self.futures_lock:
                if message_id not in self.futures:
                    return

                future: Future = self.futures.pop(message_id)
                if crc32(bytes_to_checksum).to_bytes(4, "big") != checksum:
                    future.set_exception(ChecksumDiffers("Received and computed checksums are different"))
                else:
                    future.set_result((message_type, data))


    @staticmethod
    def _get_connection() -> serial.Serial:
        usbadcs = list(serial.tools.list_ports.grep("USBADC"))
        if usbadcs == []:
            raise FileNotFoundError("Aucun USBADC n'est branché")
        else:
            usbadc = usbadcs[0]

        return serial.Serial(usbadc.device, baudrate=115200)


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
        checksum = crc32(data).to_bytes(4, "big")
        self.write_queue.put(data + checksum)


    def _send_command(self, command: Command.Request, data: bytes = b""):
        future = Future()

        message_id = self._get_message_id()
        with self.futures_lock:
            self.futures[message_id] = future

        self._send_data(
            self.magic_bytes +
            message_id.to_bytes(2, "big") +
            command.value.to_bytes(1) +
            len(data).to_bytes(1) +
            data
        )

        return future.result(timeout=1)


    def disconnect(self):
        self.connection.close()


    def ping(self):
        random_payload = random.randbytes(4)
        message_type, result_data = self._send_command(self.Command.Request.PING, random_payload)

        match message_type:
            case self.Command.Response.PONG:
                if result_data != random_payload:
                    raise ValueError("PONG payload doesn't match expected payload")
            case _:
                raise ValueError("Expected a PONG response from client")


    def read_adc(self, channel: ADC_Channel) -> float:
        """
        Returns the voltage on the given ADC channel
        """
        message_type, result_data = self._send_command(self.Command.Request.READ_ADC, channel.value.to_bytes(1))
        match message_type:
            case self.Command.Response.READ_ADC:
                if len(result_data) != 2:
                    raise ValueError("Returned data has the wrong size")
                return int.from_bytes(result_data, "big") / 1000
            case self.Command.Response.STATUS:
                if len(result_data) != 1:
                    raise ValueError("Returned data has the wrong size")
                status = self.Status(result_data[0])
                if status != self.Status.OK:
                    raise ValueError(f"Returned status is not OK: {status}")

            case _:
                raise ValueError("Expected a READ_ADC or STATUS response")


    def write_pin(self, port: Union[str, int], pin: int, state: bool):
        """
        Sets the specified pin on the specified gpio port to the specified state
        If state is true, it puts the corresponding GPIO into high mode
        """
        if isinstance(port, str):
            match port:
                case "GPIOA":
                    port = 0
                case "GPIOB":
                    port = 1
                case _:
                    raise ValueError(f"Port is out of range")
        if not 0 <= pin < 16:
            raise ValueError(f"Pin is out of range");

        self._send_command(self.Command.Request.WRITE_PIN, struct.pack(">HBB", 1 << pin, port, state))


    def reboot(self):
        self._send_command(self.Command.Request.REBOOT)


    def get_firmware_version(self):
        self._send_command(self.Command.Request.VERSION)
