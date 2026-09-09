#pragma once

#include <Base/Canvas.hpp>
#include <Base/FontInfo.hpp>
#include <Base/Math.hpp>
#include <Base/Random.hpp>
#include <Base/TextLayout.hpp>

#include <cmath>
#include <cstdint>

namespace TellerEngine::Base {

inline constexpr std::uint32_t kRevealAll = 0xFFFFFFFFu;

// 波の進み方
inline constexpr double kWaveRate = 0.2;
inline constexpr double kWavePhase = 0.5;

struct TextDrawSettings {
    FontFace face;

    // 空ならfaceを使う
    FontFace rubyFace;

    // このバイト位置より前の文字だけを出す
    std::uint32_t revealed = kRevealAll;

    // 波に使う
    std::uint64_t frame = 0;

    // 揺れに使う
    Random *random = nullptr;

    double alpha = 1.0;

    // 画面の拡大率
    // 位置はこの目盛りに合わせて丸める
    double pixelScale = 1.0;
};

inline void DrawTextLayout(Canvas &canvas, const TextLayout &layout,
                           const TextDrawSettings &settings) {
    if (!settings.face) {
        return;
    }
    const double scale = settings.pixelScale > 0.0 ? settings.pixelScale : 1.0;

    std::uint32_t index = 0;
    for (const PlacedGlyph &placed : layout.glyphs) {
        const std::uint32_t here = index;
        index += 1;
        if (placed.at >= settings.revealed) {
            continue;
        }

        const FontFace &face =
            placed.ruby && settings.rubyFace ? settings.rubyFace : settings.face;
        const Glyph *glyph = face.info->Find(placed.character);
        if (glyph == nullptr || glyph->width <= 0 || glyph->height <= 0) {
            continue;
        }

        double x = placed.x + glyph->offset * placed.scaleX;
        double y = placed.y;

        const double shake = placed.style.shake;
        if (shake > 0.0 && settings.random != nullptr) {
            x += settings.random->Real(shake) - shake / 2.0;
            y += settings.random->Real(shake) - shake / 2.0;
        }
        if (placed.style.wave != 0.0) {
            y += std::sin(static_cast<double>(settings.frame) * kWaveRate +
                          static_cast<double>(here) * kWavePhase) *
                 placed.style.wave;
        }

        canvas.SetColor(placed.style.color);
        canvas.SetAlpha(settings.alpha);
        canvas.SpritePartExt(face.image, 0, glyph->x, glyph->y, glyph->width, glyph->height,
                             Round(x * scale) / scale, Round(y * scale) / scale,
                             placed.scaleX, placed.scaleY);
    }
}

} // namespace TellerEngine::Base
