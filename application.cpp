#include "application.h"
#include "glm/ext/matrix_transform.hpp"
#include "misc.h"
#include "webgpu/webgpu_cpp.h"
#include <cstdint>
#include <emscripten/emscripten.h>

void Application::initializeBuffers()
{
    Timer t1("initializeBuffers");
    GlbReader reader;
    bool success = reader.loadGlbModel(RESOURCE_DIR "/plane.glb");
    assert(success);
    const GlbModelData& modelData = reader.getData();
    vertexBuffer = createBuffer(device, "vertex_buffer", modelData.vertices.size() * sizeof(VertexAttributes), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, modelData.vertices.data(), modelData.vertices.size() * sizeof(VertexAttributes));
    vertexCount = static_cast<int32_t>(modelData.vertices.size());

    indexBuffer = createBuffer(device, "index_buffer", modelData.indices.size() * sizeof(uint32_t), wgpu::BufferUsage::Index);
    device.GetQueue().WriteBuffer(indexBuffer, 0, modelData.indices.data(), modelData.indices.size() * sizeof(uint32_t));
    indexCount = static_cast<uint32_t>(modelData.indices.size());

    if (!modelData.textureData.empty())
    {
        planeTexture = createTextureFromBytes(device, modelData.textureData.data(), modelData.texWidth, modelData.texHeight);
    }

    GlbReader earthReader;
    bool earthOk = earthReader.loadGlbModel(RESOURCE_DIR "/earth.glb");
    assert(earthOk);
    const GlbModelData& earthData = earthReader.getData();

    earthVertexBuffer = createBuffer(device, "earth_vertex_buffer", earthData.vertices.size() * sizeof(VertexAttributes), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(earthVertexBuffer, 0, earthData.vertices.data(), earthData.vertices.size() * sizeof(VertexAttributes));
    earthIndexCount = static_cast<uint32_t>(earthData.indices.size());

    earthIndexBuffer = createBuffer(device, "earth_index_buffer", earthData.indices.size() * sizeof(uint32_t), wgpu::BufferUsage::Index);
    device.GetQueue().WriteBuffer(earthIndexBuffer, 0, earthData.indices.data(), earthData.indices.size() * sizeof(uint32_t));

    if (!earthData.textureData.empty())
    {
        earthTexture = createTextureFromBytes(device, earthData.textureData.data(), earthData.texWidth, earthData.texHeight);
    }

    glm::mat4 earthModelMatrix = glm::mat4(1.0f);
    earthModelMatrix = glm::scale(earthModelMatrix, glm::vec3(PLANET_RADIUS));
    earthModelMatrix = glm::translate(earthModelMatrix, glm::vec3(0, -1, 0));
    earthInstanceBuffer = createBuffer(device, "earth_instance_buffer", sizeof(glm::mat4), wgpu::BufferUsage::Storage);
    device.GetQueue().WriteBuffer(earthInstanceBuffer, 0, &earthModelMatrix, sizeof(glm::mat4));

    instanceBuffer = createBuffer(device, "instance", MAX_PLANES * sizeof(mat4), wgpu::BufferUsage::Storage);
    uniformBuffer = createBuffer(device, "uniform buffer", sizeof(MyUniforms), wgpu::BufferUsage::Uniform);

    wgpu::Limits limits;
    device.GetLimits(&limits);

    mat4x4 V = glm::lookAt(cameraPos, CENTER_POINT, CAMERA_UP);
    mat4x4 P = glm::perspective(FOV, 1.0f, 0.01f, 1000.0f);
    projectionMatrix = P;
    MyUniforms uniformValues {
        .projectionMatrix = P,
        .viewMatrix = V,
    };

    device.GetQueue().WriteBuffer(uniformBuffer, 0, &uniformValues, sizeof(MyUniforms));
}

void Application::initializePipeline()
{
    Timer t2("initializePipeline");
    assert(surfaceFormat != wgpu::TextureFormat::Undefined);
    assert(vertexBuffer);
    auto shader = loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
    assert(shader);
    wgpu::ColorTargetState target{.format = surfaceFormat,};

    DynamicVertexLayout vertexLayout({3, 3, 2});
    depthManager = std::make_unique<DepthManager>(DEPTH_TEXTURE_FORMAT, device, WIN_WIDTH, WIN_HEIGHT);

    wgpu::FragmentState fragState {
        .module = shader,
        .entryPoint = "fs",
        .constants = nullptr,
        .targetCount = 1,
        .targets = &target,
    };

    mainBindGroup.addBuffer(0, uniformBuffer, sizeof(MyUniforms), wgpu::BufferBindingType::Uniform);
    mainBindGroup.addBuffer(1, instanceBuffer, MAX_PLANES * sizeof(mat4), wgpu::BufferBindingType::ReadOnlyStorage, wgpu::ShaderStage::Vertex);
    mainBindGroup.addTexture(2, planeTexture.view);
    mainBindGroup.addSampler(3, planeTexture.sampler);
    mainBindGroup.build(device);

    earthBindGroup.addBuffer(0, uniformBuffer, sizeof(MyUniforms), wgpu::BufferBindingType::Uniform);
    earthBindGroup.addBuffer(1, earthInstanceBuffer, sizeof(mat4), wgpu::BufferBindingType::ReadOnlyStorage, wgpu::ShaderStage::Vertex);
    earthBindGroup.addTexture(2, earthTexture.view);
    earthBindGroup.addSampler(3, earthTexture.sampler);
    earthBindGroup.build(device);

    wgpu::BindGroupLayout bindGroupLayout = mainBindGroup.getLayout();
    wgpu::PipelineLayoutDescriptor layoutDesc {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (wgpu::BindGroupLayout*)&bindGroupLayout,
    };

    auto layout = device.CreatePipelineLayout(&layoutDesc);
    wgpu::RenderPipelineDescriptor pipelineDesc {
        .label = "Main render pipeline",
        .layout = layout,
        .vertex = {
            .module = shader,
            .entryPoint = "vs",
            .constants = nullptr,
            .bufferCount = 1,
            .buffers = vertexLayout.getLayout(),
        },
        .depthStencil = depthManager->getDepthStencilState(),
        .fragment = &fragState,
    };
    pipeline = device.CreateRenderPipeline(&pipelineDesc);
}

bool Application::initializeGLFW()
{
    Timer t3("initializeGLFW");
    glfwSetErrorCallback(glfwError);
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW.";
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Triangle", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Unable to create GLFW Window";
        return false;
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(window, this);
    return true;
}

static EM_BOOL onWindowResize(int eventType, const EmscriptenUiEvent* uiEvent, void* userData)
{
    auto app = static_cast<Application*>(userData);
    if (!app) return EM_FALSE;

    double cssWidth, cssHeight;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);
    double dpr = emscripten_get_device_pixel_ratio();

    uint32_t width = static_cast<uint32_t>(cssWidth * dpr);
    uint32_t height = static_cast<uint32_t>(cssHeight * dpr);

    emscripten_set_canvas_element_size("#canvas", width, height);
    app->onResize(width, height);
    return EM_TRUE;
}

bool Application::initialize()
{
    Timer t4("initialize");
    if (!initializeGLFW())
        return false;

    instance = wgpu::CreateInstance();
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc{};
    canvasDesc.selector = "#canvas";
    wgpu::SurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = &canvasDesc;
    surface = instance.CreateSurface(&surfaceDesc);

    wgpu::RequestAdapterOptions adapterOpts {
        .powerPreference = wgpu::PowerPreference::HighPerformance,
        .compatibleSurface = surface,
    };

    instance.RequestAdapter(&adapterOpts, wgpu::CallbackMode::AllowSpontaneous,
        [this](auto status, auto adapt, wgpu::StringView msg) {
            this->onAdapterReady(status, adapt, msg);
        }
    );
    return true;
}

void Application::onAdapterReady(wgpu::RequestAdapterStatus status, wgpu::Adapter adapt, wgpu::StringView message)
{
    Timer t5("onAdapterReady");
    if (status != wgpu::RequestAdapterStatus::Success)
    {
        std::cerr << "Adapter request failed: " << message.data << "\n";
        return;
    }
    this->adapter = adapt;
    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.label = "Primary device";
    deviceDesc.SetDeviceLostCallback(wgpu::CallbackMode::AllowProcessEvents, deviceLost);
    deviceDesc.SetUncapturedErrorCallback(uncapturedError);
    this->adapter.RequestDevice(&deviceDesc, wgpu::CallbackMode::AllowSpontaneous,
        [this](auto status, auto dev, wgpu::StringView msg) {
            this->onDeviceReady(status, dev, msg);
        }
    );
}

void Application::onDeviceReady(wgpu::RequestDeviceStatus status, wgpu::Device dev, wgpu::StringView message)
{
    Timer t6("onDeviceReady");
    if (status != wgpu::RequestDeviceStatus::Success)
    {
        std::cerr << "Device request failed: " << message.data << "\n";
        return;
    }
    this->device = dev;
    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);
    surfaceFormat = capabilities.formats[0];

    fetchPlanesOnDemand();
    initializeBuffers();
    initializePipeline();
    fetchCooldown = LONG_FETCH_COOLDOWN;
    onWindowResize(0, nullptr, this);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, onWindowResize);

    emscripten_set_main_loop_arg([](void* arg) {
        static_cast<Application*>(arg)->renderFrame();
    }, this, 0, false);
}

void Application::renderFrame()
{
    assert(device && pipeline);
    glfwPollEvents();

    double currentTime = emscripten_get_now();
    if (lastFrameTime == 0.0) lastFrameTime = currentTime;

    float dt = static_cast<float>((currentTime - lastFrameTime) / 1000.0);
    lastFrameTime = currentTime;
    if (dt > 0.1f) dt = 0.1f;
    fetchCooldown -= dt;

    processInput(dt);

    if (fetchCooldown < 0.0) fetchPlanesOnDemand();

    mat4x4 V = glm::lookAt(cameraPos, CENTER_POINT, CAMERA_UP);
    MyUniforms uniformValues {
        .projectionMatrix = projectionMatrix,
        .viewMatrix = V,
    };
    device.GetQueue().WriteBuffer(uniformBuffer, 0, &uniformValues, sizeof(MyUniforms));
    for (int i = 0; i < planesCount; i++)
    {
        planes[i].fly(dt);
        instanceData[i] = planes[i].getModelMatrix();
    }
 
    device.GetQueue().WriteBuffer(
        instanceBuffer, 0, 
        &instanceData, 
        planesCount * sizeof(glm::mat4)
    );

    auto encoder = device.CreateCommandEncoder();
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);

    auto backbufferView = surfaceTexture.texture.CreateView();

    wgpu::RenderPassColorAttachment attachment {
        .view = backbufferView,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = {0.2, 0.2, 0.2, 1.},
    };

    wgpu::RenderPassDescriptor renderPass {
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment,
        .depthStencilAttachment = depthManager->getDepthAttachment(),
    };

    auto pass = encoder.BeginRenderPass(&renderPass);
    pass.SetPipeline(pipeline);

    pass.SetVertexBuffer(0, earthVertexBuffer, 0, earthVertexBuffer.GetSize());
    pass.SetIndexBuffer(earthIndexBuffer, wgpu::IndexFormat::Uint32, 0, earthIndexBuffer.GetSize());
    earthBindGroup.bind(pass, 0);
    pass.DrawIndexed(earthIndexCount, 1, 0, 0, 0);

    pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
    pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32, 0, indexBuffer.GetSize());
    mainBindGroup.bind(pass, 0);
    pass.DrawIndexed(indexCount, static_cast<uint32_t>(planesCount), 0, 0);
    pass.End();

    auto commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}

void Application::fetchPlanesOnDemand()
{
    Timer t("fetchPlanes");
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.userData = this;
    attr.onsuccess = [](emscripten_fetch_t* fetch) {
        auto app = static_cast<Application*>(fetch->userData);
        const float* planeData = reinterpret_cast<const float*>(fetch->data);
        app->updatePlanes(planeData, fetch->numBytes);
        emscripten_fetch_close(fetch);
    };
    attr.onerror = [](emscripten_fetch_t* fetch) {
        std::cerr << "[Fetch Error] Data cannot be imported from API HTTP (status: " << fetch->status << ")\n";
        emscripten_fetch_close(fetch);
    };
    emscripten_fetch(&attr, "http://127.0.0.1:8080/api/planes");
}


void Application::updatePlanes(const float* data, int sizeInBytes)
{
    uint32_t newBatchId = reinterpret_cast<const uint32_t*>(data)[0];
    if (newBatchId == lastBatchId)
    {
        std::cout << "[Fetch] No change detected. Retry in 3 seconds..." << std::endl;
        fetchCooldown = SHORT_FETCH_COOLDOWN;
        return;
    }

    lastBatchId = newBatchId;
    fetchCooldown = LONG_FETCH_COOLDOWN;
    planesCount = 0;
    for (int i = 0; i < ((sizeInBytes - HEADER_OFFSET) / (4 * sizeof(float))); i++)
    {
        Airplane plane{};
        plane.setFlightData(
            {data[i * 4 + 1], data[i * 4 + 2]},
            data[i * 4 + 3],
            data[i * 4 + 4]);
        planes[i] = plane;
        planesCount++;
    }
    std::cout << "[Fetch] New data loaded. New fetch in 2 minutes..." << std::endl;
}

void Application::onResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0 || !device)
    {
        std::cout << "Resize failed!" << std::endl;
        return;
    }

    wgpu::SurfaceConfiguration config {
        .device = device,
        .format = surfaceFormat,
        .width = width,
        .height = height,
        .presentMode = wgpu::PresentMode::Fifo,
    };
    surface.Configure(&config);
    depthManager = std::make_unique<DepthManager>(DEPTH_TEXTURE_FORMAT, device, width, height);
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    projectionMatrix = glm::perspective(FOV, aspect, 0.01f, 1000.0f);
}

void Application::processInput(float dt)
{
    float speed = CAMERA_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        yaw += speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        yaw -= speed;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        pitch += speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        pitch -= speed;
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
        cameraDistance -= speed * 100.0f;
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        cameraDistance += speed * 100.0f;
    pitch = glm::clamp(pitch, -MAX_PITCH, MAX_PITCH);
    updateCameraPosition();
}

void Application::updateCameraPosition()
{
    float x = cameraDistance * cos(pitch) * cos(yaw);
    float y = cameraDistance * sin(pitch);
    float z = cameraDistance * cos(pitch) * sin(yaw);
    cameraPos = vec3(x, y, z);
}