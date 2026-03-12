from fastapi import FastAPI
from pydantic import BaseModel
from typing import Optional
from fastapi.middleware.cors import CORSMiddleware
from model import Battery, battery_data
import asyncio
from contextlib import asynccontextmanager
from mangum import Mangum

@asynccontextmanager
async def lifespan(app: FastAPI):
    async def protection_loop():
        while True:
            b1.check_stop()
            b1.auto_stop()
            await asyncio.sleep(0.5)
    
    task = asyncio.create_task(protection_loop())
    yield
    task.cancel()

app = FastAPI(lifespan=lifespan)

origins = ["*"]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

b1 = Battery()




#-------------frontend - backend communication----------------

@app.get("/")
async def connectivity_ping():
    return {"message": "connected"}

@app.get("/data")
async def data():
    data = b1.get_data()
    return {
        "voltage": data.voltage,
        "current": data.current,
        "temperature": data.temperature
    }

@app.get("/state")
async def get_state():
    return {"state": b1.get_state(), "warning_messages": b1.warning_messages}

@app.post("/charge")
async def charge():
    b1.start_charging()
    return { "state": b1.get_state(), "message": "Charging started"}

@app.post("/discharge")
async def discharge():
    b1.start_discharging()
    return {"state": b1.get_state(), "message": "Discharging started"}

@app.post("/stop")
async def stop():
    b1.manual_stop()
    return {"state": b1.get_state(), "message": "Battery stopped"}

@app.get("/s_o_c")
async def get_s_o_c():
    return {"s_o_c": b1.s_o_c()}

@app.get("/s_o_h")
async def get_s_o_h():
    return {"s_o_h": b1.s_o_h()}

@app.get("/r_u_l")
async def get_r_u_l():
    return {"r_u_l": b1.r_u_l()}






#-------------backend - Hardware  communication----------------

@app.post("/update")
async def update_battery(data: battery_data):
    b1.update(data)
    # Automatically check safety after updating data
    b1.check_stop()
    return {
        "message": "Battery data updated",
    }


@app.get("/state-hardware")
async def get_state_hardware():
    return {"state": b1.get_state()}


handler = Mangum(app)




