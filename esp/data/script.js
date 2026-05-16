const socket = new WebSocket(`ws://${location.host}/ws`);
const attacks = [];
const MAX_ROWS = 10;

const rpsData = Array(60).fill(0);
const blockedData = Array(60).fill(0);

let rpsChart, blockedChart;

function initCharts() {
  const ctx = document.getElementById('rpsChart').getContext('2d');
  rpsChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: Array.from({length:60},(_,i)=>i-59),
      datasets: [{label:'RPS', data:rpsData, borderColor:'lime', tension:0.2}]
    },
    options:{animation:false, scales:{x:{display:false}}}
  });

  const ctx2 = document.getElementById('blockedChart').getContext('2d');
  blockedChart = new Chart(ctx2, {
    type: 'line',
    data: {labels: rpsChart.data.labels, datasets:[{label:'Blocked',data:blockedData,borderColor:'red',tension:0.2}]},
    options:{animation:false, scales:{x:{display:false}}}
  });
}

function addAttack(event) {
  attacks.unshift(event);
  if (attacks.length>MAX_ROWS) attacks.pop();
  renderTable();
}

function renderTable(){
  const tbody = document.querySelector('#attackTable tbody');
  tbody.innerHTML = '';
  attacks.forEach((a,idx)=>{
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${a.src_ip||''}</td><td>${a.dst_port||''}</td><td>${a.country||'N/A'}</td><td>${a.timestamp||''}</td><td><button data-idx='${idx}' class='viewBtn'>View</button></td>`;
    tbody.appendChild(tr);
  });
  document.querySelectorAll('.viewBtn').forEach(b=>b.addEventListener('click',openPayload));
}

function openPayload(e){
  const idx = e.target.getAttribute('data-idx');
  const payload = attacks[idx].payload||'';
  document.getElementById('payloadPreview').textContent = payload;
  document.getElementById('modal').classList.remove('hidden');
}

document.getElementById('closeModal').addEventListener('click',()=>{
  document.getElementById('modal').classList.add('hidden');
});

socket.addEventListener('open', ()=>{
  console.log('WS open');
  document.getElementById('pulse').textContent = 'Active';
});

socket.addEventListener('message',(ev)=>{
  try{
    const data = JSON.parse(ev.data);
    // Attack event
    if (data.src_ip) {
      addAttack(data);
      document.getElementById('totalAttacks').textContent = attacks.length;
    }
    if (data.metrics) {
      // metrics.rps, metrics.blocked
      rpsData.shift(); rpsData.push(data.metrics.rps||0);
      blockedData.shift(); blockedData.push(data.metrics.blocked||0);
      rpsChart.update(); blockedChart.update();
      document.getElementById('currentRps').textContent = (data.metrics.rps||0);
    }
    if (data.rpa) {
      document.getElementById('reportsCount').textContent = data.rpa.count||0;
      document.getElementById('lastReport').textContent = data.rpa.last||'N/A';
    }
    if (data.device) {
      document.getElementById('uptime').textContent = data.device.uptime||'N/A';
      document.getElementById('rssi').textContent = data.device.rssi||'N/A';
      document.getElementById('heap').textContent = data.device.heap||'N/A';
    }
  }catch(e){
    console.warn('non-json ws message', e);
  }
});

document.getElementById('ddosToggle').addEventListener('change',(e)=>{
  const enabled = e.target.checked?1:0;
  fetch(`/api/ddos?enabled=${enabled}`).then(r=>r.json()).then(j=>console.log(j)).catch(console.error);
});

window.addEventListener('load', ()=>{
  initCharts();
});
let ws;

function setAlert(text, visible) {
  const alertBox = document.getElementById("alertBox");
  if (!alertBox) {
    return;
  }

  alertBox.innerText = text || "";
  alertBox.classList.toggle("hidden", !visible);
}

function renderState(payload) {
  if (payload.kind === "alert") {
    setAlert(payload.message, true);
    return;
  }

  if (payload.kind === "state") {
    document.getElementById("status").innerText = "Status: " + payload.led;
    setAlert(payload.alertActive ? payload.alert : "", payload.alertActive);
  }
}

function initWS() {
  ws = new WebSocket(`ws://${location.host}/ws`);

  ws.onmessage = (event) => {
    try {
      renderState(JSON.parse(event.data));
    } catch (error) {
      document.getElementById("status").innerText = "Status: " + event.data;
    }
  };
}

function toggleLED() {
  fetch("/api/toggle");
}

window.onload = initWS;
