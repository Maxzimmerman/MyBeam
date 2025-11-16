#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    FILE *beam_file = fopen("../../output_files/Elixir.FirstModule.beam", "rb");
    unsigned char buffer[20];  // RAW bytes, not ints!

    if (!beam_file) {
        perror("File open failed");
        return 1;
    }

    size_t read = fread(buffer, 1, sizeof(buffer), beam_file);
    if (read != sizeof(buffer)) {
        printf("Could not read 20 bytes, only got %zu\n", read);
        return 1;
    }

    // Print bytes in hex
    for (int i = 0; i < 20; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

    fclose(beam_file);
    return 0;
}
