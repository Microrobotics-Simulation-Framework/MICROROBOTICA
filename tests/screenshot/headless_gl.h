#pragma once

// A headless OpenGL context for the screenshot tests: EGL on Mesa's surfaceless platform (no window, no display),
// OpenGL 4.5 in the Compatibility profile as the viewport asks for, and a 4x multisample framebuffer that resolves into
// a single-sample one for reading back, as the viewport's window is multisampled.
//
// By default it asks Mesa for its software rasteriser (LIBGL_ALWAYS_SOFTWARE=1, llvmpipe) at a fixed vector width, so
// the pictures are the same on a laptop and in CI; MICROROBOTICA_SCREENSHOT_GPU=1 leaves Mesa to choose a GPU
// (the Intel one through /dev/dri). It never asks for, or can reach, an NVIDIA driver: Mesa has none.

#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace microbotica::testing {

class HeadlessGl {
public:
    HeadlessGl(int width, int height, int samples = 4) : width_(width), height_(height) {
        const char* gpu = std::getenv("MICROROBOTICA_SCREENSHOT_GPU");
        if (!(gpu && std::string(gpu) == "1")) {
            setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
            setenv("LP_NATIVE_VECTOR_WIDTH", "256", 0);  // llvmpipe's width: the same pixels on every x86 CPU
        }
        auto getPlatformDisplay =
            reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (!getPlatformDisplay) {
            why_ = "no eglGetPlatformDisplayEXT (not Mesa's EGL)";
            return;
        }
        display_ = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        EGLint major = 0, minor = 0;
        if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, &major, &minor)) {
            why_ = "eglInitialize failed on the surfaceless platform";
            return;
        }
        eglBindAPI(EGL_OPENGL_API);
        const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
                                     EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
                                     EGL_NONE};
        context_ = eglCreateContext(display_, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
        if (context_ == EGL_NO_CONTEXT || !eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_)) {
            why_ = "no OpenGL 4.5 Compatibility context";
            return;
        }
        auto text = [](GLenum name) {
            const auto* s = reinterpret_cast<const char*>(glGetString(name));
            return std::string(s ? s : "");
        };
        vendor_ = text(GL_VENDOR);
        renderer_ = text(GL_RENDERER);
        version_ = text(GL_VERSION);
        glGetIntegerv(GL_MAJOR_VERSION, &glMajor_);
        glGetIntegerv(GL_MINOR_VERSION, &glMinor_);

        // The multisampled framebuffer Storm presents into, as the viewport's 4x window.
        glGenFramebuffers(1, &msFbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
        glGenRenderbuffers(2, msBuffers_);
        glBindRenderbuffer(GL_RENDERBUFFER, msBuffers_[0]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width_, height_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msBuffers_[0]);
        glBindRenderbuffer(GL_RENDERBUFFER, msBuffers_[1]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width_, height_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msBuffers_[1]);
        const bool msComplete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

        // The single-sample framebuffer it resolves into, for glReadPixels.
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glGenRenderbuffers(1, &color_);
        glBindRenderbuffer(GL_RENDERBUFFER, color_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width_, height_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_);
        const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (!msComplete || !complete) {
            why_ = "framebuffer incomplete";
            return;
        }
        ok_ = true;
    }

    ~HeadlessGl() {
        if (display_ != EGL_NO_DISPLAY) {
            eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context_ != EGL_NO_CONTEXT) {
                eglDestroyContext(display_, context_);
            }
            eglTerminate(display_);
        }
    }

    HeadlessGl(const HeadlessGl&) = delete;
    HeadlessGl& operator=(const HeadlessGl&) = delete;

    bool ok() const { return ok_; }
    const std::string& why() const { return why_; }
    const std::string& vendor() const { return vendor_; }
    const std::string& renderer() const { return renderer_; }
    const std::string& version() const { return version_; }
    int glMajor() const { return glMajor_; }
    int glMinor() const { return glMinor_; }
    int width() const { return width_; }
    int height() const { return height_; }

    /// The framebuffer to draw into (multisampled), cleared to the viewport's background.
    std::uint32_t beginFrame() {
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
        glViewport(0, 0, width_, height_);
        glClearColor(0.18f, 0.18f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        return msFbo_;
    }

    /// Resolve the frame and read it back: RGBA8, rows from the top.
    std::vector<std::uint8_t> endFrame() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_);
        glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
        std::vector<std::uint8_t> bottomUp(static_cast<std::size_t>(width_) * height_ * 4);
        glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());
        glFinish();
        std::vector<std::uint8_t> topDown(bottomUp.size());
        const std::size_t row = static_cast<std::size_t>(width_) * 4;
        for (int y = 0; y < height_; ++y) {
            std::copy_n(&bottomUp[static_cast<std::size_t>(height_ - 1 - y) * row], row,
                        &topDown[static_cast<std::size_t>(y) * row]);
        }
        return topDown;
    }

    /// Every GL error raised since the last call (0 when none).
    static GLenum drainErrors() {
        GLenum first = GL_NO_ERROR;
        for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError()) {
            if (first == GL_NO_ERROR) {
                first = e;
            }
        }
        return first;
    }

private:
    int width_ = 0;
    int height_ = 0;
    bool ok_ = false;
    std::string why_;
    std::string vendor_, renderer_, version_;
    GLint glMajor_ = 0, glMinor_ = 0;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    GLuint msFbo_ = 0, fbo_ = 0, color_ = 0;
    GLuint msBuffers_[2] = {0, 0};
};

} // namespace microbotica::testing
