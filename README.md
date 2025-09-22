In this project we created a fluid simulation using smoothed particle hydrodynamics. 
based on this paper:

For a few particles the task, is fairly trivial as it can be computed on the cpu. However as soon as soon as we require more than a few hundred particles, the CPU struggles to maintain 60FPS as the pressure Force on each particle calculation requires checking every other particle.
Therefore, we implement a spatial hashgrid to optimize neihbor search. The goal of the spatial hashgrid, is to allow us to only search neihboring particles for each particle, by partitioning particles into cells. Each cell is given a hash value. Particles are stored in a buffer. in a seperate buffer we store the keys to each particle, and we sort them by that key such that all particles in the same cell are stored consequitively. Finally a 3rd Buffer stores the starting indices of each cell which allows us to retrieve them immediately.

particle Buffer: [0,1,3,4,5,6,7,8,9,10,11]

spatialKeys Buffer: [0,0,0,1,1,2,2,2,2,3,3]
                    ^      ^   ^       ^  
                    cell 0 Cell 1      Cell 3

spatialOffsets Buffer: [0,3,5,9]
                       C0,C1,C2,C3 starting indices


This structures allows us to efficiently indix into any particle. furthermore this implementation is perfectly suitable to be used in a GPU kernel.

![Alt text](resources/spatialgrid.png "spatial grid")
