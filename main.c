#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irq.h>
#include <libbase/uart.h>
#include <libbase/console.h>
#include <generated/csr.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

#define SYS_CLOCK_HZ 100000000  // Remplacer par la fréquence de votre horloge (100 MHz par exemple)

#include <stdint.h>

uint32_t* back_buffer = (uint32_t*) 0x41400001;



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

// Fonction pour dessiner un carré
static void draw_square(uint32_t* buffer_base, int x, int y, int SQUARE_SIZE, uint32_t color) {
    for (int i = 0; i < SQUARE_SIZE; i++) {
        for (int j = 0; j < SQUARE_SIZE; j++) {
            buffer_base[(y + i) * SCREEN_WIDTH + (x + j)] = color;
        }
    }
}

// Fonction pour effacer l'écran
static void clear_screen(uint32_t* buffer_base) {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        buffer_base[i] = 0; // Noir
    }
}

// Fonction pour copier le buffer caché vers l'affichage (échange de buffers)
static void swap_buffers(uint32_t* framebuffer_base) {
    uint32_t* temp = framebuffer_base;
    framebuffer_base = back_buffer;
    back_buffer = temp;
}

// Fonction de délai en millisecondes
static void delay_ms(uint32_t ms) {
    // Calculer la valeur à charger dans le timer
    uint32_t load_value = SYS_CLOCK_HZ/1000*ms;

    timer0_en_write(0);
    timer0_reload_write(0);          // Définir la valeur de rechargement (pour un timer continu)
    timer0_load_write(load_value);            // Charger la valeur dans le timer   
    timer0_en_write(1);                       // Activer le timer
    timer0_update_value_write(1);
    // Attendre que le timer expire
    while(timer0_value_read())
    {
    	timer0_update_value_write(1);
    }
    ;  // Vérifier si le timer a expiré (bit 0 de la valeur)

    // Désactiver le timer après utilisation
    timer0_en_write(0);
}

// Fonction de délai en millisecondes
static void delay_us(uint32_t us) {
    // Calculer la valeur à charger dans le timer
    uint32_t load_value = SYS_CLOCK_HZ/1000000*us;

    timer0_en_write(0);
    timer0_reload_write(0);          // Définir la valeur de rechargement (pour un timer continu)
    timer0_load_write(load_value);            // Charger la valeur dans le timer   
    timer0_en_write(1);                       // Activer le timer
    timer0_update_value_write(1);
    // Attendre que le timer expire
    while(timer0_value_read())
    {
    	timer0_update_value_write(1);
    }
    ;  // Vérifier si le timer a expiré (bit 0 de la valeur)

    // Désactiver le timer après utilisation
    timer0_en_write(0);
}


#ifdef CSR_VIDEO_FRAMEBUFFER_BASE
static void vga_test(void) {
    // Configurer les paramètres de timing pour un écran VGA de 800x600 à 60Hz
    /*video_framebuffer_vtg_hres_write(SCREEN_WIDTH);  
    video_framebuffer_vtg_vres_write(SCREEN_HEIGHT);  
    video_framebuffer_vtg_hsync_start_write(856);  // Horizontal sync start
	video_framebuffer_vtg_hsync_end_write(976);    // Horizontal sync end
	video_framebuffer_vtg_hscan_write(1056);       // Total pixels per line
	video_framebuffer_vtg_vsync_start_write(601);  // Vertical sync start
	video_framebuffer_vtg_vsync_end_write(604);    // Vertical sync end
	video_framebuffer_vtg_vscan_write(625);        // Total lines per frame   */    

    // Activer le VTG pour commencer à générer les signaux de synchronisation
    //video_framebuffer_vtg_enable_write(1);  

    // Obtenir la base du framebuffer
    uint32_t* framebuffer = (uint32_t*)video_framebuffer_dma_base_read();

    back_buffer = (uint32_t*) malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));

	if (!back_buffer) {
	    puts("Erreur d'allocation mémoire !");
	}

    int size = 20;

    int y = SCREEN_HEIGHT - size;

    while (y >= 0) {


        clear_screen(back_buffer);
        draw_square(back_buffer, 390, y, size, 0xF0F00F); // Carré rouge
        swap_buffers(framebuffer);
        delay_ms(5);
        y -= 5;
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
