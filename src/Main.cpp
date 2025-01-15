#include <iostream>

#include <simdjson.h>
#include <glm/glm.hpp>

#include <vector>
#include <fstream>
#include <thread>

struct Vertex {
    glm::f64vec3 position;
};

glm::f64vec3 getVec3(simdjson::simdjson_result<simdjson::ondemand::object> object, const std::string_view key) {
    auto array = object[key].get_array();
    auto it = array.begin();

    const double x = double(*it.value());
    const double y = double(*(++it).value());
    const double z = double(*(++it).value());

    return glm::f64vec3(x, y, z);
}

std::size_t get3DIndex(int x, int y, int z, int width, int height) {
    return x + y * width + z * width * height;
}

enum Plane {
    X, Y, Z
};

bool intersectLineQuad(const glm::f64vec3& linePoint1, const glm::f64vec3& linePoint2, const glm::f64vec3& quadMin, const glm::f64vec3& quadMax) {
    Plane plane = Plane::X;
    
    if (quadMin.x == quadMax.x) {
        plane = Plane::X;
    } else if (quadMin.y == quadMax.y) {
        plane = Plane::Y;
    } else if (quadMin.z == quadMax.z) {
        plane = Plane::Z;
    } else {
        throw std::runtime_error("Invalid plane");
    }

    double constant = quadMin[plane];

    double t = -1.0f;
    switch (plane) {
        case Plane::X: t = (constant - linePoint1.x) / (linePoint2.x - linePoint1.x); break; 
        case Plane::Y: t = (constant - linePoint1.y) / (linePoint2.y - linePoint1.y); break;
        case Plane::Z: t = (constant - linePoint1.z) / (linePoint2.z - linePoint1.z); break;
    }

    if (t < 0.0f || t > 1.0f) {
        // The intersection is outside the line segment, fail

        return false;
    }

    // we have an intersection with the plane, now we need to check if it is inside the quad

    const glm::f64vec3 intersection = linePoint1 + (linePoint2 - linePoint1) * t;

    if (intersection.x < quadMin.x || intersection.x > quadMax.x ||
        intersection.y < quadMin.y || intersection.y > quadMax.y ||
        intersection.z < quadMin.z || intersection.z > quadMax.z
    ) {
        // The intersection is outside the quad, fail
        return false;
    }

    return true;
}

bool intersectLineCube(const glm::f64vec3& linePoint1, const glm::f64vec3& linePoint2, const glm::f64vec3& cubeMin, const glm::f64vec3& cubeMax) {
    // If the point is inside the cube, we consider it as an intersection
    if (linePoint1.x >= cubeMin.x && linePoint1.x <= cubeMax.x &&
        linePoint1.y >= cubeMin.y && linePoint1.y <= cubeMax.y &&
        linePoint1.z >= cubeMin.z && linePoint1.z <= cubeMax.z
    ) {
        return true;
    }
    if (linePoint2.x >= cubeMin.x && linePoint2.x <= cubeMax.x &&
        linePoint2.y >= cubeMin.y && linePoint2.y <= cubeMax.y &&
        linePoint2.z >= cubeMin.z && linePoint2.z <= cubeMax.z
    ) {
        return true;
    }

    glm::f64vec3 quadMin[6] = {
        {cubeMin.x, cubeMin.y, cubeMin.z}, // Left
        {cubeMax.x, cubeMin.y, cubeMin.z}, // Right
        {cubeMin.x, cubeMin.y, cubeMin.z}, // Bottom
        {cubeMin.x, cubeMax.y, cubeMin.z}, // Top
        {cubeMin.x, cubeMin.y, cubeMin.z}, // Front
        {cubeMin.x, cubeMin.y, cubeMax.z}  // Back
    };

    glm::f64vec3 quadMax[6] = {
        {cubeMin.x, cubeMax.y, cubeMax.z}, // Left
        {cubeMax.x, cubeMax.y, cubeMax.z}, // Right
        {cubeMax.x, cubeMin.y, cubeMax.z}, // Bottom
        {cubeMax.x, cubeMax.y, cubeMax.z}, // Top
        {cubeMax.x, cubeMax.y, cubeMin.z}, // Front
        {cubeMax.x, cubeMax.y, cubeMax.z}  // Back
    };

    for (int i = 0; i < 6; ++i) {
        if (intersectLineQuad(linePoint1, linePoint2, quadMin[i], quadMax[i])) {
            return true;
        }
    }

    return false;
}

bool intersectTriangleCube(const glm::f64vec3& v0, const glm::f64vec3& v1, const glm::f64vec3& v2, const glm::f64vec3& cubeMin, const glm::f64vec3& cubeMax) {
    if (intersectLineCube(v0, v1, cubeMin, cubeMax)) {
        return true;
    }

    if (intersectLineCube(v1, v2, cubeMin, cubeMax)) {
        return true;
    }

    if (intersectLineCube(v2, v0, cubeMin, cubeMax)) {
        return true;
    }

    return false;
}

constexpr int cGridX = 128;
constexpr int cGridY = 128;
constexpr int cGridZ = 128;

constexpr double cVoxelSize = 1.0f / (double)cGridX;

void checkVoxel(int x, int y, int z, const std::vector<Vertex>& vertexBuffer, const std::vector<std::uint32_t>& indexBuffer, std::uint8_t* grid) {
    glm::f64vec3 voxelCenter = glm::f64vec3(x, y, z);
    voxelCenter /= glm::f64vec3(cGridX, cGridY, cGridZ);

    glm::f64vec3 voxelMin = voxelCenter - glm::f64vec3(cVoxelSize * 0.5f);
    glm::f64vec3 voxelMax = voxelCenter + glm::f64vec3(cVoxelSize * 0.5f);

    for (int i = 0; i < indexBuffer.size(); i += 3) {
        const Vertex& v0 = vertexBuffer[indexBuffer[i]];
        const Vertex& v1 = vertexBuffer[indexBuffer[i + 1]];
        const Vertex& v2 = vertexBuffer[indexBuffer[i + 2]];

        if (intersectTriangleCube(v0.position, v1.position, v2.position, voxelMin, voxelMax)) {
            grid[get3DIndex(x, y, z, cGridX, cGridY)] = true;
            return;
        }
    }
}

void checkTriangle(const glm::f64vec3& v0, const glm::f64vec3& v1, const glm::f64vec3& v2, const std::vector<Vertex>& vertexBuffer, const std::vector<std::uint32_t>& indexBuffer, std::uint8_t* grid) {
    glm::f64vec3 min = glm::min(v0, glm::min(v1, v2));
    glm::f64vec3 max = glm::max(v0, glm::max(v1, v2));

    for (int z = 0; z < cGridZ; z++) {
        for (int y = 0; y < cGridY; y++) {
            for (int x = 0; x < cGridX; x++) {
                glm::f64vec3 voxelCenter = glm::f64vec3(x, y, z);
                voxelCenter /= glm::f64vec3(cGridX, cGridY, cGridZ);

                glm::f64vec3 voxelMin = voxelCenter - glm::f64vec3(cVoxelSize * 0.5f);
                glm::f64vec3 voxelMax = voxelCenter + glm::f64vec3(cVoxelSize * 0.5f);

                if (min.x > voxelMax.x || max.x < voxelMin.x ||
                    min.y > voxelMax.y || max.y < voxelMin.y ||
                    min.z > voxelMax.z || max.z < voxelMin.z
                ) {
                    continue;
                }

                if (intersectTriangleCube(v0, v1, v2, voxelMin, voxelMax)) {
                    grid[get3DIndex(x, y, z, cGridX, cGridY)] = true;
                }
            }
        }
    }
}

int main() {
    simdjson::ondemand::parser parser;
    simdjson::padded_string json = simdjson::padded_string::load("model_complex.json");
    simdjson::ondemand::document document = parser.iterate(json);

    auto vertices = document["vertices"].get_array();

    std::vector<Vertex> vertexBuffer;
    for (auto vertex : vertices) {
        Vertex v;
        v.position = getVec3(vertex.get_object(), "position");
        vertexBuffer.push_back(v);
    }

    double minX = vertexBuffer[0].position.x;
    double minY = vertexBuffer[0].position.y;
    double minZ = vertexBuffer[0].position.z;
    double maxX = vertexBuffer[0].position.x;
    double maxY = vertexBuffer[0].position.y;
    double maxZ = vertexBuffer[0].position.z;

    for (auto vertex : vertexBuffer) {
        minX = std::min(minX, vertex.position.x);
        minY = std::min(minY, vertex.position.y);
        minZ = std::min(minZ, vertex.position.z);

        maxX = std::max(maxX, vertex.position.x);
        maxY = std::max(maxY, vertex.position.y);
        maxZ = std::max(maxZ, vertex.position.z);
    }

    double rangeX = maxX - minX;
    double rangeY = maxY - minY;
    double rangeZ = maxZ - minZ;

    // Find the maximum range to maintain aspect ratio
    double maxRange = std::max({rangeX, rangeY, rangeZ});

    for (auto& vertex : vertexBuffer) {
        vertex.position.x = (vertex.position.x - minX) / maxRange;
        vertex.position.y = (vertex.position.y - minY) / maxRange;
        vertex.position.z = (vertex.position.z - minZ) / maxRange;
    }

    auto indices = document["meshes"].at(0)["indices"].get_array();

    std::vector<std::uint32_t> indexBuffer;
    for (auto index : indices) {
        indexBuffer.push_back(std::int64_t(index));
    }

    std::vector<std::uint8_t> grid(cGridX * cGridY * cGridZ, 0);

    #define STRATEGY_SEQUENTIAL_TRIANGLE

#if defined(STRATEGY_SEQUENTIAL_TRIANGLE)
    for (int i = 0; i < indexBuffer.size(); i += 3) {
        const Vertex& v0 = vertexBuffer[indexBuffer[i]];
        const Vertex& v1 = vertexBuffer[indexBuffer[i + 1]];
        const Vertex& v2 = vertexBuffer[indexBuffer[i + 2]];

        checkTriangle(
            v0.position, v1.position, v2.position,
            vertexBuffer, indexBuffer,
            grid.data()
        );
    }
#elif defined(STRATEGY_SEQUENTIAL_VOXEL)
    for (int z = 0; z < cGridZ; z++) {
        for (int y = 0; y < cGridY; y++) {
            std::cout << "Checking y = " << y << ", z = " << z << "\n";

            for (int x = 0; x < cGridX; x++) {
                checkVoxel(
                    x, y, z,
                    vertexBuffer, indexBuffer,
                    grid.data()
                );
            }
        }
    }
#elif defined (STRATEGY_PARALLEL_VOXEL)
    std::vector<std::thread> threads;
    for (int z = 0; z < cGridZ; z++) {
        const auto work = [z, &vertexBuffer, &indexBuffer, &grid]() {
            for (int y = 0; y < cGridY; y++) {
                std::cout << "Checking y = " << y << ", z = " << z << "\n";

                for (int x = 0; x < cGridX; x++) {
                    checkVoxel(
                        x, y, z,
                        vertexBuffer, indexBuffer,
                        grid.data()
                    );
                }
            }
        };

        threads.emplace_back(work);
    }

    for (auto& thread : threads) {
        thread.join();
    }
#endif

    std::ofstream file("commands.txt");
    for (int z = 0; z < cGridZ; z++) {
        for (int y = 0; y < cGridY; y++) {
            for (int x = 0; x < cGridX; x++) {
                if (grid[get3DIndex(x, y, z, cGridX, cGridY)]) {
                    file << "setblock ~" << x << " ~" << y << " ~" << z << " minecraft:stone" << std::endl;
                }
            }
        }
    }
    file.close();

    return 0;
}
