import os
from fastapi import FastAPI
from pydantic import BaseModel
from typing import Optional
from fastapi.middleware.cors import CORSMiddleware
from api.model import Battery, battery_data
import asyncio
from contextlib import asynccontextmanager
import httpx


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
    if b1.validate_soc_soh_rul():
        return {"s_o_c": b1.s_o_c()}
    else:
        return {"s_o_c": "Error: cannot calculate at charging or discharging"}

@app.get("/s_o_h")
async def get_s_o_h():
    if b1.validate_soc_soh_rul():
        return {"s_o_h": b1.s_o_h()}
    else:
        return {"s_o_h": "Error: cannot calculate at charging or discharging"}

@app.get("/r_u_l")
async def get_r_u_l():
    if b1.validate_soc_soh_rul():
        return {"r_u_l": b1.r_u_l()}
    else:
        return {"r_u_l": "Error: cannot calculate at charging or discharging"}

@app.post("/cycles")
async def cycles(count: int):
    b1.update_cycle_count(count)
    return {"message": "Cycle count updated"}

@app.get("/hardware_connection")
async def hardware_connection():
    return {"hardware_connection": b1.hardware_connection}

@app.post("/ai-suggestions")
async def ai_suggestions():
    GROQ_API_KEY = os.getenv("GROQ_API_KEY")
    GROQ_URL = "https://api.groq.com/openai/v1/chat/completions"

    user_input = (
        f"DATA: Voltage: {b1.voltage}V, Current: {b1.current}A, Temperature: {b1.temperature}C, "
        f"Hardware: {b1.hardware_connection}, State: {b1.get_state()}"
    )

    payload = {
        "model": "llama-3.1-8b-instant",
        "messages": [
            {
                "role": "system",
                "content": "You are a MOTOR safety model. Analyze V, C, T data for the system. Provide crisp safety advice."
            },
            {
                "role": "user",
                "content": user_input
            }
        ],
        "temperature": 0.2,
        "max_tokens": 300
    }

    async with httpx.AsyncClient() as client:
        try:
            response = await client.post(
                GROQ_URL,
                json=payload,
                headers={"Authorization": f"Bearer {GROQ_API_KEY}"},
                timeout=20.0
            )
            response.raise_for_status()

            data = response.json()
            answer = data["choices"][0]["message"]["content"]
            return {"status": "success", "response": answer}

        except httpx.HTTPStatusError as e:
            return {"status": "error", "code": e.response.status_code, "detail": e.response.text}

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







