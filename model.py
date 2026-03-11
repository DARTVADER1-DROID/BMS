from pydantic import BaseModel
import time

class battery_data(BaseModel):
    voltage : float
    current : float
    temperature : float
    time : float
    hardware_connection : float






class Battery:
    def __init__(self):

        #default initializations
        self.voltage : float = 3.7
        self.current : float = 0
        self.temperature :float = 27


        #safe operating limits
        self.voltage_max : float = 4.2
        self.voltage_min : float = 2.5
        self.current_max : float = 1.0
        self.current_min : float = -1.0
        self.temperature_max : float = 45
        self.temperature_min : float = -20


        #battery state
        self.state_of_charge : float | None = None
        self.state_of_health : float | None = None
        self.remaining_useful_life : float | None = None
        self.possible_states = {"charging", "discharging", "idle"}
        self.state : str = "idle"
        self.possible_transitions = {
            "charging" : { "idle"},
            "discharging" : { "idle"},
            "idle" : {"charging", "discharging"}
        }


        #warning messages
        self.warning_messages : str = "Nominal"


        #lifetime tracker
        self.lastseen = time.time()
        self.hardware_connection = False


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
    

    def s_o_c(self):
        data = self.voltage * self.current * self.temperature
        self.state_of_charge = data
        return self.state_of_charge
    
    def s_o_h(self):
        data = self.voltage * self.current * self.temperature
        self.state_of_health = data
        return self.state_of_health
    
    def r_u_l(self):
        data = self.voltage * self.current * self.temperature
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
        if self.lastseen - time.time() > 3:
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
        self.time = time.time()
        self.hardware_connection = True

    def get_data(self):
        return battery_data(voltage = self.voltage, 
                            current = self.current, 
                            temperature = self.temperature, 
                            time= self.time, 
                            hardware_connection = self.hardware_connection)
    
    def get_state(self):
        return self.state
    

      
    
    
            
        
        


    