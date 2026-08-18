export const statePriority = {
  human_required: 0,
  error: 1,
  waiting_approval: 2,
  working: 3,
  completed: 4,
  offline: 5,
  idle: 6,
};

export function priorityAgent(agents) {
  if (!agents.length) return null;
  return agents.reduce((best, agent) =>
    (statePriority[agent.state] ?? Number.MAX_SAFE_INTEGER) <
    (statePriority[best.state] ?? Number.MAX_SAFE_INTEGER) ? agent : best);
}

export function selectedAgent(agents, pinnedAgent = null) {
  if (pinnedAgent) {
    const pinned = agents.find(agent => agent.agent === pinnedAgent);
    if (pinned) return pinned;
  }
  return priorityAgent(agents);
}

export function attentionForAgent(items, agent) {
  return agent ? items.filter(item => item.agent === agent) : [];
}

export function changedAgentStates(previous, agents) {
  const next = new Map(previous);
  const changed = [];
  for (const agent of agents) {
    const signature = JSON.stringify([agent.state, agent.message]);
    if (previous.get(agent.agent) !== signature) changed.push(agent);
    next.set(agent.agent, signature);
  }
  return { changed, next };
}
