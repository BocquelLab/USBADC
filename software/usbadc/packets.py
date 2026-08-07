from abc import ABC, abstractmethod
import struct
from zlib import crc32
from typing import List, Literal, Self, Union
from enum import Enum

magic_bytes = b"\xAA\x55\xAA\x55"
minimum_packet_length = 4 + 2 + 1 + 1 + 4 # Magic bytes + message id + type + length + crc32

def packet_class_from_message_type(message_type: int) -> type:
    match (message_type):
        # Request packets
        case 0x00:
            return RequestPing
        case 0x01:
            return RequestReadADC
        case 0x02:
            return RequestWritePin
        case 0x03:
            return RequestReboot
        case 0x04:
            return RequestVersion

        # Response packets
        case 0x80:
            return ResponsePong
        case 0x81:
            return ResponseReadADC
        case 0x82:
            return ResponseStatus
        case 0x83:
            return ResponseVersion
        case _:
            raise ValueError(f"No Packet associated with type {message_type}")

class ADCChannel(Enum):
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

class Packet(ABC):
    id: int

    @abstractmethod
    def get_data(self) -> bytes:
        """
        Serializes the "data" part of the packet
        """
        raise NotImplemented("Not implemented for abstract base class Packet")

    def serialize(self, message_id: int):
        if not 0 <= message_id < 2**16:
            raise ValueError("`message_id` should fit in 16 bits")

        nacked_data = self.get_data()

        data = magic_bytes
        data += struct.pack(">HBB", message_id, self.id, len(nacked_data))
        data += nacked_data
        data += crc32(data).to_bytes(4, "big") # Checksum

        return data

    @classmethod
    def deserialize(cls, data: bytes) -> Self:
        _ = data
        raise NotImplemented("Not implemented for abstract base class Packet")


class RequestPing(Packet):
    id = 0x00
    def __init__(self, data: bytes):
        if len(data) != 4:
            raise ValueError("`data` field should be 4 bytes long")
        self.data = data

    def get_data(self) -> bytes:
        return self.data

    @classmethod
    def deserialize(cls, data: bytes) -> Self:
        return RequestPing(data)

class RequestReadADC(Packet):
    id = 0x01
    def __init__(self, channel: ADCChannel):
        self.channel = channel

    def get_data(self) -> bytes:
        return struct.pack(">B", self.channel.value)


class RequestWritePin(Packet):
    id = 0x02
    def __init__(self, pins: List[int], gpio_port: Union[str, int], state: bool):
        pin_mask = 0
        for pin in pins:
            if not 0 <= pin < 16:
                raise ValueError("all pin in `pins` should be between 0 and 16")
            pin_mask |= 1 << pin
        if isinstance(gpio_port, str):
            match gpio_port:
                case "GPIOA":
                    gpio_port = 0
                case "GPIOB":
                    gpio_port = 1
                case _:
                    raise ValueError("`gpio_port` should be \"GPIOA\" or \"GPIOB\"")

        self.pin_mask = pin_mask
        self.gpio_port = gpio_port
        self.state = state

    def get_data(self) -> bytes:
        return struct.pack(">HBB",
                           self.pin_mask,
                           self.gpio_port,
                           self.state)


class RequestReboot(Packet):
    id = 0x03
    def __init__(self):
        pass

    def get_data(self) -> bytes:
        return b""


class RequestVersion(Packet):
    id = 0x04
    def __init__(self):
        pass

    def get_data(self) -> bytes:
        return b""

    @classmethod
    def deserialize(cls, data: bytes) -> Self:
        if len(data) != 0:
            raise ValueError("RequestVersion packet shouldn't have any payload")
        return RequestVersion()


class ResponsePong(Packet):
    id = 0x80
    def __init__(self, data: bytes):
        if len(data) != 4:
            raise ValueError("`data` should be 4 bytes long")
        self.data = data

    def get_data(self) -> bytes:
        return self.data

    @classmethod
    def deserialize(cls, data: bytes) -> Self:
        return ResponsePong(data)

class ResponseReadADC(Packet):
    id = 0x81
    def __init__(self, voltage_mv: int):
        if not 0 <= voltage_mv < 2**16:
            raise ValueError("`voltage` should fit in 16 bits")
        self.voltage_mv = voltage_mv

    def get_data(self) -> bytes:
        return struct.pack(">H", self.voltage_mv)

    @classmethod
    def deserialize(cls, data: bytes) -> Self:
        fields = struct.unpack(">H", data)

        return ResponseReadADC(*fields)


class ResponseStatus(Packet):
    id = 0x82
    def __init__(self, status: Status):
        self.status = status

    def is_ok(self) -> bool:
        return self.status == Status.OK

    def get_data(self) -> bytes:
        return struct.pack(">B", self.status.value)

    @classmethod
    def deserialize(cls, data: bytes) -> Self:
        fields = struct.unpack(">B", data)
        reason = Status(fields[0])

        return ResponseStatus(reason)


class ResponseVersion(Packet):
    id = 0x83
    def __init__(self, version_major: int, version_minor: int, version_patch: int):
        """
        Version should follow https://semver.org/ with all numbers fitting in a single byte.
        """

        if not 0 <= version_major < 256:
            raise ValueError("`version_major` should be between 0 and 256")
        if not 0 <= version_minor < 256:
            raise ValueError("`version_minor` should be between 0 and 256")
        if not 0 <= version_patch < 256:
            raise ValueError("`version_patch` should be between 0 and 256")

        self.version_major = version_major
        self.version_minor = version_minor
        self.version_patch = version_patch

    def get_data(self) -> bytes:
        return struct.pack(">BBB", self.version_major, self.version_minor, self.version_patch)

    @classmethod
    def deserialize(cls, data: bytes) -> Self:
        fields = struct.unpack(">BBB", data)
        return RequestVersion(*fields)

    def is_up_to_date(self):
        expected_version_major = 0
        expected_version_minor = 0
        expected_version_patch = 1
        return (self.version_minajor == expected_version_major and 
                self.version_minor == expected_version_minor and
                self.version_patch == expected_version_patch)
