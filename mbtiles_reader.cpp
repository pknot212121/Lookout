#include "mbtiles_reader.h"
#include <cstdint>
#include <cstdio>

void MBTilesReader::requestTile(uint32_t z, uint32_t x, uint32_t y, FetchCb cb)
{
    char url[96];
    std::snprintf(url, sizeof(url), "http://127.0.0.1:8070/tile/%u/%u/%u", z, x, y);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.userData = new TileRequest{ z, x, y, std::move(cb)};

    attr.onsuccess = [](emscripten_fetch_t* f) {
        auto* req = static_cast<TileRequest*>(f->userData);
        auto* p = reinterpret_cast<const uint8_t*>(f->data);
        std::vector<uint8_t> bytes(p, p + f->numBytes);
        int status = f->status;
        FetchCb cb = std::move(req->cb);
        delete req;
        emscripten_fetch_close(f);
        cb(true, status, std::move(bytes));
    };
    attr.onerror = [](emscripten_fetch_t* f) {
        auto* req = static_cast<TileRequest*>(f->userData);
        int status = f->status;
        FetchCb cb = std::move(req->cb);
        delete req;
        emscripten_fetch_close(f);
        cb(false, status, {});
    };
    
    emscripten_fetch(&attr, url);
}