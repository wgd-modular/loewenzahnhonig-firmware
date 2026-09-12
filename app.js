"use strict";

const REPO = "wgd-modular/loewenzahnhonig-firmware";
const FLASH_START = 0x08000000;
const ACCENTS = ["honey", "clover", "rust", "meadow"];

let dfuDevice = null;

const $ = (sel, el) => (el || document).querySelector(sel);

/* ---------- release loading ---------- */

async function loadRelease() {
  const res = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`);
  if (!res.ok) throw new Error(`GitHub API: ${res.status}`);
  return res.json();
}

function firmwareName(assetName) {
  // Arduino-built firmwares (compressor, delay, reverb, shimmer-reverb) are
  // named after their .ino sketch, so the asset is "name.ino.bin" rather
  // than the Makefile firmwares' "name.bin".
  return assetName.replace(/(\.ino)?\.bin$/, "");
}

// The splooge-reverb firmware's Makefile TARGET (and so its release binary)
// is "sploodge-reverb", one letter off from its src/ folder.
const README_FOLDER = { "sploodge-reverb": "splooge-reverb" };

function readmeFolder(fw) {
  return README_FOLDER[fw] || fw;
}

async function fetchReadme(fw, tag) {
  const folder = readmeFolder(fw);
  for (const ref of [tag, "main"]) {
    const res = await fetch(`https://raw.githubusercontent.com/${REPO}/${ref}/src/${folder}/README.md`);
    if (res.ok) return res.text();
  }
  return null;
}

// The firmware READMEs in this repo don't follow one fixed layout: the
// description sits in different sections ("## Description", "## Current
// State", straight after the title, or after "## What it does"), and the
// controls are documented as a table, a flat bullet list or (Nimbus) a
// heading per control. This parses all of them well enough for a card.

function isBulletRow(line) {
  return /^[-*]\s+.+:\s*.+$/.test(line);
}

function bulletRunAt(lines, idx) {
  let start = idx;
  let end = idx + 1;
  while (start > 0 && isBulletRow(lines[start - 1].trim())) start--;
  while (end < lines.length && isBulletRow(lines[end].trim())) end++;
  const rows = [];
  for (let i = start; i < end; i++) {
    const m = lines[i].trim().match(/^[-*]\s+(.+?):\s*(.+)$/);
    if (m) rows.push([m[1], m[2]]);
  }
  return rows;
}

function parseReadme(md) {
  const lines = md.split("\n");

  // Description: the first prose paragraph, skipping the title, the
  // "Author"/"Version History" sections and anything that isn't prose.
  let description = "";
  let skip = false;
  for (const raw of lines) {
    const l = raw.trim();
    const heading = l.match(/^#{1,6}\s*(.+?)\s*$/);
    if (heading) {
      if (description) break;
      skip = /^(author|version history)\b/i.test(heading[1]);
      continue;
    }
    if (!l) {
      if (description) break;
      continue;
    }
    if (skip) continue;
    if (l.startsWith("|") || l.startsWith(">") || l.startsWith("```") || /^[-*]\s/.test(l) || /^\d+\.\s/.test(l)) {
      break;
    }
    description = description ? `${description} ${l}` : l;
  }

  // Controls: prefer a "Controls" section (table or bullet list), else the
  // first control-like bullet list anywhere in the document.
  let rows = [];
  const headingIdx = lines.findIndex(l => /^#{1,6}\s*controls?\s*:?\s*$/i.test(l.trim()));
  if (headingIdx >= 0) {
    let end = lines.length;
    for (let i = headingIdx + 1; i < lines.length; i++) {
      if (/^#{1,6}\s/.test(lines[i])) { end = i; break; }
    }
    for (let i = headingIdx + 1; i < end; i++) {
      const l = lines[i].trim();
      if (!l.startsWith("|")) continue;
      const cells = l.split("|").slice(1, -1).map(c => c.trim());
      if (cells.every(c => /^[-: ]*$/.test(c))) continue;
      rows.push(cells);
    }
    if (!rows.length) {
      const bulletOffset = lines.slice(headingIdx + 1, end).findIndex(l => isBulletRow(l.trim()));
      if (bulletOffset >= 0) rows = bulletRunAt(lines, headingIdx + 1 + bulletOffset);
    }
  }
  if (!rows.length) {
    const idx = lines.findIndex(l => /^[-*]\s*(Pot|CV)\s*\d/i.test(l.trim()));
    if (idx >= 0) rows = bulletRunAt(lines, idx);
  }
  if (rows.length && !/control/i.test(rows[0][0])) {
    rows.unshift(["Control", "Function"]);
  }

  return { description, rows };
}

// A description can carry a markdown link (Nimbus' variants link back to
// "../nimbus/README.md", Cloud Seed links out to GitHub); render those as
// real links instead of leaving the raw "[text](url)" in the text.

function escapeHtml(s) {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

function resolveReadmeLink(url, folder, tag) {
  if (/^([a-z][a-z0-9+.-]*:)?\/\//i.test(url) || url.startsWith("mailto:")) return url;
  if (url.startsWith("#")) return `https://github.com/${REPO}/blob/${tag}/src/${folder}/README.md${url}`;
  const parts = `src/${folder}/${url}`.split("/");
  const resolved = [];
  for (const part of parts) {
    if (part === "." || part === "") continue;
    if (part === "..") resolved.pop();
    else resolved.push(part);
  }
  return `https://github.com/${REPO}/blob/${tag}/${resolved.join("/")}`;
}

function renderInlineLinks(text, folder, tag) {
  const re = /\[([^\]]+)\]\(([^)]+)\)/g;
  let out = "";
  let last = 0;
  let m;
  while ((m = re.exec(text))) {
    out += escapeHtml(text.slice(last, m.index));
    const href = escapeHtml(resolveReadmeLink(m[2], folder, tag));
    out += `<a href="${href}" target="_blank" rel="noopener">${escapeHtml(m[1])}</a>`;
    last = re.lastIndex;
  }
  out += escapeHtml(text.slice(last));
  return out;
}

/* ---------- cards ---------- */

function buildCard(asset, tag, accent) {
  const fw = firmwareName(asset.name);
  const card = document.createElement("article");
  card.className = `card ${accent}`;
  card.innerHTML = `
    <div class="card-head">
      <svg class="fw-mark" viewBox="0 0 86 43" aria-hidden="true"><use href="pattern.svg#leaf" fill="currentColor"/></svg>
      <h2>${fw.replace(/-/g, " ")}</h2>
      <span class="ver">${tag}</span>
    </div>
    <div class="card-body">
      <p class="desc">Loading description…</p>
      <div class="controls" hidden><h3>Controls</h3><table></table></div>
      <p class="status-line" hidden></p>
      <div class="progress" hidden><div class="bar"></div><span class="ptext"></span></div>
      <div class="card-foot">
        <a class="readme-link" target="_blank" rel="noopener"
           href="https://github.com/${REPO}/blob/${tag}/src/${readmeFolder(fw)}/README.md">Full guide ↗</a>
        <button class="btn flash-btn">Flash</button>
      </div>
    </div>`;

  $(".flash-btn", card).addEventListener("click", () => flashAsset(asset, tag, card));

  fetchReadme(fw, tag).then(md => {
    if (!md) {
      $(".desc", card).textContent = "No description available.";
      return;
    }
    const { description, rows } = parseReadme(md);
    const desc = $(".desc", card);
    if (description) desc.innerHTML = renderInlineLinks(description, readmeFolder(fw), tag);
    else desc.textContent = "No description available.";
    if (rows.length) {
      const table = $(".controls table", card);
      const [head, ...body] = rows;
      const folder = readmeFolder(fw);
      const cell = c => renderInlineLinks(c, folder, tag);
      const tr = h => `<tr>${h}</tr>`;
      table.innerHTML =
        tr(head.map(c => `<th>${cell(c)}</th>`).join("")) +
        body.map(r => tr(r.map(c => `<td>${cell(c)}</td>`).join(""))).join("");
      $(".controls", card).hidden = false;
    }
  });

  return card;
}

async function render() {
  if (!("usb" in navigator)) {
    $("#unsupported").hidden = false;
    $("#connect").disabled = true;
  }
  try {
    const release = await loadRelease();
    const date = new Date(release.published_at).toLocaleDateString("en-GB",
      { day: "numeric", month: "long", year: "numeric" });
    $("#release-chip").textContent = `latest release ${release.tag_name} · ${date}`;

    const bins = release.assets
      .filter(a => a.name.endsWith(".bin"))
      .sort((a, b) => a.name.localeCompare(b.name));

    const cards = $("#cards");
    bins.forEach((asset, i) =>
      cards.appendChild(buildCard(asset, release.tag_name, ACCENTS[i % ACCENTS.length])));
  } catch (err) {
    $("#release-chip").textContent = "release list unavailable";
    const box = $("#load-error");
    box.textContent = `Couldn’t load the release list from GitHub (${err.message}). ` +
      "You can still flash a local .bin below.";
    box.hidden = false;
  }
}

/* ---------- DFU ---------- */

async function getTransferSize(device) {
  try {
    const data = await device.readConfigurationDescriptor(0);
    const config = dfu.parseConfigurationDescriptor(data);
    for (const desc of config.descriptors) {
      if (desc.bDescriptorType === 0x21 && desc.hasOwnProperty("wTransferSize")) {
        return desc.wTransferSize;
      }
    }
  } catch (e) { /* fall through */ }
  return 1024;
}

async function connectDevice() {
  const usbDevice = await navigator.usb.requestDevice({
    filters: [{ vendorId: 0x0483, productId: 0xdf11 }]
  });

  let interfaces = dfu.findDeviceDfuInterfaces(usbDevice);
  if (!interfaces.length) throw new Error("no DFU interface — is the Seed in bootloader mode?");

  await usbDevice.open();
  const probe = new dfu.Device(usbDevice, interfaces[0]);
  const names = await probe.readInterfaceNames();
  for (const intf of interfaces) {
    if (intf.name === null) {
      const c = intf.configuration.configurationValue;
      intf.name = names[c]?.[intf.interface.interfaceNumber]?.[intf.alternate.alternateSetting];
    }
  }

  let chosen = interfaces.find(i => i.name && i.name.includes("Internal Flash")) || interfaces[0];
  const device = new dfuse.Device(usbDevice, chosen);
  await device.open();
  if (!device.memoryInfo) throw new Error("device did not report a DfuSe memory map");
  return device;
}

async function ensureDevice() {
  if (dfuDevice && dfuDevice.device_.opened) return dfuDevice;
  dfuDevice = await connectDevice();
  const status = $("#device-status");
  status.textContent = `connected: ${dfuDevice.device_.productName || "STM32 bootloader"}`;
  status.classList.add("ok");
  navigator.usb.addEventListener("disconnect", e => {
    if (dfuDevice && e.device === dfuDevice.device_) {
      dfuDevice = null;
      status.textContent = "device disconnected";
      status.classList.remove("ok");
    }
  });
  return dfuDevice;
}

async function flashBuffer(buffer, ui) {
  const device = await ensureDevice();

  if (buffer.byteLength > 128 * 1024) {
    throw new Error("file is larger than the Seed's 128 KB flash");
  }

  ui.status("Preparing…");
  let state = await device.getStatus();
  if (state.state === dfu.dfuERROR) await device.clearStatus();

  device.startAddress = FLASH_START;
  device.logProgress = (done, total) => {
    if (total) ui.progress(done / total, `${Math.round(done / total * 100)}%`);
  };
  device.logInfo = msg => {
    if (/erase/i.test(msg)) ui.status("Erasing…");
    else if (/download|copying|wrote/i.test(msg)) ui.status("Writing…");
  };
  device.logWarning = () => {};
  device.logDebug = () => {};
  device.logError = () => {};

  const transferSize = await getTransferSize(device);
  await device.do_download(transferSize, buffer, false);
  ui.progress(1, "100%");
  ui.done("Flashed! The module restarts on its own — if not, tap RESET.");
}

function cardUi(card) {
  const line = $(".status-line", card);
  const prog = $(".progress", card);
  const btn = $(".flash-btn", card) || $("#local-flash");
  return {
    start() { btn.disabled = true; line.hidden = false; line.className = "status-line"; prog.hidden = false; },
    status(t) { line.textContent = t; },
    progress(f, t) { prog.style.setProperty("--p", `${f * 100}%`); $(".ptext", prog).textContent = t; },
    done(t) { line.textContent = t; line.className = "status-line ok"; btn.disabled = false; },
    fail(t) { line.textContent = t; line.className = "status-line err"; prog.hidden = true; btn.disabled = false; }
  };
}

async function flashAsset(asset, tag, card) {
  const ui = cardUi(card);
  ui.start();
  try {
    ui.status("Downloading firmware…");
    // GitHub's release-asset download redirects to a storage host that sends
    // no Access-Control-Allow-Origin header, so a cross-origin fetch() of
    // asset.browser_download_url always fails CORS. The release workflow
    // also mirrors each .bin under firmwares/<tag>/ on this same origin.
    const res = await fetch(`firmwares/${tag}/${asset.name}`);
    if (!res.ok) throw new Error(`download failed (${res.status})`);
    const buffer = await res.arrayBuffer();
    await flashBuffer(buffer, ui);
  } catch (err) {
    ui.fail(friendly(err));
  }
}

function friendly(err) {
  const msg = err && err.message ? err.message : String(err);
  if (/No device selected/i.test(msg)) return "No device selected.";
  if (/no DFU interface|bootloader/i.test(msg)) return msg;
  return `Something went wrong: ${msg}`;
}

/* ---------- wiring ---------- */

$("#connect").addEventListener("click", async () => {
  try { await ensureDevice(); }
  catch (err) {
    const status = $("#device-status");
    status.textContent = friendly(err);
    status.classList.remove("ok");
  }
});

let localBuffer = null;
$("#local-file").addEventListener("change", async e => {
  const file = e.target.files[0];
  if (!file) return;
  localBuffer = await file.arrayBuffer();
  $("#local-name").textContent = `${file.name} · ${(file.size / 1024).toFixed(1)} KB`;
  $("#local-flash").disabled = false;
});

$("#local-flash").addEventListener("click", async () => {
  const prog = $("#local-progress");
  const line = document.createElement("p");
  const holder = $(".local-body");
  let ui = {
    start() { $("#local-flash").disabled = true; prog.hidden = false; },
    status(t) { $(".ptext", prog).textContent = t; },
    progress(f, t) { prog.style.setProperty("--p", `${f * 100}%`); $(".ptext", prog).textContent = t; },
    done(t) { $(".ptext", prog).textContent = "done"; line.className = "status-line ok"; line.textContent = t; holder.appendChild(line); $("#local-flash").disabled = false; },
    fail(t) { line.className = "status-line err"; line.textContent = t; holder.appendChild(line); $("#local-flash").disabled = false; }
  };
  ui.start();
  try { await flashBuffer(localBuffer, ui); }
  catch (err) { ui.fail(friendly(err)); }
});

render();
