

# PROJECT NGINE

The long term aspiration of this project is to become a small 3D engine that supports features I consider interesting Like importing models, basic mesh editing, particle simulation, Rigid body physics, rasterization and raytracing and perhaps eventually gaussian splatting. A specific aspect I intend to focus on is performing as much computation as possible on the GPU. As one might expect from the lack of structure of this project, the intention is not to produce a functionining product but purely to satisfy my curiosity.

## Features

![Alt text](media/teapot.png "rasterizer")

- A small ad-hoc matrix/vector library that performs all nessicary matrix/vector math.  
- OBJ model loading  
- rasterization pipeline: Model vertices are loaded in world space then transformed by the model, view, projection matrices respectively  
- a single light source shades triangles based on their normal orientation.  
- texture sampler  

## Smoothed particle hydrodynamics fluid simulation

![Alt text](media/psimsnap.png "simulation")

In this project we created a fluid simulation using smoothed particle hydrodynamics.  
based on this paper:

For a few particles the task, is fairly trivial as it can be computed on the cpu. However as soon as soon as we require more than a few hundred particles, the CPU struggles to maintain 60FPS as the pressure Force on each particle calculation requires checking every other particle.  
Therefore, we implement a spatial hashgrid to optimize neighbor search. The goal of the spatial hashgrid, is to allow us to only search neihboring particles for each particle, by partitioning particles into cells. Each cell is given a hash value. Particles are stored in a buffer. in a seperate buffer we store the keys to each particle, and we sort them by that key such that all particles in the same cell are stored consequitively. Finally a 3rd Buffer stores the starting indices of each cell which allows us to retrieve them immediately.

```cpp
particle Buffer: [0,1,3,4,5,6,7,8,9,10,11]

spatialKeys Buffer: [0,0,0,1,1,2,2,2,2,3,3]  

                      ^      ^   ^       ^  
                     cell 0 Cell 1      Cell 3


spatialOffsets Buffer: [0,3,5,9]
                       C0,C1,C2,C3 starting indices
