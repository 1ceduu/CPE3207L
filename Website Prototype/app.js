import { penData, state } from './data.js';
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
    onThresholdTyped
} from './ui.js';

// Bind functions to window so the inline HTML attributes can resolve them
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
        setTimeout(() => drawConsumptionChart(penData[state.currentPen].history, penData[state.currentPen].dangerZone), 100);
    }
});

document.addEventListener("keydown", e => {
    if (e.key.toLowerCase() === "f") toggleFullscreen();
});

window.addEventListener("resize", () => {
    const penControl = document.getElementById("pen-control-screen");
    if (penControl && penControl.classList.contains("active")) {
        drawConsumptionChart(penData[state.currentPen].history, penData[state.currentPen].dangerZone);
    }
});

// Initialization
setInterval(updateClock, 1000);
updateClock();
renderDashboard();
