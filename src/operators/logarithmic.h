/*
    src/logarithmic.h -- Logarithmic tonemapping operator

    Copyright (c) 2016 Tizian Zeltner

    Tone Mapper is provided under the MIT License.
    See the LICENSE.txt file for the conditions of the license.
*/

#pragma once

#include <tonemap.h>

class LogarithmicOperator : public TonemapOperator {
public:
    LogarithmicOperator() : TonemapOperator() {
        parameters["Gamma"] = Parameter(2.2f, 0.f, 10.f, "gamma", "Gamma correction value");
        parameters["p"] = Parameter(1.f, 0.f, 20.f, "p", "Exponent numerator scale factor");
        parameters["q"] = Parameter(1.f, 0.f, 20.f, "q", "Exponent denominator scale factor");

        name = "Logarithmic";
        description = "Logarthmic Mapping\n\nDiscussed in \"Quantization Techniques for Visualization of High Dynamic Range Pictures\" by Schlick 1994.";

        shader->init(
            "Logarithmic",

            R"glsl(
            #version 330
            in vec2 position;
            out vec2 uv;
            void main() {
                gl_Position = vec4(position.x*2-1, position.y*2-1, 0.0, 1.0);
                uv = vec2(position.x, 1-position.y);
            }
            )glsl",

            R"glsl(
            #version 330
            uniform sampler2D source;
            uniform float exposure;
            uniform float gamma;
            uniform float Lmax;
            uniform float p;
            uniform float q;
            in vec2 uv;
            out vec4 out_color;

            vec4 clampedValue(vec4 color) {
                color.a = 1.0;
                return clamp(color, 0.0, 1.0);
            }

            vec4 gammaCorrect(vec4 color) {
                return pow(color, vec4(1.0/gamma));
            }

            float getLuminance(vec4 color) {
                return 0.212671 * color.r + 0.71516 * color.g + 0.072169 * color.b;
            }

            vec4 adjustColor(vec4 color, float L, float Ld) {
                return Ld * color / L;
            }

            void main() {
                vec4 color = exposure * texture(source, uv);
                float L = getLuminance(color);
                float Ld = (log(1.0 + p * L)/log(10.0)) / (log(1.0 + q * exposure * Lmax)/log(10.0));
                color = adjustColor(color, L, Ld);
                color = clampedValue(color);
                out_color = gammaCorrect(color);
            }
            )glsl"
        );
    }

    virtual void setParameters(const Image *image) override {
        parameters["Lmax"] = Parameter(image->getMaximumLuminance(), "Lmax");
    };

    void process(const Image *image, uint8_t *dst, float exposure, float *progress) const override {
        const nanogui::Vector2i &size = image->getSize();
        *progress = 0.f;
        float delta = 1.f / (size.x() * size.y());

        float gamma = parameters.at("Gamma").value;
        float Lmax = parameters.at("Lmax").value;
        float p = parameters.at("p").value;
        float q = parameters.at("q").value;

        for (int i = 0; i < size.y(); ++i) {
            for (int j = 0; j < size.x(); ++j) {
                const Color3f &color = image->ref(i, j);
                float Lw = color.getLuminance();
                float Ld = map(Lw, exposure, Lmax, p, q);
                Color3f c = Ld * color / Lw;
                c = c.clampedValue();
                c = c.gammaCorrect(gamma);
                dst[0] = (uint8_t) (255.f * c.r());
                dst[1] = (uint8_t) (255.f * c.g());
                dst[2] = (uint8_t) (255.f * c.b());
                dst += 3;
                *progress += delta;
            }
        }
    }

    float graph(float value) const override {
        float gamma = parameters.at("Gamma").value;
        float Lmax = parameters.at("Lmax").value;
        float p = parameters.at("p").value;
        float q = parameters.at("q").value;

        value = map(value, 1.f, Lmax, p, q);
        value = clamp(value, 0.f, 1.f);
        value = std::pow(value, 1.f / gamma);
        return value;
    }

protected:
    float map(float Lw, float exposure, float Lmax, float p, float q) const {
        float L = exposure * Lw;
        float Ld = std::log10(1.f + p * L) / std::log10(1.f + q * exposure * Lmax);
        return Ld;
    }
};