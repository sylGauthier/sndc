#ifndef RAND_TOOLS_H
#define RAND_TOOLS_H

#define STATE_VECTOR_LENGTH 624
#define STATE_VECTOR_M      397
#define MT_RAND_MAX         0xfffffffful

struct MTRand {
    unsigned long mt[STATE_VECTOR_LENGTH];
    int index;
};

void mt_rand_init(struct MTRand* rand, unsigned long seed);
unsigned long mt_rand(struct MTRand* rand);

float runif(struct MTRand* rng, float min, float max);
float rnorm(struct MTRand* rng, float mu, float sigma);

#endif
