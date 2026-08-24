#include "glb_reader.h"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/matrix.hpp"
#include "tiny_gltf_v3.h"
#include <cstddef>
#include <cstdint>

template<typename T>
void appendIndicies(const uint8_t* data, size_t count, uint32_t baseVertexIndex, std::vector<uint32_t>& outIndicies)
{
    const T* ptr = reinterpret_cast<const T*>(data);
    outIndicies.reserve(outIndicies.size() + count); // Later add more efficient initial reservation
    for (size_t i = 0; i < count; i++)
    {
        outIndicies.push_back(baseVertexIndex + static_cast<uint32_t>(ptr[i]));
    }
}

const uint8_t* GlbReader::getAccessorData(const tg3_accessor& acc)
{
    if (acc.buffer_view < 0 || (uint32_t)acc.buffer_view >= model.buffer_views_count)
    {
        return nullptr;
    }
    const tg3_buffer_view& bv = model.buffer_views[acc.buffer_view];
    if (bv.buffer < 0 || (uint32_t)bv.buffer >= model.buffers_count)
    {
        return nullptr;
    }
    const tg3_buffer& buf = model.buffers[bv.buffer];
    return buf.data.data + bv.byte_offset + acc.byte_offset;
}

bool GlbReader::parseFile(const fs::path& path)
{
    tg3_parse_options opts;

    tg3_parse_options_init(&opts);
    tg3_error_stack_init(&errors);
    std::string pathStr = path.string();
    tg3_error_code err = tg3_parse_file(&model, &errors, pathStr.c_str(), pathStr.length(), &opts);
    if (err != TG3_OK)
    {
        for (uint32_t i = 0; i < errors.count; i++)
        {
            std::cerr   << "[tinygltf v3 Error] "
                        << (errors.entries[i].message ? errors.entries[i].message : "Unknown")
                        << std::endl;
        }
        tg3_error_stack_free(&errors);
        return false;
    }

    if (model.meshes_count == 0)
    {
        std::cerr << "No meshes in glb file!" << std::endl;
        tg3_model_free(&model);
        tg3_error_stack_free(&errors);
        return false;
    }
    return true;
}

glm::mat4 GlbReader::getNodeMatrix(const tg3_node& node)
{
    glm::mat4 nodeMatrix(1.0f);
    if (node.has_matrix)
    {
        nodeMatrix = glm::make_mat4(node.matrix);
    }
    else
    {
        auto safe = [&](double scale){return (scale != 0.0 ? scale : 1.0f);};
        glm::vec3 t(node.translation[0], node.translation[1], node.translation[2]);
        glm::quat r(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        glm::vec3 s(safe(node.scale[0]), safe(node.scale[1]), safe(node.scale[2]));
        nodeMatrix = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
    }
    return nodeMatrix;
}

void GlbReader::processNode(uint32_t nodeIdx, const glm::mat4& parentMatrix)
{
    if (nodeIdx >= model.nodes_count) return;
    const tg3_node& node = model.nodes[nodeIdx];
    glm::mat4 localMatrix = getNodeMatrix(node);
    glm::mat4 worldMatrix = parentMatrix * localMatrix;
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));
    for (size_t i = 0; i < node.children_count; i++)
    {
        uint32_t childIdx = static_cast<uint32_t>(node.children[i]);
        processNode(childIdx, worldMatrix);
    }
    if (node.mesh < 0 || (uint32_t)node.mesh >= model.meshes_count)
    {
        return;
    }
    const tg3_mesh& mesh = model.meshes[node.mesh];
    for (uint32_t p = 0; p < mesh.primitives_count; p++)
    {
        const tg3_primitive& prim = mesh.primitives[p];
        const tg3_accessor *posAcc = nullptr, *normAcc = nullptr, *uvAcc = nullptr;

        for (uint32_t a = 0; a < prim.attributes_count; a++)
        {
            const tg3_str_int_pair& attr = prim.attributes[a];
            if (!attr.key.data) continue;

            std::string_view name(attr.key.data, attr.key.len);
            if (name == "POSITION")         posAcc  = &model.accessors[attr.value];
            else if (name == "NORMAL")     normAcc = &model.accessors[attr.value];
            else if (name == "TEXCOORD_0") uvAcc   = &model.accessors[attr.value];
        }

        if (!posAcc) continue;

        uint32_t baseVertexIndex = static_cast<uint32_t>(outData.vertices.size());
        const float *posPtr = reinterpret_cast<const float*>(getAccessorData(*posAcc));
        const float *normPtr = normAcc ? reinterpret_cast<const float*>(getAccessorData(*normAcc)) : nullptr;
        const float *uvPtr = uvAcc ? reinterpret_cast<const float*>(getAccessorData(*uvAcc)) : nullptr;

        auto getStride = [&](uint32_t init, int32_t bufferView)->uint32_t {
            if (bufferView >= 0 && model.buffer_views[bufferView].byte_stride > 0)
                return model.buffer_views[bufferView].byte_stride / sizeof(float);
            return init;
        };
        uint32_t posStride = getStride(3, posAcc->buffer_view);
        uint32_t normStride = getStride((normAcc ? 3 : 0), normAcc->buffer_view);
        uint32_t uvStride = getStride((uvAcc ? 2 : 0), uvAcc->buffer_view);

        for (size_t i = 0; i < posAcc->count; i++)
        {
            VertexAttributes vert{};
            glm::vec4 localPos(posPtr[i * posStride + 0], posPtr[i * posStride + 1], posPtr[i * posStride + 2], 1.0f);
            vert.position = glm::vec3(worldMatrix * localPos);

            if (normPtr)
            {
                glm::vec3 localNorm(normPtr[i * normStride + 0], normPtr[i * normStride + 1], normPtr[i * normStride + 2]);
                vert.normal = glm::normalize(normalMatrix * localNorm);
            }
            else vert.normal = {0.0f, 1.0f, 0.0f};

            if (uvPtr)
                vert.uv = { uvPtr[i * uvStride + 0], uvPtr[i * uvStride + 1] };
            else
                vert.uv = {0.0f, 0.0f};

            outData.vertices.push_back(vert);
        }

        if (prim.indices < 0 || (uint32_t)prim.indices >= model.accessors_count)
        {
            continue;
        }
        const tg3_accessor& idxAcc = model.accessors[prim.indices];
        const uint8_t* data = getAccessorData(idxAcc);

        if (!data) continue;
        switch (idxAcc.component_type)
        {
            case 5121: appendIndicies<uint8_t >(data, idxAcc.count, baseVertexIndex, outData.indices); break;
            case 5123: appendIndicies<uint16_t>(data, idxAcc.count, baseVertexIndex, outData.indices); break;
            case 5125: appendIndicies<uint32_t>(data, idxAcc.count, baseVertexIndex, outData.indices); break;
        }
    }
}

bool GlbReader::loadGlbModel(const fs::path& path)
{
    Timer t("loadGlbModel");
    if (!parseFile(path))
    {
        return false;
    }

    if (model.scenes_count > 0)
    {
        std::cout << "Model scene defined!" << std::endl;
        uint32_t sceneIdx = (model.default_scene >= 0 && (uint32_t)model.default_scene < model.scenes_count) 
                            ? static_cast<uint32_t>(model.default_scene) : 0;
        
        const tg3_scene& scene = model.scenes[sceneIdx];
        for (size_t i = 0; i < scene.nodes_count; i++)
        {
            uint32_t rootNodeIdx = static_cast<uint32_t>(scene.nodes[i]);
            processNode(rootNodeIdx, glm::mat4(1.0f));
        }
    }
    else
    {
        for (uint32_t n = 0; n < model.nodes_count; n++)
        {
            processNode(n, glm::mat4(1.0f));
        }
    }

    if (model.images_count)
    {
        loadTexture();
    }
    std::cout << "[glb reader] Parsed file succesfully: " << path << std::endl;
    tg3_model_free(&model);
    tg3_error_stack_free(&errors);
    return true;
}

void GlbReader::loadTexture()
{
    const tg3_image& img = model.images[0];
    if (img.image.data && img.image.count > 0)
    {
        int w, h, comp, channels;
        unsigned char* decoded = stbi_load_from_memory(img.image.data, (int)img.image.count, &w, &h, &channels, 4);
        if (!decoded)
        {
            std::cerr << "[glb reader] cannot load texture from memory!" << std::endl;
            return;
        }
        outData.texWidth = w;
        outData.texHeight = h;
        outData.textureData.assign(decoded, decoded + (w * h * 4));
        stbi_image_free(decoded);
        std::cout << "[glb reader] texture loaded from memory" << std::endl;
    }
    else if (img.buffer_view >= 0 && (uint32_t)img.buffer_view < model.buffer_views_count)
    {
        const tg3_buffer_view& bv = model.buffer_views[img.buffer_view];
        const tg3_buffer& buf = model.buffers[bv.buffer];
        const uint8_t* rawImgData = buf.data.data + bv.byte_offset;
        int w, h, comp, channels;
        unsigned char* decoded = stbi_load_from_memory(rawImgData, (int)bv.byte_length, &w, &h, &channels, 4);
        if (!decoded)
        {
            std::cerr << "[glb reader] cannot load texture from buffer!" << std::endl;
            return;
        }
        outData.texWidth = w;
        outData.texHeight = h;
        outData.textureData.assign(decoded, decoded + (w * h * 4));
        stbi_image_free(decoded);
        std::cout << "[glb reader] texture loaded from buffer" << std::endl;
    }
}
