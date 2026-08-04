from typing import Union
from enum import Enum
import zlib
import struct
import random

import serial
import serial.tools.list_ports


class USBADC:
    magic_bytes = b"\xAA\x55\xAA\x55"
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
            ERROR = 0x83
            VERSION = 0x84

    class ADC_Channel(Enum):
        POWER_METER = 0
        USB_SENSE = 1
        TEMPERATURE = 2
        PIN_A3 = 3

    def __init__(self):
        self.connection = USBADC._get_connection()
        self.next_message_id = 0

    @staticmethod
    def _get_connection() -> serial.Serial:
        usbadcs = list(serial.tools.list_ports.grep("USBADC"))
        print(usbadcs)
        if usbadcs == []:
            raise FileNotFoundError("Aucun USBADC n'est branché")
        else:
            usbadc = usbadcs[0]

        return serial.Serial(usbadc.device, baudrate=115200)

    def _get_message_id_bytes(self) -> bytes:
        data = self.next_message_id.to_bytes(2, "big")

        self.next_message_id += 1
        self.next_message_id %= 1 << 16;

        return data

    def _send_data(self, data: bytes):
        checksum = zlib.crc32(data).to_bytes(4, "big")
        print(f"Sending {data + checksum}")
        self.connection.write(data + checksum)

    def _send_command(self, command: Command.Request, data: bytes = b""):
        self._send_data(
            self.magic_bytes +
            self._get_message_id_bytes() +
            command.value.to_bytes(1) +
            len(data).to_bytes(1) +
            data
        )

    def disconnect(self):
        self.connection.close()

    def ping(self):
        random_payload = random.randbytes(4)
        self._send_command(self.Command.Request.PING, random_payload)

    def read_adc(self, channel: ADC_Channel) -> float:
        """
        Returns the voltage on the given ADC channel
        """
        print(channel.value)
        self._send_command(self.Command.Request.READ_ADC, channel.value.to_bytes(1))
        # TODO: Return read value

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
