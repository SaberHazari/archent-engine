#include "archent_rng.h"

void rng_alias_build(const f32 *weights, u32 n, f32 *prob, 
    u32 *alias, u32 *small_stack, u32 *large_stack, 
    RngAliasTable *out) {
    out->n = n;
    out->prob = prob;
    out->alias = alias;
    if(n == 0) { return; }
    
    f64 sum = 0.0;
    for(u32 i = 0; i < n; ++i) {
        sum += (f64)weights[i];
    }
    if(sum <= 0.0) {
        for(u32 i = 0; i < n; ++i) {
            prob[i] = 1.0f;
            alias[i] = i;
        }
        return;
    }
    
    f64 scale = (f64)n / sum;
    u32 small_n = 0, large_n = 0;
    for(u32 i = 0; i < n; ++i) {
        f64 p = (f64)weights[i] * scale;
        prob[i] = (f32)p;
        if(p < 1.0) { small_stack[small_n++] = i; }
        else { large_stack[large_n++] = i; }
    }
    
    while(small_n > 0 && large_n > 0) {
        u32 sml = small_stack[--small_n];
        u32 lrg = large_stack[--large_n];
        alias[sml] = lrg;
        
        prob[lrg] -= (1.0f - prob[sml]);
        if(prob[lrg] < 1.0f) { small_stack[small_n++] = lrg; }
        else { large_stack[large_n++] = lrg; }
    }
    
    while(large_n > 0) {
        u32 i = large_stack[--large_n];
        prob[i] = 1.0f;
        alias[i] = i;
    }
    while(small_n > 0) {
        u32 i = small_stack[--small_n];
        prob[i] = 1.0f;
        alias[i] = i;
    }
}

void rng_shuffle(u64 seed, u64 a, u64 b, 
    RngSalt salt, u32 *arr, u32 n) {
    RngStream stream = rng_stream_begin(seed, a, b, salt);
    for(u32 i = n; i-- > 1;) {
        u32 j = rng_range_zero_u32(rng_stream_next(&stream), i + 1);
        u32 temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}
