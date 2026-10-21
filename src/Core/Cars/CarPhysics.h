#pragma once
// A car's physics with its parts fitted, and its performance rating (docs/spec/07-dynamics.md).

#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Base/GameFiles.h"

#include <map>
#include <optional>

namespace oj {

// The x87 precision of the thread that computes, which the results depend on: the main thread runs at single
// precision (Direct3D 9 sets it when the device is made), other threads at the runtime's double precision; both round
// to nearest.
enum class X87 { Single, Double };

// The dynamic files of the car models, each read when first asked for (the CarDef's +0x4D0, 0x004899F0):
// scripts\dynamic_<name>.txt.
class DynamicsStore {
public:
    explicit DynamicsStore(const GameFiles& files) : m_files(files) {}

    // None when the model has no dynamic file or the file has no car.
    const CarDynamics* of(const CarDef& def);

private:
    const GameFiles& m_files;
    std::map<const CarDef*, std::optional<CarDynamics>> m_loaded;
};

// The .ocd meshes of a body's mesh collider and collidee, read from the car's folder (cars\<name>; the files name
// them relative to it). A file that is missing or does not read leaves its shape out.
void loadShapes(BodyDef& body, const GameFiles& files, const std::filesystem::path& folder);

// The torque factor at an engine speed (rad/s), TorqueCurveY over TorqueCurveX (0x00554740).
float torqueFactor(const CarDynamics& d, float speed);

// 0x0048E380: the catalogue parts of the car put into a copy of its physics; also sets the car's brake value (+0x80).
// The suspension's part stiffens the tyre contacts' springs, the tyres' part their lateral grip.
void fitParts(CarDynamics& d, Car& car, const PartCatalogue& catalogue, X87 precision = X87::Single);

// 0x004F4270: the largest power of the engine, sampled every 250 rpm over the torque curve.
float peakPower(const CarDynamics& d, X87 precision = X87::Single);

// 0x0048E8A0: the performance rating (+0x88) = peak power with the parts x 0.00048103448 - 0.5; 100 without
// dynamics. Returns the rating.
float rateCar(Car& car, const CarDynamics* dynamics, const PartCatalogue& catalogue,
              X87 precision = X87::Single);

}  // namespace oj
