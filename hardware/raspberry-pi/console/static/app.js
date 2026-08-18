"use strict";

import {attentionForAgent, changedAgentStates, selectedAgent} from "./console-state.js";

const POLL_MS = 2000;
const MAX_EVENTS = 20;
const stateLabel = {
  idle: "IDLE", working: "WORKING", waiting_approval: "WAITING APPROVAL",
  human_required: "HUMAN REQUIRED", completed: "COMPLETED", error: "ERROR", offline: "OFFLINE"
};
const $ = (id) => document.getElementById(id);
let pinnedAgent = null;
let previousByAgent = new Map();
let events = JSON.parse(localStorage.getItem("stateLampEvents") || "[]");

function esc(value) {
  return String(value).replace(/[&<>'"]/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;","'":"&#39;",'"':"&quot;"}[c]));
}
function timeLabel(value = Date.now()) {
  return new Date(value).toLocaleTimeString([], {hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: false});
}
function renderEvents() {
  $("event-list").innerHTML = events.length ? events.map(event => `
    <div class="item compact-item"><div class="item-head"><span>${esc(event.agent)} · ${esc(stateLabel[event.state] || event.state)}</span><span class="item-time">${esc(timeLabel(event.at))}</span></div><p>${esc(event.message)}</p></div>`).join("") : '<p class="empty">No state changes yet</p>';
}
function recordEvents(agents) {
  const result = changedAgentStates(previousByAgent, agents);
  previousByAgent = result.next;
  if (!result.changed.length) return;
  events = [...result.changed.map(status => ({...status, at: Date.now()})), ...events].slice(0, MAX_EVENTS);
  localStorage.setItem("stateLampEvents", JSON.stringify(events));
  renderEvents();
}
function setStatus(status, online) {
  const state = online ? status.state : "offline";
  document.body.dataset.state = state;
  $("agent").textContent = online ? status.agent : "Agent Console";
  $("state").textContent = stateLabel[state] || state.toUpperCase();
  $("message").textContent = online ? (status.message || "No current task") : "StateLamp Bridge is unavailable";
  $("connection").textContent = online ? "BRIDGE ONLINE" : "BRIDGE OFFLINE";
}
function renderAgentSelector(agents, selected) {
  $("agent-selector").innerHTML = agents.map(item => `
    <button type="button" class="agent-chip ${item.agent === selected?.agent ? "selected" : ""}" data-agent="${esc(item.agent)}">
      <i class="state-dot state-${esc(item.state)}"></i>
      <span class="agent-name">${esc(item.agent)}</span>
      <strong>${esc(stateLabel[item.state] || item.state)}</strong>
      <small>${esc(item.message || "No current task")}</small>
    </button>`).join("");
  $("agent-count").textContent = `${agents.length} ${agents.length === 1 ? "AGENT" : "AGENTS"}`;
  const online = agents.filter(item => item.state !== "offline").length;
  $("working-count").textContent = `${online}/${agents.length} ONLINE`;
  $("auto-select").classList.toggle("selected", pinnedAgent === null);
  $("auto-select").textContent = pinnedAgent === null ? "AUTO ON" : "AUTO";
}
function renderAttention(items, selectedName) {
  const filtered = attentionForAgent(items, selectedName);
  $("attention-count").textContent = `${filtered.length}/${items.length}`;
  $("attention-list").innerHTML = filtered.length ? filtered.map(item => `
    <div class="item"><div class="item-head"><span>${esc(item.agent)}</span><span>${esc(item.reason.replaceAll("_", " "))}</span></div><p>${esc(item.message)}</p></div>`).join("") : '<p class="empty">No pending requests for this agent</p>';
}
function percent(used, total) { return total ? `${Math.round(used / total * 100)}%` : "N/A"; }
function renderHost(host) {
  $("host-name").textContent = host.hostname || "N/A";
  $("host-temp").textContent = host.cpu_temperature_c == null ? "N/A" : `${host.cpu_temperature_c} C`;
  $("host-load").textContent = host.load_average?.length ? host.load_average[0].toFixed(2) : "N/A";
  $("host-memory").textContent = percent(host.memory.used_bytes, host.memory.total_bytes);
  $("host-disk").textContent = percent(host.disk.used_bytes, host.disk.total_bytes);
}
async function getJson(path) {
  const response = await fetch(path, {cache: "no-store"});
  if (!response.ok) { const error = new Error(`${path}: ${response.status}`); error.status = response.status; throw error; }
  return response.json();
}
async function getAgents() {
  try { return await getJson("/api/console/agents"); }
  catch (error) {
    if (error.status === 404) return [await getJson("/api/console/status")];
    throw error;
  }
}
async function poll() {
  const [agentsResult, attention, host] = await Promise.allSettled([
    getAgents(), getJson("/api/console/attention"), getJson("/api/console/host")
  ]);
  if (agentsResult.status === "fulfilled" && agentsResult.value.length) {
    const agents = agentsResult.value;
    const selected = selectedAgent(agents, pinnedAgent);
    setStatus(selected, true);
    renderAgentSelector(agents, selected);
    recordEvents(agents);
    renderAttention(attention.status === "fulfilled" ? attention.value : [], selected.agent);
  } else {
    setStatus({}, false);
    renderAgentSelector([], null);
    renderAttention([], null);
  }
  if (host.status === "fulfilled") renderHost(host.value);
}
$("agent-selector").addEventListener("click", event => {
  const button = event.target.closest("[data-agent]");
  if (!button) return;
  pinnedAgent = button.dataset.agent;
  poll();
});
$("auto-select").addEventListener("click", () => { pinnedAgent = null; poll(); });
$("clear-events").addEventListener("click", () => {
  events = [];
  localStorage.removeItem("stateLampEvents");
  renderEvents();
});
function updateClock() {
  $("clock").textContent = new Date().toLocaleTimeString([], {hour: "2-digit", minute: "2-digit", hour12: false});
}
updateClock(); setInterval(updateClock, 1000); renderEvents(); poll(); setInterval(poll, POLL_MS);
