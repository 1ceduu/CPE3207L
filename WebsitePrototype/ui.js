import { CONFIG } from './config.js';
import { penData, state } from './data.js';
import { currentTimeStr, showToast, updateRangeTrack } from './utils.js';
import { drawConsumptionChart } from './charts.js';

/* Fullscreen */
export function toggleFullscreen() {
    if (!document.fullscreenElement) {
        document.documentElement.requestFullscreen()
            .catch(err => console.error(`Fullscreen error: ${err.message}`));
    } else {
        document.exitFullscreen();
    }
}

/* Dashboard render (Connected Pens Only) */
export function renderDashboard() {
    const grid = document.getElementById("pen-grid");
    if (!grid) return;

    const penIds = Object.keys(penData);
    if (penIds.length === 0) {
        grid.innerHTML = `
            <div class="empty-pen-card" style="grid-column: 1 / -1; text-align: center; padding: 48px 24px; background: rgba(15,23,42,0.6); border: 1px dashed rgba(56,189,248,0.3); border-radius: 16px;">
                <div style="font-size: 2.5rem; margin-bottom: 12px;">📡</div>
                <h3 style="font-size: 1.25rem; font-weight: 700; color: var(--cyan); margin-bottom: 8px;">Searching for Connected Feeder Nodes...</h3>
                <p style="color: var(--muted); font-size: 0.88rem; max-width: 480px; margin: 0 auto 18px; line-height: 1.5;">
                    Connect an ESP32 Feeder Module via USB Serial (115200 baud) or RS-485 MODBUS RTU. The pen monitoring card will automatically appear here once live telemetry is detected.
                </p>
                <div class="system-pill" style="display: inline-flex;">
                    <div class="pill-dot"></div> Listening on Serial / MODBUS
                </div>
            </div>
        `;
        return;
    }

    grid.innerHTML = "";

    penIds.forEach(id => {
        const pen  = penData[id];
        const card = document.createElement("div");
        card.className = `pen-card ${pen.cardClass || ""}`;
        card.onclick   = () => showScreen("pen-control-screen", id);

        const waterFill = pen.waterLevel < 30 ? "critical" : pen.waterLevel < 45 ? "warning" : "";
        const nh3Fill   = pen.nh3 >= CONFIG.NH3_LIMIT_PPM ? "critical" : pen.nh3 >= 20 ? "warning" : "";

        let alertsHTML = "";
        if (pen.alerts && (pen.statusClass === "warning" || pen.statusClass === "critical")) {
            const activeAlerts = pen.alerts.filter(a => a.type === "warning" || a.type === "critical");
            if (activeAlerts.length > 0) {
                alertsHTML = `
                    <div class="card-alerts-box">
                        <div class="card-alerts-title">Active Alerts</div>
                        <div class="card-alerts-list">
                            ${activeAlerts.slice(0, 2).map(a => `
                                <div class="card-alert-item ${a.type}">
                                    <span class="card-alert-dot"></span>
                                    <span class="card-alert-text">${a.text}</span>
                                </div>
                            `).join("")}
                        </div>
                    </div>
                `;
            }
        }

        card.innerHTML = `
            <div class="pen-header">
                <div class="pen-title">${pen.name}</div>
                <div class="pen-id-badge">Feeder Module #${pen.pen_id}</div>
            </div>
            <div class="metric-stack">
                <div class="metric-box">
                    <div class="metric-label">Water Level</div>
                    <div class="metric-row">
                        <div class="metric-value">${pen.waterLevel}<span class="metric-unit">%</span></div>
                        <div class="metric-delta">${pen.waterLevel >= pen.refillThreshold ? "OK" : "Low"}</div>
                    </div>
                    <div class="progress-track"><div class="progress-fill ${waterFill}" style="width:${pen.waterLevel}%"></div></div>
                </div>
                <div class="metric-box">
                    <div class="metric-label">Feed Remaining</div>
                    <div class="metric-row">
                        <div class="metric-value">${pen.feedRemaining}<span class="metric-unit">kg</span></div>
                        <div class="metric-delta">In hopper</div>
                    </div>
                    <div class="progress-track"><div class="progress-fill" style="width:${Math.min(pen.feedRemaining * 3, 100)}%"></div></div>
                </div>
                <div class="metric-box">
                    <div class="metric-label">NH₃ Level</div>
                    <div class="metric-row">
                        <div class="metric-value">${pen.nh3}<span class="metric-unit">ppm</span></div>
                        <div class="metric-delta">Limit: ${CONFIG.NH3_LIMIT_PPM}</div>
                    </div>
                    <div class="progress-track"><div class="progress-fill ${nh3Fill}" style="width:${Math.min((pen.nh3 / CONFIG.NH3_LIMIT_PPM) * 100, 100)}%"></div></div>
                </div>
            </div>
            ${alertsHTML}
            <div class="status-badge ${pen.statusClass}">
                <div class="status-dot"></div>
                ${pen.status}
            </div>
        `;
        grid.appendChild(card);
    });
}

/* Screen navigation */
export function showScreen(screenId, penId = null) {
    if (penId) state.currentPen = penId;
    document.querySelectorAll(".screen").forEach(s => s.classList.remove("active"));
    const activeScreen = document.getElementById(screenId);
    if (activeScreen) activeScreen.classList.add("active");

    if (screenId === "dashboard-screen") {
        renderDashboard();
    }
    if (screenId === "pen-control-screen") {
        setTimeout(() => loadPenControlData(state.currentPen), 50);
    }
}

export function openModal(modalId) {
    if (modalId === "feed-modal")     loadFeedSettingsData(state.currentPen);
    if (modalId === "override-modal") loadOverrideScreen(state.currentPen);
    if (modalId === "water-modal")    loadWaterSettingsData(state.currentPen);
    const modal = document.getElementById(modalId);
    if (modal) modal.classList.add("open");
}

export function closeModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) modal.classList.remove("open");
}

export function onBackdropClick(event, modalId) {
    if (event.target === document.getElementById(modalId)) closeModal(modalId);
}

/* Real-Time Screen 2 Live DOM Synchronizer (Non-destructive) */
export function updateLivePenControl(penId) {
    const pen = penData[penId];
    if (!pen) return;

    // Check if pen-control-screen is active
    const screen = document.getElementById("pen-control-screen");
    if (!screen || !screen.classList.contains("active")) return;

    // Title & Status
    const titleEl = document.getElementById("pen-control-title");
    if (titleEl) titleEl.textContent = `${pen.name} — Control`;

    const footerEl = document.getElementById("footer-pen-status");
    if (footerEl) footerEl.textContent = `${pen.name} selected (Live)`;

    // Water gauge
    const gaugeText   = document.getElementById("water-gauge-text");
    const litersText  = document.getElementById("water-liters-text");
    const fill        = document.getElementById("water-bar-fill");
    const liters      = ((pen.waterLevel / 100) * CONFIG.MAX_WATER_L).toFixed(2);

    if (gaugeText)  gaugeText.textContent  = `${pen.waterLevel}%`;
    if (litersText) litersText.textContent = `${liters} / ${CONFIG.MAX_WATER_L.toFixed(2)} L`;
    if (fill) {
        fill.style.width = `${pen.waterLevel}%`;
        fill.className   = "progress-fill " + (pen.waterLevel < 30 ? "critical" : pen.waterLevel < 45 ? "warning" : "");
    }

    // Refill threshold display
    const threshEl = document.getElementById("refill-threshold-display");
    if (threshEl) threshEl.textContent = `${pen.refillThreshold}%`;

    // Alert log container update if alert items exist
    const alertContainer = document.getElementById("alert-log-container");
    const alertCountEl   = document.getElementById("alert-count");
    if (alertContainer && pen.alerts) {
        if (alertCountEl) alertCountEl.textContent = `${pen.alerts.length} events`;
        alertContainer.innerHTML = "";
        pen.alerts.forEach(alert => {
            const div = document.createElement("div");
            div.className = `alert-item ${alert.type === "status" ? "" : alert.type}`;
            div.innerHTML = `
                <span class="alert-time">${alert.time}</span>
                <div class="alert-body">
                    <span class="alert-kind ${alert.type}">${alert.type}</span>
                    <span class="alert-text">${alert.text}</span>
                </div>
            `;
            alertContainer.appendChild(div);
        });
    }

    // Live Chart update
    if (pen.history && pen.dangerZone) {
        drawConsumptionChart(pen.history, pen.dangerZone);
    }
}

/* Pen control initial data loader */
export function loadPenControlData(penId) {
    const pen = penData[penId];
    if (!pen) return;

    updateLivePenControl(penId);

    // Schedule list
    const schedContainer = document.getElementById("schedule-container");
    if (schedContainer && pen.schedule) {
        schedContainer.innerHTML = "";
        pen.schedule.forEach(s => {
            const div = document.createElement("div");
            div.className = "schedule-card";
            div.innerHTML = `
                <div>
                    <div class="metric-label">${s.label}</div>
                    <strong>${s.time}</strong>
                </div>
                <span class="schedule-amount">${s.amount}</span>
            `;
            schedContainer.appendChild(div);
        });
    }
}

/* Feed settings */
export function loadFeedSettingsData(penId) {
    const pen = penData[penId];
    if (!pen || !pen.settings) return;
    const s = pen.settings;
    document.getElementById("feed-modal-title").textContent = `Edit Feeding Program — ${pen.name}`;
    document.getElementById("morning-hour").value   = String(s.mHour).padStart(2, "0");
    document.getElementById("morning-min").value    = String(s.mMin).padStart(2, "0");
    document.getElementById("morning-period").value = s.mPer;
    document.getElementById("morning-amount").value = s.mAmount.toFixed(1);
    document.getElementById("midday-hour").value    = String(s.aHour).padStart(2, "0");
    document.getElementById("midday-min").value     = String(s.aMin).padStart(2, "0");
    document.getElementById("midday-period").value  = s.aPer;
    document.getElementById("midday-amount").value  = s.aAmount.toFixed(1);
}

export function adjustAmount(inputId, delta) {
    const input = document.getElementById(inputId);
    if (input) {
        let v = parseFloat(input.value || "0") + delta;
        if (v < 0) v = 0;
        input.value = v.toFixed(1);
    }
}

export function clampTimeValues() {
    ["morning-hour", "midday-hour"].forEach(id => {
        const el = document.getElementById(id);
        if (el) el.value = String(Math.max(1, Math.min(12, parseInt(el.value || "1", 10)))).padStart(2, "0");
    });
    ["morning-min", "midday-min"].forEach(id => {
        const el = document.getElementById(id);
        if (el) el.value = String(Math.max(0, Math.min(59, parseInt(el.value || "0", 10)))).padStart(2, "0");
    });
}

export async function saveSettings() {
    clampTimeValues();
    const s = {
        mHour:   parseInt(document.getElementById("morning-hour").value, 10),
        mMin:    parseInt(document.getElementById("morning-min").value, 10),
        mPer:    document.getElementById("morning-period").value,
        mAmount: parseFloat(document.getElementById("morning-amount").value || "0"),
        aHour:   parseInt(document.getElementById("midday-hour").value, 10),
        aMin:    parseInt(document.getElementById("midday-min").value, 10),
        aPer:    document.getElementById("midday-period").value,
        aAmount: parseFloat(document.getElementById("midday-amount").value || "0")
    };

    if (penData[state.currentPen]) {
        penData[state.currentPen].settings = s;
        penData[state.currentPen].schedule = [
            { label: "Morning Cycle", time: `${String(s.mHour).padStart(2,"0")}:${String(s.mMin).padStart(2,"0")} ${s.mPer}`, amount: `${s.mAmount.toFixed(1)} kg` },
            { label: "Midday Cycle",  time: `${String(s.aHour).padStart(2,"0")}:${String(s.aMin).padStart(2,"0")} ${s.aPer}`, amount: `${s.aAmount.toFixed(1)} kg` }
        ];
    }

    try {
        await fetch(`/api/pens/${state.currentPen}/schedule`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ settings: s })
        });
        showToast(`${penData[state.currentPen]?.name || 'Pen'} feed schedule saved to database`);
    } catch (err) {
        showToast(`${penData[state.currentPen]?.name || 'Pen'} schedule updated locally`);
    }

    closeModal("feed-modal");
    loadPenControlData(state.currentPen);
}

/* Water threshold */
export function loadWaterSettingsData(penId) {
    const pen = penData[penId];
    if (!pen) return;
    document.getElementById("water-modal-title").textContent = `Water Refill Threshold — ${pen.name}`;
    syncThreshold(pen.refillThreshold || CONFIG.DEFAULT_THRESHOLD);
}

export function syncThreshold(v) {
    v = Math.max(CONFIG.MIN_THRESHOLD, Math.min(CONFIG.MAX_THRESHOLD, v));
    document.getElementById("water-threshold-input").value = v;
    document.getElementById("water-threshold-range").value  = v;
    updateThresholdLiterHint(v);
    updateRangeTrack(v);
}

export function updateThresholdLiterHint(val) {
    const liters = ((val / 100) * CONFIG.MAX_WATER_L).toFixed(2);
    const el = document.getElementById("threshold-liter-hint");
    if (el) el.textContent = `= ${liters} L of the ${CONFIG.MAX_WATER_L} L trough capacity`;
}

export function onRangeInput(val) {
    val = parseInt(val, 10);
    document.getElementById("water-threshold-input").value = val;
    updateThresholdLiterHint(val);
    updateRangeTrack(val);
}

export function onThresholdTyped(val) {
    let v = parseInt(val, 10);
    if (!isNaN(v)) {
        document.getElementById("water-threshold-range").value = v;
        updateThresholdLiterHint(v);
        updateRangeTrack(v);
    }
}

export function adjustThreshold(delta) {
    let v = parseInt(document.getElementById("water-threshold-input").value || String(CONFIG.DEFAULT_THRESHOLD), 10) + delta;
    syncThreshold(v);
}

export async function saveWaterThreshold() {
    let v = parseInt(document.getElementById("water-threshold-input").value || String(CONFIG.DEFAULT_THRESHOLD), 10);
    v = Math.max(CONFIG.MIN_THRESHOLD, Math.min(CONFIG.MAX_THRESHOLD, v));

    if (penData[state.currentPen]) {
        penData[state.currentPen].refillThreshold = v;
    }

    try {
        await fetch(`/api/pens/${state.currentPen}/threshold`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ threshold: v })
        });
        showToast(`${penData[state.currentPen]?.name || 'Pen'} refill threshold set to ${v}% (${((v/100)*CONFIG.MAX_WATER_L).toFixed(2)} L)`);
    } catch (err) {
        if (window.sendHardwareCommand) {
            window.sendHardwareCommand(`THRESH ${v}`);
        }
        showToast(`Refill threshold updated to ${v}%`);
    }

    closeModal("water-modal");
    loadPenControlData(state.currentPen);
}

/* Manual override screen */
export function loadOverrideScreen(penId) {
    const pen = penData[penId];
    document.getElementById("override-modal-title").textContent = `⚡ Manual Override (RA0) — ${pen ? pen.name : 'Pen'}`;
    state.feedDispatch = 1.0;
    document.getElementById("feed-dispatch-val").textContent = state.feedDispatch.toFixed(1);
}

export function adjustDispatch(type, delta) {
    if (type === "feed") {
        state.feedDispatch = Math.max(CONFIG.FEED_DISPATCH_MIN, state.feedDispatch + delta);
        document.getElementById("feed-dispatch-val").textContent = state.feedDispatch.toFixed(1);
    } else {
        state.waterDispatch = Math.max(CONFIG.WATER_DISPATCH_MIN, Math.min(CONFIG.WATER_DISPATCH_MAX, state.waterDispatch + delta));
        document.getElementById("water-dispatch-val").textContent = state.waterDispatch.toFixed(1);
    }
}

export function confirmDispatch(type) {
    const pen = penData[state.currentPen];
    const penName = pen ? pen.name : `Pen ${state.currentPen}`;

    if (type === "feed") {
        const grams = Math.round(state.feedDispatch * 1000);
        showToast(`Dispensing ${state.feedDispatch.toFixed(1)} kg (${grams}g) feed to ${penName}`);
        if (pen && pen.alerts) {
            pen.alerts.unshift({
                time: currentTimeStr(),
                type: "status",
                text: `Manual override: ${state.feedDispatch.toFixed(1)} kg feed dispensed`
            });
        }
        if (window.sendHardwareCommand) {
            window.sendHardwareCommand(`D ${grams}`);
        }
    } else {
        showToast(`Refilling trough to full (${CONFIG.MAX_WATER_L.toFixed(1)} L) — ${penName}`);
        if (pen) {
            if (pen.alerts) {
                pen.alerts.unshift({
                    time: currentTimeStr(),
                    type: "status",
                    text: `Manual override: trough refilled to full (${CONFIG.MAX_WATER_L.toFixed(1)} L)`
                });
            }
            pen.waterLevel = 100;
        }
        if (window.sendHardwareCommand) {
            window.sendHardwareCommand('REFILL');
        }
    }
    closeModal("override-modal");
    setTimeout(() => loadPenControlData(state.currentPen), 400);
}
