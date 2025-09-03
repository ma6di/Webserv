// methods: static vs dynamic switch
const btnStatic = document.getElementById("showStatic");
const btnDynamic = document.getElementById("showDynamic");
const panelStatic = document.getElementById("panel-static");
const panelDyn = document.getElementById("panel-dynamic");
function show(which) {
    const s = which === "static";
    panelStatic.classList.toggle("d-none", !s);
    panelDyn.classList.toggle("d-none", s);
    btnStatic.classList.toggle("active", s);
    btnDynamic.classList.toggle("active", !s);
}
btnStatic.addEventListener("click", () => show("static"));
btnDynamic.addEventListener("click", () => show("dynamic"));

// GET + DELETE demo
const ENDPOINT_LIST = "/upload";
const DELETE_BASE = "/upload/";

const loadBtn = document.getElementById("loadBtn");
const statusEl = document.getElementById("status");
const alertEl = document.getElementById("alert");
const listEl = document.getElementById("fileList");
const emptyEl = document.getElementById("empty");

if (loadBtn) {
    loadBtn.addEventListener("click", loadFiles);
    listEl.addEventListener("click", onListClick);
}

async function loadFiles() {
    clearUI();
    statusEl.textContent = "GET REQUEST SENT…";
    try {
        const res = await fetch(ENDPOINT_LIST, {
            cache: "no-store", headers: { "X-Frontend": "1", "Accept": "application/json" }
        });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const { files = [] } = await res.json();     // server shape: { ok:true, files:[...] }
        render(files);
        statusEl.textContent = `GET REQUEST SUCCESS ✅ (${files.length}) files found`;
        console.log("✅ Get request handled successfully!");
        console.log("Returned array from upload folder:", files);
    } catch (e) {
        showError(e.message || String(e));
        statusEl.textContent = "GET REQUEST FAILED ❌";
    }
}

function render(files) {
    listEl.innerHTML = files
        .map(
            (name) => `
        <li class="file-item" data-name="${escapeAttr(name)}">
          <a class="file-name link-light text-decoration-none"
             href="${DELETE_BASE + encodeURIComponent(name)}"
             target="_blank" rel="noopener">${escapeHtml(name)}</a>
          <button class="btn btn-outline-danger delete-btn" title="Delete">delete</button>
        </li>`
        )
        .join("");
    emptyEl.classList.toggle("d-none", files.length !== 0);
}

async function onListClick(e) {
    const btn = e.target.closest(".delete-btn");
    if (!btn) return;
    const li = btn.closest(".file-item");
    if (!li) return;
    const name = li.getAttribute("data-name");
    if (!name) return;
    if (!confirm(`Delete "${name}"?`)) return;

    try {
        const res = await fetch(
            DELETE_BASE + encodeURIComponent(name),
            { method: "DELETE" }
        );
        if (!res.ok)
            throw new Error(`Delete failed: HTTP ${res.status}`);
        li.remove();
        emptyEl.classList.toggle(
            "d-none",
            listEl.children.length !== 0
        );
        statusEl.textContent = `Deleted: ${name}`;
        console.log("✅ DELETE request handled successfully!");
        console.log("Deleted file: upload/" + name);
    } catch (e2) {
        showError(e2.message || String(e2));
    }
}

function clearUI() {
    alertEl.classList.add("d-none");
    alertEl.textContent = "";
    listEl.innerHTML = "";
    emptyEl.classList.add("d-none");
}
function showError(msg) {
    alertEl.textContent = msg;
    alertEl.classList.remove("d-none");
}
function escapeHtml(s) {
    return String(s)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}
function escapeAttr(s) {
    return String(s).replace(/"/g, "&quot;");
}