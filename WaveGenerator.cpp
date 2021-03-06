#include "WaveGenerator.h"
#include <chrono>
#include <iostream>
#include <iomanip>

WaveGenerator::WaveGenerator(ColorSource& source, ColorQueue& colorQueue, Options& options) 
    : m_bitmap(options.size.x, options.size.y) {
    m_source = &source;
    m_queue = &colorQueue;
    m_running = std::make_unique<std::atomic_bool>();

    glm::ivec2 pos = { static_cast<int>(m_bitmap.width() / 2), static_cast<int>(m_bitmap.height() / 2) };
    if (m_source->hasNext()) {
        m_bitmap.getPixel(pos.x, pos.y) = m_source->getNext();
        addNeighborsToOpenSet(pos);
    }
}

WaveGenerator::WaveGenerator(WaveGenerator&& other) : m_bitmap(std::move(other.m_bitmap)) {
    *this = std::move(other);
}

void WaveGenerator::run() {
    *m_running = true;

    m_mainThread = std::thread([this]() -> void { mainLoop(); });
}

void WaveGenerator::stop() {
    *m_running = false;

    m_mainThread.join();
}

void WaveGenerator::mainLoop() {
    auto start = std::chrono::steady_clock::now();

    while (*m_running) {
        if (!m_source->hasNext()) break;
        if (m_openSet.size() == 0) break;

        m_openList.clear();
        for (auto pos : m_openSet) {
            m_openList.push_back(pos);
        }

        m_color = m_source->getNext();
        
        size_t result = score();
        readResult(result);
    }

    auto end = std::chrono::steady_clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(33));

    auto elapsed = std::chrono::duration<double>(end - start).count();
    size_t totalPixels = m_queue->totalCount();
    size_t rate = (size_t)(totalPixels / elapsed);
    if (elapsed < 10.0) {
        std::cout << std::setprecision(1) << std::fixed;
    } else {
        std::cout << std::setprecision(0) << std::fixed;
    }
    std::cout << totalPixels << " in " << elapsed << "s (" << rate << " pps)\n";
}

size_t WaveGenerator::score() {
    size_t result;
    int32_t bestScore = std::numeric_limits<int32_t>::max();
    for (size_t i = 0; i < m_openList.size(); i++) {
        glm::ivec2 pos = m_openList[i];
        glm::ivec2 neighbors[8] = {
            pos + glm::ivec2{ -1, -1 },
            pos + glm::ivec2{ -1,  0 },
            pos + glm::ivec2{ -1,  1 },
            pos + glm::ivec2{  0, -1 },
            pos + glm::ivec2{  0,  1 },
            pos + glm::ivec2{  1, -1 },
            pos + glm::ivec2{  1,  0 },
            pos + glm::ivec2{  1,  1 },
        };

        glm::ivec3 testColor = { m_color.r, m_color.g, m_color.b };
        glm::int32_t diffs[8];

        for (size_t j = 0; j < 8; j++) {
            diffs[j] = std::numeric_limits<int32_t>::max();
        }

        for (size_t j = 0; j < 8; j++) {
            auto& n = neighbors[j];
            if (n.x >= 0 && n.y >= 0
                && n.x < m_bitmap.width() && n.y < m_bitmap.height()) {
                Color32 color = m_bitmap.getPixel(n.x, n.y);
                if (color.a == 255) {
                    diffs[j] = length2(glm::ivec3{ color.r, color.g, color.b } - testColor);
                }
            }
        }

        for (size_t j = 0; j < 8; j++) {
            int32_t score = diffs[j];
            if (score < bestScore) {
                result = i;
                bestScore = score;
            }
        }
    }

    return result;
}

void WaveGenerator::readResult(size_t result) {
    glm::ivec2 pos = m_openList[result];
    m_queue->enqueue(pos, m_color);
    m_bitmap.getPixel(pos.x, pos.y) = m_color;
    addNeighborsToOpenSet(pos);
    m_openSet.erase(pos);
}

void WaveGenerator::addToOpenSet(glm::ivec2 pos) {
    m_openSet.insert(pos);
}

void WaveGenerator::addNeighborsToOpenSet(glm::ivec2 pos) {
    glm::ivec2 neighbors[8] = {
        pos + glm::ivec2{ -1, -1 },
        pos + glm::ivec2{ -1,  0 },
        pos + glm::ivec2{ -1,  1 },
        pos + glm::ivec2{ 0, -1 },
        pos + glm::ivec2{ 0,  1 },
        pos + glm::ivec2{ 1, -1 },
        pos + glm::ivec2{ 1,  0 },
        pos + glm::ivec2{ 1,  1 },
    };

    for (size_t i = 0; i < 8; i++) {
        auto& n = neighbors[i];
        if (n.x >= 0 && n.y >= 0
            && n.x < m_bitmap.width() && n.y < m_bitmap.height()
            && m_bitmap.getPixel(n.x, n.y).a == 0) {
            addToOpenSet(n);
        }
    }
}