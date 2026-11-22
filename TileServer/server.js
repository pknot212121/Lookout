"use strict";

const http = require("node:http");
const Database = require("better-sqlite3");

const MBTILES_PATH = process.argv[2] || process.env.MBTILES_PATH;
const PORT = Number(process.env.PORT) || 8070;
const HOST = process.env.HOST || "0.0.0.0";

if (!MBTILES_PATH) {
  console.error("Użycie: node server.js <plik.mbtiles>");
  process.exit(1);
}

const db = new Database(MBTILES_PATH, { readonly: true, fileMustExist: true });
db.pragma("query_only = ON");
db.pragma("cache_size = -262144");
db.pragma("mmap_size = 1073741824");

const tileStmt = db.prepare(
  "SELECT tile_data FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?"
);

const metadata = Object.fromEntries(
  db.prepare("SELECT name, value FROM metadata").all().map((r) => [r.name, r.value])
);

const MIME = {
  png: "image/png",
  jpg: "image/jpeg",
  jpeg: "image/jpeg",
  webp: "image/webp",
  pbf: "application/x-protobuf",
};

function detectMime(buf) {
  if (buf.length >= 8 && buf[0] === 0x89 && buf[1] === 0x50) return "image/png";
  if (buf.length >= 3 && buf[0] === 0xff && buf[1] === 0xd8) return "image/jpeg";
  if (buf.length >= 12 && buf.toString("ascii", 0, 4) === "RIFF" && buf.toString("ascii", 8, 12) === "WEBP")
    return "image/webp";
  return "application/octet-stream";
}

const defaultMime = MIME[(metadata.format || "").toLowerCase()] || null;
const minZoom = metadata.minzoom !== undefined ? Number(metadata.minzoom) : 0;
const maxZoom = metadata.maxzoom !== undefined ? Number(metadata.maxzoom) : 30;

console.log(`Otwarto ${MBTILES_PATH}`);
console.log(`format=${metadata.format} minzoom=${minZoom} maxzoom=${maxZoom}`);

const TILE_RE = /^\/tile\/(\d{1,2})\/(\d{1,9})\/(\d{1,9})(?:\.\w+)?$/;

function send(res, status, headers, body) {
  res.writeHead(status, {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, HEAD, OPTIONS",
    "Access-Control-Allow-Headers": "Range, Content-Type",
    ...headers,
  });
  res.end(body);
}

const server = http.createServer((req, res) => {
  if (req.method === "OPTIONS") return send(res, 204, {});
  if (req.method !== "GET" && req.method !== "HEAD") {
    return send(res, 405, { Allow: "GET, HEAD, OPTIONS" });
  }

  const url = new URL(req.url, "http://localhost");

  if (url.pathname === "/metadata") {
    return send(
      res,
      200,
      { "Content-Type": "application/json", "Cache-Control": "public, max-age=3600" },
      JSON.stringify(metadata)
    );
  }

  const m = TILE_RE.exec(url.pathname);
  if (!m) return send(res, 404, { "Content-Type": "text/plain" }, "Not found");

  const z = Number(m[1]);
  const x = Number(m[2]);
  const y = Number(m[3]);
  const n = 2 ** z;
  if (z < minZoom || z > maxZoom || x >= n || y >= n) {
    return send(res, 404, { "Content-Type": "text/plain" }, "Tile out of range");
  }

  const tmsRow = n - 1 - y;

  let row;
  try {
    row = tileStmt.get(z, x, tmsRow);
  } catch (err) {
    console.error("Błąd SQLite:", err);
    return send(res, 500, { "Content-Type": "text/plain" }, "Internal error");
  }

  if (!row) return send(res, 404, { "Content-Type": "text/plain", "Cache-Control": "public, max-age=300" }, "No tile");

  const data = row.tile_data;
  send(
    res,
    200,
    {
      "Content-Type": defaultMime || detectMime(data),
      "Content-Length": data.length,
      "Cache-Control": "public, max-age=31536000, immutable",
    },
    req.method === "HEAD" ? undefined : data
  );
});

server.listen(PORT, HOST, () => {
  console.log(`Serwer: http://${HOST}:${PORT}/tile/{z}/{x}/{y}`);
});

function shutdown() {
  server.close(() => {
    db.close();
    process.exit(0);
  });
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
