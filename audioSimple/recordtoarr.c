#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

int main(int argc, char** argv) {
    
    assert(argc == 4);
    FILE* f = fopen(argv[1], "rb");
    int bytes = atoi(argv[2]);
    int skip = atoi(argv[3]);
    void* block = malloc(bytes);
    if (f) {
        fseek(f, skip, SEEK_SET);
        fread(block, bytes, 1, f);

        printf("u8 %s[%d] = {\n", argv[1], bytes);
        for (size_t i = 0; i < bytes; i++)
        {
            printf("0x%X, ", ((uint8_t*)block)[i] );
            if (i % 10 == 9) printf("\n"); 
        }
        printf("\n};\n");
    }
    
    return 0;
}