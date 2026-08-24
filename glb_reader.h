#include "glm/ext/matrix_float4x4.hpp"
#include "misc.h"
#include <cstdint>
#include <stb_image.h>
#include <tiny_gltf_v3.h>

namespace fs = std::filesystem;
struct GlbModelData
{
    std::vector<VertexAttributes> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint8_t> textureData;
    int texWidth = 0, texHeight = 0, texChannels = 0;
};

class GlbReader
{
    public:
        bool loadGlbModel(const fs::path& path);
        const GlbModelData& getData() const {return outData;}
    private:
        const uint8_t* getAccessorData(const tg3_accessor& acc);
        bool parseFile(const fs::path& path);
        void loadTexture();
        void loadVerticies();
        glm::mat4 getNodeMatrix(const tg3_node& node);
        void processNode(uint32_t nodeIdx, const glm::mat4& parentMatrix);
        
        tg3_model model;
        GlbModelData outData;
        tg3_error_stack errors;
};