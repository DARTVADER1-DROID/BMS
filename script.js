// API Configuration
const API_BASE_URL = 'http://localhost:8000';

// DOM Elements
const elements = {
    // Connection Status
    connectionStatus: document.getElementById('connectionStatus'),
    connectionText: document.getElementById('connectionText'),
    
    // Battery Parameters
    voltage: document.getElementById('voltage'),
    current: document.getElementById('current'),
    temperature: document.getElementById('temperature'),
    voltageBar: document.getElementById('voltageBar'),
    currentBar: document.getElementById('currentBar'),
    tempBar: document.getElementById('tempBar'),
    
    // Battery Control
    state: document.getElementById('state'),
    warning: document.getElementById('warning'),
    chargeBtn: document.getElementById('chargeBtn'),
    dischargeBtn: document.getElementById('dischargeBtn'),
    stopBtn: document.getElementById('stopBtn'),
    
    // Data Input
    dataForm: document.getElementById('dataForm'),
    voltageInput: document.getElementById('voltageInput'),
    currentInput: document.getElementById('currentInput'),
    tempInput: document.getElementById('tempInput'),
    
    // Modal
    notificationModal: document.getElementById('notificationModal'),
    modalTitle: document.getElementById('modalTitle'),
    modalMessage: document.getElementById('modalMessage'),
    closeModal: document.querySelector('.close')
};

// Safe Operating Limits
const SAFE_LIMITS = {
    voltage: { min: 2.5, max: 4.2, nominal: 3.7 },
    current: { min: -1.0, max: 1.0, nominal: 0 },
    temperature: { min: -20, max: 45, nominal: 27 }
};

// API Functions
async function fetchData(endpoint, options = {}) {
    try {
        const url = `${API_BASE_URL}${endpoint}`;
        const response = await fetch(url, {
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            },
            ...options
        });
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const data = await response.json();
        updateConnectionStatus(true);
        return data;
    } catch (error) {
        console.error('API Error:', error);
        showNotification('Error', 'Failed to connect to server. Please check if the backend is running on http://localhost:8000.');
        updateConnectionStatus(false);
        return null;
    }
}

// Status Updates
function updateConnectionStatus(isConnected) {
    elements.connectionStatus.className = `status-dot ${isConnected ? 'connected' : 'disconnected'}`;
    elements.connectionText.textContent = isConnected ? 'Connected' : 'Disconnected';
}

function updateParameterBar(element, value, min, max) {
    const percentage = ((value - min) / (max - min)) * 100;
    const clampedPercentage = Math.max(0, Math.min(100, percentage));
    element.style.width = `${clampedPercentage}%`;
    
    // Update color based on percentage
    if (clampedPercentage < 20) {
        element.style.background = 'linear-gradient(90deg, #ef4444 0%, #dc2626 100%)';
    } else if (clampedPercentage < 80) {
        element.style.background = 'linear-gradient(90deg, #10b981 0%, #059669 100%)';
    } else {
        element.style.background = 'linear-gradient(90deg, #f59e0b 0%, #d97706 100%)';
    }
}

function updateWarningLevel(warning) {
    elements.warning.textContent = warning;
    
    if (warning === 'Nominal') {
        elements.warning.className = 'warning-value';
    } else if (warning.includes('Over') || warning.includes('Under')) {
        elements.warning.className = 'warning-value danger';
    }
}

// Battery Parameter Updates
async function updateBatteryData() {
    const data = await fetchData('/data');
    if (data) {
        elements.voltage.textContent = data.voltage.toFixed(1);
        elements.current.textContent = data.current.toFixed(1);
        elements.temperature.textContent = data.temperature.toFixed(0);
        
        updateParameterBar(elements.voltageBar, data.voltage, SAFE_LIMITS.voltage.min, SAFE_LIMITS.voltage.max);
        updateParameterBar(elements.currentBar, data.current, SAFE_LIMITS.current.min, SAFE_LIMITS.current.max);
        updateParameterBar(elements.tempBar, data.temperature, SAFE_LIMITS.temperature.min, SAFE_LIMITS.temperature.max);
    }
}

async function updateBatteryState() {
    const stateData = await fetchData('/state');
    if (stateData) {
        elements.state.textContent = stateData.state;
        updateWarningLevel(stateData.warning);
    }
}

// Control Functions
async function startCharging() {
    elements.chargeBtn.disabled = true;
    const response = await fetchData('/charge', { method: 'POST' });
    
    if (response) {
        showNotification('Success', response.message || 'Charging started successfully');
        updateBatteryState();
    }
    
    elements.chargeBtn.disabled = false;
}

async function startDischarging() {
    elements.dischargeBtn.disabled = true;
    const response = await fetchData('/discharge', { method: 'POST' });
    
    if (response) {
        showNotification('Success', response.message || 'Discharging started successfully');
        updateBatteryState();
    }
    
    elements.dischargeBtn.disabled = false;
}

async function stopBattery() {
    elements.stopBtn.disabled = true;
    const response = await fetchData('/stop', { method: 'POST' });
    
    if (response) {
        showNotification('Success', response.message || 'Battery stopped successfully');
        updateBatteryState();
    }
    
    elements.stopBtn.disabled = false;
}

async function updateBatteryParameters(event) {
    event.preventDefault();
    
    const formData = {
        voltage: parseFloat(elements.voltageInput.value),
        current: parseFloat(elements.currentInput.value),
        temperature: parseFloat(elements.tempInput.value)
    };
    
    elements.dataForm.querySelector('.submit-btn').disabled = true;
    
    const response = await fetchData('/update', {
        method: 'POST',
        body: JSON.stringify(formData)
    });
    
    if (response) {
        showNotification('Success', response.message);
        updateBatteryData();
        updateBatteryState();
        elements.dataForm.reset();
    }
    
    elements.dataForm.querySelector('.submit-btn').disabled = false;
}

// Modal Functions
function showNotification(title, message) {
    elements.modalTitle.textContent = title;
    elements.modalMessage.textContent = message;
    elements.notificationModal.style.display = 'block';
}

function closeModal() {
    elements.notificationModal.style.display = 'none';
}

// Event Listeners
document.addEventListener('DOMContentLoaded', () => {
    // Control Button Events
    elements.chargeBtn.addEventListener('click', startCharging);
    elements.dischargeBtn.addEventListener('click', startDischarging);
    elements.stopBtn.addEventListener('click', stopBattery);
    
    // Form Event
    elements.dataForm.addEventListener('submit', updateBatteryParameters);
    
    // Modal Events
    elements.closeModal.addEventListener('click', closeModal);
    elements.notificationModal.addEventListener('click', (e) => {
        if (e.target === elements.notificationModal) {
            closeModal();
        }
    });
    
    // Keyboard Events
    document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape' && elements.notificationModal.style.display === 'block') {
            closeModal();
        }
    });
    
    // Initialize
    initializeDashboard();
});

// Dashboard Initialization
async function initializeDashboard() {
    // Check connection
    const pingResponse = await fetchData('/');
    if (pingResponse && pingResponse.message === 'connected') {
        updateConnectionStatus(true);
        
        // Update initial data
        await Promise.all([
            updateBatteryData(),
            updateBatteryState()
        ]);
        
        // Start periodic updates
        setInterval(async () => {
            await Promise.all([
                updateBatteryData(),
                updateBatteryState()
            ]);
        }, 2000);
    } else {
        updateConnectionStatus(false);
        showNotification('Error', 'Cannot connect to the battery management system. Please check if the backend server is running.');
    }
}

// Utility Functions
function formatNumber(value, decimals = 1) {
    return value.toFixed(decimals);
}

function validateInput(value, min, max) {
    return value >= min && value <= max;
}

// Add input validation
elements.voltageInput.addEventListener('input', () => {
    const value = parseFloat(elements.voltageInput.value);
    if (!validateInput(value, SAFE_LIMITS.voltage.min, SAFE_LIMITS.voltage.max)) {
        elements.voltageInput.style.borderColor = 'rgba(239, 68, 68, 0.6)';
    } else {
        elements.voltageInput.style.borderColor = 'rgba(255, 255, 255, 0.2)';
    }
});

elements.currentInput.addEventListener('input', () => {
    const value = parseFloat(elements.currentInput.value);
    if (!validateInput(value, SAFE_LIMITS.current.min, SAFE_LIMITS.current.max)) {
        elements.currentInput.style.borderColor = 'rgba(239, 68, 68, 0.6)';
    } else {
        elements.currentInput.style.borderColor = 'rgba(255, 255, 255, 0.2)';
    }
});

elements.tempInput.addEventListener('input', () => {
    const value = parseFloat(elements.tempInput.value);
    if (!validateInput(value, SAFE_LIMITS.temperature.min, SAFE_LIMITS.temperature.max)) {
        elements.tempInput.style.borderColor = 'rgba(239, 68, 68, 0.6)';
    } else {
        elements.tempInput.style.borderColor = 'rgba(255, 255, 255, 0.2)';
    }
});