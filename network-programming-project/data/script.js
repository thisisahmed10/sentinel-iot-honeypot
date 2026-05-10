let ws;

function initWS() {
  ws = new WebSocket(`ws://${location.host}/ws`);

  ws.onmessage = (event) => {
    document.getElementById("status").innerText = "Status: " + event.data;
  };
}

function toggleLED() {
  fetch("/api/toggle");
}

window.onload = initWS;
