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
#pragma once
#include <string>

namespace iglu::nanovg {

static std::string openglVertexShaderHeader410 = R"(#version 410
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tcoord;

out vec2 fpos;
out vec2 ftcoord;

layout(std140) uniform VertexUniformBlock {
 mat4 matrix;
 vec2 viewSize;
}uniforms;
)";

static std::string openglVertexShaderHeader460 = R"(#version 460
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tcoord;

layout (location=0) out vec2 fpos;
layout (location=1) out vec2 ftcoord;

layout(set = 1, binding = 1, std140) uniform VertexUniformBlock {
 mat4 matrix;
 vec2 viewSize;
}uniforms;
)";

static std::string openglVertexShaderBody = R"(
void main() {
  ftcoord = tcoord;
  fpos = pos;
  gl_Position = vec4(2.0 * pos.x / uniforms.viewSize.x - 1.0,
                     1.0 - 2.0 * pos.y / uniforms.viewSize.y,
                   0, 1);
  gl_Position = uniforms.matrix * gl_Position; 
}
)";

static std::string openglFragmentShaderHeader410 = R"(#version 410
precision highp int; 
precision highp float;

in vec2 fpos;
in vec2 ftcoord;

layout (location=0) out vec4 FragColor;

uniform lowp sampler2D textureUnit;

layout(std140) uniform FragmentUniformBlock {
  mat3 scissorMat;
  mat3 paintMat;
  vec4 innerCol;
  vec4 outerCol;
  vec2 scissorExt;
  vec2 scissorScale;
  vec2 extent;
  float radius;
  float feather;
  float strokeMult;
  float strokeThr;
  int texType;
  int type;
}uniforms;

)";

static std::string openglFragmentShaderHeader460 = R"(#version 460
precision highp int; 
precision highp float;

layout (location=0) in vec2 fpos;
layout (location=1) in vec2 ftcoord;

layout (location=0) out vec4 FragColor;

layout(set = 0, binding = 0)  uniform lowp sampler2D textureUnit;

layout(set = 1, binding = 2, std140) uniform FragmentUniformBlock {
  mat3 scissorMat;
  mat3 paintMat;
  vec4 innerCol;
  vec4 outerCol;
  vec2 scissorExt;
  vec2 scissorScale;
  vec2 extent;
  float radius;
  float feather;
  float strokeMult;
  float strokeThr;
  int texType;
  int type;
}uniforms;
)";

static std::string openglNoAntiAliasingFragmentShaderBody = R"(
float scissorMask(vec2 p) {
  vec2 sc = (abs((uniforms.scissorMat * vec3(p, 1.0f)).xy)
                  - uniforms.scissorExt)  * uniforms.scissorScale;
  sc = clamp(vec2(0.5f) - sc, 0.0, 1.0);
  return sc.x * sc.y;
}

float sdroundrect(vec2 pt) {
  vec2 ext2 = uniforms.extent - vec2(uniforms.radius);
  vec2 d = abs(pt) - ext2;
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - uniforms.radius;
}

float strokeMask(vec2 ftcoord) {
  return min(1.0, (1.0 - abs(ftcoord.x * 2.0 - 1.0)) * uniforms.strokeMult) * min(1.0, ftcoord.y);
}

vec4 fragmentShaderNoAntiAliasing() {
  float scissor = scissorMask(fpos);
  if (scissor == 0.0)
    return vec4(0);

  if (uniforms.type == 0) {  // MNVG_SHADER_FILLGRAD
    vec2 pt = (uniforms.paintMat * vec3(fpos, 1.0)).xy;
    float d = clamp((uniforms.feather * 0.5 + sdroundrect(pt))
                       / uniforms.feather, 0.0, 1.0);
    vec4 color = mix(uniforms.innerCol, uniforms.outerCol, d);
    return color * scissor;
  } else if (uniforms.type == 1) {  // MNVG_SHADER_FILLIMG
    vec2 pt = (uniforms.paintMat * vec3(fpos, 1.0)).xy / uniforms.extent;
    vec4 color = texture(textureUnit, pt);
    if (uniforms.texType == 1)
      color = vec4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = vec4(color.x);
    color *= scissor;
    return color * uniforms.innerCol;
  } else {  // MNVG_SHADER_IMG
    vec4 color = texture(textureUnit, ftcoord);
    if (uniforms.texType == 1)
      color = vec4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = vec4(color.x);
    color *= scissor;
    return color * uniforms.innerCol;
  }
}

void main(){
    FragColor = fragmentShaderNoAntiAliasing();
}

)";

static std::string openglAntiAliasingFragmentShaderBody = R"(
float scissorMask(vec2 p) {
  vec2 sc = (abs((uniforms.scissorMat * vec3(p, 1.0f)).xy)
                  - uniforms.scissorExt)  * uniforms.scissorScale;
  sc = clamp(vec2(0.5f) - sc, 0.0, 1.0);
  return sc.x * sc.y;
}

float sdroundrect(vec2 pt) {
  vec2 ext2 = uniforms.extent - vec2(uniforms.radius);
  vec2 d = abs(pt) - ext2;
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - uniforms.radius;
}

float strokeMask(vec2 ftcoord) {
  return min(1.0, (1.0 - abs(ftcoord.x * 2.0 - 1.0)) * uniforms.strokeMult) * min(1.0, ftcoord.y);
}

vec4 fragmentShaderAntiAliasing() {
    float scissor = scissorMask(fpos);
    if (scissor == 0.0)
      return vec4(0);

    if (uniforms.type == 2) {  // MNVG_SHADER_IMG
      vec4 color = texture(textureUnit, ftcoord);
      if (uniforms.texType == 1)
        color = vec4(color.xyz * color.w, color.w);
      else if (uniforms.texType == 2)
        color = vec4(color.x);
      color *= scissor;
      return color * uniforms.innerCol;
    }

    float strokeAlpha = strokeMask(ftcoord);
    if (strokeAlpha < uniforms.strokeThr) {
      return vec4(0);
    }

    if (uniforms.type == 0) {  // MNVG_SHADER_FILLGRAD
      vec2 pt = (uniforms.paintMat * vec3(fpos, 1.0)).xy;
      float d = clamp((uniforms.feather * 0.5 + sdroundrect(pt))
                          / uniforms.feather, 0.0, 1.0);
      vec4 color = mix(uniforms.innerCol, uniforms.outerCol, d);
      color *= scissor;
      color *= strokeAlpha;
      return color;
    } else {  // MNVG_SHADER_FILLIMG
      vec2 pt = (uniforms.paintMat * vec3(fpos, 1.0)).xy / uniforms.extent;
      vec4 color = texture(textureUnit, pt);
      if (uniforms.texType == 1)
        color = vec4(color.xyz * color.w, color.w);
      else if (uniforms.texType == 2)
        color = vec4(color.x);
      color *= scissor;
      color *= strokeAlpha;
      return color * uniforms.innerCol;
    }
}

void main(){
    FragColor = fragmentShaderAntiAliasing();
}

)";

} // namespace iglu::nanovg
