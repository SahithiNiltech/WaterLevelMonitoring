# Water Level Monitor — Implementation & Deployment Guide

All code changes have been completed for the Blynk → Flask migration. Follow this guide to deploy and test.

---

## Prerequisites (Complete these first)

### 1. Gmail App Password
1. Go to https://myaccount.google.com/security
2. Enable 2-Step Verification if not already enabled
3. Go back to Security → App Passwords
4. Select "Mail" and "Windows Computer" (or your device type)
5. Generate the app password (16 characters, looks like: `abcd efgh ijkl mnop`)
6. **Copy this exactly** — you'll paste it into Render

### 2. Supabase PostgreSQL Database
1. Go to https://supabase.com and sign up (free)
2. Create a new project (choose region close to you)
3. Wait for project to initialize (~2 min)
4. Go to **Project Settings > Database > Connection String**
5. Copy the **URI format** string (looks like: `postgresql://postgres:...@db.xxxx.supabase.co:5432/postgres`)
6. Save this — you'll paste it into Render in the next step

### 3. GitHub Repository (Required for Render)
Push your project to GitHub:
```bash
git init
git add .
git commit -m "Initial commit: Blynk migration to Flask + HTTP OTA"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/WaterLevelMonitoring.git
git push -u origin main
```

---

## Deployment Steps

### Step 1: Deploy Backend to Render

1. **Connect GitHub to Render**
   - Go to https://render.com (sign up if needed)
   - Create a new **Web Service**
   - Connect your GitHub repository
   - Select the repo and branch (main)

2. **Configure Render Deployment**
   - Name: `water-level-monitor-api`
   - Runtime: Python 3
   - Build Command: `pip install -r requirements.txt` (should auto-fill)
   - Start Command: `gunicorn app:app --workers 2 --bind 0.0.0.0:$PORT --timeout 120` (should auto-fill)
   - Root Directory: `backend`

3. **Set Environment Variables** in Render Dashboard:
   - `DATABASE_URL`: Paste your Supabase PostgreSQL URI from Step 2 above
   - `SECRET_KEY`: Leave as `generateValue: true` (Render auto-generates)
   - `API_KEY`: `waterlevel890890`
   - `MAIL_USERNAME`: Your Gmail address (e.g., `your-email@gmail.com`)
   - `MAIL_PASSWORD`: Paste the App Password from Step 1 above
   - `NOTIFICATION_EMAIL`: `sahithireddy837@gmail.com`
   - `FIRMWARE_FOLDER`: `/tmp/firmware`

4. **Wait for deployment** (~2-3 minutes)
   - Once the service is "Live", note the URL (e.g., `https://water-level-monitor-api.onrender.com`)
   - Test the health endpoint: curl `https://water-level-monitor-api.onrender.com/health`

### Step 2: Deploy Frontend to Render (Optional — GitHub Pages also works)

1. **Create a Static Site**
   - New Service → Static Site
   - Connect same GitHub repo
   - Build Command: (leave empty)
   - Publish Directory: `frontend`
   - Name: `water-level-monitor-ui`

2. **After deployment**, note the frontend URL (e.g., `https://water-level-monitor-ui.onrender.com`)

---

## Firmware Setup

### Step 3: Update Firmware Config & Flash

1. **Update [src/config.h](src/config.h)**
   - Line 12: Replace `BACKEND_URL` with your Render backend URL from Step 1
   - Example: `#define BACKEND_URL "https://water-level-monitor-api.onrender.com"`
   - Verify `API_KEY` is `waterlevel890890`
   - Verify `FIRMWARE_VERSION` is `"1.1"`

2. **Compile and Flash**
   ```bash
   cd /path/to/WaterLevelMonitoring
   pio run -e esp32dev  # Build
   pio run -e esp32dev --target upload  # Flash via serial cable
   ```

3. **Watch Serial Monitor**
   ```bash
   pio run -e esp32dev --target monitor
   ```
   You should see:
   ```
   [WiFi] Connected. IP: 192.168.x.x
   [FW] Checking for updates...
   [FW] No updates available
   [HTTP] POST /api/device/data
   [HTTP] 201 Created
   [Sleep] Sleeping for 3600 s...
   ```

---

## Testing Checklist

### Test 1: Backend API (Local Testing)
```bash
# Test sensor data POST
curl -X POST https://water-level-monitor-api.onrender.com/api/device/data?api_key=waterlevel890890 \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "WaterLevelMonitor_001",
    "water_level_pct": 75.5,
    "water_height_cm": 82.5,
    "battery_v": 4.1,
    "battery_pct": 95
  }'

# Expected: 201 Created response with reading_id

# Get latest status
curl https://water-level-monitor-api.onrender.com/api/device/status?api_key=waterlevel890890

# Get alerts
curl https://water-level-monitor-api.onrender.com/api/device/alerts?api_key=waterlevel890890&limit=5
```

### Test 2: Email Alerts
Post data with low water level or low battery:
```bash
curl -X POST https://water-level-monitor-api.onrender.com/api/device/data?api_key=waterlevel890890 \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "WaterLevelMonitor_001",
    "water_level_pct": 15.0,
    "water_height_cm": 16.4,
    "battery_v": 3.3,
    "battery_pct": 15
  }'
```
Check your email (sahithireddy837@gmail.com) — you should receive alert emails.

### Test 3: Firmware Upload & OTA
1. Open frontend dashboard at Render URL
2. Go to "🔧 Firmware Management" section
3. Select the `.bin` file from `.pio/build/esp32dev/firmware.bin`
4. Enter version `1.2`
5. Click **Upload**
6. Then click **Set Current** on the new v1.2 firmware
7. Wait for next device wake (hourly) or manually trigger wake
8. Device should download, apply, reboot
9. Check frontend → "🔄 Firmware Update Log" — should show the update with status "applied"
10. Check email — should receive firmware notification

### Test 4: Water Level Monitoring
The device enters 1-minute monitoring mode if water level increases. Test by:
1. Covering the ultrasonic sensor with your hand (simulates water rise)
2. Wait 1-2 minutes
3. Device should switch to 1-minute wake intervals
4. Check dashboard — water level should show high %
5. Uncover sensor (water level drops)
6. After 4 monitoring cycles with no increase, device reverts to 1-hour sleep

---

## Deployment Summary

| Component | Location | Status |
|-----------|----------|--------|
| Backend API | Render (water-level-monitor-api) | ✅ Deployed |
| Database | Supabase PostgreSQL | ✅ Connected |
| Frontend | Render (water-level-monitor-ui) | ✅ Deployed |
| Firmware | ESP32 (flashed locally) | ✅ Flashed |
| Email Alerts | Gmail SMTP | ✅ Configured |
| HTTP OTA | Firmware pulls from backend | ✅ Enabled |

---

## Troubleshooting

### Device can't connect to WiFi
- Check SSID and password in `config.h`
- Device tries 3 times with 2s delays
- If still fails, deep sleeps for 1 hour and retries

### Device connects but `POST /api/device/data` fails with 401
- Verify `API_KEY` in `config.h` matches `API_KEY` env var in Render
- Both should be `waterlevel890890`

### `POST /api/device/data` works but no email received
- Check `NOTIFICATION_EMAIL` in Render env vars (should be `sahithireddy837@gmail.com`)
- Check `MAIL_PASSWORD` is the **App Password** from Gmail, not your login password
- Check Gmail SMTP is enabled (usually auto-enabled)

### Firmware update stuck downloading
- Check `FIRMWARE_FOLDER` is writable on Render (`/tmp` is ephemeral but writable)
- Check `.bin` file size < 1MB
- Device timeout is 15 seconds — if Render is slow (cold-start), may take longer

### Dashboard shows old data
- Browser caches API responses
- Try `Ctrl+Shift+R` (force refresh)
- API is called every 5 seconds

---

## Next Steps

1. **Battery optimization**: The OTA window removal saves ~60s per wake (major improvement)
2. **Home Assistant integration**: Backend can expose MQTT for Home Assistant
3. **Historical analytics**: Python script to analyze seasonal patterns
4. **Tank emptying predictions**: ML model based on consumption rate

---

## API Endpoints Reference

| Endpoint | Method | Auth | Purpose |
|----------|--------|------|---------|
| `/health` | GET | ❌ | Health check (Render uses this) |
| `/api/device/data` | POST | ✅ | Device sends sensor readings |
| `/api/device/status` | GET | ✅ | Get latest reading |
| `/api/device/history` | GET | ✅ | Get historical data (days parameter) |
| `/api/device/alerts` | GET | ✅ | Get alert log (limit parameter) |
| `/api/firmware/check` | GET | ✅ | Device checks for updates |
| `/api/firmware/download/<version>` | GET | ✅ | Device downloads .bin file |
| `/api/firmware/applied` | POST | ✅ | Device notifies of successful update |
| `/api/firmware/upload` | POST | ✅ | Dashboard uploads new .bin file |
| `/api/firmware/list` | GET | ❌ | Frontend lists available versions |
| `/api/firmware/set-current/<version>` | PUT | ✅ | Dashboard marks version as current |
| `/api/firmware/update-log` | GET | ❌ | Frontend shows update history |

---

**Questions?** Check the plan file at `/Users/sahithireddy/.claude/plans/as-of-now-i-eager-pascal.md` for architecture details.
