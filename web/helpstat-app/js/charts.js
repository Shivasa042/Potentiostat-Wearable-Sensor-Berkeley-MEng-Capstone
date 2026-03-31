/** Expects Chart.js 4 UMD on window (loaded from index.html before app.js). */

let chartNyq = null;
let chartMag = null;
let chartPh = null;

function destroyAll() {
  chartNyq?.destroy();
  chartMag?.destroy();
  chartPh?.destroy();
  chartNyq = chartMag = chartPh = null;
}

/**
 * @param {HTMLElement} root
 * @param {object|null} sweep
 */
export function renderSweep(root, sweep) {
  const Chart = globalThis.Chart;
  if (!Chart) return;
  destroyAll();
  if (!sweep?.points?.length) return;

  const pts = sweep.points;
  const re = pts.map((p) => p.re);
  const im = pts.map((p) => p.im);
  const f = pts.map((p) => p.f);
  const mag = pts.map((p) => p.mag);
  const ph = pts.map((p) => p.ph);
  const logF = f.map((x) => Math.log10(x));
  const nyqData = re.map((x, i) => ({ x, y: im[i] }));

  chartNyq = new Chart(root.querySelector("#c-nyquist"), {
    type: "scatter",
    data: {
      datasets: [
        {
          label: "Z",
          data: nyqData,
          showLine: true,
          borderColor: "#38bdf8",
          backgroundColor: "#38bdf8",
          pointRadius: 3,
        },
      ],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { legend: { display: false } },
      scales: {
        x: { title: { display: true, text: "Zreal (Ω)" } },
        y: { title: { display: true, text: "Zimag (Ω)" } },
      },
    },
  });

  chartMag = new Chart(root.querySelector("#c-bode-mag"), {
    type: "line",
    data: {
      labels: logF,
      datasets: [{ label: "|Z|", data: mag, borderColor: "#4ade80", tension: 0.15 }],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { legend: { display: false } },
      scales: {
        x: { title: { display: true, text: "log10(f)" } },
        y: { title: { display: true, text: "|Z| (Ω)" } },
      },
    },
  });

  chartPh = new Chart(root.querySelector("#c-bode-ph"), {
    type: "line",
    data: {
      labels: logF,
      datasets: [{ label: "Phase", data: ph, borderColor: "#c084fc", tension: 0.15 }],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { legend: { display: false } },
      scales: {
        x: { title: { display: true, text: "log10(f)" } },
        y: { title: { display: true, text: "Phase (°)" } },
      },
    },
  });
}

export { destroyAll };
