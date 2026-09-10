#!/usr/bin/env python3
"""Static pages over an order-sweep record (sweep_job.sh TSVs) — SWEEP.md §2.

    experiments/order/sweep_pages.py results/sweep1.tsv [more.tsv ...] -o /data/ythierry/MCC26logs/web/order-sweep [--logs results/sweep1]

One page per examination and an index; the campaign pages' look (DataTables
and Plotly from their CDNs, the data embedded as JSON, one file per page).
Views: heuristics against each other (unique wins, marginal contribution,
dominated by), the pairwise matrix (a click selects the pair), the instances
heatmap with a metric selector, the scatter A against B, the cactus (runs
complete within t; answers over time), families (mean rank), and one instance
in depth (every heuristic's row, links to log and shape). `--logs DIR` links
the runs as `/logs/<absolute path>` and writes `roots.json`, so
`MCC-analysis/campaign/serve.py PAGES_DIR` serves pages and logs unchanged.
"""
from __future__ import annotations

import argparse
import csv
import datetime
import html
import json
import os
import statistics
from collections import defaultdict
from typing import Dict, List, Optional, Tuple

Row = Dict[str, str]


def num(s: Optional[str]) -> Optional[float]:
    try:
        return float(s) if s not in (None, "") else None
    except ValueError:
        return None


def load(paths: List[str]) -> List[Row]:
    rows: List[Row] = []
    for p in paths:
        with open(p, newline="") as f:
            for r in csv.DictReader(f, delimiter="\t"):
                # a torn line (concurrent appends) has the wrong field count or no instance
                if r.get("heuristic") and r["heuristic"] != "-" and r.get("instance") and r.get("exam") in ("SS", "CTLC", "CTLF", "RC", "RF", "RD") and r.get("status"):
                    rows.append(r)
    return rows


def resolve_dups(rows: List[Row]) -> None:
    """A dup:<name> row takes the answers of the heuristic it duplicates (in place)."""
    by = {(r["instance"], r["exam"], r["heuristic"]): r for r in rows}
    for r in rows:
        if r["status"].startswith("dup:"):
            src = by.get((r["instance"], r["exam"], r["status"][4:]))
            if src is not None:
                for k in ("answered", "ok", "wrong", "unknown", "complete", "wall_s", "cpu_s", "maxrss_kb",
                          "reach_s", "reach_nodes", "belly_nodes", "answer_times", "reach_states", "partial"):
                    r[k] = src.get(k, "")
                r["dup_of"] = r["status"][4:]
                r["status"] = "dup"


def quantile(xs: List[float], p: float) -> Optional[float]:
    if not xs:
        return None
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p * len(xs)))]


def exam_data(rows: List[Row], exam: str, logs: Optional[str]) -> dict:
    rows = [r for r in rows if r["exam"] == exam]
    heur = sorted({r["heuristic"] for r in rows})
    inst = sorted({r["instance"] for r in rows})
    fam = {i: (rows_i["family"] if (rows_i := next(r for r in rows if r["instance"] == i)) else "") for i in inst}
    ans: Dict[str, Dict[str, int]] = defaultdict(dict)
    cell: Dict[Tuple[str, str], Row] = {}
    for r in rows:
        ans[r["instance"]][r["heuristic"]] = int(num(r["answered"]) or 0)
        cell[(r["instance"], r["heuristic"])] = r
    full = [i for i in inst if len(ans[i]) == len(heur)]
    best = {i: max(ans[i].values()) for i in full}
    vbest = sum(best.values())
    # 1. heuristics against each other
    summary = []
    for h in heur:
        hr = [r for r in rows if r["heuristic"] == h]
        st: Dict[str, int] = defaultdict(int)
        for r in hr:
            st[r["status"]] += 1
        walls = [w for r in hr if (w := num(r["wall_s"])) is not None]
        rss = [m / 1e6 for r in hr if (m := num(r["maxrss_kb"])) is not None]
        uniq = sum(1 for i in full if ans[i][h] == best[i] and sum(1 for g in heur if ans[i][g] == best[i]) == 1)
        without = sum(max(v for g, v in ans[i].items() if g != h) for i in full) if len(heur) > 1 else 0
        dominated = [g for g in heur if g != h and all(ans[i][g] >= ans[i][h] for i in full)
                     and any(ans[i][g] > ans[i][h] for i in full)]
        summary.append({"heuristic": h, "runs": len(hr), "answered": sum(int(num(r["answered"]) or 0) for r in hr),
                        "complete": sum(1 for r in hr if r["complete"] == "1"),
                        "wrong": sum(int(num(r["wrong"]) or 0) for r in hr),
                        "timeout": st["timeout"], "memory": st["memory"], "crash": st["crash"], "noreach": st["noreach"],
                        "dup": st["dup"], "wall50": quantile(walls, .5), "wall90": quantile(walls, .9),
                        "rss50": quantile(rss, .5), "rss90": quantile(rss, .9),
                        "unique": uniq, "marginal": vbest - without, "dominated": " ".join(dominated)})
    # 2. pairwise: row beats column
    pairs = [[sum(1 for i in full if ans[i][a] > ans[i][b]) for b in heur] for a in heur]
    # 3. instances: per heuristic the metrics
    def metrics(r: Optional[Row]) -> Optional[list]:
        if r is None:
            return None
        complete = r["complete"] == "1"
        return [int(num(r["answered"]) or 0), num(r["wall_s"]) if complete else None, num(r["reach_s"]),
                num(r["reach_nodes"]), (num(r["maxrss_kb"]) or 0) / 1e6, num(r["belly_nodes"]), r["status"],
                r.get("dup_of", ""), r.get("shape_sig", ""), num(r.get("reach_states")), r.get("partial", "")]
    instances = []
    for i in inst:
        cells = [metrics(cell.get((i, h))) for h in heur]
        vals = [c[0] for c in cells if c]
        states = num(next(r for r in rows if r["instance"] == i)["states"])
        instances.append({"instance": i, "family": fam[i], "states": states, "cells": cells,
                          "spread": (max(vals) - min(vals)) if vals else 0, "best": max(vals) if vals else 0,
                          "n": len(vals)})
    # 5. cactus: complete runs' walls per heuristic; answers over time from answer_times
    cactus = {}
    answers_t = {}
    for h in heur:
        cw = sorted(w for r in rows if r["heuristic"] == h and r["complete"] == "1" and (w := num(r["wall_s"])) is not None)
        cactus[h] = cw
        ts: List[float] = []
        for r in rows:
            if r["heuristic"] != h or not r.get("answer_times"):
                continue
            for tok in r["answer_times"].split(";"):
                if ":" in tok:
                    t = num(tok.rsplit(":", 1)[1])
                    if t is not None:
                        ts.append(t)
        answers_t[h] = sorted(ts)
    # 6. families: mean rank of each heuristic inside a family (1 = best), over full instances
    families: Dict[str, Dict[str, List[float]]] = defaultdict(lambda: defaultdict(list))
    for i in full:
        order = sorted(heur, key=lambda h: -ans[i][h])
        rank = {}
        k = 0
        for j, h in enumerate(order):
            if j == 0 or ans[i][h] != ans[i][order[j - 1]]:
                k = j + 1
            rank[h] = k
        for h in heur:
            families[fam[i]][h].append(rank[h])
    fam_rows = [{"family": f, "instances": len(next(iter(d.values()))), "ranks": [statistics.mean(d[h]) for h in heur]}
                for f, d in sorted(families.items())]
    return {"exam": exam, "heuristics": heur, "n_instances": len(inst), "n_full": len(full), "vbest": vbest,
            "summary": summary, "pairs": pairs, "instances": instances, "cactus": cactus, "answers_t": answers_t,
            "families": fam_rows, "logs": logs or ""}


CSS = """body { font-family: system-ui, sans-serif; margin: 1em 2em; color: #222; }
nav a { margin-right: .3em; } .muted { color: #666; } .controls { margin: .5em 0; }
.controls select, .controls input { margin: 0 .6em 0 .2em; }
table.plain { border-collapse: collapse; } table.plain th, table.plain td { border: 1px solid #ccc; padding: 2px 6px; text-align: right; font-size: 90%; }
table.plain td:first-child, table.plain th:first-child { text-align: left; }
#pairs td { cursor: pointer; } #pairs td:hover { background: #eef; } td.sel { outline: 2px solid #36c; }
.heat td.h { text-align: center; cursor: pointer; } a.log { font-size: 85%; margin-left: .3em; }
#detail { margin-top: .5em; }"""

JS = r"""
const H = DATA.heuristics; let A = 0, B = 1;
function fmt(x, d) { return x === null || x === undefined ? "" : (typeof x === "number" ? (Number.isInteger(x) ? x : x.toFixed(d === undefined ? 2 : d)) : x); }
function heat(v, max) { if (v === null) return "#eee"; const t = max ? v / max : 0; const g = Math.round(235 - 150 * t); return `rgb(${g},${Math.round(235 - 60 * t)},${Math.round(255 - 40 * t)})`; }
function summaryTable() {
  const cols = ["heuristic","runs","answered","complete","wrong","timeout","memory","crash","noreach","dup","wall50","wall90","rss50","rss90","unique","marginal","dominated"];
  $("#summary").DataTable({ data: DATA.summary.map(r => cols.map(c => fmt(r[c]))), columns: cols.map(c => ({ title: c })), paging: false, searching: false, info: false, order: [[15, "desc"], [14, "desc"]] });
}
function pairsTable() {
  let h = "<table class='plain'><tr><th>row beats column</th>" + H.map(x => `<th>${x}</th>`).join("") + "</tr>";
  DATA.pairs.forEach((row, a) => { h += `<tr><th>${H[a]}</th>` + row.map((v, b) => `<td data-a="${a}" data-b="${b}" class="${a===A&&b===B?'sel':''}" style="background:${a===b?'#f4f4f4':heat(v, DATA.n_full)}">${a===b?'':v}</td>`).join("") + "</tr>"; });
  $("#pairs").html(h + "</table>");
  $("#pairs td[data-a]").on("click", function () { A = +this.dataset.a; B = +this.dataset.b; if (A === B) return; $("#pairs td").removeClass("sel"); $(this).addClass("sel"); scatter(); });
}
const METRICS = [["answered", 0], ["time to complete (s)", 1], ["R time (s)", 2], ["R nodes", 3], ["peak RSS (GB)", 4], ["belly (nodes)", 5], ["states reached (partial when R not built)", 9]];
// Click a column head of any plain table to sort by it (numbers as numbers, again to reverse).
$(document).on("click", "table.plain th", function () {
  const th = this, tr = th.parentNode, tbl = $(th).closest("table")[0], col = Array.from(tr.children).indexOf(th);
  const rows = Array.from(tbl.rows).slice(1), asc = !th.classList.contains("asc");
  const val = r => { const t = r.cells[col] ? r.cells[col].textContent.trim() : ""; const n = parseFloat(t); return isNaN(n) ? t : n; };
  rows.sort((x, y) => { const a = val(x), b = val(y); const c = (typeof a === "number" && typeof b === "number") ? a - b : String(a).localeCompare(String(b)); return asc ? c : -c; });
  Array.from(tr.children).forEach(h => h.classList.remove("asc", "desc")); th.classList.add(asc ? "asc" : "desc");
  rows.forEach(r => tbl.appendChild(r));
});
let instTable = null;
function instancesTable() {
  const m = +$("#metric").val(), sortBy = $("#sort").val(), fam = new RegExp($("#family").val() || ".", "i"), filter = $("#filter").val();
  let rows = DATA.instances.filter(r => fam.test(r.family) || fam.test(r.instance));
  if (filter === "spread") rows = rows.filter(r => r.spread > 0);
  if (filter === "hard") rows = rows.filter(r => r.best === 0);
  if (filter === "some0") rows = rows.filter(r => r.cells.some(c => c && c[0] === 0) && r.best > 0);
  rows.sort((x, y) => sortBy === "spread" ? y.spread - x.spread || x.instance.localeCompare(y.instance) : sortBy === "hardness" ? x.best - y.best || x.instance.localeCompare(y.instance) : x.instance.localeCompare(y.instance));
  const logm = m === 9 || m === 3; const mv = c => (c && c[m] !== null && c[m] !== undefined) ? (logm ? Math.log10(1 + c[m]) : c[m]) : null;
  const max = m === 0 ? Math.max(...rows.map(r => r.best), 1) : Math.max(...rows.flatMap(r => r.cells.map(c => mv(c) || 0)), 1);
  let h = "<table class='plain heat'><tr><th>instance</th><th>family</th><th>states</th><th>spread</th>" + H.map(x => `<th>${x}</th>`).join("") + "</tr>";
  rows.forEach((r, i) => { h += `<tr><td><a href="#" data-i="${r.instance}" class="inst">${r.instance}</a></td><td>${r.family}</td><td>${r.states === null ? "?" : r.states.toExponential(1)}</td><td>${r.spread}</td>` +
    r.cells.map(c => { if (!c) return "<td></td>"; const v = c[m]; const bad = c[6] === "memory" || c[6] === "crash" || c[6] === "timeout"; const shown = m === 9 && v !== null && v !== undefined ? v.toExponential(1) + (c[10] === "1" ? "*" : "") : fmt(v, 1); return `<td class="h" title="${c[6]}${c[7]?' = '+c[7]:''}${c[9]!==null&&c[9]!==undefined?' — '+c[9].toExponential(2)+' states reached'+(c[10]==='1'?' (partial)':''):''}" style="background:${bad ? '#f8c8c8' : heat(mv(c), max)}">${shown}</td>`; }).join("") + "</tr>"; });
  $("#instances").html(h + "</table>");
  $("a.inst").on("click", function (e) { e.preventDefault(); detail(this.dataset.i); });
}
function detail(inst) {
  const r = DATA.instances.find(x => x.instance === inst); if (!r) return;
  let h = `<h3>${inst} (${r.family}, ${r.states === null ? "unknown size" : r.states.toExponential(2) + " states"})</h3><table class='plain'><tr><th>heuristic</th><th>status</th><th>answered</th><th>complete s</th><th>R s</th><th>R nodes</th><th>RSS GB</th><th>belly</th><th>states reached</th><th>shape</th><th></th></tr>`;
  r.cells.forEach((c, k) => { if (!c) return; const base = `logs${DATA.logs}/${inst}-${DATA.exam}-${H[k]}`; h += `<tr><td>${H[k]}</td><td>${c[6]}${c[7]?' = '+c[7]:''}</td><td>${c[0]}</td><td>${fmt(c[1],1)}</td><td>${fmt(c[2],3)}</td><td>${fmt(c[3])}</td><td>${fmt(c[4],2)}</td><td>${fmt(c[5])}</td><td>${c[9]!==null&&c[9]!==undefined?c[9].toExponential(2)+(c[10]==='1'?' (partial)':''):''}</td><td><code>${c[8]||''}</code></td><td>${DATA.logs ? `<a class="log" href="${base}.out" target="_blank">out</a><a class="log" href="${base}.err" target="_blank">err</a><a class="log" href="${base}.shape" target="_blank">shape</a>` : ''}</td></tr>`; });
  $("#detail").html(h + "</table>");
  window.location.hash = inst;
}
function scatter() {
  const m = +$("#smetric").val(); const pts = { x: [], y: [], text: [], marker: { color: [] } };
  DATA.instances.forEach(r => { const a = r.cells[A], b = r.cells[B]; if (!a || !b) return; const x = a[m], y = b[m]; if (x === null || y === null) return; pts.x.push(x || 1e-3); pts.y.push(y || 1e-3); pts.text.push(r.instance); pts.marker.color.push(a[0] > b[0] ? "#c33" : a[0] < b[0] ? "#36c" : "#999"); });
  Plotly.newPlot("scatter", [{ ...pts, mode: "markers", type: "scatter", hoverinfo: "text" }], { title: `${METRICS[m][0]}: ${H[A]} (x) against ${H[B]} (y) — red: x answered more, blue: y more`, xaxis: { type: "log", title: H[A] }, yaxis: { type: "log", title: H[B] }, height: 520 });
}
function cactus() {
  const traces = H.map(h => { const xs = DATA.cactus[h]; return { x: xs, y: xs.map((_, i) => i + 1), name: h, mode: "lines", line: { shape: "hv" } }; });
  Plotly.newPlot("cactus", traces, { title: "runs complete within t", xaxis: { type: "log", title: "seconds" }, yaxis: { title: "complete runs" }, height: 420 });
  const tr2 = H.map(h => { const xs = DATA.answers_t[h]; return { x: xs, y: xs.map((_, i) => i + 1), name: h, mode: "lines", line: { shape: "hv" } }; });
  Plotly.newPlot("answers", tr2, { title: "answers given within t (from the answer timestamps)", xaxis: { type: "log", title: "seconds" }, yaxis: { title: "answers" }, height: 420 });
}
function familiesTable() {
  let h = "<table class='plain heat'><tr><th>family</th><th>instances</th>" + H.map(x => `<th>${x}</th>`).join("") + "</tr>";
  DATA.families.forEach(f => { h += `<tr><td>${f.family}</td><td>${f.instances}</td>` + f.ranks.map(v => `<td class="h" style="background:${heat(H.length - v, H.length - 1)}">${v.toFixed(1)}</td>`).join("") + "</tr>"; });
  $("#families").html(h + "</table>");
}
$(function () {
  METRICS.forEach(([n, i]) => { $("#metric, #smetric").append(`<option value="${i}">${n}</option>`); });
  summaryTable(); pairsTable(); instancesTable(); scatter(); cactus(); familiesTable();
  $("#metric, #sort, #family, #filter").on("change keyup", instancesTable); $("#smetric").on("change", scatter);
  if (window.location.hash.length > 1) detail(decodeURIComponent(window.location.hash.substring(1)));
});
"""

PAGE = """<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>order sweep — {exam}</title>
<link rel="stylesheet" href="https://cdn.datatables.net/1.13.4/css/jquery.dataTables.css">
<script src="https://code.jquery.com/jquery-3.6.0.min.js"></script>
<script src="https://cdn.datatables.net/1.13.4/js/jquery.dataTables.js"></script>
<script src="https://cdn.plot.ly/plotly-2.27.0.min.js"></script>
<style>{css}</style></head><body>
<nav><a href="index.html">index</a> {nav}</nav>
<h1>{exam}</h1>
<p class="muted">Generated {stamp}. {n_instances} instances, {n_full} with every heuristic; the comparisons (unique wins, marginal contribution, dominated by, pairs, families) are over those. Virtual best over them: {vbest} answered. A <b>dup</b> run produced the same shape as the heuristic it names and was not rerun. Statuses: ok, noreach (the budget ended inside the reachable set), timeout, memory (the node's cap), crash.</p>
<h2>Heuristics against each other</h2>
<p class="muted">unique: instances where it alone answers the most; marginal: answers the virtual best loses without it; dominated by: heuristics that answer at least as much on every instance, more on one.</p>
<table id="summary" class="display compact"></table>
<h2>Pairwise</h2><p class="muted">Row beats column: instances where the row answers strictly more. Click a cell to select the pair for the scatter.</p>
<div id="pairs"></div>
<h2>Instances</h2>
<div class="controls">metric <select id="metric"></select> sort <select id="sort"><option value="spread">spread</option><option value="hardness">hardness</option><option value="name">name</option></select>
filter <select id="filter"><option value="">all</option><option value="spread">order matters (spread &gt; 0)</option><option value="some0">some heuristic empty, another not</option><option value="hard">nobody answers</option></select>
family <input id="family" placeholder="regex"></div>
<div id="instances"></div>
<div id="detail"></div>
<h2>Scatter: A against B</h2><div class="controls">metric <select id="smetric"></select> (pair from the matrix above)</div><div id="scatter"></div>
<h2>Cactus</h2><div id="cactus"></div><div id="answers"></div>
<h2>Families</h2><p class="muted">Mean rank of the heuristic inside the family (1 = best), over the instances with every heuristic.</p><div id="families"></div>
<script>const DATA = {data};</script><script>{js}</script></body></html>"""

INDEX = """<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>order sweep</title><style>{css}</style></head><body>
<h1>Order sweep — {tag}</h1><p class="muted">Generated {stamp}. Design: experiments/order/SWEEP.md.</p><ul>{items}</ul></body></html>"""


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("tsv", nargs="+")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--logs", default=None, help="results folder with the per-run .out/.err/.shape; linked as /logs/<absolute path> and allowed in roots.json, the convention of MCC-analysis/campaign/serve.py")
    a = ap.parse_args()
    if a.logs:
        a.logs = os.path.abspath(a.logs)
    rows = load(a.tsv)
    resolve_dups(rows)
    exams = sorted({r["exam"] for r in rows})
    os.makedirs(a.out, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    nav = " ".join(f'| <a href="{e}.html">{e}</a>' for e in exams)
    items = []
    for e in exams:
        d = exam_data(rows, e, a.logs)
        page = PAGE.format(exam=html.escape(e), css=CSS, nav=nav, stamp=stamp, n_instances=d["n_instances"], n_full=d["n_full"],
                           vbest=d["vbest"], data=json.dumps(d, separators=(",", ":")), js=JS)
        with open(os.path.join(a.out, f"{e}.html"), "w") as f:
            f.write(page)
        items.append(f'<li><a href="{e}.html">{e}</a>: {d["n_instances"]} instances, {d["n_full"]} with every heuristic, virtual best {d["vbest"]}</li>')
        print(f"{e}: {d['n_instances']} instances, {d['n_full']} full, {len(page)//1024} kB")
    with open(os.path.join(a.out, "index.html"), "w") as f:
        f.write(INDEX.format(css=CSS, tag=html.escape(os.path.basename(a.tsv[0])), stamp=stamp, items="".join(items)))
    with open(os.path.join(a.out, "roots.json"), "w") as f:  # what serve.py may hand out under /logs/
        json.dump([a.logs] if a.logs else [], f)
    print(os.path.join(a.out, "index.html"))


if __name__ == "__main__":
    main()
