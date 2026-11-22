#include <cstdint>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/fetch.h>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>

using TileCallback = std::function<void(uint32_t z, uint32_t x, uint32_t y, std::vector<uint8_t> bytes)>;

struct TileRequest
{
    uint32_t z;
    uint32_t x;
    uint32_t y;
    TileCallback cb;
};

class MBTilesReader {

public:
    void requestTile(uint32_t z, uint32_t x, uint32_t y, TileCallback cb);
};