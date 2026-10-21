#include "Game/Frontend/Garage/Graph.h"

#include <array>
#include <bit>

namespace oj {

namespace {

// [0x007191E0]: the series' colours by their number modulo 10.
constexpr std::array<uint32_t, 10> kSeriesColours = {0xFF4EACD4, 0xFFFFFF00, 0xFF00FF00, 0xFF0000FF, 0xFFFF00FF,
                                                     0xFF00FFFF, 0xFF7FFF3F, 0xFFFF7F7F, 0xFFFFFFFF, 0xFF7F7FFF};

std::wstring widened32(const std::string& text) {
    std::wstring out;
    for (size_t i = 0; i < 32 && i < text.size(); ++i) {
        out.push_back(static_cast<wchar_t>(static_cast<int16_t>(static_cast<signed char>(text[i]))));
    }
    return out;
}

}  // namespace

Graph::Graph(const std::string& graphName, uint32_t count) : name(widened32(graphName)), points(count), drawn(count) {}

void Graph::addSeries(const std::string& seriesName) {
    const auto number = static_cast<uint32_t>(series.size());
    Series s;
    s.name = widened32(seriesName);
    s.values.assign(points, 0.0f);
    s.shown = true;
    s.scale = std::bit_cast<float>(number);
    series.push_back(std::move(s));
    order.push_back(number);
}

void Graph::setScale(uint32_t s, float scale) {
    if (s < series.size()) series[s].scale = scale;
}

void Graph::set(uint32_t s, float value) {
    if (!running || s >= series.size()) return;
    series[s].values[next] = value;
}

void Graph::advance() {
    if (!running) return;
    next = next + 1;
    if (points <= next) next = 0;
    for (Series& s : series) s.values[next] = 0.0f;
}

void Graph::draw(render::IRenderDevice& device, bool shadow) const {
    device.drawLine(x, y, static_cast<float>(points) * 2.0f, y, 0x80FFFFFF);
    for (uint32_t number : order) {
        const Series& s = series[number];
        const uint32_t rgb = kSeriesColours[number % 10] & 0x00FFFFFF;
        drawSeries(device, s, rgb | (s.shown ? 0xFF000000u : 0x30000000u), shadow);
    }
}

void Graph::drawSeries(render::IRenderDevice& device, const Series& s, uint32_t argb, bool shadow) const {
    const float sx = static_cast<float>(device.width()) * 0.0015625f;
    const float sy = static_cast<float>(device.height()) * 0.0020833334f;
    uint32_t at = next;
    for (uint32_t i = 0; i + 1 < drawn; ++i) {
        const uint32_t then = at + 1 == drawn ? 0 : at + 1;
        // Stock problem: a next point past the points drawn runs off the values; not reached by car_rolling.
        if (then >= s.values.size()) return;
        float ax = static_cast<float>(i) * sx + static_cast<float>(i) * sx + sx * x;
        float ay = sy * y - sy * s.values[at] * s.scale;
        float bx = static_cast<float>(i + 1) * sx + static_cast<float>(i + 1) * sx + sx * x;
        float by = sy * y - sy * s.values[then] * s.scale;
        if (shadow && s.values[then] != 0.0f) {
            const float dx = sx * 3.0f;
            const float dy = sy + sy;
            device.drawLine(dx + ax, dy + ay, dx + bx, dy + by, 0xFF000000);
        }
        device.drawLine(ax, ay, bx, by, argb);
        at = then;
    }
}

}  // namespace oj
