#include "Core/Cars/CarPhysics.h"

#include "Core/Formats/Ocd.h"

#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

namespace oj {

namespace {

// The part of a kind fitted to the car: none for 0xFF; an index past the list gives its first part. Stock problem: a
// negative index other than -1 (0x80 .. 0xFE) reads before the list's memory; the first part here.
const CarPart* fitted(const Car& car, const PartCatalogue& catalogue, ModKind kind) {
    const auto index = static_cast<int8_t>(car.mods[kind]);
    const std::vector<CarPart>& parts = catalogue.lists[kind].parts;
    if (index == -1 || parts.empty()) {
        return nullptr;
    }
    return index >= 0 && index < static_cast<int>(parts.size()) ? &parts[index] : &parts.front();
}

// R is the x87 precision: float for single, double for double. Values the stock code stores are floats.
template <typename R>
R factorAt(const CarDynamics& d, float speed) {
    const std::vector<float>& xs = d.torqueSpeeds;
    const std::vector<float>& ys = d.torqueFactors;
    const size_t n = xs.size();
    if (n == 0) {
        return R(0);
    }
    // Stock problem: fewer TorqueCurveY than TorqueCurveX values, or a curve of one point, read past the lists (0 and
    // the first value here).
    auto y = [&](size_t i) { return R(i < ys.size() ? ys[i] : 0.0f); };
    if (n == 1) {
        return y(0);
    }
    float x = speed;
    if (xs[0] <= x) {
        x = x <= xs[n - 1] ? x : xs[n - 1];
    } else {
        x = xs[0];
    }
    size_t i = 0;
    while (i < n - 2 && !(x <= xs[i + 1])) {
        ++i;
    }
    if (xs[i] != xs[i + 1]) {
        return (y(i + 1) - y(i)) * ((R(x) - xs[i]) / (R(xs[i + 1]) - xs[i])) + y(i);
    }
    return (y(i + 1) - y(i)) * R(0) + y(i);
}

template <typename R>
void fit(CarDynamics& d, Car& car, const PartCatalogue& catalogue) {
    if (const CarPart* nitro = fitted(car, catalogue, kNitro)) {
        d.nitro = static_cast<float>(R(0.2f) + nitro->dataValue);
        d.nitroTime = Car::asFloat(car.nitro);
    }
    if (const CarPart* turbo = fitted(car, catalogue, kTurbo)) {
        d.turbo = turbo->dataValue;
    } else if (car.def != nullptr && car.def->stockTurbo) {
        d.turbo = 1.01f;
    }
    const CarPart* brakes = fitted(car, catalogue, kBrake);
    const float brakeFactor = brakes != nullptr ? brakes->dataValue : 1.0f;
    for (WheelDef& wheel : d.wheels) {
        wheel.brakeMu = static_cast<float>(R(brakeFactor) * wheel.brakeMu);
        car.brake = Car::bitsOf(wheel.brakeMu);
    }
    // The suspension's part: each tyre contact's stiffness (+0x3C) times its value, its damping (+0x48) again 2 sqrt(
    // the stiffness x the mass) x the damping ratio (the product kept as the FPU holds it).
    if (const CarPart* suspension = fitted(car, catalogue, kSuspension)) {
        for (ContactDef& c : d.body.contacts) {
            if (c.type != ContactDef::Tyre) continue;
            const R stiffness = R(suspension->dataValue) * c.stiffness;
            c.stiffness = static_cast<float>(stiffness);
            const R root = std::sqrt(stiffness * c.mass) * c.dampingRatio;
            c.damping = static_cast<float>(root + root);
        }
    }
    if (const CarPart* exhaust = fitted(car, catalogue, kExhaust)) {
        d.maxTorque = static_cast<float>(R(exhaust->dataValue) * d.maxTorque);
    }
    // The tyres' part: each tyre contact's LateralMu (+0x54) times its value and its ratio of grips (+0x70) again,
    // LongitudinalMu x LateralStiffness over LateralMu x LongitudinalStiffness (0 for none).
    if (const CarPart* tyres = fitted(car, catalogue, kTyre)) {
        for (ContactDef& c : d.body.contacts) {
            if (c.type != ContactDef::Tyre) continue;
            const R mu = R(tyres->dataValue) * c.lateralMu;
            c.lateralMu = static_cast<float>(mu);
            const R below = mu * c.longitudinalStiffness;
            c.muRatio = below == R(0) ? 0.0f : static_cast<float>(R(c.longitudinalMu) * c.lateralStiffness / below);
        }
    }
    if (const CarPart* induction = fitted(car, catalogue, kInduction)) {
        d.maxTorque = static_cast<float>(R(induction->dataValue) * d.maxTorque);
    }
    d.maxTorque = static_cast<float>(R(Car::asFloat(car.torqueFactor)) * d.maxTorque);
    const R half = R(Car::asFloat(car.gearing)) - 0.5f;
    const float gearing = static_cast<float>((half + half) * 0.1f);
    for (float& ratio : d.forwardRatios) {
        ratio = static_cast<float>((R(gearing) + 1.0f) * ratio);
    }
    // A spoiler and a front bumper press the car down (the forces are the variables 0x00725478 and 0x00725474).
    if (static_cast<int8_t>(car.body[3]) > 0) {
        addAirfoil(d.body, {0.0f, 0.0f, -1.5f, 0.0f}, {0.0f, -0.25f, 0.0f, 0.0f}, 0.0f);
    }
    if (static_cast<int8_t>(car.body[0]) > 0) {
        addAirfoil(d.body, {0.0f, 0.0f, 1.5f, 0.0f}, {0.0f, -0.15f, 0.0f, 0.0f}, 0.0f);
    }
}

template <typename R>
R peak(const CarDynamics& d) {
    if (d.torqueSpeeds.empty()) {
        return R(0);
    }
    const R top = R(d.torqueSpeeds.back()) * 9.549296f;
    float rpm = static_cast<float>(R(d.torqueSpeeds.front()) * 9.549296f);
    float best = 0.0f;
    float bestRpm = 0.0f;
    while (rpm < top) {
        const float speed = static_cast<float>(R(rpm) * 0.10471976f);
        const R power = factorAt<R>(d, speed) * d.turbo * d.maxTorque * speed * 2.8f;
        if (power > best) {
            best = static_cast<float>(power);
            bestRpm = rpm;
        }
        rpm = static_cast<float>(R(rpm) + 250.0f);
    }
    const float speed = static_cast<float>(R(bestRpm) * 0.10471976f);
    return factorAt<R>(d, speed) * d.turbo * d.maxTorque * 2.8f * speed;
}

}  // namespace

const CarDynamics* DynamicsStore::of(const CarDef& def) {
    auto found = m_loaded.find(&def);
    if (found == m_loaded.end()) {
        std::optional<CarDynamics> dynamics;
        if (const auto file = m_files.read(std::filesystem::path("scripts") / ("dynamic_" + def.name + ".txt"))) {
            dynamics = parseDynamics(std::string_view(reinterpret_cast<const char*>(file->data()), file->size()));
        }
        found = m_loaded.emplace(&def, std::move(dynamics)).first;
    }
    return found->second ? &*found->second : nullptr;
}

void loadShapes(BodyDef& body, const GameFiles& files, const std::filesystem::path& folder) {
    auto load = [&](const std::string& name) -> std::shared_ptr<const Ocd> {
        if (name.empty()) return nullptr;
        const auto data = files.read(folder / name);
        if (!data) return nullptr;
        auto shape = parseOcd(reinterpret_cast<const uint8_t*>(data->data()), data->size());
        return shape ? std::make_shared<const Ocd>(std::move(*shape)) : nullptr;
    };
    body.collision.colliderShape = load(body.collision.meshCollider);
    body.collision.collideeShape = load(body.collision.meshCollidee);
}

float torqueFactor(const CarDynamics& d, float speed) {
    return factorAt<float>(d, speed);
}

void fitParts(CarDynamics& d, Car& car, const PartCatalogue& catalogue, X87 precision) {
    if (precision == X87::Single) {
        fit<float>(d, car, catalogue);
    } else {
        fit<double>(d, car, catalogue);
    }
}

float peakPower(const CarDynamics& d, X87 precision) {
    return precision == X87::Single ? peak<float>(d) : static_cast<float>(peak<double>(d));
}

float rateCar(Car& car, const CarDynamics* dynamics, const PartCatalogue& catalogue, X87 precision) {
    float rating = 100.0f;
    if (dynamics != nullptr) {
        CarDynamics tuned = *dynamics;
        fitParts(tuned, car, catalogue, precision);
        if (precision == X87::Single) {
            rating = peak<float>(tuned) * 0.00048103448f - 0.5f;
        } else {
            rating = static_cast<float>(peak<double>(tuned) * 0.00048103448f - 0.5f);
        }
    }
    car.rating = Car::bitsOf(rating);
    return rating;
}

}  // namespace oj
