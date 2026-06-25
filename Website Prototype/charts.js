import { CONFIG } from './config.js';

export function drawConsumptionChart(historyData, dangerData) {
    const canvas    = document.getElementById("consumptionCanvas");
    if (!canvas) return;
    const container = canvas.parentElement;
    const dpr       = window.devicePixelRatio || 1;
    const W         = container.clientWidth;
    const H         = container.clientHeight;

    canvas.width  = W * dpr;
    canvas.height = H * dpr;
    const ctx = canvas.getContext("2d");
    ctx.scale(dpr, dpr);

    const labels = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
    const pad    = { left: 48, right: 20, top: 28, bottom: 38 };
    const plotW  = W - pad.left - pad.right;
    const plotH  = H - pad.top  - pad.bottom;
    const maxY   = Math.max(40, Math.ceil(Math.max(...historyData, ...dangerData) / 10) * 10);

    const xAt = i => pad.left + (plotW / (historyData.length - 1)) * i;
    const yAt = v => pad.top  + plotH - (v / maxY) * plotH;

    ctx.clearRect(0, 0, W, H);

    // Grid lines
    ctx.strokeStyle = CONFIG.CHART_COLORS.gridLine;
    ctx.lineWidth   = 1;
    ctx.font        = "500 11px Inter, Segoe UI, sans-serif";
    ctx.fillStyle   = CONFIG.CHART_COLORS.text;
    ctx.textAlign   = "right";
    for (let i = 0; i <= 4; i++) {
        const v = (maxY / 4) * i;
        const y = yAt(v);
        ctx.beginPath(); ctx.moveTo(pad.left, y); ctx.lineTo(W - pad.right, y); ctx.stroke();
        ctx.fillText(`${Math.round(v)}kg`, pad.left - 6, y + 4);
    }

    // Day labels
    ctx.textAlign = "center"; ctx.fillStyle = CONFIG.CHART_COLORS.text;
    labels.forEach((lbl, i) => ctx.fillText(lbl, xAt(i), H - 14));

    // Danger zone dashed line
    ctx.save();
    ctx.beginPath(); ctx.setLineDash([6, 5]);
    ctx.strokeStyle = CONFIG.CHART_COLORS.dangerLine; ctx.lineWidth = 1.5;
    dangerData.forEach((v, i) => { i === 0 ? ctx.moveTo(xAt(i), yAt(v)) : ctx.lineTo(xAt(i), yAt(v)); });
    ctx.stroke(); ctx.restore();

    // Gradient area
    const areaGrad = ctx.createLinearGradient(0, pad.top, 0, H - pad.bottom);
    areaGrad.addColorStop(0,   CONFIG.CHART_COLORS.areaGradStart);
    areaGrad.addColorStop(0.7, CONFIG.CHART_COLORS.areaGradMid);
    areaGrad.addColorStop(1,   CONFIG.CHART_COLORS.areaGradEnd);
    ctx.beginPath();
    ctx.moveTo(xAt(0), yAt(historyData[0]));
    for (let i = 1; i < historyData.length; i++) {
        const cpx = (xAt(i-1) + xAt(i)) / 2;
        ctx.bezierCurveTo(cpx, yAt(historyData[i-1]), cpx, yAt(historyData[i]), xAt(i), yAt(historyData[i]));
    }
    ctx.lineTo(xAt(historyData.length - 1), H - pad.bottom);
    ctx.lineTo(xAt(0), H - pad.bottom);
    ctx.closePath(); ctx.fillStyle = areaGrad; ctx.fill();

    // Smooth line
    ctx.beginPath();
    ctx.moveTo(xAt(0), yAt(historyData[0]));
    for (let i = 1; i < historyData.length; i++) {
        const cpx = (xAt(i-1) + xAt(i)) / 2;
        ctx.bezierCurveTo(cpx, yAt(historyData[i-1]), cpx, yAt(historyData[i]), xAt(i), yAt(historyData[i]));
    }
    ctx.strokeStyle = CONFIG.CHART_COLORS.mainLine; ctx.lineWidth = 3;
    ctx.lineJoin = "round"; ctx.lineCap = "round"; ctx.stroke();

    // Data point dots
    historyData.forEach((v, i) => {
        ctx.beginPath(); ctx.arc(xAt(i), yAt(v), 4.5, 0, Math.PI * 2);
        ctx.fillStyle = CONFIG.CHART_COLORS.mainLine; ctx.fill();
        ctx.beginPath(); ctx.arc(xAt(i), yAt(v), 2.5, 0, Math.PI * 2);
        ctx.fillStyle = CONFIG.CHART_COLORS.dotFill; ctx.fill();
    });

    // Legend
    ctx.font = "600 11px Inter, Segoe UI, sans-serif"; ctx.textAlign = "left";
    ctx.fillStyle = CONFIG.CHART_COLORS.mainLine; ctx.fillRect(pad.left, 10, 14, 3);
    ctx.fillStyle = "#a8cfe4"; ctx.fillText("Feed Consumed (kg)", pad.left + 20, 16);
    ctx.save(); ctx.setLineDash([4, 4]);
    ctx.strokeStyle = CONFIG.CHART_COLORS.dangerLine; ctx.lineWidth = 1.5;
    ctx.beginPath(); ctx.moveTo(pad.left + 150, 12); ctx.lineTo(pad.left + 164, 12); ctx.stroke();
    ctx.restore();
    ctx.fillStyle = "#f05e6a"; ctx.fillText("Danger Zone", pad.left + 170, 16);
}
