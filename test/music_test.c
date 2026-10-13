#include "../source/music.h"
#include <stdio.h>
#include <stdlib.h>
int main(void){ size_t m=64*(MUSIC_RATE/4); int16_t*b=malloc(m*2); size_t n=synth_music(b,m);
 int pk=0; for(size_t i=0;i<n;i++){int v=abs(b[i]); if(v>pk)pk=v;} printf("%zu samples (%.1fs) peak %d\n",n,(double)n/MUSIC_RATE,pk); return 0;}
