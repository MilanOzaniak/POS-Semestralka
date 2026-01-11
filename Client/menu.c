#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


// citanie vstupu
static int read_int(const char *prompt, int def) {
    char buf[64];
    printf("%s", prompt);
    if (!fgets(buf, sizeof(buf), stdin)) return def;
    return atoi(buf);
}

// vycistenie terminalu
static void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

int menu_run(menu_result_t *out, uint8_t is_host) {
    create_game_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    
    // zakladne nastavenia
    cfg.w = WORLD_W;
    cfg.h = WORLD_H;
    cfg.map_type = MAP_EMPTY;
    cfg.obstacles = 0;
    cfg.mode = MODE_STANDARD;
    cfg.time_limit = 120;
    cfg.obstacle_density = 18;

    // menu sa zobrazuje iba hostovi
    if (!is_host) {
        out->cfg = cfg;
        return 1;
    }

    
    for (;;) {
        printf("\n=== New Game Setup (host) ===\n\n");
        printf("1) Width: %u\n", cfg.w);
        printf("2) Height: %u\n", cfg.h);
        printf("3) Obstacles: %s\n", cfg.obstacles ? "ON" : "OFF");
        printf("4) Mode: %s\n", cfg.mode == MODE_TIMED ? "TIMED" : "STANDARD");
        printf("5) Start game\n");
        printf("0) Quit\n\n");

        int choice = read_int("Choose option: ", -1);

        switch (choice) {
            case 1:
                cfg.w = read_int("Enter width: ", cfg.w);
                clear_screen();
                break;
            case 2:
                cfg.h = read_int("Enter height: ", cfg.h);
                clear_screen();
                break;
            case 3:
                cfg.obstacles = !cfg.obstacles;
                if (cfg.obstacles) {
                    cfg.obstacle_density = read_int("Obstacle count (0-60): ", cfg.obstacle_density);
                }
                clear_screen();
                break;
            case 4:
                cfg.mode = (cfg.mode == MODE_STANDARD) ? MODE_TIMED : MODE_STANDARD;
                if (cfg.mode == MODE_TIMED) {
                    cfg.time_limit = read_int("Time limit (sec): ", cfg.time_limit);
                }
                clear_screen();
                break;
            case 5:
                out->cfg = cfg;
                return 1;
            case 0:
                return 0;
            default:
                printf("Invalid choice\n");
        }
    }
}

