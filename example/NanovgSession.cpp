// Copyright (c) 2025 vinsentli
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "NanovgSession.h"

#include <chrono>
#include <filesystem>
#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/RenderCommandEncoder.h>
#include <regex>
#include <shell/shared/fileLoader/FileLoader.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

int NanovgSession::loadDemoData(NVGcontext* vg, DemoData* data) {
  auto getImageFullPath = ([this](const std::string& name) {
#if IGL_PLATFORM_ANDROID
    auto fullPath = std::filesystem::path("/data/data/com.facebook.igl.shell/files/") / name;
    if (std::filesystem::exists(fullPath)) {
      return fullPath.string();
    } else {
      IGL_DEBUG_ASSERT(false);
      return std::string("");
    }
#else
    return getPlatform().getImageLoader().fileLoader().fullPath(name);
#endif
  });

  if (!IGL_DEBUG_VERIFY(vg)) {
    return -1;
  }

  for (int i = 0; i < 12; i++) {
    char file[128];
    snprintf(file, 128, "image%d.jpg", i + 1);

    std::string full_file = getImageFullPath(file);
    data->images[i] = nvgCreateImage(vg, full_file.c_str(), 0);
    if (!IGL_DEBUG_VERIFY(data->images[i] != 0, "Could not load %s.", file)) {
      return -1;
    }
  }

  data->fontIcons = nvgCreateFont(vg, "icons", getImageFullPath("entypo.ttf").c_str());
  if (!IGL_DEBUG_VERIFY(data->fontIcons != -1, "Could not add font icons.")) {
    return -1;
  }
  data->fontNormal = nvgCreateFont(vg, "sans", getImageFullPath("Roboto-Regular.ttf").c_str());
  if (!IGL_DEBUG_VERIFY(data->fontNormal != -1, "Could not add font italic.")) {
    return -1;
  }
  data->fontBold = nvgCreateFont(vg, "sans-bold", getImageFullPath("Roboto-Bold.ttf").c_str());
  if (!IGL_DEBUG_VERIFY(data->fontBold != -1, "Could not add font bold.")) {
    return -1;
  }
  data->fontEmoji = nvgCreateFont(vg, "emoji", getImageFullPath("NotoEmoji-Regular.ttf").c_str());
  if (!IGL_DEBUG_VERIFY(data->fontEmoji != -1, "Could not add font emoji.")) {
    return -1;
  }
  nvgAddFallbackFontId(vg, data->fontNormal, data->fontEmoji);
  nvgAddFallbackFontId(vg, data->fontBold, data->fontEmoji);

  return 0;
}

void NanovgSession::initialize() noexcept {
  const CommandQueueDesc desc;
  commandQueue_ = getPlatform().getDevice().createCommandQueue(desc, nullptr);

  renderPass_.colorAttachments.resize(1);

  renderPass_.colorAttachments[0] = igl::RenderPassDesc::ColorAttachmentDesc{};
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = igl::Color(0.3f, 0.3f, 0.32f, 1.0f);
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;
  renderPass_.stencilAttachment.loadAction = LoadAction::Clear;
  renderPass_.stencilAttachment.clearStencil = 0;

  mouseListener_ = std::make_shared<MouseListener>();
  getPlatform().getInputDispatcher().addMouseListener(mouseListener_);

  touchListener_ = std::make_shared<TouchListener>();
  getPlatform().getInputDispatcher().addTouchListener(touchListener_);

  nvgContext_ = iglu::nanovg::CreateContext(
      &getPlatform().getDevice(), iglu::nanovg::NVG_ANTIALIAS | iglu::nanovg::NVG_STENCIL_STROKES);

  if (this->loadDemoData(nvgContext_, &nvgDemoData_) != 0) {
    IGL_DEBUG_ASSERT(false);
  }

  initGraph(&fps_, GRAPH_RENDER_FPS, "Frame Time");
  initGraph(&cpuGraph_, GRAPH_RENDER_MS, "CPU Time");
  initGraph(&gpuGraph_, GRAPH_RENDER_MS, "GPU Time");
  times_ = 0;
}

void NanovgSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
  framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
  framebufferDesc.stencilAttachment.texture = surfaceTextures.depth;

  const auto dimensions = surfaceTextures.color->getDimensions();
  framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);
  IGL_DEBUG_ASSERT(framebuffer_);
  framebuffer_->updateDrawable(surfaceTextures.color);

  // Command buffers (1-N per thread): create, submit and forget
  const CommandBufferDesc cbDesc;
  const std::shared_ptr<ICommandBuffer> buffer =
      commandQueue_->createCommandBuffer(cbDesc, nullptr);

  // This will clear the framebuffer
  std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  drawNanovg((float)dimensions.width, (float)dimensions.height, commands);

  commands->endEncoding();

  if (shellParams().shouldPresent) {
    buffer->present(surfaceTextures.color);
  }

  commandQueue_->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

void NanovgSession::drawNanovg(float framebuffferWidth,
                               float framebufferHeight,
                               std::shared_ptr<igl::IRenderCommandEncoder> command) {
  NVGcontext* vg = nvgContext_;

  float pxRatio = 2.0f;

  const float width = framebuffferWidth / pxRatio;
  const float height = framebufferHeight / pxRatio;

#if IGL_PLATFORM_IOS || IGL_PLATFORM_ANDROID
  int mx = touchListener_->touchX;
  int my = touchListener_->touchY;
#else
  int mx = mouseListener_->mouseX;
  int my = mouseListener_->mouseY;
#endif

  auto start = getSeconds();

  nvgBeginFrame(vg, width, height, pxRatio);
  iglu::nanovg::SetRenderCommandEncoder(
      vg,
      framebuffer_.get(),
      command.get(),
      (float*)&getPlatform().getDisplayContext().preRotationMatrix);

  times_++;

  renderDemo(vg, mx, my, width, height, times_ / 60.0f, 0, &nvgDemoData_);

  renderGraph(vg, 5, 5, &fps_);
  renderGraph(vg, 5 + 200 + 5, 5, &cpuGraph_);
  renderGraph(vg, 5 + 200 + 5 + 200 + 5, 5, &gpuGraph_);

  nvgEndFrame(vg);

  auto end = getSeconds();

  updateGraph(&fps_, getDeltaSeconds());
  updateGraph(&cpuGraph_, (end - start));
}

void NanovgSession::teardown() noexcept {
  if (nvgContext_) {
    iglu::nanovg::DestroyContext(nvgContext_);
  }
}

} // namespace igl::shell
