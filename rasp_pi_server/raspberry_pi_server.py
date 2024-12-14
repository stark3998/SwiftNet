from __future__ import print_function
import matplotlib.pyplot as plt
import sys
import time
import logging
from datetime import datetime
import threading
from netifaces import interfaces, ifaddresses, AF_INET
import RPi.GPIO as GPIO
from socket import *

class LightSwarm:
    VERSION_NUMBER = 7
    MY_PORT = 2910
    SWARM_SIZE = 6
    BUTTON_PIN = 20
    YELLOW_LED_PIN = 21
    PACKET_TYPES = {
        "LIGHT_UPDATE_PACKET": 0,
        "RESET_SWARM_PACKET": 1,
        "CHANGE_TEST_PACKET": 2,
        "RESET_ME_PACKET": 3,
        "DEFINE_SERVER_LOGGER_PACKET": 4,
        "LOG_TO_SERVER_PACKET": 5,
        "MASTER_CHANGE_PACKET": 6,
        "BLINK_BRIGHT_LED": 7,
    }

    def __init__(self):
        self.led_counter = 0
        self.master_id = 0
        self.master_value = 0
        self.count = 0
        self.master_average = 0
        self.time_slices = []
        self.temp_time_slice = []
        self.avg_values = []
        self.start_time = 0
        self.current_time = 0
        self.swarm_status = [["NP", 0, 0, 0, 0, 0] for _ in range(self.SWARM_SIZE)]
        self.socket = socket(AF_INET, SOCK_DGRAM)
        self.socket.bind(("", self.MY_PORT))
        self.setup_gpio()
        self.setup_7segment_and_led()
        GPIO.add_event_detect(self.BUTTON_PIN, GPIO.FALLING, callback=self.button_pressed, bouncetime=300)
        self.setup_logging()

    def setup_logging(self):
        current_time = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        log_filename = f"logfile_{current_time}.log"
        logging.basicConfig(filename=log_filename, level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
        logging.info("Logging setup complete.")

    def setup_gpio(self):
        GPIO.setwarnings(False)
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(self.BUTTON_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.setup(self.YELLOW_LED_PIN, GPIO.OUT)

    def send_packet(self, packet_type, swarm_id=0, seconds=0):
        self.socket.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
        data = [int("F0", 16).to_bytes(1, "little"), int(packet_type).to_bytes(1, "little")]
        if packet_type == self.PACKET_TYPES["DEFINE_SERVER_LOGGER_PACKET"]:
            for iface_name in interfaces():
                addresses = [i["addr"] for i in ifaddresses(iface_name).setdefault(AF_INET, [{"addr": "No IP addr"}])]
            my_ip = addresses[0].split(".")
            data.extend([int(my_ip[i]).to_bytes(1, "little") for i in range(4)])
        elif packet_type == self.PACKET_TYPES["BLINK_BRIGHT_LED"]:
            data.append(int(self.swarm_status[swarm_id][5]).to_bytes(1, "little"))
            data.append(int(self.VERSION_NUMBER).to_bytes(1, "little"))
            data.append(int(min(seconds * 10, 126)).to_bytes(1, "little"))
        else:
            data.extend([int(0x00).to_bytes(1, "little") for _ in range(10)])
        data.append(int(0x0F).to_bytes(1, "little"))
        self.socket.sendto(b"".join(data), ("<broadcast>".encode(), self.MY_PORT))
        logging.info(f"Packet sent: Type={packet_type}, SwarmID={swarm_id}, Seconds={seconds}")

    def display_led_matrix(self):
        self.hc595_shift(self.code_L[self.led_counter % 8], self.SDI_led, self.SRCLK_led, self.RCLK_led)
        self.hc595_shift(self.code_H[int(self.master_average) // 145], self.SDI_led, self.SRCLK_led, self.RCLK_led)
        self.led_counter += 1
        self.master_average = 0
        self.count = 0
        logging.info("LED matrix displayed.")

    def clear_display(self, sdi, srclk, rclk):
        for _ in range(8):
            GPIO.output(sdi, 1)
            GPIO.output(srclk, GPIO.HIGH)
            GPIO.output(srclk, GPIO.LOW)
        GPIO.output(rclk, GPIO.HIGH)
        GPIO.output(rclk, GPIO.LOW)

    def hc595_shift(self, data, sdi, srclk, rclk):
        for i in range(8):
            GPIO.output(sdi, 0x80 & (data << i))
            GPIO.output(srclk, GPIO.HIGH)
            GPIO.output(srclk, GPIO.LOW)
        GPIO.output(rclk, GPIO.HIGH)
        GPIO.output(rclk, GPIO.LOW)

    def pick_digit(self, digit):
        for pin in self.place_pins:
            GPIO.output(pin, GPIO.LOW)
        GPIO.output(self.place_pins[digit], GPIO.HIGH)

    def display_7segment(self):
        for _ in range(50):
            for i in range(4):
                self.pick_digit(i)
                self.hc595_shift(self.number[(self.master_id // (10 ** i)) % 10], self.SDI, self.SRCLK, self.RCLK)
                time.sleep(0.01)
        self.clear_display(self.SDI, self.SRCLK, self.RCLK)
        logging.info("7-segment display updated.")

    def setup_7segment_and_led(self):
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(self.SDI, GPIO.OUT)
        GPIO.setup(self.RCLK, GPIO.OUT)
        GPIO.setup(self.SRCLK, GPIO.OUT)
        GPIO.setup(self.SDI_led, GPIO.OUT)
        GPIO.setup(self.SRCLK_led, GPIO.OUT)
        GPIO.setup(self.RCLK_led, GPIO.OUT)
        GPIO.output(self.YELLOW_LED_PIN, GPIO.LOW)
        for pin in self.place_pins:
            GPIO.setup(pin, GPIO.OUT)

    def parse_log_packet(self, message):
        incoming_swarm_id = self.set_and_return_swarm_id(message[2])
        log_string = "".join(chr(message[i + 5]) for i in range(message[3]))
        swarm_list = log_string.split("|")
        self.master_value = swarm_list[0].split(",")[3]
        self.master_id = int(message[2])
        thread_names = [t.name for t in threading.enumerate()]
        if "Display7" not in thread_names:
            threading.Thread(target=self.display_7segment, name="Display7").start()
        self.master_average = (self.count * self.master_average + int(self.master_value)) / (self.count + 1)
        self.count += 1
        if "DisplayLED" not in thread_names:
            threading.Thread(target=self.display_led_matrix, name="DisplayLED").start()
        logging.info(f"Log packet parsed: SwarmID={incoming_swarm_id}, MasterValue={self.master_value}")
        return log_string

    def set_and_return_swarm_id(self, incoming_id):
        for i in range(self.SWARM_SIZE):
            if self.swarm_status[i][5] == incoming_id:
                return i
            elif self.swarm_status[i][5] == 0:
                self.swarm_status[i][5] = incoming_id
                return i
        old_swarm_id = min(range(self.SWARM_SIZE), key=lambda i: self.swarm_status[i][1])
        self.swarm_status[old_swarm_id][5] = incoming_id
        logging.info(f"Swarm ID set: OldSwarmID={old_swarm_id}, IncomingID={incoming_id}")
        return old_swarm_id

    def logging_function(self, incoming_swarm_id):
        swarm_list = self.log_string.split("|")
        self.log_graph[incoming_swarm_id] += 1
        current_time = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        new_log_filename = f"logfile_{current_time}.log"
        logging.basicConfig(filename=new_log_filename, level=logging.INFO, format="%(asctime)s - %(message)s")
        logging.info(f"Master Device: {incoming_swarm_id} | Duration: {self.log_graph[incoming_swarm_id]} seconds")
        logging.info(f"Raw Data: {swarm_list}")

    def load_values(self, start_time):
        if int(time.time() - start_time) % 4 == 0:
            if len(self.time_slices) > 8:
                self.time_slices = self.time_slices[-7:]
            self.time_slices.append(self.temp_time_slice)
            self.temp_time_slice = []
            self.avg_values = [sum(i) / len(i) for i in self.time_slices]
            logging.info(f"Avg Values: {self.avg_values}")
            time.sleep(1)
        else:
            swarm_list = self.log_string.split("|")
            self.master_value = swarm_list[0].split(",")[3]
            self.temp_time_slice.append(int(self.master_value))

    def button_pressed(self, channel):
        logging.info("Button pressed.")
        for handler in logging.root.handlers[:]:
            logging.root.removeHandler(handler)
            handler.close()
        self.send_packet(self.PACKET_TYPES["RESET_SWARM_PACKET"])
        GPIO.output(self.YELLOW_LED_PIN, GPIO.HIGH)
        time.sleep(3)
        GPIO.output(self.YELLOW_LED_PIN, GPIO.LOW)
        current_time = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        new_log_filename = f"logfile_{current_time}.log"
        file_handler = logging.FileHandler(new_log_filename)
        file_handler.setLevel(logging.INFO)
        file_handler.setFormatter(logging.Formatter("%(asctime)s - %(message)s"))
        logging.root.addHandler(file_handler)
        self.count_graph = 0

    def run(self):
        try:
            self.start_time = time.time()
            logging.info("LightSwarm started.")
            while True:
                if int(time.time() - self.start_time) % 30 == 0:
                    self.time_slices = self.time_slices[-8:]
                    time.sleep(1)
                data = self.socket.recvfrom(1024)
                message = data[0]
                addr = data[1]
                if len(message) == 14:
                    packet_type = message[1]
                    if packet_type == self.PACKET_TYPES["LIGHT_UPDATE_PACKET"]:
                        incoming_swarm_id = self.set_and_return_swarm_id(message[2])
                        self.swarm_status[incoming_swarm_id][0] = "P"
                        self.swarm_status[incoming_swarm_id][1] = time.time()
                        logging.info(f"Light update packet received: SwarmID={incoming_swarm_id}, Addr={addr}")
                    elif packet_type in self.PACKET_TYPES.values():
                        logging.info(f"Packet received: Type={packet_type}, Addr={addr}")
                elif message[1] == self.PACKET_TYPES["LOG_TO_SERVER_PACKET"]:
                    incoming_swarm_id = self.set_and_return_swarm_id(message[2])
                    self.log_string = self.parse_log_packet(message)
                    self.logging_function(incoming_swarm_id)
                    self.load_values(self.start_time)
                else:
                    logging.error(f"Error: Invalid message length = {len(message)}")
        except KeyboardInterrupt:
            logging.info("Program terminated by user.")

if __name__ == "__main__":
    light_swarm = LightSwarm()
    light_swarm.run()
