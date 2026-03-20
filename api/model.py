from pydantic import BaseModel
import time
import math




class battery_data(BaseModel):
    voltage : float
    current : float
    temperature : float
    time : float
    hardware_connection : bool 


class Battery:
    def __init__(self):

        #default initializations
        self.voltage : float = 3.7
        self.current : float = 0
        self.temperature : float = 27


        #safe operating limits
        self.voltage_max : float = 10
        self.voltage_min : float = -10
        self.current_max : float = 10
        self.current_min : float = -10
        self.temperature_max : float = 100
        self.temperature_min : float = -100


        #battery state
        self.state_of_charge : float | None = None
        self.state_of_health : float | None = None
        self.remaining_useful_life : float | None = None
        self.possible_states = {"charging", "discharging", "idle"}
        self.state : str = "idle"
        self.possible_transitions = {
            "charging"    : {"idle"},
            "discharging" : {"idle"},
            "idle"        : {"charging", "discharging"}
        }


        #warning messages
        self.warning_messages : str = "Nominal"


        #lifetime tracker
        self.lastseen : float = time.time()         # timestamp of last /update from hardware
        self.hardware_connection : bool = False
        self.rated_useful_life : float = 1000
        self.cycle_count : int = 112


    def validate_transition(self, new_state):
        if new_state not in self.possible_transitions[self.state]:
            return False
        self.state = new_state
        return True
    
    def validate_safety(self):
        if self.voltage > self.voltage_max:
            self.warning_messages = "Over voltage!"
            return False
        if self.voltage < self.voltage_min:
            self.warning_messages = "Under voltage!"
            return False
        if self.current > self.current_max:
            self.warning_messages = "Over current!"
            return False
        if self.current < self.current_min:
            self.warning_messages = "Under current!"
            return False
        if self.temperature > self.temperature_max:
            self.warning_messages = "Over temperature!"
            return False
        if self.temperature < self.temperature_min:
            self.warning_messages = "Under temperature!"
            return False
        self.warning_messages = "Nominal"
        return True
    
    def s_o_h(self):
        data = 0.000088875 * ((self.cycle_count - 760.58) ** 2) + (math.log(self.cycle_count / 3.97379) ** 2) + 47.917
        self.state_of_health = data
        return self.state_of_health
    
    def s_o_c(self):
        data = ((self.voltage - self.voltage_min) / (self.voltage_max - self.voltage_min) * (self.s_o_h() / 100)) * 100
        self.state_of_charge = data
        return self.state_of_charge
    
    def r_u_l(self):
        data = self.rated_useful_life - self.cycle_count 
        self.remaining_useful_life = data
        return self.remaining_useful_life

    def start_charging(self):
        if self.validate_safety():
            self.validate_transition("charging")
    
    def start_discharging(self):
        if self.validate_safety():
            self.validate_transition("discharging")
    
    def check_stop(self):
        if not self.validate_safety():
            self.validate_transition("idle")

        # FIX: was (self.lastseen - time.time() > 3) — always negative, never triggered
        # FIX: now correctly measures seconds elapsed since last /update from hardware
        if time.time() - self.lastseen > 3:
            self.validate_transition("idle")
            self.hardware_connection = False

    def auto_stop(self):
        if self.voltage >= self.voltage_max or self.voltage <= self.voltage_min:
            self.validate_transition("idle") 

    def manual_stop(self):
        self.validate_transition("idle")

    def update(self, battery_data : battery_data):
        self.voltage = battery_data.voltage
        self.current = battery_data.current
        self.temperature = battery_data.temperature
        # FIX: was (self.time = time.time()) — updated wrong field, lastseen was never refreshed
        # FIX: now correctly updates lastseen so check_stop() timeout resets on each /update
        self.lastseen = time.time()
        self.hardware_connection = True

    def get_data(self):
        return battery_data(voltage=self.voltage, 
                            current=self.current, 
                            temperature=self.temperature, 
                            time=self.lastseen, 
                            hardware_connection=self.hardware_connection)
    
    def get_state(self):
        return self.state
    
    def update_cycle_count(self, count: int):
        self.cycle_count = count
        self.s_o_h()
        self.s_o_c()
        self.r_u_l()

    def validate_soc_soh_rul(self):
        if self.state != "charging" and self.state != "discharging":
            return True
        return False