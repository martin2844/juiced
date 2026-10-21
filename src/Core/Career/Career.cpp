#include "Core/Career/Career.h"

#include <cstring>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace oj {

namespace {

// Byte ranges {offset, size} of an object in the order a stock writer stores them.
using Ranges = std::initializer_list<std::pair<size_t, size_t>>;

constexpr Ranges kCalendar = {{0x000, 4}, {0x004, 4}, {0x008, 4}, {0x00C, 4}, {0x010, 0xF78}, {0xF8C, 4}, {0xF90, 4}};
constexpr Ranges kProfileHead = {{0x00C, 4}, {0x500, 16}, {0x510, 4}};
constexpr Ranges kProfileTail = {{0x620, 4}, {0x624, 4}, {0x010, 4}, {0x014, 4}, {0x04E, 2}, {0x018, 14},
                                 {0x026, 18}, {0x038, 18}, {0x04A, 2}, {0x04C, 2}, {0x050, 0x212}, {0x262, 0x212},
                                 {0x474, 0x35}, {0x4A9, 0x35}, {0x000, 4}, {0x008, 4}, {0x4E8, 4}, {0x4EC, 4},
                                 {0x4F0, 4}, {0x4F4, 4}, {0x4F8, 4}, {0x4FC, 4}, {0x628, 4}};
constexpr Ranges kPhone = {{0x83C, 0x750}, {0xF8C, 4}, {0x008, 4}, {0xFA4, 4}};
constexpr Ranges kDealer = {{0x27EC, 0x27C0}, {0x4FAC, 4}, {0x27E0, 4}};
constexpr Ranges kAdvisor = {{0x24, 0x36}, {0x60, 4}};
constexpr Ranges kHistory = {{0x708, 4}, {0x000, 0x708}};
constexpr Ranges kCrewHead = {{0x02C, 0x28}, {0x070, 4}, {0x054, 0x10}, {0x064, 4}, {0x068, 4}, {0x06C, 4}};
constexpr Ranges kCrewNames = {{0x07A, 100}, {0x0DE, 100}};
constexpr Ranges kCrewTail = {{0x2C0, 4}, {0x2C4, 4}, {0x2C8, 4}, {0x2CC, 4}, {0x2F0, 4}, {0x30C, 4}, {0x328, 4},
                              {0x330, 4}, {0x224, 4}, {0x2D4, 4}, {0x2D8, 4}, {0x334, 4}, {0x338, 4}, {0x2D0, 4}};
constexpr Ranges kCrewmate = {{0x8C, 4}, {0x94, 0x18}, {0xAC, 4}, {0xB0, 4}, {0xB4, 4}, {0xB8, 4}, {0xBC, 4},
                              {0xC0, 4}, {0x04, 4}};

class Reader {
public:
    explicit Reader(const std::vector<uint8_t>& data) : m_data(data) {}

    const uint8_t* take(size_t n, const char* what) {
        if (m_offset + n > m_data.size()) {
            throw std::runtime_error(std::format("{}: {} bytes at {} run past the end ({})", what, n, m_offset,
                                                 m_data.size()));
        }
        const uint8_t* p = m_data.data() + m_offset;
        m_offset += n;
        return p;
    }
    uint32_t u32(const char* what) {
        uint32_t v = 0;
        std::memcpy(&v, take(4, what), 4);
        return v;
    }
    template <class T>
    void ranges(T& object, Ranges ranges, const char* what) {
        auto* base = reinterpret_cast<uint8_t*>(&object);
        for (const auto& [offset, size] : ranges) {
            std::memcpy(base + offset, take(size, what), size);
        }
    }
    void cars(std::vector<Car>& cars, uint32_t& tail, const char* what) {
        cars.resize(u32(what));
        for (Car& car : cars) {
            if (!car.read(m_data, m_offset)) {
                throw std::runtime_error(std::format("{}: a car at {} runs past the end", what, m_offset));
            }
        }
        tail = u32(what);
    }
    size_t offset() const { return m_offset; }
    size_t size() const { return m_data.size(); }

private:
    const std::vector<uint8_t>& m_data;
    size_t m_offset = 0;
};

void putU32(std::vector<uint8_t>& out, uint32_t v) {
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
}

template <class T>
void putRanges(std::vector<uint8_t>& out, const T& object, Ranges ranges) {
    const auto* base = reinterpret_cast<const uint8_t*>(&object);
    for (const auto& [offset, size] : ranges) {
        out.insert(out.end(), base + offset, base + offset + size);
    }
}

void putCars(std::vector<uint8_t>& out, const std::vector<Car>& cars, uint32_t tail) {
    putU32(out, static_cast<uint32_t>(cars.size()));
    for (const Car& car : cars) car.write(out);
    putU32(out, tail);
}

}  // namespace

// 0x004BD650 writes (after the save system's u32); the loader reads the same order.
bool Career::read(const std::vector<uint8_t>& data, std::string* error) {
    try {
        Reader r(data);
        head = r.u32("head");
        r.ranges(calendar, kCalendar, "calendar");
        r.ranges(profile, kProfileHead, "profile");
        for (int32_t& member : members) member = static_cast<int32_t>(r.u32("profile members"));
        r.ranges(profile, kProfileTail, "profile");
        r.cars(garage, garageTail, "garage");
        r.cars(padding, paddingTail, "garage padding");
        r.ranges(phone, kPhone, "phone");
        r.ranges(dealer, kDealer, "dealer");
        r.ranges(advisor, kAdvisor, "advisor");
        r.ranges(history, kHistory, "history");
        for (size_t i = 0; i < crews.size(); ++i) {
            Crew& crew = crews[i];
            r.ranges(crew, kCrewHead, "crew");
            if (i == 0) {
                r.ranges(crew, kCrewNames, "crew names");
            }
            r.ranges(crew, kCrewTail, "crew");
        }
        for (Crewmate& mate : crewmates) r.ranges(mate, kCrewmate, "crewmate");
        if (r.offset() != r.size()) {
            throw std::runtime_error(std::format("{} bytes left after the career", r.size() - r.offset()));
        }
        return true;
    } catch (const std::exception& e) {
        if (error != nullptr) *error = e.what();
        return false;
    }
}

std::vector<uint8_t> Career::write() const {
    std::vector<uint8_t> out;
    putU32(out, head);
    putRanges(out, calendar, kCalendar);
    putRanges(out, profile, kProfileHead);
    for (int32_t member : members) putU32(out, static_cast<uint32_t>(member));
    putRanges(out, profile, kProfileTail);
    putCars(out, garage, garageTail);
    putCars(out, padding, paddingTail);
    putRanges(out, phone, kPhone);
    putRanges(out, dealer, kDealer);
    putRanges(out, advisor, kAdvisor);
    putRanges(out, history, kHistory);
    for (size_t i = 0; i < crews.size(); ++i) {
        putRanges(out, crews[i], kCrewHead);
        if (i == 0) {
            putRanges(out, crews[i], kCrewNames);
        }
        putRanges(out, crews[i], kCrewTail);
    }
    for (const Crewmate& mate : crewmates) putRanges(out, mate, kCrewmate);
    return out;
}

}  // namespace oj
