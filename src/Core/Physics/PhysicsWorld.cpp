#include "Core/Physics/PhysicsWorld.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace oj {

namespace {

// The bodies whose near bodies the world steps find next ([0x0079E894] of the primary list, [0x0079E890] of the
// active list): the process's, never reset, so a race goes on from where the one before left them.
size_t g_primaryTurn = 0;
size_t g_activeTurn = 0;

}  // namespace

namespace {

const Cld& noRoute() {
    static const Cld empty;
    return empty;
}

}  // namespace

PhysicsWorld::PhysicsWorld() : m_cld(noRoute()), m_collision(noRoute(), false) {}

bool PhysicsWorld::add(RigidBody& body) {
    if (body.world != nullptr) return false;
    body.world = this;
    body.pools = &m_pools;
    body.collision = &m_collision;
    if (body.def().collision.primary) {
        m_primary.push_back(&body);
    } else {
        m_secondary.push_back(&body);
    }
    if (m_gridMade) {
        placeInGrid(body);
        activate(body, true);
    }
    return true;
}

void PhysicsWorld::remove(RigidBody& body) {
    if (body.world == nullptr) return;
    if (body.cell != -1) {
        if (body.cellPrevious == nullptr) {
            m_cells[static_cast<size_t>(body.cell)].first = body.cellNext;
        } else {
            body.cellPrevious->cellNext = body.cellNext;
        }
        if (body.cellNext != nullptr) body.cellNext->cellPrevious = body.cellPrevious;
        body.cell = -1;
    }
    for (RigidBody* b : m_primary) {
        b->nearBodies.clear();
        b->nearDistances.clear();
    }
    for (RigidBody* b : m_secondary) {
        b->nearBodies.clear();
        b->nearDistances.clear();
    }
    auto drop = [&](std::vector<RigidBody*>& list) {
        const auto it = std::find(list.begin(), list.end(), &body);
        if (it == list.end()) return;
        *it = list.back();
        list.pop_back();
    };
    if (!body.def().collision.primary) {
        drop(m_active);
        drop(m_secondary);
    } else {
        drop(m_primary);
    }
    body.world = nullptr;
}

void PhysicsWorld::activate(RigidBody& body, bool on) {
    if (on) {
        if (body.active) return;
        m_active.push_back(&body);
        findNear(body);
        body.active = true;
        return;
    }
    if (!body.active) return;
    const auto it = std::find(m_active.begin(), m_active.end(), &body);
    if (it != m_active.end()) {
        *it = m_active.back();
        m_active.pop_back();
    }
    body.active = false;
}

float PhysicsWorld::smoothTime(float frameTime) {
    float t = 0.0f;
    if (frameTime >= 0.0f) t = frameTime > 0.2f ? 0.2f : frameTime;
    m_times[m_next] = t;
    m_next = (m_next + 1) % m_times.size();
    float sum = 0.0f;
    for (float v : m_times) sum = v + sum;
    m_time = sum * 0.0625f - m_error * 0.1f;
    m_error = (m_time - t) + m_error;
    if (m_time < 0.0f) {
        m_time = 0.0f;
    } else if (m_time > 0.2f) {
        m_time = 0.2f;
    }
    if (++m_frames > 49) return m_time;
    if (t >= 0.0f) {
        m_time = t > 0.066f ? 0.066f : t;
    } else {
        m_time = 0.0f;
    }
    return m_time;
}

void PhysicsWorld::update(float frameTime) {
    smoothTime(frameTime);
    stepFrame();
}

void PhysicsWorld::countSteps() {
    unsigned steps = 1;
    if (static_cast<double>(m_time) > 0.05) steps = 2;
    if (static_cast<double>(m_time) > 0.1) steps = 3;
    m_steps = steps;
}

void PhysicsWorld::stepFrame() {
    countSteps();
    const unsigned steps = m_steps;
    const float each = m_time / static_cast<float>(steps);
    for (unsigned s = 0; s < steps; ++s) {
        m_pools.empty();
        step(each);
    }
}

void PhysicsWorld::makeGrid() {
    // The route's bounds in x and z over the collision's vertices (a value below the least is not compared with the
    // greatest).
    m_minX = FLT_MAX;
    m_minZ = FLT_MAX;
    m_maxX = -FLT_MAX;
    m_maxZ = -FLT_MAX;
    for (const auto& v : m_cld.vertices) {
        if (m_minX <= v[0]) {
            if (m_maxX < v[0]) m_maxX = v[0];
        } else {
            m_minX = v[0];
        }
        if (m_minZ <= v[2]) {
            if (m_maxZ < v[2]) m_maxZ = v[2];
        } else {
            m_minZ = v[2];
        }
    }
    // Each cell's neighbours: the cells either side, kept within the grid (an edge cell takes the one inside).
    auto range = [](int i, uint8_t& lo, uint8_t& hi) {
        if (i == 0) {
            lo = 0;
            hi = 1;
        } else if (i == 15) {
            lo = 14;
            hi = 15;
        } else {
            lo = static_cast<uint8_t>(i - 1);
            hi = static_cast<uint8_t>(i + 1);
        }
    };
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            Cell& c = m_cells[static_cast<size_t>(z * 16 + x)];
            range(x, c.x0, c.x1);
            range(z, c.z0, c.z1);
            c.first = nullptr;
        }
    }
    for (std::vector<RigidBody*>* list : {&m_primary, &m_secondary}) {
        for (RigidBody* b : *list) {
            b->cell = -1;
            b->cellNext = nullptr;
            b->cellPrevious = nullptr;
            placeInGrid(*b);
            if (b->active) continue;
            m_active.push_back(b);
            findNear(*b);
            b->active = true;
        }
    }
    m_gridMade = true;
}

void PhysicsWorld::placeInGrid(RigidBody& b) {
    // The cell of the node's x and z: the share of the bounds x 16 as a float, cut toward zero (x87 chop or SSE with
    // the game's MXCSR); as an unsigned number 16 or more is 15 (also below the bounds).
    auto index = [](float share) {
        const float f = share * 16.0f;
        uint32_t u = 0x80000000u;
        if (std::isfinite(f) && f > -2147483648.0f && f < 2147483648.0f) {
            u = static_cast<uint32_t>(static_cast<int32_t>(f));
        }
        return u < 16u ? u : 15u;
    };
    const uint32_t x = index((b.node[3] - m_minX) / (m_maxX - m_minX));
    const uint32_t z = index((b.node[11] - m_minZ) / (m_maxZ - m_minZ));
    const auto cell = static_cast<int32_t>(z * 16 + x);
    if (cell == b.cell) return;
    // 0x0053E770: out of the old cell's list, in at the head of the new one.
    if (b.cell != -1) {
        if (b.cellPrevious == nullptr) {
            m_cells[static_cast<size_t>(b.cell)].first = b.cellNext;
        } else {
            b.cellPrevious->cellNext = b.cellNext;
        }
        if (b.cellNext != nullptr) b.cellNext->cellPrevious = b.cellPrevious;
        b.cell = -1;
    }
    Cell& c = m_cells[static_cast<size_t>(cell)];
    if (c.first != nullptr) c.first->cellPrevious = &b;
    b.cellNext = c.first;
    c.first = &b;
    b.cellPrevious = nullptr;
    b.cell = cell;
}

void PhysicsWorld::findNear(RigidBody& b) {
    b.nearBodies.clear();
    b.nearDistances.clear();
    if (!b.collides || b.cell == -1) return;
    const size_t most = static_cast<size_t>(std::max(b.def().collision.maxCollidees, 0));
    const float x = b.node[3];
    const float z = b.node[11];
    const Cell& here = m_cells[static_cast<size_t>(b.cell)];
    for (uint32_t cz = here.z0; cz <= here.z1; ++cz) {
        for (uint32_t cx = here.x0; cx <= here.x1; ++cx) {
            for (RigidBody* n = m_cells[cz * 16 + cx].first; n != nullptr; n = n->cellNext) {
                if (n == &b || !n->collides) continue;
                const float dx = n->node[3] - x;
                const float dz = n->node[11] - z;
                const float d = dx * dx + dz * dz;
                const float reach = n->boundsRadius + b.boundsRadius + 1000.0f;
                const bool primary = n->def().collision.primary;
                if (!primary && !(d < reach * reach)) continue;
                if (b.nearBodies.size() == most) {
                    // Full: the first kept body a primary one outranks, or one of the same rank farther away.
                    for (size_t k = 0; k < b.nearBodies.size(); ++k) {
                        const bool kept = b.nearBodies[k]->def().collision.primary;
                        if ((primary && !kept) || (primary == kept && d < b.nearDistances[k])) {
                            b.nearBodies[k] = n;
                            b.nearDistances[k] = d;
                            break;
                        }
                    }
                } else {
                    b.nearBodies.push_back(n);
                    b.nearDistances.push_back(d);
                }
            }
        }
    }
}

void PhysicsWorld::step(float dt) {
    if (!m_gridMade) makeGrid();
    if (m_primary.empty()) return;
    for (RigidBody* b : m_primary) placeInGrid(*b);
    if (++g_primaryTurn >= m_primary.size()) g_primaryTurn = 0;
    findNear(*m_primary[g_primaryTurn]);
    if (++g_activeTurn >= m_active.size()) g_activeTurn = 0;
    if (!m_active.empty()) {
        placeInGrid(*m_active[g_activeTurn]);
        findNear(*m_active[g_activeTurn]);
    }
    // The lists are walked by index with their length read anew: a body woken or put to sleep changes the active
    // list as it is walked.
    for (size_t i = 0; i < m_primary.size(); ++i) m_primary[i]->step(dt);
    for (size_t i = 0; i < m_active.size(); ++i) m_active[i]->step(dt);
    for (size_t i = 0; i < m_primary.size(); ++i) m_primary[i]->syncNode();
    for (size_t i = 0; i < m_active.size(); ++i) m_active[i]->syncNode();
}

}  // namespace oj
