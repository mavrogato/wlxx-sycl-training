
#include <iostream>
#include <iterator>
#include <algorithm>
#include <thread>
#include <chrono>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <CL/sycl.hpp>
#include "experimental_generator.hpp"

template <class T, class D>
auto attach_unique(T* ptr, D deleter) noexcept {
    assert(ptr);
    return std::unique_ptr<T, D>(ptr, deleter);
}
template <class WL_TYPE>
auto attach_unique(WL_TYPE* ptr) noexcept {
    assert(ptr);
    constexpr static auto deleter = [](WL_TYPE* ptr) noexcept {
        std::cout << typeid (ptr).name() << ':' << ptr << " deleted as proxy" << std::endl;
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(ptr));
    };
    return std::unique_ptr<WL_TYPE, decltype (deleter)>(ptr, deleter);
}
inline auto attach_unique(wl_display* ptr) noexcept {
    assert(ptr);
    constexpr static auto deleter = wl_display_disconnect;
    return std::unique_ptr<wl_display, decltype (deleter)>(ptr, deleter);
}

int main() {
    // cl_uint num_platforms;
    // clGetPlatformIDs(0, nullptr, &num_platforms);
    // std::cout << num_platforms << std::endl;
    // auto ret = sycl::gpu_selector();
    // auto dev = ret.select_device();
    // std::cout << typeid (dev).name() << std::endl;
    // std::cout << dev.is_gpu() << std::endl;
    // std::cout << dev.is_cpu() << std::endl;
    // return 0;

    try {
        auto display = attach_unique(wl_display_connect(nullptr));
        auto registry = attach_unique(wl_display_get_registry(display.get()));

        static void* compositor_raw = nullptr;
        static void* shell_raw = nullptr;
        static void* seat_raw = nullptr;
        {
            wl_registry_listener listener = {
                .global = [](void*,
                             wl_registry* registry_raw,
                             uint32_t id,
                             char const* interface,
                             uint32_t version) {
                    if (0 == std::strcmp(interface, wl_compositor_interface.name)) {
                        compositor_raw = wl_registry_bind(registry_raw,
                                                          id,
                                                          &wl_compositor_interface,
                                                          version);
                        std::cout << "compositor version: " << version << std::endl;
                    }
                    if (0 == std::strcmp(interface, wl_shell_interface.name)) {
                        shell_raw = wl_registry_bind(registry_raw,
                                                     id,
                                                     &wl_shell_interface,
                                                     version);
                        std::cout << "shell version: " << version << std::endl;
                    }
                    if (0 == std::strcmp(interface, wl_seat_interface.name)) {
                        seat_raw = wl_registry_bind(registry_raw,
                                                    id,
                                                    &wl_seat_interface,
                                                    version);//std::min(7u, version));
                        std::cout << "seat version: " << version << std::endl;
                    }
                },
            };
            auto r = wl_registry_add_listener(registry.get(), &listener, nullptr);
            assert(0 == r);
        }
        wl_display_roundtrip(display.get());
        auto compositor = attach_unique((wl_compositor*) compositor_raw);
        auto shell = attach_unique((wl_shell*) shell_raw);
        auto seat = attach_unique((wl_seat*) seat_raw);

        auto surface = attach_unique(wl_compositor_create_surface(compositor.get()));
        auto shell_surface = attach_unique(wl_shell_get_shell_surface(shell.get(),
                                                                      surface.get()));
        auto egl_display = attach_unique(eglGetDisplay(display.get()), eglTerminate);
        auto eglInitialized = eglInitialize(egl_display.get(), nullptr, nullptr);
        assert(eglInitialized);

        static float resolution_coords[2] = { 800, 600 };
        auto& cx = resolution_coords[0];
        auto& cy = resolution_coords[1];
        auto egl_window = attach_unique(wl_egl_window_create(surface.get(), cx, cy),
                                        wl_egl_window_destroy);
        {
            wl_shell_surface_listener listener = {
                .ping = [](void*,
                           wl_shell_surface* shell_surface_raw,
                           uint32_t serial) noexcept
                {
                    wl_shell_surface_pong(shell_surface_raw, serial);
                },
                .configure = [](void* egl_window_raw,
                                wl_shell_surface* shell_surface_raw,
                                uint32_t edges,
                                int32_t width,
                                int32_t height) noexcept
                {
                    wl_egl_window_resize(reinterpret_cast<wl_egl_window*>(egl_window_raw),
                                         width,
                                         height,
                                         0, 0);
                    glViewport(0, 0, cx = width, cy = height);
                },
                .popup_done = [](auto...) noexcept {
                    std::cout << "popup done." << std::endl;
                },
            };
            auto r = wl_shell_surface_add_listener(shell_surface.get(), &listener, egl_window.get());
            assert(r == 0);
        }
        wl_shell_surface_set_toplevel(shell_surface.get());
        {
            wl_seat_listener listener = {
                .capabilities = [](void*, wl_seat* seat_raw, uint32_t caps) noexcept {
                    if (caps & WL_SEAT_CAPABILITY_POINTER) {
                        std::cout << "pointer device found." << std::endl;
                    }
                    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
                        std::cout << "keyboard device found." << std::endl;
                    }
                    if (caps & WL_SEAT_CAPABILITY_TOUCH) {
                        std::cout << "touch device found." << std::endl;
                    }
                    std::cout << "seat capability: " << caps << std::endl;
                },
                .name = [](void*, wl_seat* seat_raw, char const* name) noexcept {
                    std::cout << name << std::endl;
                },
            };
            auto r = wl_seat_add_listener(seat.get(), &listener, nullptr);
            assert(0 == r);
        }
        static uint32_t scancode = 0;
        auto keyboard = attach_unique(wl_seat_get_keyboard(seat.get()));
        {
            wl_keyboard_listener listener {
                .keymap = [](auto...) { },
                .enter = [](auto...) { },
                .leave = [](auto...) { },
                .key = [](void *data,
                          wl_keyboard* keyboard_raw,
                          uint32_t serial,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state)
                {
                    std::cout << (scancode = key) << std::endl;
                },
                .modifiers = [](auto...) { },
                .repeat_info = [](auto...) { },
            };
            wl_keyboard_add_listener(keyboard.get(), &listener, nullptr);
        }

        static float pointer_coords[2] = { -256, -256 };
        auto& px = pointer_coords[0];
        auto& py = pointer_coords[1];
        auto pointer = attach_unique(wl_seat_get_pointer(seat.get()));
        {
            wl_pointer_listener listener {
                .enter = [](void* data,
                            wl_pointer* pointer_raw,
                            uint32_t serial,
                            wl_surface* surface_raw,
                            wl_fixed_t sx,
                            wl_fixed_t sy)
                {
                    std::cout << "pointer entered: "
                              << wl_fixed_to_int(sx) << ','
                              << wl_fixed_to_int(sy) << std::endl;
                },
                .leave = [](void* data,
                            wl_pointer* pointer_raw,
                            uint32_t serial,
                            wl_surface* surface_raw)
                {
                    std::cout << "pointer left." << std::endl;
                },
                .motion = [](void* data,
                             wl_pointer* pointer_raw,
                             uint32_t time,
                             wl_fixed_t sx,
                             wl_fixed_t sy)
                {
                    std::cout << "pointer moved: "
                              << wl_fixed_to_int(sx) << ','
                              << wl_fixed_to_int(sy) << std::endl;
                    px = wl_fixed_to_int(sx);
                    py = cy - wl_fixed_to_int(sy) - 1;
                },
                .button = [](void* data,
                             wl_pointer* pointer_raw,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t button,
                             uint32_t state)
                {
                    std::cout << "pointer button: " << button << ';' << state << std::endl;
                },
                .axis = [](void* data,
                           wl_pointer* pointer_raw,
                           uint32_t time,
                           uint32_t axis,
                           wl_fixed_t value)
                {
                    std::cout << "pointer axis: " << axis << ';' << value << std::endl;
                },
                .frame = [](auto...) { },
            };
            auto r = wl_pointer_add_listener(pointer.get(), &listener, nullptr);
            assert(0 == r);
        }

        auto eglBound = eglBindAPI(EGL_OPENGL_ES_API);
        //auto eglBound = eglBindAPI(EGL_OPENGL_API);
        assert(eglBound);
        EGLint attributes[] = {
            EGL_LEVEL, 0,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE,   8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE,  8,
            EGL_ALPHA_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            //EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_NONE,
        };
        EGLConfig config;
        EGLint num_config;
        auto eglConfig = eglChooseConfig(egl_display.get(), attributes, &config, 1, &num_config);
        assert(eglConfig);
        EGLint contextAttributes[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE,
        };
        auto egl_context = attach_unique(eglCreateContext(egl_display.get(),
                                                          config,
                                                          nullptr,
                                                          contextAttributes),
                                         [&egl_display](auto ptr) {
                                             eglDestroyContext(egl_display.get(),
                                                               ptr);
                                         });
        assert(egl_context);
        auto egl_surface = attach_unique(eglCreateWindowSurface(egl_display.get(),
                                                                config,
                                                                egl_window.get(),
                                                                nullptr),
                                         [&egl_display](auto ptr) {
                                             eglDestroySurface(egl_display.get(),
                                                               ptr);
                                         });
        assert(egl_surface);
        auto eglMadeCurrent = eglMakeCurrent(egl_display.get(),
                                             egl_surface.get(),
                                             egl_surface.get(),
                                             egl_context.get());
        assert(eglMadeCurrent);

        auto vid = glCreateShader(GL_VERTEX_SHADER);
        auto fid = glCreateShader(GL_FRAGMENT_SHADER);

#define CODE(x) (#x)
        auto vcd = CODE(
            attribute vec4 position;
            varying vec2 vert;

            void main(void) {
                vert = position.xy;
                gl_Position = position;
            }
        );
        auto fcd = CODE(
            precision mediump float;
            varying vec2 vert;
            uniform vec2 resolution;
            uniform vec2 pointer;

            void main(void) {
                float brightness = length(gl_FragCoord.xy - resolution / 2.0) / length(resolution);
                brightness = 1.0 - brightness;
                gl_FragColor = vec4(0.0, 0.0, brightness, brightness);
                float radius = length(pointer - gl_FragCoord.xy);
                float touchMark = smoothstep(16.0, 40.0, radius);
                gl_FragColor *= touchMark;
            }
        );
#undef CODE
        auto compile = [](auto id, auto code) {
            glShaderSource(id, 1, &code, nullptr);
            glCompileShader(id);
            GLint result;
            glGetShaderiv(id, GL_COMPILE_STATUS, &result);
            GLint infoLogLength = 0;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
            if (infoLogLength) {
                std::vector<char> buf(infoLogLength);
                glGetShaderInfoLog(id, infoLogLength, nullptr, &buf.front());
                std::cerr << "<<<" << std::endl;
                std::cerr << code << std::endl;
                std::cerr << "---" << std::endl;
                std::cerr << std::string(buf.begin(), buf.end()).c_str() << std::endl;
                std::cerr << ">>>" << std::endl;
            }
            return result;
        };

        auto retVertCompilation = compile(vid, vcd);
        assert(retVertCompilation);
        auto retFragCompilation = compile(fid, fcd);
        assert(retFragCompilation);

        auto program = glCreateProgram();
        assert(program);

        glAttachShader(program, vid);
        glAttachShader(program, fid);

        glDeleteShader(vid);
        glDeleteShader(fid);

        glBindAttribLocation(program, 0, "position");
        glLinkProgram(program);
        {
            GLint linked;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            assert(linked);
            GLint length;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            assert(0 == length);
        }
        glUseProgram(program);
        glFrontFace(GL_CW);

        // using namespace std::literals::chrono_literals;
        // std::this_thread::sleep_for(100ms);
        // wl_display_roundtrip(display.get());

        for (;;) {
            auto ret = wl_display_dispatch_pending(display.get());
            if (-1 == ret) break;
            // auto ret = wl_display_iterate(display.get(), 0);
            // auto ret = wl_display_dispatch_pending(display.get());
            // auto ret = wl_display_dispatch_pending(nullptr);
            // auto ret = wl_display_prepare_read(nullptr);
            if (scancode == 1) {
                std::cout << "Bye" << std::endl;
                break;
            }
            glClearColor(0.0, 0.7, 0.0, 0.7);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(program);
            float vertices_coords[] = {
                -1, +1, 0,
                +1, +1, 0,
                +1, -1, 0,
                -1, -1, 0,
            };
            glUniform2fv(glGetUniformLocation(program, "resolution"), 1, resolution_coords);
            glUniform2fv(glGetUniformLocation(program, "pointer"), 1, pointer_coords);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices_coords);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            eglSwapBuffers(egl_display.get(), egl_surface.get());
        }

        glDeleteProgram(program);

        return 0;
    }
    catch (std::exception& ex) {
        std::cerr << "exception: " << ex.what() <<  std::endl;
    }
    return -1;
}
