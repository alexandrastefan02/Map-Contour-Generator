// Author: APD team, except where source was noted

#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
//#include "pthread_barrier_mac.h"

#define CONTOUR_CONFIG_COUNT    16
#define FILENAME_MAX_SIZE       50
#define STEP                    8
#define SIGMA                   200
#define RESCALE_X               2048
#define RESCALE_Y               2048

#define CLAMP(v, min, max) if(v < min) { v = min; } else if(v > max) { v = max; }

typedef struct {
    int P;
    pthread_barrier_t *barrier;
    ppm_image *img;
    ppm_image **countours;
    int thread_id;
    unsigned char **grid;
    ppm_image *scaled;
} thread_args;

void free_resources(ppm_image *image, ppm_image **contour_map, unsigned char **grid, int step_x) {
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(contour_map[i]->data);
        free(contour_map[i]);
    }
    free(contour_map);

    for (int i = 0; i <= image->x / step_x; i++) {
        free(grid[i]);
    }
    free(grid);

    free(image->data);
    free(image);
}

ppm_image **init_contour_map() {
    ppm_image **map = (ppm_image **) malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!map) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        map[i] = read_ppm(filename);
    }

    return map;
}

// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.
void update_image(ppm_image *image, ppm_image *contour, int x, int y) {
    for (int i = 0; i < contour->x; i++) {
        for (int j = 0; j < contour->y; j++) {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
}


void *f(void *arg) {
    thread_args *t = (thread_args *) arg;
    uint8_t sample[3];

    if (t->img->x > RESCALE_X || t->img->y > RESCALE_Y) {
    /*daca imaginea nu este in dimensiunile necesare, aplic algoritmul de rescale
     * daca are dimensiunile bune, atunci pur si simplu folosesc imaginea citita in main*/
        int start = t->thread_id * (double) (t->scaled->x) / t->P;
        int end = (t->thread_id + 1) * (double) (t->scaled->x) / t->P;
        if (end > t->scaled->x)
            end = t->scaled->x;
        //paralelizez iteratia exterioara
        for (int i = start; i < end; i++) {
            for (int j = 0; j < t->scaled->y; j++) {
                float u = (float) i / (float) (t->scaled->x - 1);
                float v = (float) j / (float) (t->scaled->y - 1);
                sample_bicubic(t->img, u, v, sample);

                t->scaled->data[i * t->scaled->y + j].red = sample[0];
                t->scaled->data[i * t->scaled->y + j].green = sample[1];
                t->scaled->data[i * t->scaled->y + j].blue = sample[2];
            }
        }

    }
    //sfarsitul rescale-ului
    pthread_barrier_wait(t->barrier);
    int step_x = STEP;
    int step_y = STEP;
    int p = (t->scaled->x) / step_x;
    int q = (t->scaled->y) / step_y;
    int start2 = (t->thread_id) * (double) p / (t->P);
    int end2 = (t->thread_id + 1) * (double) p / (t->P);
    if (end2 > p) {
        end2 = p;
    }


    int sigma = SIGMA;
    //paralelizez iteratia exterioara
    for (int i = start2; i < end2; i++) {
        for (int j = 0; j < q; j++) {
            ppm_pixel curr_pixel = t->scaled->data[i * step_x * t->scaled->y + j * step_y];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > sigma) {
                t->grid[i][j] = 0;
            } else {
                t->grid[i][j] = 1;
            }
        }
    }
    t->grid[p][q] = 0; //end2

    //paralelizez
    for (int i = start2; i < end2; i++) {
        ppm_pixel curr_pixel = t->scaled->data[i * step_x * t->scaled->y + t->scaled->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            t->grid[i][q] = 0;
        } else {
            t->grid[i][q] = 1;
        }
    }

    for (int j = 0; j < q; j++) {
        ppm_pixel curr_pixel = t->scaled->data[(t->scaled->x - 1) * t->scaled->y + j * step_y];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            t->grid[p][j] = 0;
        } else {
            t->grid[p][j] = 1;
        }
    }
    //sfarsitul sampling-ului
    pthread_barrier_wait(t->barrier);
    //lastly, march the squares
    //paralelizez iteratia exterioara
    for (int i = start2; i < end2; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k =
                    8 * t->grid[i][j] + 4 * t->grid[i][j + 1] + 2 * t->grid[i + 1][j + 1] + 1 * t->grid[i + 1][j];
            update_image(t->scaled, t->countours[k], i * step_x, j * step_y);
        }
    }
    pthread_exit(NULL);

}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }
    ppm_image *image = read_ppm(argv[1]);
    ppm_image *scaled_image;
    int step_x = STEP;
    int step_y = STEP;
    int P = atoi(argv[3]);
    int r;
    pthread_barrier_t barrier;
    pthread_t threads[P];
    thread_args *tags = (thread_args *) malloc(P * sizeof(thread_args));
    if (!tags) {
        fprintf(stderr, "Unable to allocate memory for args_array\n");
        exit(1);
    }
    int p = RESCALE_X / step_x;
    int q = RESCALE_Y / step_y;
    // 0. Initialize contour map
    ppm_image **contour_map = init_contour_map();

    //initializez grid
    unsigned char **grid = (unsigned char **) malloc((p + 1) * sizeof(unsigned char *));
    if (!grid) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    for (int i = 0; i <= p; i++) {
        grid[i] = (unsigned char *) malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }

    if (image->x <= RESCALE_X && image->y <= RESCALE_Y) {
        scaled_image = image;
        /*fac scaled_image sa pointeze la imaginea citita in cazul in care are dimensiunile bune
         * pentru a o folosi mai departe in functia f */
    } else {
        //doar aloc memorie pentru scaled_image daca trebuie sa aplic algoritmul de rescale
        scaled_image = (ppm_image *) malloc(sizeof(ppm_image));
        if (!scaled_image) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
        scaled_image->x = RESCALE_X;
        scaled_image->y = RESCALE_Y;
        scaled_image->data = (ppm_pixel *) malloc(scaled_image->x * scaled_image->y * sizeof(ppm_pixel));
    }
    //initializez bariera
    pthread_barrier_init(&barrier, NULL, P);
    for (int i = 0; i < P; i++) {
        tags[i].countours = contour_map;
        tags[i].img = image;
        tags[i].thread_id = i;
        tags[i].P = P;
        tags[i].barrier = &barrier;
        tags[i].grid = grid;
        tags[i].scaled = scaled_image;
    }
    //atribui structurii de args elementele dorite
    for (int i = 0; i < P; i++) {
        r = pthread_create(&threads[i], NULL, f, &tags[i]);
        if (r) {
            printf("Eroare la crearea thread-ului %d\n", i);
            exit(-1);
        }
    }
    for (int i = 0; i < P; i++) {
        r = pthread_join(threads[i], NULL);

        if (r) {

            printf("Eroare la asteptarea thread-ului %d\n", i);
            exit(-1);
        }
    }
    // 4. Write output
    write_ppm(scaled_image, argv[2]);
    pthread_barrier_destroy(&barrier);

    free_resources(scaled_image, contour_map, grid, step_x);
    free(tags);
    return 0;
}
