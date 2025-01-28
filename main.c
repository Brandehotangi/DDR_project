#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irq.h>
#include <libbase/uart.h>
#include <libbase/console.h>
#include <generated/csr.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

#define MAX_COUNT 300 // 5 secondes à 60 Hz
#define TIMER_LOAD_VALUE (100000000 / 60) // Valeur de chargement pour 60 Hz avec 100 MHz

#include <stdint.h>
#include <time.h>

// Fonction pour initialiser le timer
static void timer_init(void) {
    timer0_load_write(TIMER_LOAD_VALUE); // Charger la valeur initiale
    timer0_reload_write(TIMER_LOAD_VALUE); // Définir la valeur de rechargement
    timer0_en_write(1); // Activer le timer
}

// Fonction pour vérifier si le timer a atteint zéro
static int timer_has_expired(void) {
    return timer0_ev_pending_read() & (1 << CSR_TIMER0_EV_PENDING_ZERO_OFFSET);
}

/*-----------------------------------------------------------------------*/
/* Uart                                                                  */
/*-----------------------------------------------------------------------*/

static char *readstr(void)
{
	char c[2];
	static char s[64];
	static int ptr = 0;

	if(readchar_nonblock()) {
		c[0] = getchar();
		c[1] = 0;
		switch(c[0]) {
			case 0x7f:
			case 0x08:
				if(ptr > 0) {
					ptr--;
					fputs("\x08 \x08", stdout);
				}
				break;
			case 0x07:
				break;
			case '\r':
			case '\n':
				s[ptr] = 0x00;
				fputs("\n", stdout);
				ptr = 0;
				return s;
			default:
				if(ptr >= (sizeof(s) - 1))
					break;
				fputs(c, stdout);
				s[ptr] = c[0];
				ptr++;
				break;
		}
	}

	return NULL;
}

static char *get_token(char **str)
{
	char *c, *d;

	c = (char *)strchr(*str, ' ');
	if(c == NULL) {
		d = *str;
		*str = *str+strlen(*str);
		return d;
	}
	*c = 0;
	d = *str;
	*str = c+1;
	return d;
}

static void prompt(void)
{
	printf("\e[92;1mdance-dance-revolution-app\e[0m> ");
}

/*-----------------------------------------------------------------------*/
/* Help                                                                  */
/*-----------------------------------------------------------------------*/

static void help(void)
{
	puts("\nDance Dance Revolution App built on "__DATE__" "__TIME__"\n");
	puts("Available commands:");
	puts("help               - Show this command");
	puts("reboot             - Reboot CPU");
#ifdef CSR_LEDS_BASE
	puts("led                - Led demo");
#endif
	puts("donut              - Spinning Donut demo");
	puts("helloc             - Hello C");
#ifdef WITH_CXX
	puts("hellocpp           - Hello C++");
#endif
#ifdef CSR_VIDEO_FRAMEBUFFER_BASE
	puts("vga_test           - Test VGA");
#endif
}

/*-----------------------------------------------------------------------*/
/* Welcome                                                               */
/*-----------------------------------------------------------------------*/

static void welcome(void)
{
    puts("\033[1;35m" // Magenta + Gras
        "    ___                          ___                         __                 _       _   _             \n"
        "   /   \\__ _ _ __   ___ ___     /   \\__ _ _ __   ___ ___    /__\\ _____   _____ | |_   _| |_(_) ___  _ __  \n"
        "  / /\\ / _` | '_ \\ / __/ _ \\   / /\\ / _` | '_ \\ / __/ _ \\  / \\/// _ \\ \\ / / _ \\| | | | | __| |/ _ \\| '_ \\ \n"
        " / /_// (_| | | | | (_|  __/  / /_// (_| | | | | (_|  __/ / _  \\  __/\\ V / (_) | | |_| | |_| | (_) | | | |\n"
        "/___,' \\__,_|_| |_|\\___\\___| /___,' \\__,_|_| |_|\\___\\___| \\/ \\_/\\___| \\_/ \\___/|_|\\__,_|\\__|_|\\___/|_| |_|\n"
        "\033[0m" // Réinitialisation après l'ASCII art
        "\n"
        "----------------------------------------------------------------------------------------------------------\n"
        "\033[1m" // Gras uniquement
        "                         Developed by Antoine Madrelle and Tangi Brandeho\n"
        "\033[0m" // Désactive le gras
        "----------------------------------------------------------------------------------------------------------\n"
    );
}

/*-----------------------------------------------------------------------*/
/* Commands                                                              */
/*-----------------------------------------------------------------------*/

static void reboot_cmd(void)
{
	ctrl_reset_write(1);
}

#ifdef CSR_LEDS_BASE
static void led_cmd(void)
{
	int i;
	printf("Led demo...\n");

	printf("Counter mode...\n");
	for(i=0; i<32; i++) {
		leds_out_write(i);
		busy_wait(100);
	}

	printf("Shift mode...\n");
	for(i=0; i<4; i++) {
		leds_out_write(1<<i);
		busy_wait(200);
	}
	for(i=0; i<4; i++) {
		leds_out_write(1<<(3-i));
		busy_wait(200);
	}

	printf("Dance mode...\n");
	for(i=0; i<4; i++) {
		leds_out_write(0x55);
		busy_wait(200);
		leds_out_write(0xaa);
		busy_wait(200);
	}
}
#endif

// Fonction pour dessiner une flèche bleue
static void draw_arrow(uint32_t* framebuffer, int x, int y) {
    // Définir la couleur bleue
    uint32_t blue_color = 0x0000FF;

    // Dessiner une flèche simple
    // La flèche est représentée par un triangle
    // Exemple de flèche de 10 pixels de large et 20 pixels de haut
    for (int i = 0; i < 20; i++) {
        for (int j = -5; j <= 5; j++) {
            if (x + j >= 0 && x + j < SCREEN_WIDTH && y + i >= 0 && y + i < SCREEN_HEIGHT) {
                framebuffer[(y + i) * SCREEN_WIDTH + (x + j)] = blue_color; // Dessiner la flèche
            }
        }
        // Réduire la largeur de la flèche à mesure qu'elle monte
        if (i < 10) {
            for (int j = -1; j <= 1; j++) {
                if (x + j >= 0 && x + j < SCREEN_WIDTH && y + 20 >= 0 && y + 20 < SCREEN_HEIGHT) {
                    framebuffer[(y + 20) * SCREEN_WIDTH + (x + j)] = blue_color; // Dessiner la base de la flèche
                }
            }
        }
    }
}

#ifdef CSR_VIDEO_FRAMEBUFFER_BASE
static void vga_test(void) {
    // Configurer les paramètres de timing pour un écran VGA de 800x600 à 60Hz
    video_framebuffer_vtg_hres_write(SCREEN_WIDTH);  
    video_framebuffer_vtg_vres_write(SCREEN_HEIGHT);  
    video_framebuffer_vtg_hsync_start_write(856);  
    video_framebuffer_vtg_hsync_end_write(976);    
    video_framebuffer_vtg_hscan_write(1056);       
    video_framebuffer_vtg_vsync_start_write(601);  
    video_framebuffer_vtg_vsync_end_write(604);    
    video_framebuffer_vtg_vscan_write(625);        

    // Activer le VTG pour commencer à générer les signaux de synchronisation
    video_framebuffer_vtg_enable_write(1);  

    // Obtenir la base du framebuffer
    uint32_t* framebuffer = (uint32_t*)video_framebuffer_dma_base_read();

    // Position initiale de la flèche
    int arrow_x = SCREEN_WIDTH / 2; // Centré horizontalement
    int arrow_y = 0; // Commence en haut de l'écran

    // Initialiser le timer
    timer_init();

    uint32_t count = 0; // Compteur pour les expirations du timer

    while (1) {
        // Effacer l'écran (remplir de noir)
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                framebuffer[y * SCREEN_WIDTH + x] = 0x000000; // Fond noir
            }
        }

        // Dessiner la flèche à la position actuelle
        draw_arrow(framebuffer, arrow_x, arrow_y);

        // Mettre à jour la position de la flèche
        arrow_y += 1; // Descendre d'un pixel

        // Réinitialiser la position si la flèche sort de l'écran
        if (arrow_y > SCREEN_HEIGHT) {
            arrow_y = 0; // Remonter en haut de l'écran
        }

        // Vérifier si le timer a expiré
        if (timer_has_expired()) {
            count++; // Incrémenter le compteur

            // Réinitialiser le timer
            timer0_ev_pending_write(1); // Effacer le drapeau d'expiration
            timer0_load_write(TIMER_LOAD_VALUE); // Recharger la valeur

            // Vérifier si 5 secondes se sont écoulées
            if (count >= MAX_COUNT) {
                break; // Sortir de la boucle après 5 secondes
            }
        }
    }

    // Optionnel : Ajoutez du code ici pour gérer la fin du programme, comme éteindre l'affichage ou libérer des ressources.
}
#endif

extern void donut(void);

static void donut_cmd(void)
{
	printf("Donut demo...\n");
	donut();
}

extern void helloc(void);

static void helloc_cmd(void)
{
	printf("Hello C demo...\n");
	helloc();
}

#ifdef WITH_CXX
extern void hellocpp(void);

static void hellocpp_cmd(void)
{
	printf("Hello C++ demo...\n");
	hellocpp();
}
#endif

/*-----------------------------------------------------------------------*/
/* Console service / Main                                                */
/*-----------------------------------------------------------------------*/

static void console_service(void)
{
	char *str;
	char *token;

	str = readstr();
	if(str == NULL) return;
	token = get_token(&str);
	if(strcmp(token, "help") == 0)
		help();
	else if(strcmp(token, "reboot") == 0)
		reboot_cmd();
#ifdef CSR_LEDS_BASE
	else if(strcmp(token, "led") == 0)
		led_cmd();
#endif
	else if(strcmp(token, "donut") == 0)
		donut_cmd();
	else if(strcmp(token, "helloc") == 0)
		helloc_cmd();
#ifdef WITH_CXX
	else if(strcmp(token, "hellocpp") == 0)
		hellocpp_cmd();
#endif
#ifdef CSR_VIDEO_FRAMEBUFFER_BASE
	else if(strcmp(token, "vga_test") == 0)
		vga_test();
#endif
	prompt();
}

int main(void)
{
#ifdef CONFIG_CPU_HAS_INTERRUPT
	irq_setmask(0);
	irq_setie(1);
#endif
	uart_init();

	welcome();
	help();
	prompt();

	while(1) {
		console_service();
	}

	return 0;
}
