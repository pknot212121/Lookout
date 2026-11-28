#pragma once
#include <glm/glm.hpp>
#include "webgpu/webgpu_cpp.h"
#include <cstdint>

constexpr float PI = 3.14159265358979323846f;
constexpr uint32_t WIN_WIDTH = 512;
constexpr uint32_t WIN_HEIGHT = 512;
constexpr wgpu::TextureFormat DEPTH_TEXTURE_FORMAT = wgpu::TextureFormat::Depth24Plus;

constexpr float CAMERA_SPEED = 1.0f;
constexpr glm::vec3 CAMERA_UP {0.0f, 1.0f, 0.0f};
constexpr glm::vec3 CENTER_POINT {0.0f, 0.0f, 0.0f};
constexpr float FOV = 1.047198f;
constexpr uint32_t MAX_PLANES = 30000;
constexpr float LONG_FETCH_COOLDOWN = 122.0f;
constexpr float SHORT_FETCH_COOLDOWN = 3.0f;
constexpr uint32_t HEADER_OFFSET = 4;
constexpr float PLANET_RADIUS = 90.0f;
constexpr float ROTATION_SPEED = PI / 2.0f;
constexpr float MAX_PITCH = PI / 2.0f - 0.01f;
constexpr float ORBIT_RADIUS = 100.0f;
constexpr float REAL_EARTH_RADIUS_METERS = 6'371'000.0f;
constexpr glm::vec3 WORLD_NORTH {0.0f, 1.0f, 0.0f};

constexpr uint32_t RESOLUTION = 33;
constexpr uint32_t MAX_TILES = 256;
constexpr uint32_t TEX_SIZE = 256;
constexpr uint32_t TEST_ZOOM = 5;
constexpr uint32_t TILES_PER_SIDE = 1u << TEST_ZOOM;