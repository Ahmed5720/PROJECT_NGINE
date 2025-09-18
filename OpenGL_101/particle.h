#include <glad/glad.h>
#include <GLFW/glfw3.h>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <omp.h>
class particle
{
public:
    // renders (and builds at first invocation) a sphere // adapted from learnopengl.com
    // -------------------------------------------------
    unsigned int sphereVAO = 0;
    unsigned int indexCount;
    std::vector<glm::vec3> velocities;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> predictedPositions;
    std::vector<float> densities;

    std::vector<std::pair<int, int>> gridmap;
    std::vector<int> startingIdxs;


    float smoothingRadius = 5.0f;
    float mass = 5;
    float targetDensity = 1;
    float pressureMultiplier = 2.0f;
    float damping = 0.7;
    double PI = 3.14159265358979323846;
    float min_dist = 0.2f;
    float offset = 0.03f;

    glm::vec3 red = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 yellow = glm::vec3(1.0f, 1.0f, 0.0f);
    glm::vec3 green = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 blue = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 bbox;
    float cell_size = 1.0f;
    int n_cells_x = 0;
    int n_cells_y = 0;
    int n_cells_z = 0;
    int highlightIdx = 0;
    int n_cells = n_cells_x * n_cells_y * n_cells_z;


    void renderSphere()
    {
        if (sphereVAO == 0)
        {
            glGenVertexArrays(1, &sphereVAO);

            unsigned int vbo, ebo;
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);

            std::vector<glm::vec3> positions;
            std::vector<glm::vec2> uv;
            std::vector<glm::vec3> normals;
            std::vector<unsigned int> indices;

            const unsigned int X_SEGMENTS = 64;
            const unsigned int Y_SEGMENTS = 64;
            const float PI = 3.14159265359f;
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
                {
                    float xSegment = (float)x / (float)X_SEGMENTS;
                    float ySegment = (float)y / (float)Y_SEGMENTS;
                    float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                    float yPos = std::cos(ySegment * PI);
                    float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                    positions.push_back(glm::vec3(xPos, yPos, zPos));
                    uv.push_back(glm::vec2(xSegment, ySegment));
                    normals.push_back(glm::vec3(xPos, yPos, zPos));
                }
            }

            bool oddRow = false;
            for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
            {
                if (!oddRow) // even rows: y == 0, y == 2; and so on
                {
                    for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
                    {
                        indices.push_back(y * (X_SEGMENTS + 1) + x);
                        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    }
                }
                else
                {
                    for (int x = X_SEGMENTS; x >= 0; --x)
                    {
                        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                        indices.push_back(y * (X_SEGMENTS + 1) + x);
                    }
                }
                oddRow = !oddRow;
            }
            indexCount = static_cast<unsigned int>(indices.size());

            std::vector<float> data;
            for (unsigned int i = 0; i < positions.size(); ++i)
            {
                data.push_back(positions[i].x);
                data.push_back(positions[i].y);
                data.push_back(positions[i].z);
                if (normals.size() > 0)
                {
                    data.push_back(normals[i].x);
                    data.push_back(normals[i].y);
                    data.push_back(normals[i].z);
                }
                if (uv.size() > 0)
                {
                    data.push_back(uv[i].x);
                    data.push_back(uv[i].y);
                }
            }
            glBindVertexArray(sphereVAO);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
            unsigned int stride = (3 + 2 + 3) * sizeof(float);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        }

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    }

    //smoothed particle hydrodynamics
    //first we need a function to calculate a particle's influence at a given distance from it
    float smoothingKernel(float radius, float dst)
    {   
        if (dst >= radius) return 0;
        float volume = PI * pow(radius, 4) / 6;
        return (radius - dst) * (radius - dst) / volume;
        //float value = std::max(0.0f, radius * radius - dst * dst);
        //return dst == 0? 0 : value * value * value / volume;
    }


    float smoothingKernelDerivative(float radius, float dst)
    {
        if (dst >= radius) return 0;
        float scale = 12 / (PI * pow(radius, 4));
        return scale * (dst - radius);
    }
    //now we can calculate density at a given point by summing up particle influences at this point
    
    //#pragma omp parallel for
    float CalcDensity(glm::vec3 p)
    {
        float density = 0;
      //  int i = 0;
        std::vector<std::pair<int, int>> ranges = get3x3CellParticles(p);
        std::pair<int, int> start_end;
        start_end = getParticlesInCell(p);
        for (std::pair<int, int> start_end : ranges)
        {
            for (int i = start_end.first; i < start_end.second; i++)
           // for (const glm::vec3 pos : predictedPositions)
            {   
                int particleIdx = gridmap[i].second;
                glm::vec3 pos = predictedPositions[particleIdx];
           //     glm::vec3 pos = predictedPositions[i];
                float dst = glm::distance(pos, p);
                density += mass * smoothingKernel(smoothingRadius, dst);
             //   i++;
            }

        }

        return density;
    }

    //#pragma omp parallel for
    float DensityToPressure(float density)
    {
        float densityError = density - targetDensity;
        return densityError * pressureMultiplier;
    }
    //#pragma omp parallel for
    glm::vec3 CalcPressureForce(int p)
    {
        glm::vec3 force(0,0,0);

        std::pair<int, int> start_end; 
        start_end = getParticlesInCell(predictedPositions[p]);

        std::vector<std::pair<int, int>> ranges = get3x3CellParticles(predictedPositions[p]);
        for (std::pair<int, int> start_end : ranges)
        {
            //   int i = 0;
            for (int i = start_end.first; i < start_end.second; i++)
             // for (const glm::vec3 pos : predictedPositions)
            {   
           //     glm::vec3 pos = predictedPositions[i];
                int particleIdx = gridmap[i].second;
                glm::vec3 pos = predictedPositions[particleIdx];
                float dst = glm::distance(pos, predictedPositions[p]);
                if (fabsf(dst) > 0) {

                    glm::vec3 dir = (pos - predictedPositions[p]) / dst;
                    float slope = smoothingKernelDerivative(smoothingRadius, dst);
                    float density = densities[i];
                    float pressure = DensityToPressure(densities[i]) + DensityToPressure(densities[p]) / 2;
                    force += pressure * dir * slope * mass / density;
              //      i++;
                }
            
            }

        }
    
        return force;
    }


    /* 
    Currently for each particle we check every other particle against it to compute the density and the pressure force.
    we can avoid that by using a hashgrid such that instead of searching the entire set of particles. we only check particles in a given cell
    to do this we must split our bounding box in to a set of cells. now each particle can map into a cell. then we sort by cell idx such that particles in the same cell are contagious in the vector.
    finally we create a new vector for the starting indices for each cell. now whenever we need to search neihbours of a particle, we check which cell it belongs to and search only particles in that cell. or ofc we can 
    search for other cells as well
    */ 
    //hash grid needs dimensions of bounding box and radius of each cell. radius makes it easier for us to choose radii similiar to the smoothing radii we already have.
    
    //#pragma omp parallel for
    void updateSpatialHashgrid(std::vector<glm::vec3> points, glm::vec3 bbox, float radius)
    {   
        //float cell_size = radius * 2.0f;
        //for now fixed radius
        


        n_cells_x = int(bbox.x / cell_size);
        n_cells_y = int(bbox.y / cell_size);
        n_cells_z = int(bbox.z / cell_size);

        n_cells = n_cells_x * n_cells_y * n_cells_z;
       // std::printf("n cells is %d \n", n_cells);
        //grid map maps a particle's given index to a given cell index
       
         // for each cell we need to know the idx of the first particle in it.
        startingIdxs.assign(n_cells, -1);
        gridmap.clear();
        int idx = 0;
        for (int p = 0; p < points.size(); p++)
        {
            int cell_idx = getCellIdx(points[p]);
            gridmap.emplace_back(cell_idx, p);
        }

        //TO DO parallel sort
        std::sort(gridmap.begin(), gridmap.end());
        //find starting indices
        
        /*int last = -1;
        int cell = 0;
        for (int particle = 0; particle < gridmap.size(); particle++)
        {
            if (gridmap[particle].first != last)
            {
                last = gridmap[particle].first;
                startingIdxs[cell] = particle;
                cell++;
            }
        }*/

        startingIdxs.assign(n_cells, -1);

        int prev = -1;
        for (int i = 0; i < (int)gridmap.size(); ++i) {
            int cid = gridmap[i].first;         // true cell id
            if (cid != prev) {
                startingIdxs[cid] = i;          // write to the real cell id
                prev = cid;
            }
        }

        // now if i know which cell i need, i can find the index of the firt particle in that cell in startingindexes and now i can just open my gridmap and get all particles after it until the next cell
        // 
        //printGridMap(gridmap, points);



    }
    
     // gets all neihbors of particle with index p in grid. assumes all points are sorted so calc_hash_grid must be called first
    std::pair<int, int> getParticlesInCell(glm::vec3 p)
    {
        std::vector<glm::vec3> particles_in_cell;
        int cell_idx = getCellIdx(p);
      //  std::printf("cell idx %d \n", cell_idx);
        int cell_start = startingIdxs[cell_idx];

        if (cell_start == -1) {
            return std::make_pair(-1, -1); // empty cell
        }


        int cell_end = cell_start;
        while (cell_end < gridmap.size() && gridmap[cell_end].first == cell_idx) {
            cell_end++;
        }

        return std::make_pair(cell_start, cell_end);

    }
    std::pair<int, int> getParticlesInCellByIndex(int cid) {
        int start = startingIdxs[cid];
        if (start == -1) return { -1,-1 };
        int end = start;
        while (end < (int)gridmap.size() && gridmap[end].first == cid) ++end;
        return { start, end };
    }

    int getCellIdx(glm::vec3 point)
    {   
      
        int x = (int)glm::floor(point.x + (bbox.x / 2) / cell_size);
        int y = (int)glm::floor(point.y + (bbox.y / 2) / cell_size);
        int z = (int)glm::floor(point.z + (bbox.z / 2) / cell_size);

        // Clamp to valid cell range [0, n-1]
        x = std::max(0, std::min(x, n_cells_x - 1));
        y = std::max(0, std::min(y, n_cells_y - 1));
        z = std::max(0, std::min(z, n_cells_z - 1));


        int cell_idx = x + y * n_cells_x + z * n_cells_x * n_cells_y;
        return cell_idx;

    }
    glm::ivec3 getCellCoords(glm::vec3 point)
    {
        int x = (int)glm::floor(point.x + (bbox.x / 2) / cell_size);
        int y = (int)glm::floor(point.y + (bbox.y / 2) / cell_size);
        int z = (int)glm::floor(point.z + (bbox.z / 2) / cell_size);

        // Clamp to valid cell range [0, n-1]
        x = std::max(0, std::min(x, n_cells_x - 1));
        y = std::max(0, std::min(y, n_cells_y - 1));
        z = std::max(0, std::min(z, n_cells_z - 1));

        //std::cout << "cell coord " << x << " " << y << " " << z << "\n";
        return glm::ivec3(x, y, z);
    }
    std::vector<std::pair<int, int>> get3x3CellParticles(glm::vec3 p)
    {

        std::vector<std::pair<int, int>> ranges;
        glm::ivec3 base = getCellCoords(p);

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    int nx = base.x + dx;
                    int ny = base.y + dy;
                    int nz = base.z + dz;

                    if (nx < 0 || ny < 0 || nz < 0 || nx >= n_cells_x || ny >= n_cells_y || nz >= n_cells_z)
                        continue; // skip out of bounds
                    int cid = nx + ny * n_cells_x + nz * n_cells_x * n_cells_y;
                    ranges.push_back(getParticlesInCellByIndex(cid));

                }
            }
        }
                return ranges;
    }
    void printGridMap(const std::vector<std::pair<int, int>>& gridmap,
        const std::vector<glm::vec3>& points)
    {
        std::cout << "CellIdx -> ParticleIdx (x, y, z)\n";
        for (auto& entry : gridmap) {
            int cell_idx = entry.first;
            int particle_idx = entry.second;
            const glm::vec3& pos = points[particle_idx];
            std::cout << "Cell " << cell_idx
                << " -> Particle " << particle_idx
                << " (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
        }
    }
    void check_particle_collision(int id)
    {
        for (int i = 0; i < positions.size(); i++) {
            if (i == id) continue;

            glm::vec3 delta = positions[id] - positions[i];
            float dist = glm::length(delta);

            if (dist < min_dist && dist > 1e-6f) {
                glm::vec3 normal = delta / dist;

                // --- Velocity along normal
                float v1n = glm::dot(velocities[id], normal);
                float v2n = glm::dot(velocities[i], normal);

                // --- Swap normal components (elastic collision, equal masses)
                float temp = v1n;
                v1n = v2n;
                v2n = temp;

                // --- Recombine into new velocities
                velocities[id] += (v1n - glm::dot(velocities[id], normal)) * normal;
                velocities[i] += (v2n - glm::dot(velocities[i], normal)) * normal;

                // --- Push them apart to avoid overlap
                float penetration = min_dist - dist;
                glm::vec3 correction = 0.5f * penetration * normal;
                positions[id] += correction;
                positions[i] -= correction;
            }
        }
    }
    void check_collision(float x1, float x2, float y1, float y2, float z1, float z2, int i)
    {   

        // checks if particle collides with bounding box, if so invert velocity direction and damp it.
        // X-axis
        if (positions[i].x < x1) {
            positions[i].x = x1 + offset; // clamp inside
            velocities[i].x *= -damping;
        }
        else if (positions[i].x > x2) {
            positions[i].x = x2 - offset;
            velocities[i].x *= -damping;
        }

        // Y-axis
        if (positions[i].y < y1) {
            positions[i].y = y1 + offset;
            velocities[i].y *= -damping;
        }
        else if (positions[i].y > y2) {
            positions[i].y = y2 - offset;
            velocities[i].y *= -damping;
        }

        // Z-axis
        if (positions[i].z < z1) {
            positions[i].z = z1 + offset;
            velocities[i].z *= -damping;
        }
        else if (positions[i].z > z2) {
            positions[i].z = z2 - offset;
            velocities[i].z *= -damping;
        }
    }

    glm::vec3 heatmap(float minVal, float maxVal, float val) {
        // Normalize val to [0,1]
        float t = (val - minVal) / (maxVal - minVal);
        t = std::clamp(t, 0.0f, 1.0f);
        t *= t;
        

        if (t < 0.33f) {
            float localT = t / 0.33f; // [0,1] within [0,0.33]
            return glm::mix(blue, green, localT);
        }
        else if (t < 0.66f) {
            float localT = (t - 0.33f) / (0.33f); // [0,1] within [0.33,0.66]
            return glm::mix(green, yellow, localT);
        }
        else {
            float localT = (t - 0.66f) / (0.34f); // [0,1] within [0.66,1.0]
            return glm::mix(yellow, red, localT);
        }
    }

  
};