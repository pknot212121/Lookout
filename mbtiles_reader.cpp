#include "mbtiles_reader.h"
#include <cstdint>
#include <cstdio>

void MBTilesReader::requestTile(uint32_t z, uint32_t x, uint32_t y, TileCallback cb)
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
        req->cb(req->z, req->x, req->y, std::vector<uint8_t>(p, p + f->numBytes));
        delete req;
        emscripten_fetch_close(f);
    };
    
    attr.onerror = [](emscripten_fetch_t* f) {
        auto* req = static_cast<TileRequest*>(f->userData);
        std::cerr << "[Fetch] tile " << req->z << "/" << req->x << "/" << req->y
                  << " failed, status " << f->status << "\n";
        delete req;
        emscripten_fetch_close(f);
    };
    
    emscripten_fetch(&attr, url);
}