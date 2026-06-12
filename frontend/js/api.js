// ─────────────────────────────────────────────────────────────
// Configuration - UPDATE THESE VALUES
// ─────────────────────────────────────────────────────────────

const BACKEND_URL = 'https://your-backend.render.com';  // Change to your Render backend URL
const API_KEY = 'waterlevel890890';                      // Must match backend API_KEY

// ─────────────────────────────────────────────────────────────
// API Functions
// ─────────────────────────────────────────────────────────────

async function apiCall(endpoint, method = 'GET', data = null) {
    const url = `${BACKEND_URL}${endpoint}?api_key=${API_KEY}`;
    
    const options = {
        method: method,
        headers: {
            'Content-Type': 'application/json',
        }
    };
    
    if (data && method !== 'GET') {
        options.body = JSON.stringify(data);
    }
    
    try {
        const response = await fetch(url, options);
        if (!response.ok) {
            console.error(`HTTP ${response.status}: ${response.statusText}`);
            return null;
        }
        return await response.json();
    } catch (error) {
        console.error('API Error:', error);
        return null;
    }
}

async function getDeviceStatus() {
    return await apiCall('/api/device/status');
}

async function getDeviceHistory(days = 7) {
    return await apiCall(`/api/device/history?days=${days}`);
}

async function uploadFirmwareFile(file, version) {
    const formData = new FormData();
    formData.append('file', file);
    formData.append('version', version);
    
    const url = `${BACKEND_URL}/api/firmware/upload?api_key=${API_KEY}`;
    
    try {
        const response = await fetch(url, {
            method: 'POST',
            body: formData
        });
        if (!response.ok) {
            console.error(`Upload failed: ${response.statusText}`);
            return null;
        }
        return await response.json();
    } catch (error) {
        console.error('Upload Error:', error);
        return null;
    }
}

async function checkFirmware(currentVersion = '1.0') {
    return await apiCall(`/api/firmware/check?current_version=${currentVersion}`);
}

async function setCurrentFirmware(version) {
    const url = `${BACKEND_URL}/api/firmware/set-current/${version}?api_key=${API_KEY}`;
    
    try {
        const response = await fetch(url, { method: 'PUT' });
        if (!response.ok) {
            console.error(`Error: ${response.statusText}`);
            return null;
        }
        return await response.json();
    } catch (error) {
        console.error('Error:', error);
        return null;
    }
}

async function listFirmware() {
    return await apiCall('/api/firmware/list');
}

async function getAlerts(limit = 20) {
    return await apiCall(`/api/device/alerts?limit=${limit}`);
}

async function getFirmwareUpdateLog() {
    return await apiCall('/api/firmware/update-log');
}
