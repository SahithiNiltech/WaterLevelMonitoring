// ─────────────────────────────────────────────────────────────
// Dashboard Logic
// ─────────────────────────────────────────────────────────────

let chart = null;
let currentFirmwareVersion = '1.1';

async function updateDashboard() {
    try {
        // Get latest status
        const status = await getDeviceStatus();
        if (status) {
            updateGauges(status);
        }

        // Get history and update chart
        const history = await getDeviceHistory(7);
        if (history && history.length > 0) {
            updateChart(history);
        }

        // Load alerts
        const alerts = await getAlerts(10);
        if (alerts) updateAlerts(alerts);

        // Load firmware update log
        const fwLog = await getFirmwareUpdateLog();
        if (fwLog) updateFirmwareLog(fwLog);

        // Load versions
        loadVersions();

    } catch (error) {
        console.error('Dashboard update error:', error);
    }
}

function updateGauges(status) {
    // Water level
    const waterLevel = Math.round(status.water_level_pct || status.water_level);
    const waterHeightCm = status.water_height_cm || 0;
    document.getElementById('waterLevel').textContent = waterLevel + '%';
    document.getElementById('waterFill').style.width = waterLevel + '%';
    document.getElementById('waterLabel').textContent = waterHeightCm.toFixed(1) + ' cm';
    
    // Battery
    const battery = status.battery;
    document.getElementById('batteryLevel').textContent = battery + '%';
    document.getElementById('batteryFill').style.width = battery + '%';
    
    // Status indicator
    let statusText = '✅ All Good';
    let statusColor = '#00d084';
    
    if (battery < 20) {
        statusText = '⚠️ Battery Low!';
        statusColor = '#ffa502';
    } else if (battery < 10) {
        statusText = '🚨 Battery Critical!';
        statusColor = '#ff6b6b';
    }
    
    if (waterLevel >= 95) {
        statusText = '🚨 Tank Full!';
        statusColor = '#ff6b6b';
    } else if (waterLevel <= 10) {
        statusText = '🚨 Tank Critical!';
        statusColor = '#ff6b6b';
    }
    
    document.getElementById('batteryLabel').textContent = statusText;
    
    // Last update
    const lastUpdate = new Date(status.timestamp);
    const now = new Date();
    const diffMs = now - lastUpdate;
    const diffMins = Math.floor(diffMs / 60000);
    
    let updateText = lastUpdate.toLocaleString();
    if (diffMins < 1) {
        updateText = 'Just now';
    } else if (diffMins < 60) {
        updateText = `${diffMins} minutes ago`;
    }
    
    document.getElementById('lastUpdate').textContent = updateText;
}

function updateChart(data) {
    const ctx = document.getElementById('historyChart').getContext('2d');
    
    const labels = data.map(d => {
        const date = new Date(d.timestamp);
        return date.toLocaleDateString('en-US', {month: 'short', day: 'numeric'}) + ' ' + 
               date.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
    });
    
    const values = data.map(d => d.water_level_pct);
    
    if (chart) {
        chart.data.labels = labels;
        chart.data.datasets[0].data = values;
        chart.update('none');  // Update without animation
    } else {
        chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Water Level (%)',
                    data: values,
                    borderColor: '#667eea',
                    backgroundColor: 'rgba(102, 126, 234, 0.1)',
                    borderWidth: 2,
                    tension: 0.4,
                    fill: true,
                    pointRadius: 4,
                    pointBackgroundColor: '#667eea',
                    pointBorderColor: '#fff',
                    pointBorderWidth: 2
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                plugins: {
                    legend: {
                        display: true,
                        position: 'top',
                        labels: {
                            boxWidth: 15,
                            padding: 15,
                            font: {
                                size: 12
                            }
                        }
                    },
                    tooltip: {
                        backgroundColor: 'rgba(0, 0, 0, 0.8)',
                        padding: 12,
                        titleFont: { size: 14 },
                        bodyFont: { size: 12 }
                    }
                },
                scales: {
                    y: {
                        min: 0,
                        max: 100,
                        grid: {
                            color: 'rgba(0, 0, 0, 0.05)'
                        },
                        ticks: {
                            callback: function(value) {
                                return value + '%';
                            }
                        }
                    },
                    x: {
                        grid: {
                            display: false
                        }
                    }
                }
            }
        });
    }
}

function updateAlerts(alerts) {
    const list = document.getElementById('alertsList');
    if (!alerts || alerts.length === 0) {
        list.innerHTML = '<p style="color:#999; text-align:center; padding:20px;">No alerts yet</p>';
        return;
    }

    list.innerHTML = alerts.map(a => {
        const date = new Date(a.timestamp);
        return `
            <div class="alert-item">
                <strong>${a.alert_type.replace(/_/g, ' ').toUpperCase()}</strong>
                — ${a.message}
                <span style="float:right; color:#aaa; font-size:12px;">
                    ${date.toLocaleString()}
                </span>
            </div>
        `;
    }).join('');
}

function updateFirmwareLog(logs) {
    const list = document.getElementById('updateLogList');
    if (!logs || logs.length === 0) {
        list.innerHTML = '<p style="color:#999; text-align:center; padding:20px;">No updates yet</p>';
        return;
    }

    list.innerHTML = logs.map(l => {
        const date = new Date(l.timestamp);
        const statusColor = l.status === 'applied' ? '#00d084' : '#ff6b6b';
        return `
            <div class="version-item">
                <div class="version-info">
                    <div class="version-number">v${l.from_version} → v${l.to_version}</div>
                    <div class="version-details">
                        <span style="color: ${statusColor}; font-weight: bold;">${l.status.toUpperCase()}</span> |
                        ${l.device_id} | ${date.toLocaleString()}
                    </div>
                </div>
            </div>
        `;
    }).join('');
}

async function loadVersions() {
    const versions = await listFirmware();
    if (versions) {
        const list = document.getElementById('versionsList');
        list.innerHTML = '';
        
        if (versions.length === 0) {
            list.innerHTML = '<p style="color: #999; padding: 20px; text-align: center;">No firmware versions available</p>';
            return;
        }
        
        versions.forEach(v => {
            const div = document.createElement('div');
            div.className = 'version-item' + (v.is_current ? ' current' : '');
            
            const sizeKb = (v.size / 1024).toFixed(2);
            
            div.innerHTML = `
                <div class="version-info">
                    <div class="version-number">v${v.version} ${v.is_current ? '✓ Current' : ''}</div>
                    <div class="version-details">Size: ${sizeKb} KB | MD5: ${v.md5.substring(0, 8)}...</div>
                </div>
                ${!v.is_current ? `<button onclick="setCurrentFirmware('${v.version}')">Set Current</button>` : ''}
            `;
            list.appendChild(div);
        });
    }
}

window.uploadFirmware = async function() {
    const file = document.getElementById('firmwareFile').files[0];
    const version = document.getElementById('versionInput').value;
    
    if (!file) {
        alert('❌ Please select a firmware file');
        return;
    }
    
    if (!version) {
        alert('❌ Please enter a version number (e.g., 2.0)');
        return;
    }
    
    // Validate file extension
    if (!file.name.endsWith('.bin')) {
        alert('❌ Please select a .bin file');
        return;
    }
    
    const uploadBtn = event.target;
    uploadBtn.disabled = true;
    uploadBtn.textContent = 'Uploading...';
    
    const result = await uploadFirmwareFile(file, version);
    
    if (result) {
        alert('✅ Firmware uploaded successfully!\n\nVersion: ' + result.version + '\nSize: ' + (result.size / 1024).toFixed(2) + ' KB');
        document.getElementById('firmwareFile').value = '';
        document.getElementById('versionInput').value = '';
        loadVersions();
    } else {
        alert('❌ Upload failed. Check console for errors.');
    }
    
    uploadBtn.disabled = false;
    uploadBtn.textContent = 'Upload';
}

window.setCurrentFirmware = async function(version) {
    if (confirm(`Set firmware v${version} as current?\n\nDevice will download on next wake.`)) {
        const result = await setCurrentFirmware(version);
        if (result) {
            alert('✅ Firmware v' + version + ' is now current.\n\nDevice will auto-update on next wake.');
            loadVersions();
        } else {
            alert('❌ Error setting current version');
        }
    }
}

// Update dashboard every 5 seconds
setInterval(updateDashboard, 5000);

// Initial load
document.addEventListener('DOMContentLoaded', function() {
    updateDashboard();
});
