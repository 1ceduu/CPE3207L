import { penData, state, createDefaultPen } from './data.js';
import { updateClock } from './utils.js';
import { drawConsumptionChart } from './charts.js';
import {
    toggleFullscreen,
    renderDashboard,
    showScreen,
    openModal,
    closeModal,
    onBackdropClick,
    adjustAmount,
    saveSettings,
    adjustThreshold,
    saveWaterThreshold,
    adjustDispatch,
    confirmDispatch,
    onRangeInput,
    onThresholdTyped,
    updateLivePenControl,
    loadPenControlData
} from './ui.js';

export async function sendHardwareCommand(cmd) {
    console.log("[DASHBOARD] Sending hardware command:", cmd);
    try {
        const res = await fetch('/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: cmd })
        });
        const json = await res.json();
        console.log("[DASHBOARD] Command acknowledged by server:", json);
    } catch (err) {
        console.error("[DASHBOARD] Failed to send hardware command:", err);
    }
}

// Bind functions to window so the inline HTML attributes can resolve them
window.sendHardwareCommand = sendHardwareCommand;
window.toggleFullscreen = toggleFullscreen;
window.showScreen = showScreen;
window.openModal = openModal;
window.closeModal = closeModal;
window.onBackdropClick = onBackdropClick;
window.adjustAmount = adjustAmount;
window.saveSettings = saveSettings;
window.adjustThreshold = adjustThreshold;
window.saveWaterThreshold = saveWaterThreshold;
window.adjustDispatch = adjustDispatch;
window.confirmDispatch = confirmDispatch;
window.onRangeInput = onRangeInput;
window.onThresholdTyped = onThresholdTyped;

// Event wiring
document.addEventListener("fullscreenchange", () => {
    const btn = document.getElementById("fs-toggle-btn");
    if (btn) {
        btn.innerHTML = document.fullscreenElement
            ? "<span>✕</span> Exit Fullscreen"
            : "<span>⛶</span> Fullscreen";
    }
    const penControl = document.getElementById("pen-control-screen");
    if (penControl && penControl.classList.contains("active")) {
        setTimeout(() => {
            if (penData[state.currentPen]) {
                drawConsumptionChart(penData[state.currentPen].history, penData[state.currentPen].dangerZone);
            }
        }, 100);
    }
});

document.addEventListener("keydown", e => {
    if (e.key.toLowerCase() === "f") toggleFullscreen();
});

window.addEventListener("resize", () => {
    const penControl = document.getElementById("pen-control-screen");
    if (penControl && penControl.classList.contains("active")) {
        if (penData[state.currentPen]) {
            drawConsumptionChart(penData[state.currentPen].history, penData[state.currentPen].dangerZone);
        }
    }
});

// Live Telemetry Polling & Dynamic Multi-Pen Discovery
let prevPenKeys = "";

async function pollLiveTelemetry() {
    try {
        const response = await fetch('/api/telemetry');
        if (!response.ok) return;

        const data = await response.json();
        const serverPens = data.pens || {};
        const serverPenIds = Object.keys(serverPens);
        let penListChanged = false;

        // 1. Remove disconnected pens from local state
        Object.keys(penData).forEach(id => {
            if (!serverPens[id]) {
                delete penData[id];
                penListChanged = true;
            }
        });

        // 2. Insert or update live pen data
        serverPenIds.forEach(id => {
            const sp = serverPens[id];
            if (!penData[id]) {
                penData[id] = sp;
                penListChanged = true;
            } else {
                // Update live metrics
                penData[id].feedRemaining = sp.feedRemaining;
                penData[id].waterLevel    = sp.waterLevel;
                penData[id].nh3           = sp.nh3;
                penData[id].status        = sp.status;
                penData[id].statusClass   = sp.statusClass;
                penData[id].cardClass     = sp.cardClass || "";
                if (sp.refillThreshold) penData[id].refillThreshold = sp.refillThreshold;
                if (sp.schedule)        penData[id].schedule        = sp.schedule;
                if (sp.settings)        penData[id].settings        = sp.settings;
                if (sp.alerts)          penData[id].alerts          = sp.alerts;
                if (sp.history)         penData[id].history         = sp.history;
                if (sp.dangerZone)      penData[id].dangerZone      = sp.dangerZone;
            }
        });

        state.connectedCount = serverPenIds.length;

        // 3. Update Footer Status
        const footerStatusEl = document.getElementById("footer-pen-status");
        if (footerStatusEl) {
            if (serverPenIds.length === 0) {
                footerStatusEl.textContent = "No Feeder Modules Connected (Searching...)";
            } else if (serverPenIds.length === 1) {
                const pName = penData[serverPenIds[0]]?.name || `Pen ${serverPenIds[0]}`;
                footerStatusEl.textContent = `1 Feeder Module Connected (${pName})`;
            } else {
                footerStatusEl.textContent = `${serverPenIds.length} Feeder Modules Connected`;
            }
        }

        // 4. Update Screen 1 (Main Dashboard) if active or if roster changed
        const dashboardScreen = document.getElementById("dashboard-screen");
        const isDashboardActive = dashboardScreen && dashboardScreen.classList.contains("active");

        const currentPenKeys = serverPenIds.sort().join(",");
        if (penListChanged || currentPenKeys !== prevPenKeys || isDashboardActive) {
            prevPenKeys = currentPenKeys;
            renderDashboard();
        }

        // 5. Update Screen 2 (Pen Control) in real time if active
        const penControlScreen = document.getElementById("pen-control-screen");
        if (penControlScreen && penControlScreen.classList.contains("active")) {
            // If currentPen is not in connected pens, select first available pen
            if (!penData[state.currentPen] && serverPenIds.length > 0) {
                state.currentPen = serverPenIds[0];
            }
            if (penData[state.currentPen]) {
                updateLivePenControl(state.currentPen);
            }
        }

    } catch (err) {
        // Server offline / starting up
    }
}

// Initialization
setInterval(updateClock, 1000);
setInterval(pollLiveTelemetry, 1000);
updateClock();
pollLiveTelemetry();
renderDashboard();
