// SisTer Reflexa — MVP-0 frontend
// AG-RFX-MVP0-001: vertical slice with EvidenceBundle + Process Vector

const DIMS = [
  ['exposure',       'Exposição'],
  ['interaction',    'Interação'],
  ['appropriation',  'Apropriação'],
  ['incorporation',  'Incorporação'],
  ['propagation',    'Propagação'],
  ['reflexivity',    'Reflexividade'],
  ['stabilization',  'Estabilização'],
];

// Legacy qualitative dimensions (for human snapshot form)
const QDIMS = [
  ['t','T · temporal'],['e','E · evidência'],['d','Δ · mudança'],
  ['k','K · contribuição'],['a','A · apropriação'],['r','R · alcance'],
  ['p','P · persistência'],['f','F · fechamento']
];

let STATE = {events:[],claims:[],evidence:[],relations:[],evaluations:[],bundles:[],assessments:[]};

// ---------------------------------------------------------------------------
// Build contribution inputs
// ---------------------------------------------------------------------------
function buildContribInputs() {
  const el = document.getElementById('contrib-inputs');
  if (!el) return;
  el.innerHTML = DIMS.map(([d,label]) => `
    <div class="contrib-row">
      <span class="contrib-dim">${label}</span>
      <label class="contrib-label">Suporte <small>−1..+1</small>
        <input type="number" id="c-sup-${d}" min="-1" max="1" step="0.05" placeholder="—">
      </label>
      <label class="contrib-label">Confiança <small>0..1</small>
        <input type="number" id="c-con-${d}" min="0" max="1" step="0.05" placeholder="—">
      </label>
    </div>
  `).join('');
}

function collectContributions() {
  const parts = [];
  for (const [d] of DIMS) {
    const supEl = document.getElementById(`c-sup-${d}`);
    const conEl = document.getElementById(`c-con-${d}`);
    if (!supEl || !conEl) continue;
    const sup = supEl.value.trim();
    const con = conEl.value.trim();
    if (sup !== '' && con !== '') {
      const sv = parseFloat(sup), cv = parseFloat(con);
      if (!isNaN(sv) && !isNaN(cv)) {
        parts.push(`${d}:${Math.max(-1,Math.min(1,sv)).toFixed(3)}:${Math.max(0,Math.min(1,cv)).toFixed(3)}`);
      }
    }
  }
  return parts.join(',');
}

function clearContribInputs() {
  for (const [d] of DIMS) {
    const s = document.getElementById(`c-sup-${d}`);
    const c = document.getElementById(`c-con-${d}`);
    if (s) s.value = '';
    if (c) c.value = '';
  }
}

// Build qualitative dims for human snapshot form
document.addEventListener('DOMContentLoaded', () => {
  const qd = document.getElementById('dimensions');
  if (qd) {
    qd.innerHTML = QDIMS.map(([n,l]) =>
      `<div class="dim"><label>${l}<select name="${n}"><option>NA</option><option>LOW</option><option>MED</option><option>HIGH</option></select></label></div>`
    ).join('');
  }
  buildContribInputs();
});

// ---------------------------------------------------------------------------
// API helpers
// ---------------------------------------------------------------------------
async function api(path, opts = {}) {
  const r = await fetch(path, opts);
  if (!r.ok) throw new Error(`${r.status} ${await r.text()}`);
  return r.json();
}

function esc(s = '') {
  return String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

// ---------------------------------------------------------------------------
// Fill selects
// ---------------------------------------------------------------------------
function fillSelects() {
  const evOpts = '<option value="">— selecione —</option>' +
    STATE.events.map(e => `<option value="${esc(e.id)}">${esc(e.id)} · ${esc(e.title)}</option>`).join('');
  document.querySelectorAll('.event-select').forEach(s => s.innerHTML = evOpts);

  const clmOpts = '<option value="">— selecione —</option>' +
    STATE.claims.map(c => `<option value="${esc(c.id)}">${esc(c.id)} · ${esc(c.text.slice(0,55))}</option>`).join('');
  document.querySelectorAll('.claim-select').forEach(s => s.innerHTML = clmOpts);

  const bndOpts = '<option value="">— selecione bundle —</option>' +
    STATE.bundles.map(b => `<option value="${esc(b.bundle_id)}">${esc(b.bundle_id)} · ${esc(b.evidence_count)} evid. · ${esc(b.created_at.slice(0,19))}</option>`).join('');
  const bndSel = document.getElementById('sel-bundle-assess');
  if (bndSel) bndSel.innerHTML = bndOpts;
}

// ---------------------------------------------------------------------------
// Render metrics
// ---------------------------------------------------------------------------
function renderMetrics() {
  const arr = [
    ['Eventos', STATE.events.length],
    ['Alegações', STATE.claims.length],
    ['Evidências', STATE.evidence.length],
    ['Bundles', STATE.bundles.length],
    ['Avaliações', STATE.assessments.length],
  ];
  document.getElementById('metrics').innerHTML = arr.map(([l,v]) =>
    `<div class="metric"><strong>${v}</strong><span>${l}</span></div>`
  ).join('');
}

// ---------------------------------------------------------------------------
// Render trajectory
// ---------------------------------------------------------------------------
function renderTrajectory() {
  const el = document.getElementById('trajectory');
  if (!STATE.events.length) {
    el.innerHTML = '<p>Nenhum evento ainda. O primeiro passo é preservar um ponto de partida.</p>';
    return;
  }
  el.innerHTML = STATE.events.map(e => {
    const nClaims = STATE.claims.filter(c => c.event_id === e.id).length;
    const bundles = STATE.bundles.filter(b => b.event_id === e.id);
    const assessments = STATE.assessments.filter(a => a.event_id === e.id);
    const lastAsr = assessments.at(-1);
    return `<article class="event-card">
      <div class="id">${esc(e.id)}</div>
      <h3>${esc(e.title)}</h3>
      <p>${esc(e.type)} · ${nClaims} alegação(ões)</p>
      <p>${bundles.length} bundle(s) · ${assessments.length} avaliação(ões)</p>
      ${lastAsr ? `<p class="ep ep-der">● vetor: exp=${lastAsr.vector.exposure.toFixed(2)} int=${lastAsr.vector.interaction.toFixed(2)}</p>` : `<p class="ep ep-unk">○ sem avaliação</p>`}
      <time>válido: ${esc(e.valid_time || 'não informado')}</time>
    </article>`;
  }).join('');
}

// ---------------------------------------------------------------------------
// Render vector (Step 6)
// ---------------------------------------------------------------------------
function renderVector(assessment) {
  const el = document.getElementById('vector-display');
  if (!assessment) {
    el.innerHTML = '<p class="ep ep-unk">○ Nenhuma avaliação ainda.</p>';
    return;
  }
  const vec = assessment.vector;
  const bars = DIMS.map(([d, label]) => {
    const v = vec[d] ?? 0;
    const pct = Math.round(v * 100);
    return `<div class="vec-row">
      <span class="vec-label">${label}</span>
      <div class="vec-bar-bg">
        <div class="vec-bar" style="width:${pct}%"></div>
      </div>
      <span class="vec-val">${v.toFixed(3)}</span>
    </div>`;
  }).join('');

  el.innerHTML = `
    <div class="vec-meta">
      <span class="ep ep-der">● Avaliação derivada</span>
      <code>${esc(assessment.assessment_id)}</code>
      <span>bundle: <code>${esc(assessment.bundle_id)}</code></span>
      <span>avaliador: <code>${esc(assessment.evaluator)}</code></span>
      <span class="chip-warn">${esc(assessment.status?.toUpperCase() ?? 'experimental')}</span>
    </div>
    <div class="vec-bars">${bars}</div>
    <div class="epistemic-note vec-ep-note">
      <strong>Lembrete:</strong> Estes valores refletem apenas o que o avaliador determinístico extraiu das contribuições explicitamente codificadas.
      Não representam medidas de propriedades reais.
    </div>
  `;
}

// ---------------------------------------------------------------------------
// Render records
// ---------------------------------------------------------------------------
function group(title, items, fmt) {
  return `<div class="records-group"><h3>${title}</h3>${items.length ? items.slice().reverse().map(fmt).join('') : '<p>—</p>'}</div>`;
}

function renderRecords() {
  let html = '';
  html += group('Bundles (congelados)', STATE.bundles, b =>
    `<div class="record">
       <code>${esc(b.bundle_id)}</code>
       <p>evento: ${esc(b.event_id)} · ${esc(b.evidence_count)} evid. · criado ${esc(b.created_at.slice(0,19))}<br>
          <span class="ep ep-obs">● digest: <code>${esc(b.content_digest)}</code></span></p>
     </div>`
  );
  html += group('Avaliações (processo)', STATE.assessments, a =>
    `<div class="record">
       <code>${esc(a.assessment_id)}</code>
       <p class="ep ep-der">● bundle: ${esc(a.bundle_id)} · ${esc(a.assessed_at.slice(0,19))}<br>
       exp=${a.vector.exposure.toFixed(2)} int=${a.vector.interaction.toFixed(2)} apr=${a.vector.appropriation.toFixed(2)}
       inc=${a.vector.incorporation.toFixed(2)} prp=${a.vector.propagation.toFixed(2)}
       rfx=${a.vector.reflexivity.toFixed(2)} stb=${a.vector.stabilization.toFixed(2)}<br>
       <span class="chip-warn">${esc(a.evaluator)}</span></p>
     </div>`
  );
  html += group('Avaliações (qualitativa)', STATE.evaluations, x =>
    `<div class="record">
       <code>${esc(x.id)}</code>
       <p><strong>${esc(x.verdict)}</strong> · T:${esc(x.T)} E:${esc(x.E)} Δ:${esc(x.D)} K:${esc(x.K)} A:${esc(x.A)} R:${esc(x.R)} P:${esc(x.P)} F:${esc(x.F)}<br>${esc(x.summary)}</p>
     </div>`
  );
  html += group('Evidências', STATE.evidence, x =>
    `<div class="record">
       <code>${esc(x.id)}</code>
       <p>${esc(x.kind || '—')} → ${esc(x.claim_id)} · válido ${esc(x.valid_time || '—')}<br>
       <span class="ep ep-obs">● ${esc(x.content)}</span>
       ${x.contributions ? `<br><small>contrib: ${esc(x.contributions)}</small>` : ''}</p>
     </div>`
  );
  html += group('Alegações', STATE.claims, x =>
    `<div class="record"><code>${esc(x.id)}</code><p>${esc(x.event_id)} · ${esc(x.text)}</p></div>`
  );
  document.getElementById('records').innerHTML = html;
}

// ---------------------------------------------------------------------------
// Freeze bundle
// ---------------------------------------------------------------------------
async function freezeBundle() {
  const sel = document.getElementById('sel-event-bundle');
  const event_id = sel?.value;
  if (!event_id) { alert('Selecione um evento'); return; }
  const result = document.getElementById('bundle-result');
  result.className = 'bundle-result';
  result.innerHTML = '<span class="loading">Congelando...</span>';
  try {
    const b = await api(`/api/events/${encodeURIComponent(event_id)}/bundles`, {method:'POST'});
    result.innerHTML = `
      <div class="bundle-created">
        <span class="ep ep-obs">● Bundle congelado</span>
        <div><strong>ID:</strong> <code>${esc(b.bundle_id)}</code></div>
        <div><strong>Timestamp:</strong> ${esc(b.created_at)}</div>
        <div><strong>Evidências:</strong> ${esc(b.evidence_count)}</div>
        <div><strong>Digest:</strong> <code>${esc(b.content_digest)}</code></div>
      </div>`;
    await refresh();
  } catch(err) { result.innerHTML = `<span class="err">Erro: ${esc(err.message)}</span>`; }
}

// ---------------------------------------------------------------------------
// Assess bundle
// ---------------------------------------------------------------------------
async function assessBundle() {
  const sel = document.getElementById('sel-bundle-assess');
  const bundle_id = sel?.value;
  if (!bundle_id) { alert('Selecione um bundle'); return; }
  const result = document.getElementById('assess-result');
  result.className = 'assess-result';
  result.innerHTML = '<span class="loading">Avaliando...</span>';
  try {
    const r = await api(`/api/bundles/${encodeURIComponent(bundle_id)}/assessments`, {method:'POST'});
    result.innerHTML = `
      <div class="assess-created">
        <span class="ep ep-der">● Avaliação criada</span>
        <div><strong>ID:</strong> <code>${esc(r.assessment_id)}</code></div>
        <div><strong>Avaliador:</strong> <code>${esc(r.evaluator)}</code></div>
        <div class="chip-warn">${esc(r.note)}</div>
      </div>`;
    await refresh();
  } catch(err) { result.innerHTML = `<span class="err">Erro: ${esc(err.message)}</span>`; }
}

// ---------------------------------------------------------------------------
// Refresh state
// ---------------------------------------------------------------------------
async function refresh() {
  STATE = await api('/api/state');
  // normalize assessments: parse vector floats
  STATE.assessments = (STATE.assessments || []).map(a => ({
    ...a,
    vector: {
      exposure:      parseFloat(a.vector?.exposure      ?? 0),
      interaction:   parseFloat(a.vector?.interaction   ?? 0),
      appropriation: parseFloat(a.vector?.appropriation ?? 0),
      incorporation: parseFloat(a.vector?.incorporation ?? 0),
      propagation:   parseFloat(a.vector?.propagation   ?? 0),
      reflexivity:   parseFloat(a.vector?.reflexivity   ?? 0),
      stabilization: parseFloat(a.vector?.stabilization ?? 0),
    }
  }));
  STATE.bundles = STATE.bundles || [];

  document.getElementById('health').textContent = 'core: READY';
  document.getElementById('model').textContent = 'modelo: ' + (STATE.system?.model ?? '—');

  fillSelects();
  renderMetrics();
  renderTrajectory();
  renderRecords();

  // Show last assessment in vector panel
  const lastAsr = STATE.assessments.at(-1);
  renderVector(lastAsr);

  try {
    const m = await api('/api/model/status');
    document.getElementById('model-note').textContent = m.note;
  } catch(_) {}
}

// ---------------------------------------------------------------------------
// Form submission
// ---------------------------------------------------------------------------
document.addEventListener('DOMContentLoaded', () => {
  document.querySelectorAll('form[data-endpoint]').forEach(form => {
    form.addEventListener('submit', async e => {
      e.preventDefault();
      // For evidence form: collect contributions first
      if (form.id === 'form-evidence') {
        const hc = document.getElementById('hidden-contributions');
        if (hc) hc.value = collectContributions();
      }
      const body = new URLSearchParams(new FormData(form));
      try {
        await api(form.dataset.endpoint, {
          method: 'POST',
          headers: {'Content-Type': 'application/x-www-form-urlencoded'},
          body
        });
        form.reset();
        if (form.id === 'form-evidence') clearContribInputs();
        await refresh();
      } catch(err) { alert('Falha: ' + err.message); }
    });
  });

  refresh().catch(err => {
    document.getElementById('health').textContent = 'erro';
    console.error(err);
  });
});
