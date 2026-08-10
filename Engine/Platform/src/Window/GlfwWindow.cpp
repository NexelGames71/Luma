#include "GlfwWindow.h"

// Pull in Vulkan types via GLFW so surface creation is available. This keeps the
// only Vulkan dependency of Luma::Platform confined to this translation unit.
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Luma/Core/Assert.h"
#include "Luma/Core/Events.h"
#include "Luma/Core/Log.h"

namespace Luma {
namespace {

// Refcount so multiple windows share one glfwInit/glfwTerminate.
int g_glfwWindowCount = 0;

void GlfwErrorCallback(int code, const char* description) {
    LUMA_LOG_ERROR("GLFW", "error {}: {}", code, description ? description : "");
}

GlfwWindow::WindowData* DataFrom(GLFWwindow* window) {
    return static_cast<GlfwWindow::WindowData*>(glfwGetWindowUserPointer(window));
}

}  // namespace

std::unique_ptr<Window> Window::Create(const WindowProps& props) {
    return std::make_unique<GlfwWindow>(props);
}

GlfwWindow::GlfwWindow(const WindowProps& props) {
    m_data.title = props.title;
    m_data.width = props.width;
    m_data.height = props.height;

    if (g_glfwWindowCount == 0) {
        glfwSetErrorCallback(&GlfwErrorCallback);
        if (!glfwInit()) {
            LUMA_ASSERT(false, "glfwInit failed");
            return;
        }
    }

    // No graphics API yet — Vulkan attaches to this surface in Milestone 2.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(static_cast<int>(props.width),
                                static_cast<int>(props.height),
                                m_data.title.c_str(), nullptr, nullptr);
    if (!m_window) {
        LUMA_ASSERT(false, "glfwCreateWindow failed");
        if (g_glfwWindowCount == 0) glfwTerminate();
        return;
    }
    ++g_glfwWindowCount;

    glfwSetWindowUserPointer(m_window, &m_data);
    InstallCallbacks();

    LUMA_LOG_INFO("Window", "created '{}' ({}x{})", m_data.title, m_data.width,
                  m_data.height);
}

GlfwWindow::~GlfwWindow() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        --g_glfwWindowCount;
        if (g_glfwWindowCount == 0) glfwTerminate();
    }
}

void GlfwWindow::InstallCallbacks() {
    glfwSetWindowCloseCallback(m_window, [](GLFWwindow* w) {
        auto* data = DataFrom(w);
        if (!data || !data->callback) return;
        WindowCloseEvent e;
        data->callback(e);
    });

    glfwSetWindowSizeCallback(m_window, [](GLFWwindow* w, int width, int height) {
        auto* data = DataFrom(w);
        if (!data) return;
        data->width = static_cast<u32>(width);
        data->height = static_cast<u32>(height);
        if (!data->callback) return;
        WindowResizeEvent e(data->width, data->height);
        data->callback(e);
    });

    glfwSetWindowFocusCallback(m_window, [](GLFWwindow* w, int focused) {
        auto* data = DataFrom(w);
        if (!data || !data->callback) return;
        if (focused) {
            WindowFocusEvent e;
            data->callback(e);
        } else {
            WindowLostFocusEvent e;
            data->callback(e);
        }
    });

    glfwSetKeyCallback(m_window, [](GLFWwindow* w, int key, int /*scancode*/,
                                    int action, int /*mods*/) {
        auto* data = DataFrom(w);
        if (!data || !data->callback) return;
        switch (action) {
            case GLFW_PRESS: {
                KeyPressedEvent e(key, false);
                data->callback(e);
                break;
            }
            case GLFW_REPEAT: {
                KeyPressedEvent e(key, true);
                data->callback(e);
                break;
            }
            case GLFW_RELEASE: {
                KeyReleasedEvent e(key);
                data->callback(e);
                break;
            }
            default:
                break;
        }
    });

    glfwSetCharCallback(m_window, [](GLFWwindow* w, unsigned int codepoint) {
        auto* data = DataFrom(w);
        if (!data || !data->callback) return;
        KeyTypedEvent e(codepoint);
        data->callback(e);
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* w, int button,
                                            int action, int /*mods*/) {
        auto* data = DataFrom(w);
        if (!data || !data->callback) return;
        if (action == GLFW_PRESS) {
            MouseButtonPressedEvent e(button);
            data->callback(e);
        } else if (action == GLFW_RELEASE) {
            MouseButtonReleasedEvent e(button);
            data->callback(e);
        }
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* w, double x, double y) {
        auto* data = DataFrom(w);
        if (!data || !data->callback) return;
        MouseMovedEvent e(static_cast<f32>(x), static_cast<f32>(y));
        data->callback(e);
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* w, double xOff, double yOff) {
        auto* data = DataFrom(w);
        if (!data || !data->callback) return;
        MouseScrolledEvent e(static_cast<f32>(xOff), static_cast<f32>(yOff));
        data->callback(e);
    });
}

void GlfwWindow::PollEvents() { glfwPollEvents(); }

bool GlfwWindow::ShouldClose() const {
    return m_window ? glfwWindowShouldClose(m_window) != 0 : true;
}

void* GlfwWindow::NativeHandle() const { return m_window; }

std::vector<const char*> GlfwWindow::RequiredVulkanInstanceExtensions() const {
    u32 count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (!extensions) return {};
    return std::vector<const char*>(extensions, extensions + count);
}

void* GlfwWindow::CreateVulkanSurface(void* instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(
        reinterpret_cast<VkInstance>(instance), m_window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        LUMA_LOG_ERROR("Window", "glfwCreateWindowSurface failed ({})",
                       static_cast<int>(result));
        return nullptr;
    }
    return reinterpret_cast<void*>(surface);
}

}  // namespace Luma
