For a more active and recent version of this project on WEBGPU:
[SimuWebGPU]([https://link-url-here.org](https://github.com/Ahmed5720/SimuWebGPU))

# PROJECT NGINE
### 

The long term aspiration of this project is to become a small 3D engine that supports features I consider interesting Like importing models, basic mesh editing, particle simulation, Rigid body physics, rasterization and raytracing and perhaps eventually gaussian splatting. A specific aspect I intend to focus on is performing as much computation as possible on the GPU. As one might expect from the lack of structure of this project, the intention is not to produce a functionining product but purely to satisfy my curiosity. 

###
For now, this project is in OpenGL/C++ and shaders are written in glsl, because those are more than enough for a basic, simple implementation of all the features I have in mind. though I realize OpenGL is getting considerably old now, and I am considering switching to a more modern standard. WebGPU has been increasingly appealing for me.



## Smoothed particle hydrodynamics fluid simulation
![Alt text](media/psimsnap.png "simulation")
In this project we created a fluid simulation using smoothed particle hydrodynamics. 
based on this paper:

For a few particles the task, is fairly trivial as it can be computed on the cpu. However as soon as soon as we require more than a few hundred particles, the CPU struggles to maintain 60FPS as the pressure Force on each particle calculation requires checking every other particle.
Therefore, we implement a spatial hashgrid to optimize neighbor search. The goal of the spatial hashgrid, is to allow us to only search neihboring particles for each particle, by partitioning particles into cells. Each cell is given a hash value. Particles are stored in a buffer. in a seperate buffer we store the keys to each particle, and we sort them by that key such that all particles in the same cell are stored consequitively. Finally a 3rd Buffer stores the starting indices of each cell which allows us to retrieve them immediately.


`cpp
particle Buffer: [0,1,3,4,5,6,7,8,9,10,11]

spatialKeys Buffer: [0,0,0,1,1,2,2,2,2,3,3]  

                      ^      ^   ^       ^  
                     cell 0 Cell 1      Cell 3


spatialOffsets Buffer: [0,3,5,9]
                       C0,C1,C2,C3 starting indices
`

This structures allows us to efficiently index into any particle in almost O(1) time (we still need to iterate over the 3*3 grid sorrounding the particle but that's a significant reduction over looping through all particles. Intuitively this implementation lends itself to a highly parallilized GPU implementation.

![Alt text](media/spatialgrid.png "spatial grid" with a hashgrid, we only search 3*3 grid sorrounding a particle)
