#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h> 

#define SAMPLE_RATE 44100
#define TAU 6.283185307179586f
#define BUFFER_SIZE 256

// comando d reproduccion
char audioCommand[1024];
// array de frecuencias
float chromaticScale[256] = {0.0f};
// terminal de backup para q funque cuando volvamos del raw mode
struct termios backupTerminal;

// raw mode
void initRawMode(){
    struct termios rawMode;
    // respaldamos configuracion de terminal
    tcgetattr(STDIN_FILENO, &backupTerminal);

    // clonamos para modificar la copia
    rawMode = backupTerminal;
    
    // apagamos el enter (ICANON) y el dibujado (ECHO)
    rawMode.c_lflag &= ~(ICANON | ECHO);

    // lectura no bloqueante
    rawMode.c_cc[VMIN] = 0;
    rawMode.c_cc[VTIME] = 0;

    // inyectamos cambios
    tcsetattr(STDIN_FILENO, TCSANOW, &rawMode);
}

// devolver terminal a la normalidad
void returnCookedMode(){
    tcsetattr(STDIN_FILENO, TCSANOW, &backupTerminal);
}

int main(){
    // frecuencia base
    float baseHz = 0.0f;
    // la fase (posicion de la onda)
    float phase = 0.0f;

    signal(SIGPIPE, SIG_IGN);

    sprintf(audioCommand, "aplay -f U8 -r %d -c 1 --buffer-size=512 --disable-resample - 2>/dev/null", SAMPLE_RATE);

    // pipe de aplay
    FILE * pipeOut = popen(audioCommand, "w");
    if (pipeOut == NULL){
        printf("aplay not found. exit...");
        return 1;
    }

    // reducimos el tamaño del pipe a 1024 para menos delay
    int fd = fileno(pipeOut);
    fcntl(fd, F_SETPIPE_SZ, 1024);

    // iniciamos raw mode 
    initRawMode();

    // mapeo de teclas (awsedftgyhjk)
    chromaticScale['a'] = 261.63f; // do
    chromaticScale['w'] = 277.18f; // do#
    chromaticScale['s'] = 293.66f; // re
    chromaticScale['e'] = 311.13f; // re#
    chromaticScale['d'] = 329.63f; // mi
    chromaticScale['f'] = 349.23f; // fa
    chromaticScale['t'] = 369.99f; // fa#
    chromaticScale['g'] = 392.00f; // sol
    chromaticScale['y'] = 415.30f; // sol#
    chromaticScale['h'] = 440.00f; // la 
    chromaticScale['u'] = 466.16f; // la#
    chromaticScale['j'] = 493.88f; // si
    chromaticScale['k'] = 523.26f; // do

    printf("\033[2J\033[H"); // secuencia ansi para limpiar terminal y resetear cursor
    printf("    CliSynth: Interactive terminal synth      \n");
    printf("Top letter: Note | Under letter: Execution key\n\n");
    printf("┌──┬──┬┬──┬─┬──┬──┬┬──┬┬──┬─┬───┐\n");
    printf("│  │C#││D#│ │  │F#││G#││A#│ │   │\n");
    printf("│  │W ││E │ │  │T ││Y ││U │ │   │\n");
    printf("│  └┬─┘└┬─┘ │  └┬─┘└┬─┘└┬─┘ │   │\n");
    printf("│C  │D  │E  │F  │G  │A  │B  │C  │\n");
    printf("│A  │S  │D  │F  │G  │H  │J  │K  │\n");
    printf("└───┴───┴───┴───┴───┴───┴───┴───┘\n\n");
    printf("Press 'q' to quit\n\n");

    // indice de ciclos para apagar el sonido
    int activeDecay = 0;

    unsigned char buffer[BUFFER_SIZE];

    // bucle d reproduccion
    while(1){
        char pressedKey = 0;
        // read intenta buscar un byte del teclado 
        int isRead = read(STDIN_FILENO, &pressedKey, 1);

        // solo procesamos el teclado si realmente el sensor detecto un byte valido (> 0)
        if (isRead > 0) {
            // si es q salimos
            if (pressedKey == 'q') {
                printf("\nThanks for using! Exit...\n");
                break; 
            }
            
            // definimos a baseHz como el valor del indice d chromaticScale correspondiente al codigo ascii de la tecla
            baseHz = chromaticScale[(unsigned char)pressedKey];
            
            // si es mayor a 0 ponemos a activeDecay en 40
            if (baseHz > 0.0f) {
                activeDecay = 100; 
            }
        } 
        
        // el else va atado a si el sensor no detecto teclas en esta vuelta
        if (isRead <= 0) {
            // si se deja de tocar la tecla se baja a active decay d a poco
            if (activeDecay > 0) {
                activeDecay--;
                if (activeDecay == 0) {
                    baseHz = 0.0f; 
                } // si ya llego a 0 dejamos en silencio
            }
        }

        // calculamos incremento de la fase
        float phaseIncrement = (TAU * baseHz) / SAMPLE_RATE;

        for(int i = 0; i < BUFFER_SIZE; i++){
            if (baseHz == 0.0f){
                buffer[i] = 127; // si hay silencio, dejamos la membrana del parlante en reposo
            } else {
                float wave = sinf(phase); // calculamos el seno flotante de la fase
                
                // un poco de gain
                float gain = 6.0f;
                wave = wave * gain;
                if (wave > 1.0f)  wave = 1.0f;
                if (wave < -1.0f) wave = -1.0f;
                
                buffer[i] = (unsigned char)((wave + 1.0f) * 127.5f); // mapeo a byte
                phase += phaseIncrement; // incrementamos la fase
                // si la fase supera a tau le restamos tau
                if (phase >= TAU){
                    phase -= TAU;
                }
            }
        }
        // mandamos el buffer por la pipe
        fwrite(buffer, 1, BUFFER_SIZE, pipeOut);
        fflush(pipeOut);
        
    }


    // volvemos a cooked mode
    returnCookedMode();
    // cerramos el pipe
    pclose(pipeOut);
    return 0;
}